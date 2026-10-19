#pragma once

#include "configuration.hpp"

#include <filesystem>
#include <memory>
#include <map>
#include <string>
#include <string_view>
#include <vector>

enum class build_input {
	disk,
	editor,
};

struct CompilerBuild {
	const std::filesystem::path& root;
	const ProjectTarget& target;
	const ProjectProfile& profile;
	const std::vector<std::filesystem::path>& sources;
	const std::filesystem::path& output;
	build_input input;
	std::vector<std::filesystem::path> dependency_artifacts;
};

struct CompilerResult {
	bool succeeded;
	std::string diagnostics;
};

enum class artifact_usage {
	executable,
	module,
};

struct CompilerArtifact {
	std::filesystem::path filename;
	artifact_usage usage;
};

struct CompilerIntegration {
	virtual ~CompilerIntegration();
	virtual std::string_view name() const = 0;
	virtual bool accepts_source(const std::filesystem::path& path) const = 0;
	virtual CompilerArtifact artifact(const ProjectTarget& target) const = 0;
	virtual bool validate(const ProjectTarget& target, const ProjectProfile& profile, std::string& diagnostics) const = 0;
	virtual CompilerResult build(const CompilerBuild& request) = 0;
};

struct CompilerRegistry {
	bool add(std::unique_ptr<CompilerIntegration> integration);
	CompilerIntegration* find(std::string_view name) const;
	struct Configuration {
		std::string fingerprint;
		ProjectConfiguration value;
	};
	struct Build {
		std::string request;
		std::string fingerprint;
		std::string response;
		std::vector<std::filesystem::path> inputs;
	};
	std::map<std::string, Configuration> configurations;
	std::map<std::string, Build> builds;

private:
	std::vector<std::unique_ptr<CompilerIntegration>> integrations;
};

std::string kiln_build(CompilerRegistry& compilers, std::string_view request, build_input input = build_input::disk);
std::string kiln_list();
std::vector<std::string> kiln_option_values(std::string_view text);
std::string kiln_describe(CompilerRegistry& compilers, std::string_view profile);
void kiln_poll(CompilerRegistry& compilers, build_input input = build_input::disk);
std::string kiln_clean(CompilerRegistry& compilers, std::string_view request);
