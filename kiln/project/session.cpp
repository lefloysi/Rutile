#include "project.hpp"

#include "status.hpp"
#include "configuration.hpp"

#include "platform.hpp"
#include "version.hpp"

#include <array>
#include <iostream>
#include <chrono>
#include <memory>
#include <thread>
#include <system_error>
#include <utility>

std::string project_path_utf8(const std::filesystem::path& path) {
	auto text = path.generic_u8string();
	return std::string(reinterpret_cast<const char*>(text.data()), text.size());
}

bool project_current_root(struct Project& project) {
	std::error_code error;
	auto current = std::filesystem::current_path(error);
	if (error) {
		return false;
	}
	project.root = std::move(current);
	return true;
}

bool project_find(struct Project& project) {
	if (!project_current_root(project)) {
		return false;
	}
	while (true) {
		std::error_code error;
		if (std::filesystem::is_directory(project.root / ".rt", error)) {
			return true;
		}
		if (error && error != std::errc::no_such_file_or_directory) {
			return false;
		}
		auto parent = project.root.parent_path();
		if (parent == project.root) {
			break;
		}
		project.root = std::move(parent);
	}
	return false;
}

bool project_kiln_path(std::filesystem::path& path) {
	std::array<char, 32768> executable{};
	if (!platform_executable_path(executable.data(), executable.size())) {
		return false;
	}
	path = std::filesystem::u8path(executable.data());
	path.replace_filename(KILN_EXECUTABLE_NAME);
	return true;
}

std::optional<struct PeerVersion> project_peer(const struct Project& project, std::string* reply = nullptr) {
	std::array<char, 256> response{};
	if (!platform_request(project.endpoint.c_str(), "hello", response.data(), response.size())) {
		return std::nullopt;
	}
	if (reply) {
		*reply = response.data();
	}
	return version_parse(response.data());
}

std::optional<struct PeerVersion> project_wait(const struct Project& project, bool allow_legacy) {
	auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
	while (true) {
		std::string reply;
		if (auto peer = project_peer(project, &reply)) {
			if (!allow_legacy) {
				auto path = project_path_utf8(project.root / ".rt" / "process.lock");
				if (auto* lock = platform_lock_acquire(path.c_str())) {
					platform_lock_release(lock);
					return std::nullopt;
				}
			}
			return peer;
		}
		if (allow_legacy) {
			std::array<char, 64> response{};
			if (reply == "Unknown command." && platform_request(project.endpoint.c_str(), "ping", response.data(), response.size()) && std::string_view(response.data()) == "ready") {
				return PeerVersion{ 0, "unversioned" };
			}
			auto path = project_path_utf8(project.root / ".rt" / "process.lock");
			if (auto* lock = platform_lock_acquire(path.c_str())) {
				platform_lock_release(lock);
				return PeerVersion{ 0, "" };
			}
		}
		if (std::chrono::steady_clock::now() >= deadline) {
			break;
		}
		std::this_thread::sleep_for(std::chrono::milliseconds(50));
	}
	return std::nullopt;
}

struct PlatformLock* project_wait_for_lock(const std::filesystem::path& path) {
	auto text = project_path_utf8(path);
	auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
	while (true) {
		if (auto* lock = platform_lock_acquire(text.c_str())) {
			return lock;
		}
		if (std::chrono::steady_clock::now() >= deadline) {
			break;
		}
		std::this_thread::sleep_for(std::chrono::milliseconds(50));
	}
	return nullptr;
}

bool project_stop(const struct Project& project) {
	std::array<char, 64> response{};
	if (!platform_request(project.endpoint.c_str(), "exit", response.data(), response.size())) {
		std::cerr << "Could not confirm project shutdown; no replacement was started.\n";
		return false;
	}
	std::string_view acknowledgment{ response.data() };
	if (acknowledgment != "Project closed." && acknowledgment != "Kiln stopped.") {
		std::cerr << "Project shutdown was rejected: " << acknowledgment << " No replacement was started.\n";
		return false;
	}
	auto* lock = project_wait_for_lock(project.root / ".rt" / "process.lock");
	if (!lock) {
		std::cerr << "Project shutdown did not finish; no replacement was started.\n";
		return false;
	}
	platform_lock_release(lock);
	return true;
}

