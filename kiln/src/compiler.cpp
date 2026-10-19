#include "compiler.hpp"

#include "platform.hpp"
#include "project.hpp"
#include "status.hpp"
#include "configuration.hpp"
#include "../project/sources.hpp"
#include "../configuration/execution.hpp"

#include <nlohmann/json.hpp>
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <memory>
#include <vector>

CompilerIntegration::~CompilerIntegration() = default;

bool CompilerRegistry::add(std::unique_ptr<CompilerIntegration> integration) {
	if (!integration || integration->name().empty() || find(integration->name())) {
		return false;
	}
	integrations.push_back(std::move(integration));
	return true;
}

CompilerIntegration* CompilerRegistry::find(std::string_view name) const {
	for (const auto& integration : integrations) {
		if (integration->name() == name) {
			return integration.get();
		}
	}
	return nullptr;
}

std::filesystem::path kiln_artifact(const std::filesystem::path& root, std::string_view target, std::string_view profile, const std::filesystem::path& filename) {
	const char* digits = "0123456789abcdef";
	std::string directory = "target-";
	for (unsigned char character : target) {
		directory += digits[character >> 4];
		directory += digits[character & 15];
	}
	return root / ".rt" / "build" / std::filesystem::u8path(profile) / directory / filename;
}

std::string kiln_build_failure(std::string_view message) {
	return nlohmann::json{ { "succeeded", false }, { "diagnostics", message } }.dump(-1, ' ', false, nlohmann::json::error_handler_t::replace);
}

std::string kiln_build(CompilerRegistry& compilers, std::string_view request, build_input source_input) {
	std::error_code error;
	auto root = std::filesystem::current_path(error);
	if (error) {
		return kiln_build_failure("Could not determine the project folder.\n");
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
	struct ProjectConfiguration configuration;
	std::string configuration_error;
	if (!project_configuration_read(root, configuration, configuration_error) || !kiln_configure_project(root, configuration, configuration_error, profile_name)) {
		return kiln_build_failure(configuration_error);
	}
	if (target_name.empty()) {
		if (configuration.targets.size() != 1) {
			return kiln_build_failure("Choose a target to run. Run kiln list to see the targets.\n");
		}
		target_name = configuration.targets.front().name;
	}
	auto target = std::find_if(configuration.targets.begin(), configuration.targets.end(), [&](const struct ProjectTarget& item) {
		return item.name == target_name;
	});
	if (target == configuration.targets.end()) {
		return kiln_build_failure("Unknown target '" + target_name + "'. Run kiln list.\n");
	}
	auto profile = std::find_if(configuration.profiles.begin(), configuration.profiles.end(), [&](const struct ProjectProfile& item) {
		return item.name == profile_name;
	});
	if (profile == configuration.profiles.end()) {
		return kiln_build_failure("Unknown build profile '" + profile_name + "'.\n");
	}
	CompilerIntegration* integration = compilers.find(target->compiler);
	if (!integration) {
		return kiln_build_failure("Unknown compiler integration '" + target->compiler + "' for target '" + target->name + "'.\n");
	}
	std::string messages;
	if (!integration->validate(*target, *profile, messages)) {
		return kiln_build_failure(messages);
	}
	auto artifact = integration->artifact(*target);
	if (artifact.filename.empty() || artifact.filename != artifact.filename.filename() || artifact.filename == "." || artifact.filename == "..") {
		return kiln_build_failure("Compiler integration returned an invalid artifact filename.\n");
	}
	auto output = kiln_artifact(root, target->name, profile->name, artifact.filename);
	std::vector<std::filesystem::path> sources;
	if (!project_target_sources(*target, sources, [&](std::string_view compiler, const std::filesystem::path& path) {
		return compiler == integration->name() && (path.empty() || integration->accepts_source(path));
	}, messages)) {
		return kiln_build_failure(messages);
	}
	for (const auto& program : configuration.projects) {
		std::erase(sources, program.source);
	}
	if (sources.empty()) {
		return kiln_build_failure("Target has no compiler sources after excluding configuration programs.\n");
	}
	std::filesystem::create_directories(output.parent_path(), error);
	if (error) {
		return kiln_build_failure("Could not create .rt/build.\n");
	}
	auto staged = output.parent_path() / (project_path_utf8(output.stem()) + ".pending" + project_path_utf8(output.extension()));
	for (const auto& source : sources) {
		for (const auto& destination : { output, staged }) {
			std::error_code comparison_error;
			if (source.lexically_normal() == destination.lexically_normal() || std::filesystem::equivalent(source, destination, comparison_error)) {
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
	auto result = integration->build(CompilerBuild{ root, *target, *profile, sources, staged, source_input });
	if (result.succeeded && !platform_replace_file(staged, output)) {
		result.succeeded = false;
		result.diagnostics += "Could not publish the compiled executable.\n";
	}
	std::filesystem::remove(staged, error);
	auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start).count();
	project_record_compilation(target->source, output, sources.size(), duration, result.succeeded);
	if (!result.succeeded) {
		return kiln_build_failure(result.diagnostics.empty() ? "Compilation failed.\n" : result.diagnostics);
	}
	auto response = nlohmann::json{ { "succeeded", true }, { "artifact", project_path_utf8(output) } };
	if (artifact.usage == artifact_usage::executable) {
		response["executable"] = project_path_utf8(output);
	}
	return response.dump();
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
	if (!project_configuration_read(root, configuration, configuration_error) || !kiln_configure_project(root, configuration, configuration_error)) {
		return kiln_build_failure(configuration_error);
	}
	for (const auto& target : configuration.targets) {
		executables.push_back(target.name);
	}
	return executables.dump();
}
