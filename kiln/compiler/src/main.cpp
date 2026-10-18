#include "platform.hpp"

#include <CLI/CLI.hpp>
#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

extern "C" int rutile_compile_library(const char* const* paths, std::size_t count,
	const char* output, std::FILE* (*open)(const char*, const char*), int optimization,
	std::FILE* diagnostics);
extern const std::string_view software_version;

extern "C" std::FILE* compiler_open_native_file(const char* path, const char* mode) {
	return platform_open_file(std::filesystem::u8path(path), mode);
}

int main(int argc, char** argv) {
	CLI::App command{ "Compile Rutile sources into a native object." };
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
			if (cursor->is_regular_file(error)) {
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
