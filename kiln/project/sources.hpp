#pragma once

#include "configuration.hpp"

#include <filesystem>
#include <functional>
#include <string>
#include <string_view>
#include <vector>

enum class ProjectSourceKind {
	target,
	analysis,
};

struct ProjectSourceGroup {
	std::string name;
	std::vector<std::filesystem::path> sources;
	ProjectSourceKind kind = ProjectSourceKind::target;
	std::string compiler = "rutile";
};

// An empty path queries compiler support; other paths query accepted source files.
using ProjectSourceFilter = std::function<bool(std::string_view compiler, const std::filesystem::path& path)>;

bool project_target_sources(const ProjectTarget& target, std::vector<std::filesystem::path>& sources, const ProjectSourceFilter& filter, std::string& diagnostics);
bool project_source_groups(const std::filesystem::path& root, const ProjectConfiguration& configuration, std::vector<ProjectSourceGroup>& groups, const ProjectSourceFilter& filter, std::string& diagnostics);
bool project_sources(const std::filesystem::path& root, const ProjectConfiguration& configuration, std::vector<std::filesystem::path>& sources, const ProjectSourceFilter& filter, std::string& diagnostics);
