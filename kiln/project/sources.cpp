#include "sources.hpp"

#include "project.hpp"

#include <algorithm>
#include <fstream>
#include <set>
#include <system_error>
#include <utility>

bool project_folder_sources(std::string_view compiler, const std::string& name, const std::filesystem::path& source, std::vector<std::filesystem::path>& sources, const ProjectSourceFilter& filter, std::string& diagnostics) {
	std::error_code error;
	std::vector<std::filesystem::path> result;
	std::filesystem::recursive_directory_iterator iterator(source, error);
	while (!error && iterator != std::filesystem::recursive_directory_iterator{}) {
		if (iterator->is_directory(error) && (iterator->path().filename() == ".rt" || iterator->path().filename() == ".kiln")) {
			iterator.disable_recursion_pending();
		}
		else if (!error && iterator->is_regular_file(error) && filter(compiler, iterator->path())) {
			result.push_back(iterator->path());
		}
		iterator.increment(error);
	}
	if (error) {
		diagnostics = "Cannot read source folder '" + project_path_utf8(source) + "' for target '" + name + "': " + error.message() + ".\n";
		return false;
	}
	if (result.empty()) {
		diagnostics = "Target '" + name + "' has no accepted source files in '" + project_path_utf8(source) + "'.\n";
		return false;
	}
	std::sort(result.begin(), result.end());
	sources = std::move(result);
	return true;
}

bool project_target_sources(const ProjectTarget& target, std::vector<std::filesystem::path>& sources, const ProjectSourceFilter& filter, std::string& diagnostics) {
	if (target.source_folders.empty()) {
		return project_folder_sources(target.compiler, target.name, target.source, sources, filter, diagnostics);
	}
	std::vector<std::filesystem::path> result;
	std::set<std::filesystem::path> included;
	for (const auto& folder : target.source_folders) {
		std::vector<std::filesystem::path> nested;
		if (!project_folder_sources(target.compiler, target.name, folder, nested, filter, diagnostics)) {
			return false;
		}
		for (auto& source : nested) {
			if (included.insert(source).second) {
				result.push_back(std::move(source));
			}
		}
	}
	sources = std::move(result);
	return true;
}

bool project_analysis_path(std::string_view compiler, const std::string& name, const std::filesystem::path& path, std::vector<std::filesystem::path>& sources, const ProjectSourceFilter& filter, std::string& diagnostics) {
	std::error_code error;
	if (std::filesystem::is_directory(path, error) && !error) {
		std::vector<std::filesystem::path> nested;
		if (!project_folder_sources(compiler, "analysis:" + name, path, nested, filter, diagnostics)) {
			return false;
		}
		sources.insert(sources.end(), nested.begin(), nested.end());
		return true;
	}
	if (!error && std::filesystem::is_regular_file(path, error) && !error && filter(compiler, path)) {
		sources.push_back(path);
		return true;
	}
	diagnostics = "Analysis group '" + name + "' requires an available accepted source file or folder: '" + project_path_utf8(path) + "'.\n";
	return false;
}

bool project_analysis_sources(const ProjectAnalysisSource& group, std::vector<std::filesystem::path>& sources, const ProjectSourceFilter& filter, std::string& diagnostics) {
	std::vector<std::filesystem::path> result;
	for (const auto& input : group.sources) {
		if (const auto* path = std::get_if<ProjectSourcePath>(&input)) {
			if (!project_analysis_path(group.compiler, group.name, path->path, result, filter, diagnostics)) {
				return false;
			}
			continue;
		}
		const auto& manifest = std::get<ProjectSourceManifest>(input);
		std::ifstream file(manifest.path);
		if (!file) {
			diagnostics = "Cannot read analysis source manifest '" + project_path_utf8(manifest.path) + "' for group '" + group.name + "'.\n";
			return false;
		}
		std::string line;
		std::size_t number = 0;
		while (std::getline(file, line)) {
			++number;
			auto first = line.find_first_not_of(" \t\r");
			if (first == std::string::npos || line[first] == '#') {
				continue;
			}
			auto last = line.find_last_not_of(" \t\r");
			auto path = (manifest.root / std::filesystem::u8path(line.substr(first, last - first + 1))).lexically_normal();
			std::error_code error;
			if (!std::filesystem::is_regular_file(path, error) || error || !filter(group.compiler, path)) {
				diagnostics = project_path_utf8(manifest.path) + ":" + std::to_string(number) + ": unavailable or unsupported source file '" + project_path_utf8(path) + "' in analysis group '" + group.name + "'.\n";
				return false;
			}
			result.push_back(std::move(path));
		}
		if (file.bad()) {
			diagnostics = "Cannot finish reading analysis source manifest '" + project_path_utf8(manifest.path) + "'.\n";
			return false;
		}
	}
	std::set<std::filesystem::path> included;
	for (auto& path : result) {
		if (included.insert(path).second) {
			sources.push_back(std::move(path));
		}
	}
	return true;
}

bool project_source_groups(const std::filesystem::path& root, const ProjectConfiguration& configuration, std::vector<ProjectSourceGroup>& groups, const ProjectSourceFilter& filter, std::string& diagnostics) {
	std::vector<ProjectSourceGroup> result;
	for (const auto& configured : configuration.targets) {
		if (!filter(configured.compiler, {})) {
			continue;
		}
		auto target = configured;
		if (target.source.is_relative()) {
			target.source = root / target.source;
		}
		for (auto& folder : target.source_folders) {
			if (folder.is_relative()) {
				folder = root / folder;
			}
		}
		ProjectSourceGroup group{ target.name, {}, ProjectSourceKind::target, target.compiler };
		if (!project_target_sources(target, group.sources, filter, diagnostics)) {
			return false;
		}
		result.push_back(std::move(group));
	}
	for (const auto& configured : configuration.analysis) {
		if (!filter(configured.compiler, {})) {
			continue;
		}
		ProjectSourceGroup group{ configured.name, {}, ProjectSourceKind::analysis, configured.compiler };
		if (!project_analysis_sources(configured, group.sources, filter, diagnostics)) {
			return false;
		}
		result.push_back(std::move(group));
	}
	groups = std::move(result);
	return true;
}

bool project_sources(const std::filesystem::path& root, const ProjectConfiguration& configuration, std::vector<std::filesystem::path>& sources, const ProjectSourceFilter& filter, std::string& diagnostics) {
	std::vector<ProjectSourceGroup> groups;
	if (!project_source_groups(root, configuration, groups, filter, diagnostics)) {
		return false;
	}
	std::vector<std::filesystem::path> result;
	for (const auto& group : groups) {
		if (group.kind != ProjectSourceKind::target) {
			continue;
		}
		result.insert(result.end(), group.sources.begin(), group.sources.end());
	}
	std::sort(result.begin(), result.end());
	result.erase(std::unique(result.begin(), result.end()), result.end());
	sources = std::move(result);
	return true;
}
