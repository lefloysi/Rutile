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
#include <set>
#include <sstream>
#include <cstdint>

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
	return root / ".kiln" / "build" / std::filesystem::u8path(profile) / directory / filename;
}

std::string kiln_build_failure(std::string_view message) {
	return nlohmann::json{ { "succeeded", false }, { "diagnostics", message } }.dump(-1, ' ', false, nlohmann::json::error_handler_t::replace);
}

bool kiln_publish(const std::filesystem::path& source, const std::filesystem::path& destination) {
	unsigned int attempt = 0;
	while (attempt < 20) {
		if (platform_replace_file(source, destination)) {
			return true;
		}
		++attempt;
		if (attempt < 20) {
			platform_sleep(50);
		}
	}
	return false;
}

bool kiln_ignored_folder(const std::filesystem::path& path) {
	auto name = path.filename();
	return name == ".rt" || name == ".kiln" || name == ".git" || name == ".vs" || name == "build";
}

bool kiln_same_contents(const std::filesystem::path& first, const std::filesystem::path& second) {
	std::ifstream left(first, std::ios::binary);
	std::ifstream right(second, std::ios::binary);
	if (!left || !right) {
		return false;
	}
	return std::equal(std::istreambuf_iterator<char>(left), std::istreambuf_iterator<char>(), std::istreambuf_iterator<char>(right), std::istreambuf_iterator<char>());
}

std::string kiln_fingerprint(const std::vector<std::filesystem::path>& inputs) {
	std::set<std::filesystem::path> files;
	std::string failures;
	for (const auto& input : inputs) {
		std::error_code error;
		files.insert(input);
		if (!std::filesystem::is_directory(input, error)) {
			continue;
		}
		std::filesystem::recursive_directory_iterator cursor(input, std::filesystem::directory_options::skip_permission_denied, error);
		while (!error && cursor != std::filesystem::recursive_directory_iterator{}) {
			if (cursor->is_directory(error) && kiln_ignored_folder(cursor->path())) {
				cursor.disable_recursion_pending();
			}
			else if (!error && cursor->is_regular_file(error)) {
				files.insert(cursor->path());
			}
			cursor.increment(error);
		}
		if (error) {
			failures += error.message();
		}
	}
	std::uint64_t hash = 14695981039346656037ull;
	auto append = [&](std::string_view text) {
		for (unsigned char byte : text) {
			hash = (hash ^ byte) * 1099511628211ull;
		}
	};
	append(failures);
	for (const auto& file : files) {
		append(project_path_utf8(file));
		std::error_code error;
		auto stamp = std::filesystem::last_write_time(file, error);
		append(error ? error.message() : std::to_string(stamp.time_since_epoch().count()));
		if (std::filesystem::is_regular_file(file, error)) {
			append(std::to_string(std::filesystem::file_size(file, error)));
		}
	}
	return std::to_string(hash);
}

std::vector<std::filesystem::path> kiln_configuration_inputs(const std::filesystem::path& root, const ProjectConfiguration& configuration) {
	std::vector<std::filesystem::path> inputs{ project_configuration_path(root) };
	for (const auto& program : configuration.projects) {
		inputs.push_back(program.source);
	}
	if (!configuration.projects.empty()) {
		inputs.push_back(std::filesystem::u8path(KILN_CONFIGURATION_API));
		inputs.push_back(std::filesystem::u8path(KILN_CONFIGURATION_STDLIB));
	}
	return inputs;
}

bool kiln_configuration(CompilerRegistry& compilers, const std::filesystem::path& root, std::string_view profile, ProjectConfiguration& configuration, std::string& diagnostics) {
	if (!project_configuration_read(root, configuration, diagnostics)) {
		return false;
	}
	auto key = project_path_utf8(root) + "\n" + std::string(profile);
	auto found = compilers.configurations.find(key);
	auto fingerprint = kiln_fingerprint(kiln_configuration_inputs(root, found == compilers.configurations.end() ? configuration : found->second.value));
	if (found != compilers.configurations.end() && found->second.fingerprint == fingerprint) {
		configuration = found->second.value;
		return true;
	}
	if (!kiln_configure_project(root, configuration, diagnostics, profile)) {
		return false;
	}
	fingerprint = kiln_fingerprint(kiln_configuration_inputs(root, configuration));
	compilers.configurations[key] = { std::move(fingerprint), configuration };
	return true;
}

