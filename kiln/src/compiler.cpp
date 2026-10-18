#include "compiler.hpp"

#include "platform.hpp"
#include "project.hpp"
#include "status.hpp"
#include "configuration.hpp"
#include "../project/sources.hpp"
#include "linker.hpp"

#include <nlohmann/json.hpp>
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <memory>
#include <vector>

extern "C" std::FILE* compiler_open_file(const char* path, const char* mode) {
	return platform_open_file(std::filesystem::u8path(path), mode);
}

extern "C" int kiln_emit_target(Compiler* compiler, const char* root, const char* name, const char* output, const char* entry, int optimization, std::FILE* diagnostics);

std::filesystem::path kiln_artifact(const std::filesystem::path& root, std::string_view target, std::string_view profile) {
	const char* digits = "0123456789abcdef";
	std::string directory = "target-";
	for (unsigned char character : target) {
		directory += digits[character >> 4];
		directory += digits[character & 15];
	}
	return root / ".rt" / "build" / std::filesystem::u8path(profile) / directory / std::filesystem::u8path(std::string(target) + RUTILE_EXECUTABLE_SUFFIX);
}

std::string kiln_build_failure(std::string_view message) {
	return nlohmann::json{ { "succeeded", false }, { "diagnostics", message } }.dump(-1, ' ', false, nlohmann::json::error_handler_t::replace);
}

std::string kiln_diagnostics(std::FILE* file) {
	std::rewind(file);
	std::string text;
	char buffer[4096];
	while (true) {
		std::size_t count = std::fread(buffer, 1, sizeof(buffer), file);
		if (!count) {
			break;
		}
		text.append(buffer, count);
		if (text.size() >= 128 * 1024) {
			text += "\nFurther diagnostics omitted.\n";
			break;
		}
	}
	return text;
}

std::string kiln_build(struct Compiler* compiler, std::string_view request, build_input source_input) {
	std::error_code error;
	auto root = std::filesystem::current_path(error);
	if (error) {
		return kiln_build_failure("Could not determine the project folder.\n");
	}
	struct ProjectConfiguration configuration;
	std::string configuration_error;
	if (!project_configuration_read(root, configuration, configuration_error)) {
		return kiln_build_failure(configuration_error);
	}
	std::string target_name;
	std::string profile_name;
	try {
		auto input = nlohmann::json::parse(request);
		target_name = input.at("target").get<std::string>();
		profile_name = input.at("profile").get<std::string>();
	}
	catch (const nlohmann::json::exception&) {
		return kiln_build_failure("Invalid run request.\n");
	}
	if (target_name.empty()) {
		if (configuration.targets.size() != 1) {
			return kiln_build_failure("Choose a target to run. Run rutile list to see the targets.\n");
		}
		target_name = configuration.targets.front().name;
	}
	auto target = std::find_if(configuration.targets.begin(), configuration.targets.end(), [&](const struct ProjectTarget& item) {
		return item.name == target_name;
	});
	if (target == configuration.targets.end()) {
		return kiln_build_failure("Unknown target '" + target_name + "'. Run rutile list.\n");
	}
	auto profile = std::find_if(configuration.profiles.begin(), configuration.profiles.end(), [&](const struct ProjectProfile& item) {
		return item.name == profile_name;
	});
	if (profile == configuration.profiles.end()) {
		return kiln_build_failure("Unknown build profile '" + profile_name + "'.\n");
	}
	auto output = kiln_artifact(root, target->name, profile->name);
	std::vector<std::filesystem::path> sources;
	std::string source_diagnostics;
	if (!project_target_sources(*target, sources, source_diagnostics)) {
		return kiln_build_failure(source_diagnostics);
	}
	std::filesystem::create_directories(output.parent_path(), error);
	if (error) {
		return kiln_build_failure("Could not create .rt/build.\n");
	}
	std::unique_ptr<std::FILE, decltype(&std::fclose)> diagnostics{ std::tmpfile(), std::fclose };
	if (!diagnostics) {
		return kiln_build_failure("Could not create compiler diagnostics buffer.\n");
	}
	std::vector<std::string> paths;
	for (const auto& source : sources) {
		paths.push_back(project_path_utf8(source));
	}
	std::vector<const char*> arguments;
	for (const auto& path : paths) {
		arguments.push_back(path.c_str());
	}
	auto object = output;
	object.replace_extension(".o");
	auto log = output.parent_path() / "diagnostics.log";
	auto staged = output.parent_path() / (project_path_utf8(output.stem()) + ".pending" + RUTILE_EXECUTABLE_SUFFIX);
	for (const auto& source : sources) {
		for (const auto& destination : { output, object, log, staged }) {
			std::error_code comparison_error;
			if (std::filesystem::equivalent(source, destination, comparison_error)) {
				return kiln_build_failure("Build output would overwrite a source file.\n");
			}
			if (comparison_error && comparison_error != std::errc::no_such_file_or_directory) {
				return kiln_build_failure("Could not verify the build output paths.\n");
			}
		}
	}
	std::filesystem::remove(staged, error);
	if (error) {
		return kiln_build_failure("Could not prepare the executable output.\n");
	}
	auto start = std::chrono::steady_clock::now();
	int result = 1;
	if (source_input == build_input::editor) {
		result = kiln_emit_target(compiler, project_path_utf8(root).c_str(), target->name.c_str(), project_path_utf8(object).c_str(), target->entry.c_str(), profile->optimization, diagnostics.get());
	}
	else {
		result = rutile_compile(compiler, arguments.data(), arguments.size(), project_path_utf8(object).c_str(), compiler_open_file, target->entry.c_str(), profile->optimization, diagnostics.get());
	}
	std::string messages = kiln_diagnostics(diagnostics.get());
	if (!result) {
		result = kiln_link(object, staged, log);
		std::unique_ptr<std::FILE, decltype(&std::fclose)> backend_log{ platform_open_file(log, "rb"), std::fclose };
		if (backend_log) {
			messages += kiln_diagnostics(backend_log.get());
		}
		if (!result && !platform_replace_file(staged, output)) {
			result = 1;
			messages += "Could not publish the compiled executable.\n";
		}
	}
	std::filesystem::remove(staged, error);
	auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start).count();
	project_record_compilation(target->source, output, sources.size(), duration, !result);
	if (result) {
		return kiln_build_failure(messages.empty() ? "Compilation failed.\n" : messages);
	}
	return nlohmann::json{ { "succeeded", true }, { "executable", project_path_utf8(output) } }.dump();
}

std::string kiln_list() {
	std::error_code error;
	auto root = std::filesystem::current_path(error);
	auto executables = nlohmann::json::array();
	if (error) {
		return kiln_build_failure("Could not determine the project folder.\n");
	}
	struct ProjectConfiguration configuration;
	std::string configuration_error;
	if (!project_configuration_read(root, configuration, configuration_error)) {
		return kiln_build_failure(configuration_error);
	}
	for (const auto& target : configuration.targets) {
		executables.push_back(target.name);
	}
	return executables.dump();
}
