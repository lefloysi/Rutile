#include "server.hpp"

#include "compiler.hpp"

#include "platform.hpp"
#include "version.hpp"

#include <cstdio>
#include <array>
#include <charconv>
#include <string_view>
#include <system_error>
#include <filesystem>
#include <atomic>
#include <thread>
#include <memory>
#include <new>

enum class kiln_owner {
	standalone,
	ide,
};

bool kiln_handle_request(struct PlatformServer* server, CompilerRegistry& compilers, kiln_owner owner) {
	std::array<char, 4096> buffer{};
	if (!platform_server_read(server, buffer.data(), buffer.size())) {
		return true;
	}
	std::string_view command{ buffer.data() };
	// Lifecycle control stays stable so init can replace incompatible or legacy processes.
	if (command == "exit") {
		if (owner == kiln_owner::ide) {
			platform_server_write(server, "Project is managed by the IDE; close it there.");
			return true;
		}
		return !platform_server_write(server, "Project closed.");
	}
	if (command == "hello") {
		std::string response = version_handshake();
		platform_server_write(server, response.c_str());
		return true;
	}
	if (command == "ping") {
		platform_server_write(server, "ready");
		return true;
	}
	if (!command.starts_with("request ")) {
		platform_server_write(server, "Incompatible protocol.");
		return true;
	}
	command.remove_prefix(8);
	std::size_t separator = command.find(' ');
	if (separator == std::string_view::npos || !separator) {
		platform_server_write(server, "Incompatible protocol.");
		return true;
	}
	std::uint32_t protocol{};
	auto result = std::from_chars(command.data(), command.data() + separator, protocol);
	if (result.ec != std::errc{} || result.ptr != command.data() + separator || protocol != protocol_version) {
		platform_server_write(server, "Incompatible protocol.");
		return true;
	}
	command.remove_prefix(separator + 1);
	if (command == "exit") {
		if (owner == kiln_owner::ide) {
			platform_server_write(server, "Project is managed by the IDE; close it there.");
			return true;
		}
		return !platform_server_write(server, "Project closed.");
	}
	if (command == "status") {
		platform_server_write(server, owner == kiln_owner::ide ? "Project initialized (managed by the IDE)." : "Project initialized.");
	}
	else if (command == "list") {
		auto response = kiln_list();
		platform_server_write(server, response.c_str());
	}
	else if (command.starts_with("build ")) {
		auto response = kiln_build(compilers, command.substr(6), owner == kiln_owner::ide ? build_input::editor : build_input::disk);
		platform_server_write(server, response.c_str());
	}
	else {
		platform_server_write(server, "Unknown command.");
	}
	return true;
}

int kiln_serve(CompilerRegistry& compilers, const char* endpoint) {
	std::error_code error;
	auto root = std::filesystem::current_path(error);
	if (error) {
		std::fputs("Could not determine the project directory.\n", stderr);
		return 1;
	}
	auto text = root.generic_u8string();
	std::string root_text(reinterpret_cast<const char*>(text.data()), text.size());
	std::array<char, 256> expected_endpoint{};
	if (!platform_endpoint(root_text.c_str(), expected_endpoint.data(), expected_endpoint.size()) || std::string_view(endpoint) != expected_endpoint.data()) {
		std::fputs("The connection does not belong to this project.\n", stderr);
		return 1;
	}
	struct PlatformLock* lock = platform_lock_acquire(".rt/process.lock");
	if (!lock) {
		std::fputs("Project is already initialized, or its lock is unavailable.\n", stderr);
		return 1;
	}
	struct PlatformServer* server = platform_server_open(endpoint);
	if (!server) {
		platform_lock_release(lock);
		std::fputs("Could not open the project connection.\n", stderr);
		return 1;
	}

	while (true) {
		if (!platform_server_accept(server)) {
			platform_server_close(server);
			platform_lock_release(lock);
			return 1;
		}
		if (!kiln_handle_request(server, compilers, kiln_owner::standalone)) {
			platform_server_disconnect(server);
			break;
		}
		platform_server_disconnect(server);
	}
	platform_server_close(server);
	platform_lock_release(lock);
	return 0;
}

