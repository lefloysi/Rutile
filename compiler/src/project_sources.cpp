#include "service.hpp"

#include <nlohmann/json.hpp>
#include <mutex>
#include <cstdint>
#include <string>

std::mutex rutile_project_sources_mutex;
nlohmann::json rutile_project_sources_manifest;
std::uint64_t rutile_project_sources_generation = 0;

void rutile_project_sources_update(const nlohmann::json& sources) {
	std::lock_guard guard(rutile_project_sources_mutex);
	if (rutile_project_sources_manifest != sources) {
		rutile_project_sources_manifest = sources;
		++rutile_project_sources_generation;
	}
}

bool rutile_project_sources_ready() {
	std::lock_guard guard(rutile_project_sources_mutex);
	return !rutile_project_sources_manifest.is_null();
}

extern "C" std::uint64_t kiln_project_sources_revision() {
	std::lock_guard guard(rutile_project_sources_mutex);
	return rutile_project_sources_generation;
}

extern "C" int kiln_project_sources(const char* root, int (*add)(void*, const char*, const char*, int), void* context, void (*diagnostic)(void*, const char*)) {
	if (!root || !add) {
		return 0;
	}

	nlohmann::json manifest;
	{
		std::lock_guard guard(rutile_project_sources_mutex);
		manifest = rutile_project_sources_manifest;
	}
	if (manifest.is_null()) {
		if (diagnostic) {
			diagnostic(context, "Kiln has not supplied project sources to the running compiler.");
		}
		return 0;
	}

	try {
		if (!manifest.is_object() || !manifest.contains("sources") || !manifest["sources"].is_array()) {
			if (diagnostic) {
				diagnostic(context, "Kiln returned an invalid project source manifest.");
			}
			return 0;
		}

		for (const auto& source : manifest["sources"]) {
			if (!source.is_object()
				|| !source.contains("group")
				|| !source["group"].is_string()
				|| !source.contains("path")
				|| !source["path"].is_string()) {
				if (diagnostic) {
					diagnostic(context, "Kiln returned an invalid project source entry.");
				}
				return 0;
			}

			const auto group = source["group"].get<std::string>();
			const auto path = source["path"].get<std::string>();
			if (!add(context, group.c_str(), path.c_str(), source.value("kind", 0))) {
				if (diagnostic) {
					diagnostic(context, "Rutile could not add a source supplied by Kiln.");
				}
				return 0;
			}
		}
	}
	catch (const nlohmann::json::exception&) {
		if (diagnostic) {
			diagnostic(context, "Kiln returned malformed project source data.");
		}
		return 0;
	}

	return 1;
}
