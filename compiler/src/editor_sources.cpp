#include "editor_sources.hpp"

#include <filesystem>
#include <map>
#include <mutex>

std::mutex rutile_editor_mutex;
std::map<std::string, std::string> rutile_editor_sources;

std::string rutile_editor_source_key(std::string_view path) {
	auto normalized = std::filesystem::absolute(std::filesystem::u8path(path)).lexically_normal().generic_u8string();
	std::string key(reinterpret_cast<const char*>(normalized.data()), normalized.size());
#ifdef _WIN32
	for (char& character : key) {
		if (character >= 'A' && character <= 'Z') {
			character += 'a' - 'A';
		}
	}
#endif
	return key;
}

std::optional<std::string> rutile_editor_source(std::string_view path) {
	const auto key = rutile_editor_source_key(path);
	std::lock_guard guard(rutile_editor_mutex);
	const auto found = rutile_editor_sources.find(key);
	if (found == rutile_editor_sources.end()) {
		return {};
	}
	return found->second;
}

extern "C" void rutile_editor_source_update(const char* path, const char* text, std::size_t size) {
	if (!path) {
		return;
	}
	const auto key = rutile_editor_source_key(path);
	std::lock_guard guard(rutile_editor_mutex);
	if (!text) {
		rutile_editor_sources.erase(key);
		return;
	}
	rutile_editor_sources.insert_or_assign(key, std::string(text, size));
}

extern "C" void rutile_editor_sources_clear() {
	std::lock_guard guard(rutile_editor_mutex);
	rutile_editor_sources.clear();
}