struct EditorSession {
	~EditorSession();
	void run();

	CompilerRegistry* compilers = nullptr;
	PlatformLock* lock = nullptr;
	PlatformServer* server = nullptr;
	std::atomic<bool> stopping = false;
	std::thread worker;
};

EditorSession::~EditorSession() {
	stopping.store(true);
	if (worker.joinable()) {
		worker.join();
	}
	if (server) {
		platform_server_close(server);
	}
	platform_lock_release(lock);
}

void EditorSession::run() {
	while (!stopping.load()) {
		const auto accepted = platform_server_accept_for(server, 100);
		if (accepted == platform_accept::timeout) {
			continue;
		}
		if (accepted == platform_accept::error) {
			std::fputs("The IDE project connection failed.\n", stderr);
			return;
		}
		kiln_handle_request(server, *compilers, kiln_owner::ide);
		platform_server_disconnect(server);
	}
}

EditorSession* kiln_editor_start(CompilerRegistry& compilers, const char* directory) {
	if (!directory) {
		std::fputs("The IDE project owner requires a project directory.\n", stderr);
		return nullptr;
	}
	const auto root = std::filesystem::u8path(directory);
	std::error_code error;
	const auto current = std::filesystem::current_path(error);
	if (error || !std::filesystem::equivalent(root, current, error) || error) {
		std::fputs("The IDE project directory does not match Kiln's working directory.\n", stderr);
		return nullptr;
	}
	std::filesystem::create_directories(root / ".rt", error);
	if (error) {
		std::fputs("Could not create the IDE project's .rt directory.\n", stderr);
		return nullptr;
	}
	auto path = root / ".rt" / "init.lock";
	auto path_bytes = path.generic_u8string();
	std::unique_ptr<PlatformLock, decltype(&platform_lock_release)> initialization(platform_lock_acquire(reinterpret_cast<const char*>(path_bytes.c_str())), platform_lock_release);
	if (!initialization) {
		std::fputs("Project initialization is in progress; the IDE owner was not started.\n", stderr);
		return nullptr;
	}
	std::unique_ptr<EditorSession> session(new (std::nothrow) EditorSession());
	if (!session) {
		std::fputs("Could not allocate the IDE project owner.\n", stderr);
		return nullptr;
	}
	path = root / ".rt" / "process.lock";
	path_bytes = path.generic_u8string();
	session->lock = platform_lock_acquire(reinterpret_cast<const char*>(path_bytes.c_str()));
	if (!session->lock) {
		std::fputs("The project already has a Kiln owner, or its process lock is unavailable.\n", stderr);
		return nullptr;
	}
	std::array<char, 256> endpoint{};
	if (!platform_endpoint(directory, endpoint.data(), endpoint.size())) {
		std::fputs("Could not determine the IDE project's connection.\n", stderr);
		return nullptr;
	}
	session->server = platform_server_open(endpoint.data());
	if (!session->server) {
		std::fputs("Could not open the IDE project's connection.\n", stderr);
		return nullptr;
	}
	session->compilers = &compilers;
	try {
		session->worker = std::thread(&EditorSession::run, session.get());
	}
	catch (const std::system_error&) {
		std::fputs("Could not start the IDE project's connection worker.\n", stderr);
		return nullptr;
	}
	path = root / ".rt" / "connection";
	path_bytes = path.generic_u8string();
	if (!platform_write_file(reinterpret_cast<const char*>(path_bytes.c_str()), endpoint.data())) {
		std::fputs("Could not publish the IDE project's connection.\n", stderr);
		return nullptr;
	}
	return session.release();
}

void kiln_editor_stop(EditorSession* session) {
	delete session;
}