bool project_read_connection(struct Project& project) {
	std::array<char, 256> endpoint{};
	if (!platform_read_file(project_path_utf8(project.root / ".rt" / "connection").c_str(), endpoint.data(), endpoint.size())) {
		return false;
	}
	project.endpoint = endpoint.data();
	return true;
}

bool project_save_report(const struct Project& project, std::string_view report, bool initialized) {
	auto status = project_status_read(project.root).value_or(ProjectStatus{});
	auto now = project_status_timestamp();
	if (initialized) {
		status.initialized_at = now;
	}
	status.observed_at = now;
	status.report = report;
	return project_status_write(project.root, status);
}

int project_report(struct Project& project) {
	auto saved = project_status_read(project.root);
	if (!project_read_connection(project)) {
		project_status_print(project.root, saved, false);
		return 0;
	}
	std::unique_ptr<struct PlatformLock, decltype(&platform_lock_release)> initialization_lock{ project_wait_for_lock(project.root / ".rt" / "init.lock"), platform_lock_release };
	if (!initialization_lock) {
		project_status_print(project.root, saved, false);
		return 0;
	}
	std::string reply;
	auto peer = project_peer(project, &reply);
	if (!peer || !version_compatible(*peer)) {
		project_status_print(project.root, saved, peer.has_value() || !reply.empty());
		return 0;
	}
	std::array<char, 256> response{};
	std::string request = "request " + std::to_string(protocol_version) + " status";
	if (!platform_request(project.endpoint.c_str(), request.c_str(), response.data(), response.size()) || std::string_view(response.data()) == "Incompatible protocol.") {
		project_status_print(project.root, saved, false);
		return 0;
	}
	auto current = saved.value_or(ProjectStatus{});
	current.observed_at = project_status_timestamp();
	current.report = response.data();
	bool persisted = project_status_write(project.root, current);
	project_status_print(project.root, current, true);
	if (!persisted) {
		std::cerr << "Could not save the project report.\n";
		return 1;
	}
	return 0;
}

int project_initialize() {
	struct Project project{};
	if (!project_current_root(project)) {
		std::cerr << "Could not determine the project directory.\n";
		return 1;
	}
	std::error_code error;
	std::filesystem::create_directory(project.root / ".rt", error);
	if (error) {
		std::cerr << "Could not create the .rt directory.\n";
		return 1;
	}
	std::array<char, 256> endpoint{};
	if (!platform_endpoint(project_path_utf8(project.root).c_str(), endpoint.data(), endpoint.size())) {
		std::cerr << "Could not determine the project connection.\n";
		return 1;
	}
	project.endpoint = endpoint.data();
	std::filesystem::path executable;
	if (!project_kiln_path(executable) || !std::filesystem::is_regular_file(executable, error) || error) {
		std::cerr << "Could not locate the installed kiln executable; the existing project was left intact.\n";
		return 1;
	}
	std::unique_ptr<struct PlatformLock, decltype(&platform_lock_release)> initialization_lock{ project_wait_for_lock(project.root / ".rt" / "init.lock"), platform_lock_release };
	if (!initialization_lock) {
		std::cerr << "Another initialization is in progress, or the initialization lock is unavailable.\n";
		return 1;
	}
	struct ProjectConfiguration configuration;
	std::string configuration_error;
	if (!project_configuration_initialize(project.root, configuration_error) || !project_configuration_read(project.root, configuration, configuration_error)) {
		std::cerr << configuration_error;
		return 1;
	}
	std::optional<struct PeerVersion> previous;
	auto lock_path = project_path_utf8(project.root / ".rt" / "process.lock");
	if (auto* lock = platform_lock_acquire(lock_path.c_str())) {
		platform_lock_release(lock);
	}
	else {
		previous = project_wait(project, true);
		if (!previous) {
			std::cerr << "The existing project is unresponsive, or its lock is unavailable; no replacement was started.\n";
			return 1;
		}
		if (previous->release.empty()) {
			previous.reset();
		}
		else if (!project_stop(project)) {
			return 1;
		}
	}
	if (!platform_spawn(project_path_utf8(executable).c_str(), project_path_utf8(project.root).c_str(), project.endpoint.c_str())) {
		std::cerr << "Could not launch the installed kiln executable.\n";
		return 1;
	}
	auto current = project_wait(project, false);
	if (!current) {
		std::cerr << "The replacement did not provide a valid version handshake.\n";
		return 1;
	}
	if (!version_compatible(*current)) {
		std::cerr << "Installed kiln uses protocol " << current->protocol << "; rutile requires " << protocol_version << ". Update both executables together.\n";
		return 1;
	}
	if (!platform_write_file(project_path_utf8(project.root / ".rt" / "connection").c_str(), project.endpoint.c_str())) {
		std::cerr << "Could not write .rt/connection.\n";
		return 1;
	}
	std::array<char, 256> report{};
	std::string request = "request " + std::to_string(protocol_version) + " status";
	if (!platform_request(project.endpoint.c_str(), request.c_str(), report.data(), report.size()) || !project_save_report(project, report.data(), true)) {
		std::cerr << "Project started, but its report could not be saved.\n";
		return 1;
	}
	std::cout << (previous ? "Project restarted: " : "Project initialized: ") << project_path_utf8(project.root) << '\n';
	if (previous && previous->release != current->release) {
		std::cout << "Kiln version: " << previous->release << " -> " << current->release << '\n';
	}
	else {
		std::cout << "Kiln version: " << current->release << '\n';
	}
	if (previous && previous->protocol != current->protocol) {
		std::cout << "Protocol: " << (previous->protocol ? std::to_string(previous->protocol) : "unversioned") << " -> " << current->protocol << '\n';
	}
	return 0;
}

