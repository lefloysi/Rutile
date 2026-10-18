#include "configuration.hpp"

#include "project.hpp"

#include <yaml-cpp/yaml.h>

#include <charconv>
#include <fstream>
#include <initializer_list>
#include <set>
#include <string_view>
#include <system_error>
#include <utility>

bool configuration_failure(const std::filesystem::path& path, const YAML::Node& node, std::string_view message, std::string& diagnostics) {
	diagnostics = project_path_utf8(path);
	if (node.IsDefined() && !node.Mark().is_null()) {
		diagnostics += ":" + std::to_string(node.Mark().line + 1) + ":" + std::to_string(node.Mark().column + 1);
	}
	diagnostics += ": " + std::string(message) + '\n';
	return false;
}

bool project_configuration_initialize(const std::filesystem::path& root, std::string& diagnostics) {
	auto path = root / "rutile.yaml";
	diagnostics.clear();
	std::error_code error;
	if (std::filesystem::exists(path, error) && !error) {
		return true;
	}
	if (error) {
		diagnostics = project_path_utf8(path) + ": could not inspect project configuration.\n";
		return false;
	}
	std::ofstream file(path, std::ios::out | std::ios::noreplace);
	if (!file) {
		diagnostics = project_path_utf8(path) + ": could not create project configuration.\n";
		return false;
	}
	file << "targets:\n  app:\n    source: .\n    entry: main\nprofiles:\n  debug:\n    optimization: 0\n  release:\n    optimization: 2\n";
	file.close();
	if (!file) {
		diagnostics = project_path_utf8(path) + ": could not write project configuration.\n";
		return false;
	}
	return true;
}

bool configuration_name(std::string_view name, bool target) {
	if (name.empty()) {
		return false;
	}
	for (std::size_t i = 0; i < name.size(); ++i) {
		char value = name[i];
		bool letter = (value >= 'a' && value <= 'z') || (value >= 'A' && value <= 'Z');
		bool digit = value >= '0' && value <= '9';
		if (!letter && value != '_' && !(digit && (target || i)) && !(target && value == '-')) {
			return false;
		}
	}
	return true;
}

bool configuration_entry(std::string_view name) {
	while (true) {
		auto separator = name.find("::");
		if (!configuration_name(name.substr(0, separator), false)) {
			return false;
		}
		if (separator == std::string_view::npos) {
			return true;
		}
		name.remove_prefix(separator + 2);
	}
}

bool configuration_mapping(const std::filesystem::path& path, const YAML::Node& node, std::initializer_list<std::string_view> fields, bool unrestricted, std::string& diagnostics) {
	if (!node.IsMap()) {
		return configuration_failure(path, node, "expected a mapping", diagnostics);
	}
	std::set<std::string> keys;
	for (const auto& field : node) {
		if (!field.first.IsScalar()) {
			return configuration_failure(path, field.first, "mapping keys must be strings", diagnostics);
		}
		auto key = field.first.Scalar();
		if (!keys.insert(key).second) {
			return configuration_failure(path, field.first, "duplicate field '" + key + "'", diagnostics);
		}
		bool known = unrestricted;
		for (auto allowed : fields) {
			if (key == allowed) {
				known = true;
				break;
			}
		}
		if (!known) {
			return configuration_failure(path, field.first, "unknown field '" + key + "'", diagnostics);
		}
	}
	return true;
}

bool configuration_string(const std::filesystem::path& path, const YAML::Node& node, std::string_view field, std::string& value, std::string& diagnostics) {
	if (!node || !node.IsScalar() || node.Scalar().empty()) {
		return configuration_failure(path, node, "'" + std::string(field) + "' must be a nonempty string", diagnostics);
	}
	value = node.Scalar();
	if (value.find('\0') != std::string::npos) {
		return configuration_failure(path, node, "'" + std::string(field) + "' must not contain a null byte", diagnostics);
	}
	return true;
}

