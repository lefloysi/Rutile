#include "status.hpp"

#include "project.hpp"
#include "platform.hpp"

#include <chrono>
#include <iostream>
#include <memory>
#include <system_error>
#include <thread>

void project_record_compilation(const std::filesystem::path& source, const std::filesystem::path& output, std::size_t source_files, std::int64_t duration_ms, bool succeeded) {
	struct Project project{};
	std::error_code error;
	if (!project_find(project)) {
		project.root = std::filesystem::current_path(error);
		if (error) {
			std::cerr << "Could not save compilation statistics: project directory unavailable.\n";
			return;
		}
	}
	std::filesystem::create_directory(project.root / ".rt", error);
	if (error) {
		std::cerr << "Could not save compilation statistics: .rt directory unavailable.\n";
		return;
	}
	auto lock_path = project_path_utf8(project.root / ".rt" / "init.lock");
	std::unique_ptr<struct PlatformLock, decltype(&platform_lock_release)> lock{ nullptr, platform_lock_release };
	auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
	while (true) {
		lock.reset(platform_lock_acquire(lock_path.c_str()));
		if (lock || std::chrono::steady_clock::now() >= deadline) {
			break;
		}
		std::this_thread::sleep_for(std::chrono::milliseconds(50));
	}
	if (!lock) {
		std::cerr << "Could not save compilation statistics: project snapshot is locked.\n";
		return;
	}
	auto status = project_status_read(project.root).value_or(ProjectStatus{});
	status.compilation = CompilationStatus{ project_path_utf8(source), project_path_utf8(output), project_status_timestamp(), source_files, duration_ms, succeeded };
	if (!project_status_write(project.root, status)) {
		std::cerr << "Could not save compilation statistics.\n";
	}
}