bool project_exchange(std::string_view command, std::string& response) {
	struct Project project{};
	if (!project_find(project) || !project_read_connection(project)) {
		std::cerr << "Project is not initialized. Run rutile init.\n";
		return false;
	}
	auto peer = project_peer(project);
	if (!peer || !version_compatible(*peer)) {
		std::cerr << "Project is unavailable. Run rutile init.\n";
		return false;
	}
	std::vector<char> buffer(1024 * 1024);
	std::string request = "request " + std::to_string(protocol_version) + " " + std::string(command);
	if (!platform_request(project.endpoint.c_str(), request.c_str(), buffer.data(), buffer.size(), 60000)) {
		std::cerr << "Project request did not finish.\n";
		return false;
	}
	response = buffer.data();
	return true;
}

int project_request(std::string_view command) {
	struct Project project{};
	if (!project_find(project)) {
		if (command == "status" && project_current_root(project)) {
			project_status_print(project.root, std::nullopt, false);
			return 0;
		}
		std::cerr << "Project is not initialized. Run rutile init.\n";
		return 1;
	}
	if (command == "status") {
		return project_report(project);
	}
	if (!project_read_connection(project)) {
		std::cerr << "Project is not initialized. Run rutile init.\n";
		return 1;
	}
	std::unique_ptr<struct PlatformLock, decltype(&platform_lock_release)> initialization_lock{ project_wait_for_lock(project.root / ".rt" / "init.lock"), platform_lock_release };
	if (!initialization_lock) {
		std::cerr << "Project initialization is in progress, or its lock is unavailable.\n";
		return 1;
	}
	auto peer = project_peer(project);
	if (!peer) {
		std::cerr << "The project has no valid version handshake. Run rutile init to restart it.\n";
		return 1;
	}
	if (!version_compatible(*peer)) {
		std::cerr << "Project protocol " << peer->protocol << " is incompatible with rutile protocol " << protocol_version << ". Run rutile init to restart it.\n";
		return 1;
	}
	std::array<char, 256> response{};
	std::string request = "request " + std::to_string(protocol_version) + " " + std::string(command);
	if (!platform_request(project.endpoint.c_str(), request.c_str(), response.data(), response.size())) {
		std::cerr << "Project is unavailable. Run rutile init.\n";
		return 1;
	}
	if (std::string_view(response.data()) == "Incompatible protocol.") {
		std::cerr << "The project rejected the request's protocol. Run rutile init to restart it.\n";
		return 1;
	}
	const std::string_view acknowledgment{ response.data() };
	if (command == "exit" && acknowledgment != "Project closed." && acknowledgment != "Kiln stopped.") {
		std::cerr << "Project shutdown was rejected: " << acknowledgment << '\n';
		return 1;
	}
	std::cout << response.data() << '\n';
	return 0;
}

