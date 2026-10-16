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

bool kiln_handle_request(struct PlatformServer* server, struct Compiler* compiler) {
	std::array<char, 4096> buffer{};
	if (!platform_server_read(server, buffer.data(), buffer.size())) {
		return true;
	}
	std::string_view command{ buffer.data() };
	// Lifecycle control stays stable so init can replace incompatible or legacy processes.
	if (command == "exit") {
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
		return !platform_server_write(server, "Project closed.");
	}
	if (command == "status") {
		platform_server_write(server, "Project initialized.");
	}
	else if (command == "list") {
		auto response = kiln_list();
		platform_server_write(server, response.c_str());
	}
	else if (command.starts_with("build ")) {
		auto response = kiln_build(compiler, command.substr(6));
		platform_server_write(server, response.c_str());
	}
	else {
		platform_server_write(server, "Unknown command.");
	}
	return true;
}

extern "C" int kiln_serve_session(struct Compiler* compiler, void* data) {
	const char* endpoint = *static_cast<const char* const*>(data);
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
		if (!kiln_handle_request(server, compiler)) {
			platform_server_disconnect(server);
			break;
		}
		platform_server_disconnect(server);
	}
	platform_server_close(server);
	platform_lock_release(lock);
	return 0;
}

int kiln_serve(const char* endpoint) {
	return rutile_compiler_session(kiln_serve_session, &endpoint);
}