std::vector<std::string> kiln_option_values(std::string_view text) {
	if (text.starts_with("[")) {
		try {
			return nlohmann::json::parse(text).get<std::vector<std::string>>();
		}
		catch (const nlohmann::json::exception&) {
			return {};
		}
	}
	std::vector<std::string> values;
	std::istringstream stream{ std::string(text) };
	std::string value;
	while (std::getline(stream, value, ';')) {
		if (!value.empty()) {
			values.push_back(std::move(value));
		}
	}
	return values;
}

std::vector<std::filesystem::path> kiln_build_inputs(const std::filesystem::path& root, const ProjectConfiguration& configuration, const ProjectTarget& target, const std::vector<std::filesystem::path>& dependencies) {
	auto inputs = kiln_configuration_inputs(root, configuration);
	inputs.insert(inputs.end(), target.source_folders.begin(), target.source_folders.end());
	inputs.push_back(target.source);
	inputs.insert(inputs.end(), dependencies.begin(), dependencies.end());
	if (target.compiler == "rutile") {
		inputs.push_back(std::filesystem::u8path(KILN_CONFIGURATION_STDLIB));
	}
	for (const auto* key : { "includes", "forced_includes", "objects", "runtime_files", "libraries", "library_paths" }) {
		auto option = target.options.find(key);
		if (option == target.options.end()) {
			continue;
		}
		for (const auto& value : kiln_option_values(option->second)) {
			inputs.push_back((root / std::filesystem::u8path(value)).lexically_normal());
		}
	}
	return inputs;
}

std::string kiln_build_target(CompilerRegistry& compilers, const std::filesystem::path& root, const ProjectConfiguration& configuration, const ProjectProfile& profile, const ProjectTarget& target, build_input source_input, std::set<std::string>& visiting);

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
	if (!kiln_configuration(compilers, root, profile_name, configuration, configuration_error)) {
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
	std::set<std::string> visiting;
	return kiln_build_target(compilers, root, configuration, *profile, *target, source_input, visiting);
}

