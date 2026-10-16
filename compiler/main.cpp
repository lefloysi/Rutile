#include <CLI/CLI.hpp>
#include <cstddef>
#include <cstdio>
#include <filesystem>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

struct Compiler;

extern "C" struct Compiler* rutile_compiler_create();
extern "C" void rutile_compiler_destroy(struct Compiler* compiler);
extern "C" int rutile_compile_library(struct Compiler* compiler, const char* const* source_paths, std::size_t source_count, const char* output_path, std::FILE* (*open_file)(const char*, const char*), int optimization, std::FILE* diagnostics);

std::FILE* platform_open_file(const std::filesystem::path& path, const char* mode);
extern const std::string_view software_version;

extern "C" std::FILE* bootstrap_open_file(const char* path, const char* mode) {
	return platform_open_file(std::filesystem::u8path(path), mode);
}

int main(int argc, char* argv[]) {
	CLI::App app{ "Build a native library object for bootstrapping." };
	app.set_version_flag("--version", std::string(software_version));
	argv = app.ensure_utf8(argv);
	std::vector<std::string> sources;
	std::string output;
	int optimization = 0;
	app.add_option("sources", sources, "Rutile source files.")->required();
	app.add_option("-o,--output", output, "Native object output.")->required();
	app.add_option("--optimization", optimization, "Optimization level.")->check(CLI::Range(0, 3));
	CLI11_PARSE(app, argc, argv);

	auto destination = std::filesystem::u8path(output);
	std::error_code error;
	for (const auto& source : sources) {
		if (std::filesystem::equivalent(std::filesystem::u8path(source), destination, error)) {
			std::fputs("Output would overwrite a source file.\n", stderr);
			return 1;
		}
		if (error && error != std::errc::no_such_file_or_directory) {
			std::fputs("Could not verify the output path.\n", stderr);
			return 1;
		}
	}
	if (!destination.parent_path().empty()) {
		std::filesystem::create_directories(destination.parent_path(), error);
		if (error) {
			std::fputs("Could not create the object output folder.\n", stderr);
			return 1;
		}
	}
	std::unique_ptr<struct Compiler, decltype(&rutile_compiler_destroy)> compiler{ rutile_compiler_create(), rutile_compiler_destroy };
	if (!compiler) {
		std::fputs("Could not create the compiler.\n", stderr);
		return 1;
	}
	std::vector<const char*> paths;
	for (const auto& source : sources) {
		paths.push_back(source.c_str());
	}
	return rutile_compile_library(compiler.get(), paths.data(), paths.size(), output.c_str(), bootstrap_open_file, optimization, stderr);
}