bool project_configuration_read(const std::filesystem::path& root, struct ProjectConfiguration& configuration, std::string& diagnostics) {
	auto path = root / "rutile.yaml";
	diagnostics.clear();
	std::ifstream file(path);
	if (!file) {
		diagnostics = project_path_utf8(path) + ": could not read project configuration.\n";
		return false;
	}
	try {
		auto document = YAML::Load(file);
		if (!configuration_mapping(path, document, { "targets", "profiles", "analysis" }, false, diagnostics)) {
			return false;
		}
		auto targets = document["targets"];
		if (!targets || !configuration_mapping(path, targets, {}, true, diagnostics)) {
			if (diagnostics.empty()) {
				configuration_failure(path, document, "missing 'targets' mapping", diagnostics);
			}
			return false;
		}
		if (!targets.size()) {
			return configuration_failure(path, targets, "'targets' must contain at least one target", diagnostics);
		}
		struct ProjectConfiguration parsed{};
		for (const auto& item : targets) {
			struct ProjectTarget target{};
			target.name = item.first.Scalar();
			if (!configuration_name(target.name, true)) {
				return configuration_failure(path, item.first, "target names may contain only ASCII letters, digits, '_' and '-'", diagnostics);
			}
			if (!configuration_mapping(path, item.second, { "source", "entry" }, false, diagnostics)) {
				return false;
			}
			std::string source;
			if (!configuration_string(path, item.second["source"], "source", source, diagnostics) || !configuration_string(path, item.second["entry"], "entry", target.entry, diagnostics)) {
				return false;
			}
			if (!configuration_entry(target.entry)) {
				return configuration_failure(path, item.second["entry"], "entry must be a Rutile identifier or a namespace-qualified identifier", diagnostics);
			}
			target.source = (root / std::filesystem::u8path(source)).lexically_normal();
			std::error_code error;
			if (!std::filesystem::is_directory(target.source, error) || error) {
				return configuration_failure(path, item.second["source"], "source folder is unavailable: " + source, diagnostics);
			}
			parsed.targets.push_back(std::move(target));
		}
		auto analysis = document["analysis"];
		if (analysis) {
			if (!configuration_mapping(path, analysis, {}, true, diagnostics)) {
				return false;
			}
			for (const auto& item : analysis) {
				ProjectAnalysisSource source{};
				source.name = item.first.Scalar();
				if (!configuration_name(source.name, true)) {
					return configuration_failure(path, item.first, "analysis group names may contain only ASCII letters, digits, '_' and '-'", diagnostics);
				}
				if (!configuration_mapping(path, item.second, { "source", "sources" }, false, diagnostics)) {
					return false;
				}
				auto folder = item.second["source"];
				auto inputs = item.second["sources"];
				if (folder && inputs) {
					return configuration_failure(path, item.second, "analysis groups require either 'source' or 'sources', not both", diagnostics);
				}
				if (folder) {
					std::string text;
					if (!configuration_string(path, folder, "source", text, diagnostics)) {
						return false;
					}
					auto directory = (root / std::filesystem::u8path(text)).lexically_normal();
					std::error_code error;
					if (!std::filesystem::is_directory(directory, error) || error) {
						return configuration_failure(path, folder, "analysis source folder is unavailable: " + text, diagnostics);
					}
					source.sources.push_back(ProjectSourcePath{ std::move(directory) });
				}
				else {
					if (!inputs || !inputs.IsSequence() || !inputs.size()) {
						return configuration_failure(path, item.second, "analysis groups require a 'source' folder or a nonempty ordered 'sources' sequence", diagnostics);
					}
					for (const auto& input : inputs) {
						if (input.IsScalar()) {
							std::string text;
							if (!configuration_string(path, input, "sources entry", text, diagnostics)) {
								return false;
							}
							auto resolved = (root / std::filesystem::u8path(text)).lexically_normal();
							std::error_code error;
							if ((!std::filesystem::is_regular_file(resolved, error) && !std::filesystem::is_directory(resolved, error)) || error) {
								return configuration_failure(path, input, "analysis source path is unavailable: " + text, diagnostics);
							}
							source.sources.push_back(ProjectSourcePath{ std::move(resolved) });
							continue;
						}
						if (!configuration_mapping(path, input, { "manifest", "root" }, false, diagnostics)) {
							return false;
						}
						std::string manifest;
						std::string directory;
						if (!configuration_string(path, input["manifest"], "manifest", manifest, diagnostics) || !configuration_string(path, input["root"], "root", directory, diagnostics)) {
							return false;
						}
						ProjectSourceManifest entry{ (root / std::filesystem::u8path(manifest)).lexically_normal(), (root / std::filesystem::u8path(directory)).lexically_normal() };
						std::error_code error;
						if (!std::filesystem::is_regular_file(entry.path, error) || error) {
							return configuration_failure(path, input["manifest"], "analysis source manifest is unavailable: " + manifest, diagnostics);
						}
						if (!std::filesystem::is_directory(entry.root, error) || error) {
							return configuration_failure(path, input["root"], "analysis manifest root is unavailable: " + directory, diagnostics);
						}
						source.sources.push_back(std::move(entry));
					}
				}
				parsed.analysis.push_back(std::move(source));
			}
		}
		parsed.profiles = { { "debug", 0 }, { "release", 2 } };
		auto profiles = document["profiles"];
		if (profiles) {
			if (!configuration_mapping(path, profiles, { "debug", "release" }, false, diagnostics)) {
				return false;
			}
			for (auto& profile : parsed.profiles) {
				auto settings = profiles[profile.name];
				if (!settings) {
					continue;
				}
				if (!configuration_mapping(path, settings, { "optimization" }, false, diagnostics)) {
					return false;
				}
				auto optimization = settings["optimization"];
				if (!optimization) {
					continue;
				}
				if (!optimization.IsScalar() || optimization.Tag() == "!" || optimization.Tag() == "tag:yaml.org,2002:str") {
					return configuration_failure(path, optimization, "optimization must be an integer from 0 through 3", diagnostics);
				}
				auto text = optimization.Scalar();
				auto result = std::from_chars(text.data(), text.data() + text.size(), profile.optimization);
				if (result.ec != std::errc{} || result.ptr != text.data() + text.size() || profile.optimization < 0 || profile.optimization > 3) {
					return configuration_failure(path, optimization, "optimization must be an integer from 0 through 3", diagnostics);
				}
			}
		}
		configuration = std::move(parsed);
		return true;
	}
	catch (const YAML::Exception& error) {
		diagnostics = project_path_utf8(path);
		if (!error.mark.is_null()) {
			diagnostics += ":" + std::to_string(error.mark.line + 1) + ":" + std::to_string(error.mark.column + 1);
		}
		diagnostics += ": " + error.msg + '\n';
		return false;
	}
}
