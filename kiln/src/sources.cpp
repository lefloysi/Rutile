#include "sources.hpp"

#include "configuration.hpp"
#include "project.hpp"
#include "../project/sources.hpp"

#include <filesystem>
#include <string>
#include <vector>

int kiln_project_sources(const char* root, int (*add)(void*, const char*, const char*), void* context, void (*diagnostic)(void*, const char*)) {
	if (!root || !add) {
		return 0;
	}
	const auto directory = std::filesystem::u8path(root);
	std::error_code inspection_error;
	const auto configured = std::filesystem::exists(directory / "rutile.yaml", inspection_error);
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
		|| !project_source_groups(directory, configuration, groups, diagnostics)
	) {
		if (diagnostic) {
			diagnostic(context, diagnostics.c_str());
		}
		return 0;
	}

	for (const auto& group : groups) {
		for (const auto& source : group.sources) {
			const auto path = project_path_utf8(source);
			if (!add(context, group.name.c_str(), path.c_str())) {
				return 0;
			}
		}
	}
	return 1;
}
