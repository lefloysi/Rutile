#include "service.hpp"

#include "platform.hpp"

#include <CLI/CLI.hpp>
#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <nlohmann/json.hpp>
#include <string>
#include <string_view>
#include <vector>
#include <variant>

struct Compiler;
extern "C" int rutile_compiler_session(int (*callback)(Compiler*, void*), void* data);
extern "C" int kiln_language_server(Compiler* compiler, std::FILE* input, std::FILE* output, const char* root);
extern "C" int rutile_compile_library(const char* const* paths, std::size_t count,
	const char* output, std::FILE* (*open)(const char*, const char*), int optimization,
	std::FILE* diagnostics);
extern const std::string_view software_version;

int rutile_language_server_session(Compiler* compiler, void* context) {
	auto* root = static_cast<std::string*>(context);
	return kiln_language_server(compiler, platform_standard_input(), platform_standard_output(), root->c_str());
}

extern "C" std::FILE* compiler_open_native_file(const char* path, const char* mode) {
	return platform_open_file(std::filesystem::u8path(path), mode);
}

int main(int argc, char** argv) {
	if (argc == 3 && std::string_view(argv[1]) == "serve") {
		return rutile_service_serve(argv[2]);
	}
	if (argc == 3 && std::string_view(argv[1]) == "language-server" && std::string_view(argv[2]) == "--stdio") {
		if (!platform_language_stdio()) {
			std::fputs("Could not configure the IDE connection streams.\n", stderr);
			return 1;
		}
		std::error_code error;
		const auto directory = std::filesystem::current_path(error);
		if (error) {
			std::fputs("Could not determine the project directory.\n", stderr);
			return 1;
		}
		const auto encoded = directory.generic_u8string();
		std::string root(reinterpret_cast<const char*>(encoded.data()), encoded.size());
		return rutile_compiler_session(rutile_language_server_session, &root);
	}
	if (argc == 2 && (std::string_view(argv[1]) == "status" || std::string_view(argv[1]) == "exit" || std::string_view(argv[1]) == "progress")) {
		return rutile_service_control(argv[1]);
	}
	if (argc >= 2 && std::string_view(argv[1]) == "request") {
		if (argc < 3 || argc > 4) {
			std::fputs("Usage: rutilec request <operation> [payload.json|-]\n", stderr);
			return 1;
		}
		std::string request = argv[2];
		if (argc == 4) {
			std::ifstream file;
			std::istream* input = &std::cin;
			if (std::string_view(argv[3]) != "-") {
				file.open(std::filesystem::u8path(argv[3]), std::ios::binary);
				if (!file) {
					std::fputs("Cannot read the request payload file.\n", stderr);
					return 1;
				}
				input = &file;
			}
			std::string payload;
			char buffer[4096];
			while (input->read(buffer, sizeof(buffer)) || input->gcount()) {
				payload.append(buffer, static_cast<std::size_t>(input->gcount()));
				if (payload.size() > 8 * 1024 * 1024) {
					std::fputs("Compiler request payload exceeds 8 MiB.\n", stderr);
					return 1;
				}
			}
			if (input->bad()) {
				std::fputs("Cannot finish reading the request payload.\n", stderr);
				return 1;
			}
			try {
				request += " " + nlohmann::json::parse(payload).dump();
			}
			catch (const nlohmann::json::exception& error) {
				std::fprintf(stderr, "Invalid JSON request payload: %s\n", error.what());
				return 1;
			}
		}
		return rutile_service_control(request.c_str());
	}
	CLI::App command{ "Compile Rutile sources into a native object." };
	command.footer("Persistent compiler: rutilec status, rutilec progress, rutilec exit.\nSend an operation: rutilec request <operation> [payload.json|-].\nOperations include sources, configure, compile, result <request_id>, and progress <request_id>.\nQuote operations containing a request ID. Use '-' to read a JSON payload from stdin.\nKiln starts the persistent compiler and supplies its project configuration.");
	command.set_version_flag("--version", std::string(software_version));
	argv = command.ensure_utf8(argv);
	std::vector<std::string> inputs;
	std::string output;
	int optimization = 0;
	command.add_option("sources", inputs, "Source files or directories.")->required();
	command.add_option("-o,--output", output, "Native object output.")->required();
	command.add_option("--optimization", optimization, "Optimization level.")->check(CLI::Range(0, 3));
	CLI11_PARSE(command, argc, argv);

	std::error_code error;
	std::vector<std::filesystem::path> sources;
	for (const auto& input : inputs) {
		const auto path = std::filesystem::u8path(input);
		if (!std::filesystem::is_directory(path, error)) {
			if (error && error != std::errc::no_such_file_or_directory) {
				std::fputs("Cannot inspect source input.\n", stderr);
				return 1;
			}
			sources.push_back(path);
			continue;
		}
		std::vector<std::filesystem::path> members;
		for (std::filesystem::recursive_directory_iterator cursor{ path, error }, end;
			!error && cursor != end; cursor.increment(error)) {
			if (cursor->is_regular_file(error) && cursor->path().extension() == ".rt") {
				members.push_back(cursor->path());
			}
		}
		if (error) {
			std::fputs("Cannot enumerate source directory.\n", stderr);
			return 1;
		}
		std::sort(members.begin(), members.end(), [&path](const auto& first, const auto& second) {
			return first.lexically_relative(path).generic_u8string()
				< second.lexically_relative(path).generic_u8string();
		});
		sources.insert(sources.end(), members.begin(), members.end());
	}
	const auto destination = std::filesystem::u8path(output);
	for (const auto& source : sources) {
		if (std::filesystem::equivalent(source, destination, error)) {
			std::fputs("Output would overwrite a source file.\n", stderr);
			return 1;
		}
		if (error && error != std::errc::no_such_file_or_directory) {
			std::fputs("Cannot verify the output path.\n", stderr);
			return 1;
		}
	}
	if (!destination.parent_path().empty()) {
		std::filesystem::create_directories(destination.parent_path(), error);
		if (error) {
			std::fputs("Cannot create the output directory.\n", stderr);
			return 1;
		}
	}
	std::vector<std::string> names;
	for (const auto& source : sources) {
		const auto bytes = source.u8string();
		names.emplace_back(bytes.begin(), bytes.end());
	}
	std::vector<const char*> paths;
	for (const auto& name : names) {
		paths.push_back(name.c_str());
	}
	return rutile_compile_library(paths.data(), paths.size(), output.c_str(),
		compiler_open_native_file, optimization, stderr);
}
