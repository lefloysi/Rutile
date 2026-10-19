#include "configuration.hpp"
#include "sources.hpp"

#include <filesystem>
#include <chrono>
#include <fstream>
#include <string>
#include <vector>

bool source_filter(std::string_view compiler, const std::filesystem::path& path) {
	if (compiler == "rutile") {
		return path.empty() || path.extension() == ".rt";
	}
	if (compiler == "cpp") {
		return path.empty() || path.extension() == ".cpp";
	}
	return false;
}

bool rutile_source_filter(std::string_view compiler, const std::filesystem::path& path) {
	return compiler == "rutile" && source_filter(compiler, path);
}

int main(int count, char** arguments) {
	if (count != 2) {
		return 1;
	}
	const auto root = std::filesystem::u8path(arguments[1]) / ("case-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
	std::filesystem::create_directories(root / "application");
	std::filesystem::create_directories(root / "compiler/nested");
	std::ofstream(root / "application/main.rt") << "fn main()->i32 {return 42;};";
	std::ofstream(root / "compiler/nested/types.rt") << "struct SourceManager {};";
	const std::string target = "targets:\n  app:\n    source: application\n    entry: main\n";
	const auto write = [&](const std::string& text) {
		std::ofstream(root / "rutile.yaml") << text;
	};
	std::string diagnostics;
	ProjectConfiguration configuration;
	write(target + "analysis:\n  compiler:\n    source: compiler\n");
	if (!project_configuration_read(root, configuration, diagnostics) || configuration.targets.size() != 1 || configuration.analysis.size() != 1 || configuration.targets[0].name != "app") {
		return 2;
	}
	std::vector<ProjectSourceGroup> groups;
	if (!project_source_groups(root, configuration, groups, source_filter, diagnostics) || groups.size() != 2 || groups[0].kind != ProjectSourceKind::target || groups[1].kind != ProjectSourceKind::analysis || groups[1].sources.size() != 1 || groups[1].sources[0].filename() != "types.rt") {
		return 3;
	}
	std::vector<std::filesystem::path> runnable;
	if (!project_sources(root, configuration, runnable, source_filter, diagnostics) || runnable.size() != 1 || runnable[0].filename() != "main.rt") {
		return 4;
	}
	write(target);
	if (!project_configuration_read(root, configuration, diagnostics) || !configuration.analysis.empty()) {
		return 5;
	}
	write(target + "analysis:\n  compiler:\n    source: compiler\n    entry: main\n");
	if (project_configuration_read(root, configuration, diagnostics) || diagnostics.find("unknown field 'entry'") == std::string::npos) {
		return 6;
	}
	write(target + "analysis:\n  compiler:\n    source: unavailable\n");
	if (project_configuration_read(root, configuration, diagnostics) || diagnostics.find("analysis source folder is unavailable") == std::string::npos) {
		return 7;
	}
	write(target + "analysis:\n  compiler:\n    source: compiler\n  compiler:\n    source: compiler\n");
	if (project_configuration_read(root, configuration, diagnostics) || diagnostics.find("duplicate field 'compiler'") == std::string::npos) {
		return 8;
	}
	std::ofstream(root / "compiler/nested/a.rt") << "struct Early {};";
	std::ofstream(root / "compiler/nested/z.rt") << "struct Late {};";
	std::ofstream(root / "ordered.list") << "# declaration order\n nested/z.rt\nnested/a.rt\n";
	write(target + "analysis:\n  compiler:\n    sources:\n      - application/main.rt\n      - manifest: ordered.list\n        root: compiler\n");
	if (!project_configuration_read(root, configuration, diagnostics) || !project_source_groups(root, configuration, groups, source_filter, diagnostics) || groups[1].sources.size() != 3 || groups[1].sources[0].filename() != "main.rt" || groups[1].sources[1].filename() != "z.rt" || groups[1].sources[2].filename() != "a.rt") {
		return 9;
	}
	write(target + "analysis:\n  compiler:\n    source: compiler\n    sources: [application/main.rt]\n");
	if (project_configuration_read(root, configuration, diagnostics) || diagnostics.find("not both") == std::string::npos) {
		return 10;
	}
	write(target + "analysis:\n  compiler:\n    sources: [unavailable.rt]\n");
	if (project_configuration_read(root, configuration, diagnostics) || diagnostics.find("source path is unavailable") == std::string::npos) {
		return 11;
	}
	write(target + "analysis:\n  compiler:\n    sources:\n      - manifest: absent.list\n        root: compiler\n");
	if (project_configuration_read(root, configuration, diagnostics) || diagnostics.find("source manifest is unavailable") == std::string::npos) {
		return 12;
	}
	std::ofstream(root / "ordered.list") << "nested/z.rt\nmissing.rt\n";
	write(target + "analysis:\n  compiler:\n    sources:\n      - manifest: ordered.list\n        root: compiler\n");
	if (!project_configuration_read(root, configuration, diagnostics) || project_source_groups(root, configuration, groups, source_filter, diagnostics) || diagnostics.find("ordered.list:2:") == std::string::npos || diagnostics.find("missing.rt") == std::string::npos) {
		return 13;
	}
	std::ofstream(root / "application/main.cpp") << "int main() { return 42; }";
	std::ofstream(root / "kiln.yaml") << "targets:\n  native:\n    compiler: cpp\n    source: application\n    options:\n      standard: c++23\n  app:\n    source: application\n    entry: a compiler specific value\n";
	if (!project_configuration_read(root, configuration, diagnostics) || configuration.targets.size() != 2 || configuration.targets[0].compiler != "cpp" || configuration.targets[0].options.at("standard") != "c++23" || !configuration.targets[0].entry.empty()) {
		return 14;
	}
	if (!project_source_groups(root, configuration, groups, source_filter, diagnostics) || groups.size() != 2 || groups[0].compiler != "cpp" || groups[0].sources.size() != 1 || groups[0].sources[0].filename() != "main.cpp" || groups[1].sources.size() != 1 || groups[1].sources[0].filename() != "main.rt") {
		return 15;
	}
	if (!project_source_groups(root, configuration, groups, rutile_source_filter, diagnostics) || groups.size() != 1 || groups[0].name != "app") {
		return 16;
	}
	std::ofstream(root / "kiln.rt") << "fn configure();";
	std::ofstream(root / "kiln.yaml") << "projects:\n  setup: kiln.rt\n";
	if (!project_configuration_read(root, configuration, diagnostics) || configuration.projects.size() != 1 || configuration.projects[0].source.filename() != "kiln.rt" || !configuration.targets.empty()) {
		return 17;
	}
	std::ofstream(root / "kiln.yaml") << "targets: {}\n";
	if (project_configuration_read(root, configuration, diagnostics)) {
		return 18;
	}
	std::ofstream(root / "kiln.yaml") << "projects:\n  setup: absent.rt\n";
	if (project_configuration_read(root, configuration, diagnostics) || diagnostics.find("project configuration program is unavailable") == std::string::npos) {
		return 19;
	}
	std::ofstream(root / "kiln.yaml") << "targets:\n  app:\n    compiler: unknown\n    source: application\n";
	if (!project_configuration_read(root, configuration, diagnostics) || configuration.targets[0].compiler != "unknown") {
		return 20;
	}
	std::ofstream(root / "kiln.yaml") << "projects:\n  first:\n  second: kiln.rt\n";
	if (!project_configuration_read(root, configuration, diagnostics) || configuration.projects.size() != 2 || configuration.projects[0].function != "first" || configuration.projects[0].source != root / "kiln.rt" || configuration.projects[1].source != root / "kiln.rt" || !configuration.targets.empty()) {
		return 21;
	}
	std::ofstream(root / "kiln.yaml") << "projects:\n  bad-name:\n";
	if (project_configuration_read(root, configuration, diagnostics) || diagnostics.find("function identifiers") == std::string::npos) {
		return 22;
	}
	std::ofstream(root / "kiln.yaml") << "projects:\n  missing: absent.rt\n";
	if (project_configuration_read(root, configuration, diagnostics) || diagnostics.find("project configuration program is unavailable") == std::string::npos) {
		return 23;
	}
	std::ofstream(root / "kiln.yaml") << "projects:\n  first:\ntargets:\n  library:\n    compiler: cpp\n    source: application\n    type: static_lib\n    output: custom.lib\n";
	if (!project_configuration_read(root, configuration, diagnostics) || configuration.projects.size() != 1 || configuration.targets[0].type != target_type::static_lib || configuration.targets[0].output != "custom.lib") {
		return 24;
	}
	std::ofstream(root / "kiln.yaml") << "targets:\n  library:\n    source: application\n    type: unknown\n";
	if (project_configuration_read(root, configuration, diagnostics) || diagnostics.find("unknown target type") == std::string::npos) {
		return 25;
	}
	std::ofstream(root / "kiln.yaml") << "targets:\n  library:\n    source: application\n    output: ../escape.lib\n";
	if (project_configuration_read(root, configuration, diagnostics) || diagnostics.find("output must be a filename") == std::string::npos) {
		return 26;
	}
	ProjectTarget folders;
	folders.name = "combined";
	folders.source_folders = { root / "application", root / "compiler", root / "application" };
	std::vector<std::filesystem::path> combined;
	if (!project_target_sources(folders, combined, source_filter, diagnostics) || combined.size() != 4 || combined[0].filename() != "main.rt") {
		return 27;
	}
	return 42;
}