std::string kiln_build_target(CompilerRegistry& compilers, const std::filesystem::path& root, const ProjectConfiguration& configuration, const ProjectProfile& selected_profile, const ProjectTarget& selected_target, build_input source_input, std::set<std::string>& visiting) {
	const auto* target = &selected_target;
	const auto* profile = &selected_profile;
	if (!visiting.insert(target->name).second) {
		return kiln_build_failure("Target dependency cycle at '" + target->name + "'.\n");
	}
	std::vector<std::filesystem::path> dependency_artifacts;
	std::vector<std::filesystem::path> runtime_artifacts;
	for (const auto& name : target->dependencies) {
		auto dependency = std::find_if(configuration.targets.begin(), configuration.targets.end(), [&](const ProjectTarget& item) {
			return item.name == name;
		});
		if (dependency == configuration.targets.end()) {
			return kiln_build_failure("Unknown dependency '" + name + "' for target '" + target->name + "'.\n");
		}
		auto result = nlohmann::json::parse(kiln_build_target(compilers, root, configuration, *profile, *dependency, source_input, visiting));
		if (!result.value("succeeded", false)) {
			return result.dump();
		}
		for (const auto& value : result.value("runtime_artifacts", nlohmann::json::array())) {
			auto path = std::filesystem::u8path(value.get<std::string>());
			if (std::find(runtime_artifacts.begin(), runtime_artifacts.end(), path) == runtime_artifacts.end()) {
				runtime_artifacts.push_back(std::move(path));
			}
		}
		auto artifacts = result.value("link_artifacts", nlohmann::json::array({ result.at("artifact") }));
		for (const auto& value : artifacts) {
			auto artifact = std::filesystem::u8path(value.get<std::string>());
#if defined(_WIN32)
			if (artifact.extension() == ".dll") {
				artifact.replace_extension(".lib");
			}
#endif
			if (std::find(dependency_artifacts.begin(), dependency_artifacts.end(), artifact) == dependency_artifacts.end()) {
				dependency_artifacts.push_back(std::move(artifact));
			}
		}
	}
	visiting.erase(target->name);
	if (auto files = target->options.find("runtime_files"); files != target->options.end()) {
		for (const auto& value : kiln_option_values(files->second)) {
			auto path = (root / std::filesystem::u8path(value)).lexically_normal();
			if (std::find(runtime_artifacts.begin(), runtime_artifacts.end(), path) == runtime_artifacts.end()) {
				runtime_artifacts.push_back(std::move(path));
			}
		}
	}
	std::error_code error;
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
	auto inputs = kiln_build_inputs(root, configuration, *target, dependency_artifacts);
	auto fingerprint = kiln_fingerprint(inputs);
	auto key = project_path_utf8(root) + "\n" + profile->name + "\n" + target->name;
	auto previous = compilers.builds.find(key);
	if (source_input == build_input::disk && previous != compilers.builds.end() && previous->second.fingerprint == fingerprint && std::filesystem::is_regular_file(output, error)) {
		auto cached = nlohmann::json::parse(previous->second.response);
		if (cached.value("succeeded", false)) {
			cached["up_to_date"] = true;
			return cached.dump();
		}
	}
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
		return kiln_build_failure("Could not create .kiln/build.\n");
	}
	auto staged = output.parent_path() / ".pending" / output.filename();
	std::filesystem::create_directories(staged.parent_path(), error);
	if (error) {
		return kiln_build_failure("Could not prepare staged build output.\n");
	}
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
	auto result = integration->build(CompilerBuild{ root, *target, *profile, sources, staged, source_input, dependency_artifacts });
	if (result.succeeded) {
		for (const auto& runtime : runtime_artifacts) {
			auto destination = output.parent_path() / runtime.filename();
			if (runtime == destination || kiln_same_contents(runtime, destination)) {
				continue;
			}
			std::filesystem::copy_file(runtime, destination, std::filesystem::copy_options::overwrite_existing, error);
			if (error) {
				result.succeeded = false;
				result.diagnostics += "Could not deploy a shared target dependency: " + error.message() + "\n";
				break;
			}
		}
		for (const auto& file : std::filesystem::directory_iterator(staged.parent_path(), error)) {
			if (file.path().extension() != ".dll" || file.path() == staged) {
				continue;
			}
			auto destination = output.parent_path() / file.path().filename();
			if (kiln_same_contents(file.path(), destination)) {
				continue;
			}
			std::filesystem::copy_file(file.path(), destination, std::filesystem::copy_options::overwrite_existing, error);
			if (error) {
				result.succeeded = false;
				result.diagnostics += "Could not deploy runtime file: " + error.message() + "\n";
				break;
			}
		}
		auto commands = staged.parent_path() / "compile_commands.json";
		if (std::filesystem::is_regular_file(commands, error)) {
			std::filesystem::copy_file(commands, output.parent_path() / commands.filename(), std::filesystem::copy_options::overwrite_existing, error);
		}
	}
	if (result.succeeded) {
		for (const auto* extension : { ".pdb", ".lib", ".exp" }) {
			if (output.extension() == extension) {
				continue;
			}
			auto companion = staged;
			companion.replace_extension(extension);
			auto destination = output;
			destination.replace_extension(extension);
			if (std::filesystem::is_regular_file(companion, error) && !kiln_publish(companion, destination)) {
				result.succeeded = false;
				result.diagnostics += "Could not publish the build's companion file.\n";
				break;
			}
		}
	}
	if (result.succeeded && !kiln_publish(staged, output)) {
		result.succeeded = false;
		result.diagnostics += "Could not publish the compiled executable.\n";
	}
	std::filesystem::remove(staged, error);
	auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start).count();
	project_record_compilation(target->source, output, sources.size(), duration, result.succeeded);
	auto response = nlohmann::json{ { "succeeded", result.succeeded }, { "artifact", project_path_utf8(output) }, { "diagnostics", result.diagnostics }, { "up_to_date", false } };
	response["runtime_artifacts"] = nlohmann::json::array();
	for (const auto& runtime : runtime_artifacts) {
		response["runtime_artifacts"].push_back(project_path_utf8(runtime));
	}
	if (target->type == target_type::shared_lib) {
		response["runtime_artifacts"].push_back(project_path_utf8(output));
	}
	response["link_artifacts"] = nlohmann::json::array({ project_path_utf8(output) });
	for (const auto& dependency : dependency_artifacts) {
		response["link_artifacts"].push_back(project_path_utf8(dependency));
	}
	if (result.succeeded && artifact.usage == artifact_usage::executable) {
		response["executable"] = project_path_utf8(output);
	}
	auto serialized = response.dump(-1, ' ', false, nlohmann::json::error_handler_t::replace);
	auto request = nlohmann::json{ { "target", target->name }, { "profile", profile->name } }.dump();
	compilers.builds[key] = { std::move(request), std::move(fingerprint), serialized, std::move(inputs) };
	return serialized;
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

