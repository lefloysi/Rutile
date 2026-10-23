#include "service.hpp"

#include <nlohmann/json.hpp>
#include <mutex>
#include <cstdint>
#include <string>
#include <map>
#include <set>
#include <filesystem>
#include <fstream>
#include "version.hpp"

std::mutex rutile_project_sources_mutex;
nlohmann::json rutile_project_sources_manifest;
std::uint64_t rutile_project_sources_generation = 0;
struct RutileProjectInterfaceSource {
	std::string path;
	std::string text;
	std::string module;
	int access;
};
std::map<std::string, RutileProjectInterfaceSource> rutile_project_interface_sources;
std::map<std::string, std::pair<std::int64_t, std::uintmax_t>> rutile_project_interface_stamps;

extern "C" int kiln_project_source_load(const char* path, int (*load)(void*, const char*, const char*, std::size_t, const char*, int), void* context) {
	if (!path || !load) {
		return 0;
	}
	RutileProjectInterfaceSource source;
	{
		std::lock_guard guard(rutile_project_sources_mutex);
		auto found = rutile_project_interface_sources.find(path);
		if (found == rutile_project_interface_sources.end()) {
			return std::string_view(path).starts_with("rti://") ? 0 : -1;
		}
		source = found->second;
	}
	return load(context, source.path.c_str(), source.text.data(), source.text.size(), source.module.c_str(), source.access);
}


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
	std::map<std::string, std::pair<std::int64_t, std::uintmax_t>> stamps;
	if (rutile_project_sources_manifest.is_object()) {
		for (const auto& source : rutile_project_sources_manifest.value("sources", nlohmann::json::array())) {
			const auto kind = source.value("kind", 0);
			if (kind < 3 || kind > 5) {
				continue;
			}
			const auto path = source.at("path").get<std::string>();
			std::error_code error;
			const auto stamp = std::filesystem::last_write_time(std::filesystem::u8path(path), error);
			const auto revision = error ? 0 : stamp.time_since_epoch().count();
			const auto size = std::filesystem::file_size(std::filesystem::u8path(path), error);
			stamps[path] = { revision, error ? 0 : size };
		}
	}
	if (stamps != rutile_project_interface_stamps) {
		rutile_project_interface_stamps = std::move(stamps);
		++rutile_project_sources_generation;
	}
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

		std::set<std::string> imported;
		std::set<std::string> test_groups;
		for (const auto& source : manifest["sources"]) {
			if (source.value("kind", 0) == 6) {
				test_groups.insert(source.at("group").get<std::string>());
			}
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
			const auto kind = source.value("kind", 0);
			if (kind >= 3 && kind <= 5) {
				std::ifstream file(std::filesystem::u8path(path), std::ios::binary);
				if (!file) {
					continue;
				}
				const auto interface = nlohmann::json::parse(file);
				if (interface.value("format", 0) != 1 || interface.value("compiler", std::string{}) != software_version || interface.value("pointer_bits", 0) != sizeof(void*) * 8) {
					if (diagnostic) {
						diagnostic(context, "An imported interface does not match the running compiler.");
					}
					return 0;
				}
				const auto tested = kind == 4 ? interface.at("module").get<std::string>() : std::string{};
				for (const auto& record : interface.at("declarations")) {
					const auto owner = record.at("module").get<std::string>();
					const auto original = record.at("path").get<std::string>();
					const auto access = !tested.empty() && owner == tested ? 2 : 1;
					const auto virtual_path = "rti://" + group + "/" + owner + "/" + original;
					const auto first = imported.insert(virtual_path).second;
					RutileProjectInterfaceSource embedded{ virtual_path, record.at("text").get<std::string>(), owner, access };
					{
						std::lock_guard guard(rutile_project_sources_mutex);
						auto found = rutile_project_interface_sources.find(virtual_path);
						if (first || found == rutile_project_interface_sources.end() || found->second.access < access) {
							rutile_project_interface_sources[virtual_path] = std::move(embedded);
						}
					}
					if (first && !add(context, group.c_str(), virtual_path.c_str(), kind == 5 ? 1 : test_groups.contains(group) ? 6 : 0)) {
						return 0;
					}
				}
				continue;
			}
			if (!add(context, group.c_str(), path.c_str(), source.value("kind", 0))) {
				if (diagnostic) {
					diagnostic(context, "Rutile could not add a source supplied by Kiln.");
				}
				return 0;
			}
		}
	}
	catch (const std::exception&) {
		if (diagnostic) {
			diagnostic(context, "Kiln returned malformed project source data.");
		}
		return 0;
	}

	return 1;
}
