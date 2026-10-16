#include <CLI/CLI.hpp>
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdio>
#include <filesystem>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

extern "C" int rutile_compile_library(const char* const* source_paths, std::size_t source_count, const char* output_path, std::FILE* (*open_file)(const char*, const char*), int optimization, std::FILE* diagnostics);

std::FILE* platform_open_file(const std::filesystem::path& path, const char* mode);
extern const std::string_view software_version;

std::vector<std::filesystem::path> bootstrap_files(const std::filesystem::path& directory, std::error_code& error) {
	std::vector<std::filesystem::path> files;
	for (std::filesystem::recursive_directory_iterator entry{ directory, error }, end; !error && entry != end; entry.increment(error)) {
		const auto regular = entry->is_regular_file(error);
		if (error) {
			return {};
		}
		if (regular) {
			files.push_back(entry->path());
		}
	}
	if (error) {
		return {};
	}
	std::sort(files.begin(), files.end(), [&directory](const auto& first, const auto& second) {
		return first.lexically_relative(directory).generic_u8string() < second.lexically_relative(directory).generic_u8string();
	});
	return files;
}

extern "C" std::FILE* bootstrap_open_file(const char* path, const char* mode) {
	auto source = std::filesystem::u8path(path);
	std::error_code error;
	if (std::string_view(mode) != "rb" || !std::filesystem::is_directory(source, error)) {
		if (error && error != std::errc::no_such_file_or_directory) {
			std::fputs("Could not inspect source input.\n", stderr);
			return nullptr;
		}
		return platform_open_file(source, mode);
	}
	auto files = bootstrap_files(source, error);
	if (error) {
		std::fputs("Could not list source directory.\n", stderr);
		return nullptr;
	}
	std::unique_ptr<std::FILE, decltype(&std::fclose)> joined{ std::tmpfile(), std::fclose };
	if (!joined) {
		std::fputs("Could not create concatenated source stream.\n", stderr);
		return nullptr;
	}
	std::array<char, 65536> bytes;
	for (const auto& file : files) {
		std::unique_ptr<std::FILE, decltype(&std::fclose)> input{ platform_open_file(file, "rb"), std::fclose };
		if (!input) {
			std::fputs("Could not open a source directory member.\n", stderr);
			return nullptr;
		}
		while (true) {
			const auto size = std::fread(bytes.data(), 1, bytes.size(), input.get());
			if (size && std::fwrite(bytes.data(), 1, size, joined.get()) != size) {
				std::fputs("Could not write concatenated source stream.\n", stderr);
				return nullptr;
			}
			if (size < bytes.size()) {
				if (std::ferror(input.get())) {
					std::fputs("Could not read a source directory member.\n", stderr);
					return nullptr;
				}
				break;
			}
		}
		if (std::fclose(input.release())) {
			std::fputs("Could not close a source directory member.\n", stderr);
			return nullptr;
		}
	}
	if (std::fflush(joined.get()) || std::fseek(joined.get(), 0, SEEK_SET)) {
		std::fputs("Could not rewind concatenated source stream.\n", stderr);
		return nullptr;
	}
	return joined.release();
}

int main(int argc, char* argv[]) {
	CLI::App app{ "Build a native library object for bootstrapping." };
	app.set_version_flag("--version", std::string(software_version));
	argv = app.ensure_utf8(argv);
	std::vector<std::string> sources;
	std::string output;
	int optimization = 0;
	app.add_option("sources", sources, "Rutile source files or directories (all files recursively concatenated by relative path).")->required();
	app.add_option("-o,--output", output, "Native object output.")->required();
	app.add_option("--optimization", optimization, "Optimization level.")->check(CLI::Range(0, 3));
	CLI11_PARSE(app, argc, argv);

	auto destination = std::filesystem::u8path(output);
	std::error_code error;
	for (const auto& source : sources) {
		const auto input = std::filesystem::u8path(source);
		if (std::filesystem::equivalent(input, destination, error)) {
			std::fputs("Output would overwrite a source file.\n", stderr);
			return 1;
		}
		if (error && error != std::errc::no_such_file_or_directory) {
			std::fputs("Could not verify the output path.\n", stderr);
			return 1;
		}
		if (!std::filesystem::is_directory(input, error)) {
			if (error && error != std::errc::no_such_file_or_directory) {
				std::fputs("Could not inspect source input.\n", stderr);
				return 1;
			}
			continue;
		}
		const auto files = bootstrap_files(input, error);
		if (error) {
			std::fputs("Could not list source directory.\n", stderr);
			return 1;
		}
		for (const auto& file : files) {
			if (std::filesystem::equivalent(file, destination, error)) {
				std::fputs("Output would overwrite a source file.\n", stderr);
				return 1;
			}
			if (error && error != std::errc::no_such_file_or_directory) {
				std::fputs("Could not verify the output path.\n", stderr);
				return 1;
			}
		}
	}
	if (!destination.parent_path().empty()) {
		std::filesystem::create_directories(destination.parent_path(), error);
		if (error) {
			std::fputs("Could not create the object output folder.\n", stderr);
			return 1;
		}
	}
	std::vector<const char*> paths;
	for (const auto& source : sources) {
		paths.push_back(source.c_str());
	}
	return rutile_compile_library(paths.data(), paths.size(), output.c_str(), bootstrap_open_file, optimization, stderr);
}
