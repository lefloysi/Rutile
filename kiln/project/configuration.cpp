#include "configuration.hpp"

#include "project.hpp"

#include <fstream>
#include <string_view>

bool configuration_name(std::string_view name, bool target) {
	if (name.empty()) {
		return false;
	}
	for (std::size_t index = 0; index < name.size(); ++index) {
		char value = name[index];
		bool letter = (value >= 'a' && value <= 'z') || (value >= 'A' && value <= 'Z');
		bool digit = value >= '0' && value <= '9';
		if (!letter && value != '_' && !(digit && (target || index)) && !(target && value == '-')) {
			return false;
		}
	}
	return true;
}

std::filesystem::path project_configuration_path(const std::filesystem::path& root) {
	return root / "kiln.rt";
}

bool project_configuration_read(const std::filesystem::path& root, ProjectConfiguration& configuration, std::string& diagnostics) {
	std::error_code error;
	auto path = project_configuration_path(root);
	if (!std::filesystem::is_regular_file(path, error) || error) {
		diagnostics = "Project configuration is unavailable: " + project_path_utf8(path) + "\n";
		return false;
	}
	configuration = {};
	configuration.projects.push_back({ "main", path });
	configuration.profiles = { { "debug", 0 }, { "release", 2 } };
	diagnostics.clear();
	return true;
}

bool project_configuration_initialize(const std::filesystem::path& root, std::string& diagnostics) {
	auto path = project_configuration_path(root);
	std::error_code error;
	if (std::filesystem::exists(path, error) && !error) {
		return true;
	}
	if (error) {
		diagnostics = "Could not inspect project configuration: " + error.message() + "\n";
		return false;
	}
	std::ofstream file(path, std::ios::out | std::ios::noreplace);
	file << "fn main(kiln::Options& options) {\n\tkiln::profile(\"debug\", 0);\n\tkiln::profile(\"release\", 2);\n\tvar app = kiln::add_target(\"app.exe\", kiln::TargetType::executable);\n\t@app.sources.add(\".\");\n};\n";
	file.close();
	if (!file) {
		diagnostics = "Could not create project configuration.\n";
		return false;
	}
	return true;
}
