#pragma once

#include "configuration.hpp"

#include <filesystem>
#include <string>
#include <vector>

enum class ProjectSourceKind {
	target,
	analysis,
};

struct ProjectSourceGroup {
	std::string name;
	std::vector<std::filesystem::path> sources;
	ProjectSourceKind kind = ProjectSourceKind::target;
};

bool project_target_sources(const ProjectTarget& target, std::vector<std::filesystem::path>& sources, std::string& diagnostics);
bool project_source_groups(const std::filesystem::path& root, const ProjectConfiguration& configuration, std::vector<ProjectSourceGroup>& groups, std::string& diagnostics);
bool project_sources(const std::filesystem::path& root, const ProjectConfiguration& configuration, std::vector<std::filesystem::path>& sources, std::string& diagnostics);
