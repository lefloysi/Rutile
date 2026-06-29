#include "basic_diagnostics.hpp"
#include "compiler.hpp"
#include "linker.hpp"
#include "artifact.hpp"
#include "text_rtir.hpp"
#include "rtsl.h"

#include <CLI/CLI.hpp>

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace {

std::vector<std::uint8_t> read_file(const std::string &path) {
    std::ifstream input(path, std::ios::binary);
    if (!input) return {};
    return {std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
}

bool write_bytes(const std::string &path, const std::vector<rtsl::u8> &bytes) {
    std::ofstream output(path, std::ios::binary);
    if (!output) return false;
    output.write(reinterpret_cast<const char *>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    return output.good();
}

void print_engine_diagnostics(const rtsl::DiagnosticEngine &diagnostics) {
    diagnostics.render(std::cerr);
}

std::string with_extension(std::string_view path, std::string_view new_ext) {
    std::filesystem::path p{std::string(path)};
    p.replace_extension(std::string(new_ext));
    return p.string();
}

int compile_mode(const std::string &input_path, const std::string &output, bool verbose) {
    const auto source = read_file(input_path);
    if (source.empty()) {
        std::cerr << "rtslc: failed to read " << input_path << '\n';
        return 1;
    }

    if (verbose) {
        std::cerr << "rtslc: compiling " << input_path << " -> " << output << '\n';
    }

    rtsl::CompilerInstance compiler;
    rtsl::CompilerInvocation invocation{.source_name = input_path};
    rtsl::Artifact artifact;
    compiler.compile_source_to(
        artifact,
        std::string_view(reinterpret_cast<const char *>(source.data()), source.size()),
        std::move(invocation));

    if (compiler.diagnostics().has_error() || artifact.bytes.empty()) {
        print_engine_diagnostics(compiler.diagnostics());
        return 1;
    }
    if (!write_bytes(output, artifact.bytes)) {
        std::cerr << "rtslc: failed to write " << output << '\n';
        return 1;
    }

    const rtsl::Artifact module_artifact = rtsl::extract_module_interface(artifact);
    if (!module_artifact.bytes.empty()) {
        const std::string module_path = with_extension(output, ".rtslm");
        if (!write_bytes(module_path, module_artifact.bytes)) {
            std::cerr << "rtslc: failed to write " << module_path << '\n';
            return 1;
        }
    }
    return 0;
}

int link_mode(const std::vector<std::string> &inputs, const std::string &output, bool produce_program, bool verbose) {
    if (inputs.empty()) {
        std::cerr << "rtslc: no inputs provided\n";
        return 1;
    }
    if (verbose) {
        std::cerr << "rtslc: linking " << inputs.size() << " inputs -> " << output << '\n';
    }

    rtsl::DiagnosticEngine diagnostics;
    rtsl::Linker linker(diagnostics);
    for (const auto &input_path : inputs) {
        const auto bytes = read_file(input_path);
        if (bytes.empty()) {
            std::cerr << "rtslc: failed to read " << input_path << '\n';
            return 1;
        }
        if (!linker.add_artifact_bytes(std::span<const rtsl::u8>(bytes.data(), bytes.size()))) {
            print_engine_diagnostics(diagnostics);
            return 1;
        }
    }

    rtsl::Artifact linked = produce_program ? linker.link_program() : linker.link_library();
    if (diagnostics.has_error() || linked.bytes.empty()) {
        print_engine_diagnostics(diagnostics);
        return 1;
    }
    if (!write_bytes(output, linked.bytes)) {
        std::cerr << "rtslc: failed to write " << output << '\n';
        return 1;
    }

    if (!produce_program) {
        const rtsl::Artifact module_artifact = rtsl::extract_module_interface(linked);
        if (!module_artifact.bytes.empty()) {
            const std::string module_path = with_extension(output, ".rtslm");
            if (!write_bytes(module_path, module_artifact.bytes)) {
                std::cerr << "rtslc: failed to write " << module_path << '\n';
                return 1;
            }
        }
    }
    return 0;
}

int dump_mode(const std::string &input_path) {
    const auto bytes = read_file(input_path);
    rtsl::Artifact artifact;
    rtsl::DiagnosticEngine diagnostics;
    if (bytes.empty() || !rtsl::read_artifact(bytes, artifact, &diagnostics)) {
        std::cerr << "rtslc: failed to read artifact " << input_path << '\n';
        print_engine_diagnostics(diagnostics);
        return 1;
    }
    std::cout << rtsl::disassemble_artifact(artifact);
    return 0;
}

int assemble_mode(const std::string &input_path, const std::string &output) {
    const auto bytes = read_file(input_path);
    rtsl::Artifact artifact;
    rtsl::DiagnosticEngine diagnostics;
    const std::string text(reinterpret_cast<const char *>(bytes.data()), bytes.size());
    if (bytes.empty() || !rtsl::assemble_text_rtir(text, artifact, &diagnostics)) {
        std::cerr << "rtslc: failed to assemble RTIR " << input_path << '\n';
        print_engine_diagnostics(diagnostics);
        return 1;
    }
    if (!write_bytes(output, artifact.bytes)) {
        std::cerr << "rtslc: failed to write " << output << '\n';
        return 1;
    }
    return 0;
}

void add_common_options(CLI::App &app, bool &verbose) {
    app.set_version_flag("--version", "rtslc 0.1");
    app.add_flag("-v,--verbose", verbose, "Print extra driver diagnostics");
}

} // namespace

int main(int argc, char **argv) {
    CLI::App app{"RTSL compiler driver"};
    app.require_subcommand(0, 1);

    bool verbose = false;
    add_common_options(app, verbose);

    auto compile = app.add_subcommand("compile", "Compile source to an object");
    std::string compile_input;
    std::string compile_output;
    compile->add_option("input", compile_input, "Input .rtsl file")->required();
    compile->add_option("-o,--output", compile_output, "Output .rtslo file")->required();

    auto link_program = app.add_subcommand("link-program", "Link objects/libraries into a program");
    std::vector<std::string> link_program_inputs;
    std::string link_program_output;
    link_program->add_option("inputs", link_program_inputs, "Input .rtslo/.rtsll files")->required();
    link_program->add_option("-o,--output", link_program_output, "Output .rtslp file")->required();

    auto link_library = app.add_subcommand("link-library", "Link objects/libraries into a library");
    std::vector<std::string> link_library_inputs;
    std::string link_library_output;
    link_library->add_option("inputs", link_library_inputs, "Input .rtslo/.rtsll files")->required();
    link_library->add_option("-o,--output", link_library_output, "Output .rtsll file")->required();

    auto dump = app.add_subcommand("dump", "Disassemble any RTSL artifact");
    std::string dump_input;
    dump->add_option("input", dump_input, "Input artifact")->required();

    auto assemble = app.add_subcommand("assemble", "Assemble textual RTIR to an object");
    std::string assemble_input;
    std::string assemble_output;
    assemble->add_option("input", assemble_input, "Input .rtir file")->required();
    assemble->add_option("-o,--output", assemble_output, "Output .rtslo file")->required();

    CLI11_PARSE(app, argc, argv);

    if (*compile) return compile_mode(compile_input, compile_output, verbose);
    if (*link_program) return link_mode(link_program_inputs, link_program_output, true, verbose);
    if (*link_library) return link_mode(link_library_inputs, link_library_output, false, verbose);
    if (*dump) return dump_mode(dump_input);
    if (*assemble) return assemble_mode(assemble_input, assemble_output);
    return 0;
}
