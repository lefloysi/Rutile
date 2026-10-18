#include "sources.hpp"

#include "project.hpp"

#include <algorithm>
#include <system_error>
#include <utility>

bool project_target_sources(const ProjectTarget& target, std::vector<std::filesystem::path>& sources, std::string& diagnostics) {
	std::error_code error;
	std::vector<std::filesystem::path> result;
	std::filesystem::recursive_directory_iterator iterator(target.source, error);
	while (!error && iterator != std::filesystem::recursive_directory_iterator{}) {
		if (iterator->is_directory(error) && iterator->path().filename() == ".rt") {
			iterator.disable_recursion_pending();
		}
		else if (!error && iterator->is_regular_file(error) && iterator->path().extension() == ".rt") {
			result.push_back(iterator->path());
		}
		iterator.increment(error);
	}
	if (error) {
		diagnostics = "Cannot read source folder '" + project_path_utf8(target.source) + "' for target '" + target.name + "': " + error.message() + ".\n";
		return false;
	}
	if (result.empty()) {
		diagnostics = "Target '" + target.name + "' has no Rutile source files in '" + project_path_utf8(target.source) + "'.\n";
		return false;
	}
	std::sort(result.begin(), result.end());
	sources = std::move(result);
	return true;
}

bool project_source_groups(const std::filesystem::path& root, const ProjectConfiguration& configuration, std::vector<ProjectSourceGroup>& groups, std::string& diagnostics) {
	std::vector<ProjectSourceGroup> result;
	for (const auto& configured : configuration.targets) {
		auto target = configured;
		if (target.source.is_relative()) {
			target.source = root / target.source;
		}
		ProjectSourceGroup group{ target.name, {} };
		if (!project_target_sources(target, group.sources, diagnostics)) {
			return false;
		}
		result.push_back(std::move(group));
	}
	groups = std::move(result);
	return true;
}

bool project_sources(const std::filesystem::path& root, const ProjectConfiguration& configuration, std::vector<std::filesystem::path>& sources, std::string& diagnostics) {
	std::vector<ProjectSourceGroup> groups;
	if (!project_source_groups(root, configuration, groups, diagnostics)) {
		return false;
	}
	std::vector<std::filesystem::path> result;
	for (const auto& group : groups) {
		result.insert(result.end(), group.sources.begin(), group.sources.end());
	}
	std::sort(result.begin(), result.end());
	result.erase(std::unique(result.begin(), result.end()), result.end());
	sources = std::move(result);
	return true;
}
