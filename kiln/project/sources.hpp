#pragma once

#include "configuration.hpp"

#include <filesystem>
#include <string>
#include <vector>

struct ProjectSourceGroup {
	std::string name;
	std::vector<std::filesystem::path> sources;
};

bool project_target_sources(const ProjectTarget& target, std::vector<std::filesystem::path>& sources, std::string& diagnostics);
bool project_source_groups(const std::filesystem::path& root, const ProjectConfiguration& configuration, std::vector<ProjectSourceGroup>& groups, std::string& diagnostics);
bool project_sources(const std::filesystem::path& root, const ProjectConfiguration& configuration, std::vector<std::filesystem::path>& sources, std::string& diagnostics);
