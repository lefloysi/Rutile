#include "configuration.hpp"
#include "sources.hpp"

#include <filesystem>
#include <chrono>
#include <fstream>
#include <string>
#include <vector>

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
	if (!project_source_groups(root, configuration, groups, diagnostics) || groups.size() != 2 || groups[0].kind != ProjectSourceKind::target || groups[1].kind != ProjectSourceKind::analysis || groups[1].sources.size() != 1 || groups[1].sources[0].filename() != "types.rt") {
		return 3;
	}
	std::vector<std::filesystem::path> runnable;
	if (!project_sources(root, configuration, runnable, diagnostics) || runnable.size() != 1 || runnable[0].filename() != "main.rt") {
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
	if (!project_configuration_read(root, configuration, diagnostics) || !project_source_groups(root, configuration, groups, diagnostics) || groups[1].sources.size() != 3 || groups[1].sources[0].filename() != "main.rt" || groups[1].sources[1].filename() != "z.rt" || groups[1].sources[2].filename() != "a.rt") {
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
	if (!project_configuration_read(root, configuration, diagnostics) || project_source_groups(root, configuration, groups, diagnostics) || diagnostics.find("ordered.list:2:") == std::string::npos || diagnostics.find("missing.rt") == std::string::npos) {
		return 13;
	}
	return 42;
}
