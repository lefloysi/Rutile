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
		if (!result.contains("executable")) {
			std::cerr << "Target produced a module and cannot be run.\n";
			return 1;
		}
		auto path = std::filesystem::u8path(result.at("executable").get<std::string>());
		int exit_code = platform_run(path, arguments);
		return exit_code < 0 ? 1 : exit_code;
	}
	catch (const nlohmann::json::exception&) {
		std::cerr << "Project returned an invalid build result. Run kiln init.\n";
		return 1;
	}
}

int project_build(std::string_view target, std::string_view profile, bool json) {
	nlohmann::json request{ { "target", target }, { "profile", profile } };
	std::string response;
	if (!project_exchange("build " + request.dump(), response)) {
		return 1;
	}
	try {
		auto result = nlohmann::json::parse(response);
		if (json) {
			std::cout << response << '\n';
			return result.value("succeeded", false) ? 0 : 1;
		}
		if (!result.at("succeeded").get<bool>()) {
			std::cerr << result.at("diagnostics").get<std::string>();
			return 1;
		}
		std::cout << result.at("artifact").get<std::string>() << '\n';
		return 0;
	}
	catch (const nlohmann::json::exception&) {
		std::cerr << "Project returned an invalid build result. Run kiln init.\n";
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
			throw nlohmann::json::type_error::create(302, "expected target list", &executables);
		}
		for (const auto& executable : executables) {
			std::cout << executable.get<std::string>() << '\n';
		}
		if (executables.empty()) {
			std::cout << "No configured targets.\n";
		}
		return 0;
	}
	catch (const nlohmann::json::exception&) {
		std::cerr << "Project returned an invalid target list. Run kiln init.\n";
		return 1;
	}
}

int project_describe(std::string_view profile, bool json) {
	std::string response;
	if (!project_exchange("describe " + std::string(profile), response)) {
		return 1;
	}
	try {
		auto result = nlohmann::json::parse(response);
		if (json) {
			std::cout << response << '\n';
		}
		else if (result.value("succeeded", false)) {
			std::cout << "Continuous project: " << result.at("root").get<std::string>() << '\n';
			for (const auto& target : result.at("targets")) {
				std::cout << target.at("name").get<std::string>() << " (" << target.at("compiler").get<std::string>() << ", " << target.at("type").get<std::string>() << ")\n";
			}
		}
		else {
			std::cerr << result.value("diagnostics", "Project configuration failed.\n");
		}
		return result.value("succeeded", false) ? 0 : 1;
	}
	catch (const nlohmann::json::exception&) {
		std::cerr << "Project returned an invalid description.\n";
		return 1;
	}
}

int project_clean(std::string_view target, std::string_view profile) {
	std::string response;
	auto request = nlohmann::json{ { "target", target }, { "profile", profile } }.dump();
	if (!project_exchange("clean " + request, response)) {
		return 1;
	}
	try {
		auto result = nlohmann::json::parse(response);
		if (!result.value("succeeded", false)) {
			std::cerr << result.value("diagnostics", "Clean failed.\n");
			return 1;
		}
		std::cout << "Build outputs cleaned.\n";
		return 0;
	}
	catch (const nlohmann::json::exception&) {
		std::cerr << "Project returned an invalid clean result.\n";
		return 1;
	}
}
