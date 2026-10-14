#pragma once

#include <filesystem>
#include <string>
#include <vector>

struct ProjectTarget {
	std::string name;
	std::filesystem::path source;
	std::string entry;
};

struct ProjectProfile {
	std::string name;
	int optimization;
};

struct ProjectConfiguration {
	std::vector<struct ProjectTarget> targets;
	std::vector<struct ProjectProfile> profiles;
};

bool project_configuration_read(const std::filesystem::path& root, struct ProjectConfiguration& configuration, std::string& diagnostics);
bool project_configuration_initialize(const std::filesystem::path& root, std::string& diagnostics);
