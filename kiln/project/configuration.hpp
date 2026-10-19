#pragma once

#include <filesystem>
#include <string>
#include <vector>
#include <variant>
#include <map>

enum class target_type {
	executable,
	shared_lib,
	static_lib,
};

struct ProjectProgram {
	std::string function;
	std::filesystem::path source;
};

struct ProjectTarget {
	std::string name;
	std::filesystem::path source;
	std::string entry;
	std::string compiler = "rutile";
	std::map<std::string, std::string> options;
	target_type type = target_type::executable;
	std::filesystem::path output;
	std::vector<std::filesystem::path> source_folders;
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
	std::string compiler = "rutile";
};

struct ProjectProfile {
	std::string name;
	int optimization;
};

struct ProjectConfiguration {
	std::vector<ProjectProgram> projects;
	std::vector<struct ProjectTarget> targets;
	std::vector<struct ProjectAnalysisSource> analysis;
	std::vector<struct ProjectProfile> profiles;
};

std::filesystem::path project_configuration_path(const std::filesystem::path& root);

bool project_configuration_read(const std::filesystem::path& root, struct ProjectConfiguration& configuration, std::string& diagnostics);
bool project_configuration_initialize(const std::filesystem::path& root, std::string& diagnostics);
