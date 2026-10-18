#pragma once

#include <filesystem>
#include <string>
#include <vector>
#include <variant>

struct ProjectTarget {
	std::string name;
	std::filesystem::path source;
	std::string entry;
};

struct ProjectSourcePath {
	std::filesystem::path path;
};

struct ProjectSourceManifest {
	std::filesystem::path path;
	std::filesystem::path root;
};

using ProjectSourceInput = std::variant<ProjectSourcePath, ProjectSourceManifest>;

struct ProjectAnalysisSource {
	std::string name;
	std::vector<ProjectSourceInput> sources;
};

struct ProjectProfile {
	std::string name;
	int optimization;
};

struct ProjectConfiguration {
	std::vector<struct ProjectTarget> targets;
	std::vector<struct ProjectAnalysisSource> analysis;
	std::vector<struct ProjectProfile> profiles;
};

bool project_configuration_read(const std::filesystem::path& root, struct ProjectConfiguration& configuration, std::string& diagnostics);
bool project_configuration_initialize(const std::filesystem::path& root, std::string& diagnostics);
