#include "project.hpp"

#include "platform.hpp"

#include <nlohmann/json.hpp>
#include <iostream>

int project_run(std::string_view target, std::string_view profile, const std::vector<std::string>& arguments) {
	nlohmann::json request{ { "target", target }, { "profile", profile } };
	std::string response;
	if (!project_exchange("build " + request.dump(), response)) {
		return 1;
	}
	try {
		auto result = nlohmann::json::parse(response);
		if (!result.at("succeeded").get<bool>()) {
			std::cerr << result.at("diagnostics").get<std::string>();
			return 1;
		}
		auto path = std::filesystem::u8path(result.at("executable").get<std::string>());
		int exit_code = platform_run(path, arguments);
		return exit_code < 0 ? 1 : exit_code;
	}
	catch (const nlohmann::json::exception&) {
		std::cerr << "Project returned an invalid build result. Run rutile init.\n";
		return 1;
	}
}

int project_list() {
	std::string response;
	if (!project_exchange("list", response)) {
		return 1;
	}
	try {
		auto executables = nlohmann::json::parse(response);
		if (executables.is_object()) {
			std::cerr << executables.at("diagnostics").get<std::string>();
			return 1;
		}
		if (!executables.is_array()) {
			throw nlohmann::json::type_error::create(302, "expected executable list", &executables);
		}
		for (const auto& executable : executables) {
			std::cout << executable.get<std::string>() << '\n';
		}
		if (executables.empty()) {
			std::cout << "No executable targets.\n";
		}
		return 0;
	}
	catch (const nlohmann::json::exception&) {
		std::cerr << "Project returned an invalid executable list. Run rutile init.\n";
		return 1;
	}
}