std::string kiln_describe(CompilerRegistry& compilers, std::string_view profile) {
	std::error_code error;
	auto root = std::filesystem::current_path(error);
	ProjectConfiguration configuration;
	std::string diagnostics;
	if (error || !kiln_configuration(compilers, root, profile, configuration, diagnostics)) {
		return kiln_build_failure(error ? error.message() : diagnostics);
	}
	if (std::none_of(configuration.profiles.begin(), configuration.profiles.end(), [&](const ProjectProfile& item) { return item.name == profile; })) {
		return kiln_build_failure("Unknown build profile '" + std::string(profile) + "'.\n");
	}
	auto response = nlohmann::json{ { "succeeded", true }, { "root", project_path_utf8(root) }, { "continuous", true }, { "profile", profile }, { "targets", nlohmann::json::array() }, { "profiles", nlohmann::json::array() }, { "builds", nlohmann::json::array() } };
	for (const auto& item : configuration.profiles) {
		response["profiles"].push_back({ { "name", item.name }, { "optimization", item.optimization } });
	}
	for (const auto& target : configuration.targets) {
		const char* kind = target.type == target_type::executable ? "executable" : target.type == target_type::shared_lib ? "shared_lib" : target.type == target_type::static_lib ? "static_lib" : "object_lib";
		auto item = nlohmann::json{ { "name", target.name }, { "compiler", target.compiler }, { "type", kind }, { "options", target.options }, { "dependencies", target.dependencies }, { "sources", nlohmann::json::array() } };
		if (target.source_folders.empty()) {
			item["sources"].push_back(project_path_utf8(target.source));
		}
		for (const auto& folder : target.source_folders) {
			item["sources"].push_back(project_path_utf8(folder));
		}
		if (auto* integration = compilers.find(target.compiler)) {
			auto artifact = integration->artifact(target);
			if (artifact.filename.empty() || artifact.filename != artifact.filename.filename() || artifact.filename == "." || artifact.filename == "..") {
				return kiln_build_failure("Compiler integration returned an invalid artifact filename.\n");
			}
			auto output = project_path_utf8(kiln_artifact(root, target.name, profile, artifact.filename));
			item["artifact"] = output;
			if (artifact.usage == artifact_usage::executable) {
				item["executable"] = output;
			}
		}
		response["targets"].push_back(std::move(item));
	}
	for (const auto& [key, build] : compilers.builds) {
		auto item = nlohmann::json::parse(build.response);
		auto request = nlohmann::json::parse(build.request);
		item["target"] = request["target"];
		item["profile"] = request["profile"];
		response["builds"].push_back(std::move(item));
	}
	return response.dump(-1, ' ', false, nlohmann::json::error_handler_t::replace);
}

void kiln_poll(CompilerRegistry& compilers, build_input input) {
	std::vector<std::pair<std::string, std::string>> pending;
	for (auto& [key, build] : compilers.builds) {
		auto fingerprint = kiln_fingerprint(build.inputs);
		if (fingerprint != build.fingerprint) {
			build.fingerprint = std::move(fingerprint);
			build.response = kiln_build_failure("Build inputs changed.\n");
			pending.emplace_back(key, build.request);
		}
	}
	for (const auto& [key, request] : pending) {
		auto response = kiln_build(compilers, request, input);
		if (auto build = compilers.builds.find(key); build != compilers.builds.end()) {
			build->second.response = std::move(response);
		}
	}
}

std::string kiln_clean(CompilerRegistry& compilers, std::string_view request) {
	try {
		auto input = nlohmann::json::parse(request);
		auto target = input.at("target").get<std::string>();
		auto profile = input.at("profile").get<std::string>();
		auto description = nlohmann::json::parse(kiln_describe(compilers, profile));
		if (!description.value("succeeded", false)) {
			return description.dump();
		}
		bool found = false;
		for (const auto& item : description["targets"]) {
			if (item["name"] == target || target.empty()) {
				found = true;
				if (!item.contains("artifact")) {
					return kiln_build_failure("Target has no installed compiler integration.\n");
				}
				auto path = std::filesystem::u8path(item.at("artifact").get<std::string>()).parent_path();
				std::error_code error;
				std::filesystem::remove_all(path, error);
				if (error) {
					return kiln_build_failure("Could not clean target: " + error.message() + "\n");
				}
				auto key = description["root"].get<std::string>() + "\n" + profile + "\n" + item["name"].get<std::string>();
				compilers.builds.erase(key);
			}
		}
		return found ? nlohmann::json{ { "succeeded", true } }.dump() : kiln_build_failure("Unknown target.\n");
	}
	catch (const nlohmann::json::exception&) {
		return kiln_build_failure("Invalid clean request.\n");
	}
}
