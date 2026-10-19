#include "status.hpp"

#include "project.hpp"
#include "platform.hpp"

#include <nlohmann/json.hpp>

#include <chrono>
#include <format>
#include <fstream>
#include <iostream>

std::optional<struct ProjectStatus> project_status_read(const std::filesystem::path& root) {
	std::ifstream file(root / ".kiln" / "status.json");
	if (!file) {
		return std::nullopt;
	}
	try {
		auto saved = nlohmann::json::parse(file);
		if (!saved.is_object() || !saved.contains("initialized_at") || !saved.contains("observed_at") || !saved.contains("report")) {
			return std::nullopt;
		}
		if (!saved["initialized_at"].is_string() || !saved["observed_at"].is_string() || !saved["report"].is_string()) {
			return std::nullopt;
		}
		struct ProjectStatus status{ saved["initialized_at"].get<std::string>(), saved["observed_at"].get<std::string>(), saved["report"].get<std::string>(), std::nullopt };
		if (saved.contains("last_compilation")) {
			try {
				const auto& compilation = saved["last_compilation"];
				if (compilation.is_object() && compilation.contains("source") && compilation["source"].is_string() && compilation.contains("output") && compilation["output"].is_string() && compilation.contains("observed_at") && compilation["observed_at"].is_string() && compilation.contains("source_files") && compilation["source_files"].is_number_unsigned() && compilation.contains("duration_ms") && compilation["duration_ms"].is_number_integer() && compilation["duration_ms"].get<std::int64_t>() >= 0 && compilation.contains("succeeded") && compilation["succeeded"].is_boolean()) {
					status.compilation = CompilationStatus{ compilation["source"].get<std::string>(), compilation["output"].get<std::string>(), compilation["observed_at"].get<std::string>(), compilation["source_files"].get<std::size_t>(), compilation["duration_ms"].get<std::int64_t>(), compilation["succeeded"].get<bool>() };
				}
			}
			catch (const nlohmann::json::exception&) {
				status.compilation.reset();
			}
		}
		return status;
	}
	catch (const nlohmann::json::exception&) {
		return std::nullopt;
	}
}

bool project_status_write(const std::filesystem::path& root, const struct ProjectStatus& status) {
	nlohmann::json saved{
		{ "initialized_at", status.initialized_at },
		{ "observed_at", status.observed_at },
		{ "report", status.report },
	};
	if (status.compilation) {
		const auto& compilation = *status.compilation;
		saved["last_compilation"] = {
			{ "source", compilation.source },
			{ "output", compilation.output },
			{ "observed_at", compilation.observed_at },
			{ "source_files", compilation.source_files },
			{ "duration_ms", compilation.duration_ms },
			{ "succeeded", compilation.succeeded },
		};
	}
	auto text = saved.dump(2) + '\n';
	auto path = project_path_utf8(root / ".kiln" / "status.json");
	return platform_write_file(path.c_str(), text.c_str());
}

std::string project_status_timestamp() {
	auto now = std::chrono::floor<std::chrono::seconds>(std::chrono::system_clock::now());
	return std::format("{:%FT%TZ}", now);
}

void project_status_print(const std::filesystem::path& root, const std::optional<struct ProjectStatus>& status, bool online) {
	std::cout << (online ? "Kiln online\n" : "Kiln offline\n");
	std::cout << "Project: " << project_path_utf8(root) << '\n';
	if (!status) {
		std::cout << "No saved project statistics.\n";
		return;
	}
	if (!status->initialized_at.empty()) {
		std::cout << "Last initialized: " << status->initialized_at << '\n';
	}
	if (!status->report.empty()) {
		std::cout << "Last report (" << status->observed_at << "):\n" << status->report << '\n';
	}
	else {
		if (!status->compilation) {
			std::cout << "No saved project statistics.\n";
		}
	}
	if (status->compilation) {
		const auto& compilation = *status->compilation;
		std::cout << "Last compilation: " << (compilation.succeeded ? "success" : "failed") << '\n';
		std::cout << "Source: " << compilation.source << '\n';
		std::cout << "Output: " << compilation.output << '\n';
		std::cout << "Source files: " << compilation.source_files << '\n';
		std::cout << "Duration: " << compilation.duration_ms << " ms\n";
		std::cout << "Compiled at: " << compilation.observed_at << '\n';
	}
}
