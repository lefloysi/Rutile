#pragma once

#include <filesystem>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>

struct CompilationStatus {
	std::string source;
	std::string output;
	std::string observed_at;
	std::size_t source_files;
	std::int64_t duration_ms;
	bool succeeded;
};

struct ProjectStatus {
	std::string initialized_at;
	std::string observed_at;
	std::string report;
	std::optional<struct CompilationStatus> compilation;
};

std::optional<struct ProjectStatus> project_status_read(const std::filesystem::path& root);
bool project_status_write(const std::filesystem::path& root, const struct ProjectStatus& status);
std::string project_status_timestamp();
void project_status_print(const std::filesystem::path& root, const std::optional<struct ProjectStatus>& status, bool online);
void project_record_compilation(const std::filesystem::path& source, const std::filesystem::path& output, std::size_t source_files, std::int64_t duration_ms, bool succeeded);
