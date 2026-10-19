#include "server.hpp"
#include "compiler.hpp"
#include "platform.hpp"
#include "version.hpp"

#include <array>
#include <chrono>
#include <cstdio>
#include <filesystem>
#include <string>

CompilerRegistry* expected_registry = nullptr;
const std::string_view software_version = "test";
const std::uint32_t protocol_version = 1;

std::string version_handshake() {
	return "test-handshake";
}

std::string kiln_list() {
	return "[]";
}

std::string kiln_describe(CompilerRegistry&, std::string_view) {
	return "{}";
}

std::string kiln_clean(CompilerRegistry&, std::string_view) {
	return "{}";
}

void kiln_poll(CompilerRegistry&, build_input) {
}

std::string kiln_build(CompilerRegistry& compilers, std::string_view request, build_input input) {
	return &compilers == expected_registry && request == "same-owner" && input == build_input::editor ? "same registry" : "wrong registry";
}

int main() {
	const auto root = std::filesystem::current_path().generic_u8string();
	const auto* directory = reinterpret_cast<const char*>(root.c_str());
	std::array<char, 256> endpoint{};
	if (!platform_endpoint(directory, endpoint.data(), endpoint.size())) {
		return 1;
	}
	CompilerRegistry registry;
	expected_registry = &registry;
	EditorSession* session = kiln_editor_start(registry, directory);
	if (!session) {
		return 2;
	}
	std::array<char, 256> reply{};
	if (!platform_request(endpoint.data(), "hello", reply.data(), reply.size()) || std::string_view(reply.data()) != "test-handshake") {
		return 3;
	}
	if (!platform_request(endpoint.data(), "request 1 status", reply.data(), reply.size()) || std::string_view(reply.data()).find("managed by the IDE") == std::string_view::npos) {
		return 4;
	}
	if (!platform_request(endpoint.data(), "request 1 build same-owner", reply.data(), reply.size()) || std::string_view(reply.data()) != "same registry") {
		return 5;
	}
	if (kiln_editor_start(registry, directory)) {
		return 6;
	}
	for (const char* command : { "exit", "request 1 exit" }) {
		if (!platform_request(endpoint.data(), command, reply.data(), reply.size()) || std::string_view(reply.data()).find("managed by the IDE") == std::string_view::npos) {
			return 7;
		}
	}
	if (!platform_request(endpoint.data(), "hello", reply.data(), reply.size())) {
		return 8;
	}
	const auto begin = std::chrono::steady_clock::now();
	kiln_editor_stop(session);
	if (std::chrono::steady_clock::now() - begin > std::chrono::seconds(2)) {
		return 9;
	}
	auto* lock = platform_lock_acquire(".kiln/process.lock");
	if (!lock) {
		return 10;
	}
	platform_lock_release(lock);
	session = kiln_editor_start(registry, directory);
	if (!session) {
		return 11;
	}
	kiln_editor_stop(session);
	return 42;
}
