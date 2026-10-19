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
	std::string diagnostics;
	ProjectConfiguration configuration;
	if (project_configuration_path(root) != root / "kiln.rt" || project_configuration_read(root, configuration, diagnostics)) {
		return 2;
	}
	if (!project_configuration_initialize(root, diagnostics) || !project_configuration_read(root, configuration, diagnostics) || configuration.projects.size() != 1 || configuration.projects[0].function != "main" || configuration.projects[0].source != root / "kiln.rt" || !configuration.targets.empty() || configuration.profiles.size() != 2) {
		return 3;
	}
	std::ofstream(root / "kiln.rt") << "preserve existing configuration";
	if (!project_configuration_initialize(root, diagnostics)) {
		return 4;
	}
	std::string existing;
	std::ifstream file(root / "kiln.rt");
	std::getline(file, existing);
	if (existing != "preserve existing configuration") {
		return 5;
	}
	ProjectTarget app;
	app.name = "app";
	app.source = root / "application";
	configuration.targets.push_back(app);
	ProjectAnalysisSource analysis;
	analysis.name = "compiler";
	analysis.sources.push_back(ProjectSourcePath{ root / "compiler" });
	configuration.analysis.push_back(analysis);
	std::vector<ProjectSourceGroup> groups;
	if (!project_source_groups(root, configuration, groups, source_filter, diagnostics) || groups.size() != 2 || groups[0].kind != ProjectSourceKind::target || groups[1].kind != ProjectSourceKind::analysis || groups[1].sources.size() != 1 || groups[1].sources[0].filename() != "types.rt") {
		return 6;
	}
	std::vector<std::filesystem::path> runnable;
	if (!project_sources(root, configuration, runnable, source_filter, diagnostics) || runnable.size() != 1 || runnable[0].filename() != "main.rt") {
		return 7;
	}
	std::ofstream(root / "compiler/nested/a.rt") << "struct Early {};";
	std::ofstream(root / "compiler/nested/z.rt") << "struct Late {};";
	std::ofstream(root / "ordered.list") << "# declaration order\n nested/z.rt\nnested/a.rt\n";
	configuration.analysis[0].sources = { ProjectSourcePath{ root / "application/main.rt" }, ProjectSourceManifest{ root / "ordered.list", root / "compiler" } };
	if (!project_source_groups(root, configuration, groups, source_filter, diagnostics) || groups[1].sources.size() != 3 || groups[1].sources[0].filename() != "main.rt" || groups[1].sources[1].filename() != "z.rt" || groups[1].sources[2].filename() != "a.rt") {
		return 8;
	}
	std::ofstream(root / "ordered.list") << "nested/z.rt\nmissing.rt\n";
	if (project_source_groups(root, configuration, groups, source_filter, diagnostics) || diagnostics.find("ordered.list:2:") == std::string::npos || diagnostics.find("missing.rt") == std::string::npos) {
		return 9;
	}
	configuration.analysis[0].sources = { ProjectSourcePath{ root / "missing.rt" } };
	if (project_source_groups(root, configuration, groups, source_filter, diagnostics) || diagnostics.find("available accepted source") == std::string::npos) {
		return 10;
	}
	configuration.analysis[0].sources = { ProjectSourceManifest{ root / "absent.list", root } };
	if (project_source_groups(root, configuration, groups, source_filter, diagnostics) || diagnostics.find("Cannot read analysis source manifest") == std::string::npos) {
		return 11;
	}
	configuration.analysis.clear();
	std::ofstream(root / "application/main.cpp") << "int main() { return 42; }";
	ProjectTarget native;
	native.name = "native";
	native.compiler = "cpp";
	native.source = "application";
	configuration.targets.insert(configuration.targets.begin(), native);
	if (!project_source_groups(root, configuration, groups, source_filter, diagnostics) || groups.size() != 2 || groups[0].compiler != "cpp" || groups[0].sources.size() != 1 || groups[0].sources[0].filename() != "main.cpp" || groups[1].sources.size() != 1 || groups[1].sources[0].filename() != "main.rt") {
		return 12;
	}
	if (!project_source_groups(root, configuration, groups, rutile_source_filter, diagnostics) || groups.size() != 1 || groups[0].name != "app") {
		return 13;
	}
	ProjectTarget folders;
	folders.name = "combined";
	folders.source_folders = { root / "application", root / "compiler", root / "application" };
	std::vector<std::filesystem::path> combined;
	if (!project_target_sources(folders, combined, source_filter, diagnostics) || combined.size() != 4 || combined[0].filename() != "main.rt") {
		return 14;
	}
	return 42;
}