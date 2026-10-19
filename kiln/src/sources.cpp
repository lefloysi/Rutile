#include "sources.hpp"

#include "configuration.hpp"
#include "project.hpp"
#include "../project/sources.hpp"
#include "../configuration/execution.hpp"
#include "../integrations/rutile.hpp"

#include <filesystem>
#include <algorithm>
#include <string>
#include <vector>

int kiln_project_sources(const char* root, int (*add)(void*, const char*, const char*, int), void* context, void (*diagnostic)(void*, const char*)) {
	if (!root || !add) {
		return 0;
	}
	const auto directory = std::filesystem::u8path(root);
	std::error_code inspection_error;
	const auto configured = std::filesystem::exists(project_configuration_path(directory), inspection_error);
	if (!configured && !inspection_error) {
		return 1;
	}
	if (inspection_error) {
		if (diagnostic) {
			const auto message = "Cannot inspect project configuration: " + inspection_error.message();
			diagnostic(context, message.c_str());
		}
		return 0;
	}
	ProjectConfiguration configuration;
	std::vector<ProjectSourceGroup> groups;
	std::string diagnostics;
	if (
		!project_configuration_read(directory, configuration, diagnostics)
		|| !kiln_configure_project(directory, configuration, diagnostics)
		|| !project_source_groups(directory, configuration, groups, [](std::string_view compiler, const std::filesystem::path& path) {
			return compiler == "rutile" && (path.empty() || path.extension() == ".rt");
		}, diagnostics)
	) {
		if (diagnostic) {
			diagnostic(context, diagnostics.c_str());
		}
		return 0;
	}

	for (const auto& group : groups) {
		if (group.kind == ProjectSourceKind::target) {
			for (const auto& standard : kiln_rutile_standard_sources()) {
				bool provided = std::any_of(group.sources.begin(), group.sources.end(), [&](const std::filesystem::path& source) {
					return source.filename() == standard.filename();
				});
				if (!provided) {
					const auto path = project_path_utf8(standard);
					if (!add(context, group.name.c_str(), path.c_str(), 0)) {
						return 0;
					}
				}
			}
		}
		for (const auto& source : group.sources) {
			if (std::any_of(configuration.projects.begin(), configuration.projects.end(), [&](const ProjectProgram& program) {
				return source == program.source;
			})) {
				continue;
			}
			const auto path = project_path_utf8(source);
			if (!add(context, group.name.c_str(), path.c_str(), group.kind == ProjectSourceKind::analysis ? 1 : 0)) {
				return 0;
			}
		}
	}
	return 1;
}
