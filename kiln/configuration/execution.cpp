#include "execution.hpp"

#include "platform.hpp"
#include "../project/project.hpp"
#include "../src/compiler.hpp"
#include <nlohmann/json.hpp>

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <fstream>
#include <memory>
#include <string_view>
#include <utility>

extern "C" int rutile_compile_library(const char* const* paths, std::size_t count, const char* output, std::FILE* (*opener)(const char*, const char*), int optimization, std::FILE* diagnostics);
bool configuration_name(std::string_view name, bool target);

struct KilnConfigurationContext {
	std::filesystem::path root;
	ProjectConfiguration configuration;
	std::string diagnostics;
	std::filesystem::path directory;
	std::string profile;
};

struct KilnConfigurationApi {
	void* context;
	std::size_t (*target)(void*, const char*, std::size_t, const char*, std::size_t, const char*, std::size_t, int);
	int (*option)(void*, std::size_t, const char*, std::size_t, const char*, std::size_t);
	int (*source)(void*, std::size_t, const char*, std::size_t);
	struct View {
		const char* data;
		std::size_t size;
	};
	View root;
	View os;
	View profile;
	int (*add_profile)(void*, const char*, std::size_t, int);
};

bool kiln_configuration_program(const std::filesystem::path& root, const ProjectProgram& program, ProjectConfiguration& configuration, std::string& diagnostics, std::string_view profile);

int kiln_configuration_profile(void* opaque, const char* name_data, std::size_t name_size, int optimization) {
	auto& context = *static_cast<KilnConfigurationContext*>(opaque);
	std::string name(name_data, name_size);
	if (!configuration_name(name, true) || optimization < 0 || optimization > 3) {
		context.diagnostics += "Build profiles need an identifier and optimization between 0 and 3.\n";
		return 0;
	}
	auto profile = std::find_if(context.configuration.profiles.begin(), context.configuration.profiles.end(), [&](const ProjectProfile& item) { return item.name == name; });
	if (profile == context.configuration.profiles.end()) {
		context.configuration.profiles.push_back({ std::move(name), optimization });
	}
	else {
		profile->optimization = optimization;
	}
	return 1;
}

std::size_t kiln_configuration_target(void* opaque, const char* name_data, std::size_t name_size, const char* compiler_data, std::size_t compiler_size, const char* source_data, std::size_t source_size, int kind) {
	auto& context = *static_cast<KilnConfigurationContext*>(opaque);
	std::string name(name_data, name_size);
	std::string compiler(compiler_data, compiler_size);
	std::string source(source_data, source_size);
	if (name.empty() || name == "." || name == ".." || name.find_first_of("/\\:\0", 0, 4) != std::string::npos || !configuration_name(compiler, false) || kind < 0 || kind > 3 || source.find('\0') != std::string::npos) {
		context.diagnostics += "Configuration declared an invalid target name, compiler, or source folder.\n";
		return 0;
	}
	if (std::any_of(context.configuration.targets.begin(), context.configuration.targets.end(), [&](const ProjectTarget& target) { return target.name == name; })) {
		context.diagnostics += "Configuration declared duplicate target '" + name + "'.\n";
		return 0;
	}
	auto path = (context.directory / std::filesystem::u8path(source)).lexically_normal();
	std::error_code error;
	if (!source.empty() && (!std::filesystem::is_directory(path, error) || error)) {
		context.diagnostics += "Target '" + name + "' source folder does not exist.\n";
		return 0;
	}
	ProjectTarget target;
	target.name = std::move(name);
	target.compiler = std::move(compiler);
	target.output = std::filesystem::u8path(target.name);
	target.type = kind == 1 ? target_type::shared_lib : kind == 2 ? target_type::static_lib : kind == 3 ? target_type::object_lib : target_type::executable;
	if (!source.empty()) {
		target.source = path;
		target.source_folders.push_back(std::move(path));
	}
	target.entry = "main";
	context.configuration.targets.push_back(std::move(target));
	return context.configuration.targets.size();
}

int kiln_configuration_option(void* opaque, std::size_t target_id, const char* key_data, std::size_t key_size, const char* value_data, std::size_t value_size) {
	auto& context = *static_cast<KilnConfigurationContext*>(opaque);
	std::string key(key_data, key_size);
	std::string value(value_data, value_size);
	if (!target_id && key == "subdirectory") {
		std::error_code error;
		auto path = std::filesystem::weakly_canonical(context.directory / std::filesystem::u8path(value) / "kiln.rt", error);
		if (value.empty() || value.find('\0') != std::string::npos || error || !std::filesystem::is_regular_file(path, error) || error) {
			context.diagnostics += "Subdirectory configuration is unavailable: " + project_path_utf8(path) + "\n";
			return 0;
		}
		if (std::any_of(context.configuration.projects.begin(), context.configuration.projects.end(), [&](const ProjectProgram& program) { return program.source.lexically_normal() == path; })) {
			context.diagnostics += "Duplicate subdirectory configuration was already included (or forms a cycle): " + project_path_utf8(path) + "\n";
			return 0;
		}
		ProjectProgram program{ "main", path };
		context.configuration.projects.push_back(program);
		std::string diagnostics;
		bool succeeded = kiln_configuration_program(context.root, program, context.configuration, diagnostics, context.profile);
		if (!succeeded) {
			context.diagnostics += diagnostics;
		}
		return succeeded ? 1 : 0;
	}
	auto target = target_id && target_id <= context.configuration.targets.size() ? context.configuration.targets.begin() + target_id - 1 : context.configuration.targets.end();
	if (target == context.configuration.targets.end() || key.empty() || key.find('\0') != std::string::npos || value.find('\0') != std::string::npos) {
		context.diagnostics += "Configuration option requires an existing target and a valid key and value.\n";
		return 0;
	}
	if (key == "compiler") {
		if (!configuration_name(value, false)) {
			context.diagnostics += "Invalid compiler integration identifier.\n";
			return 0;
		}
		target->compiler = std::move(value);
		return 1;
	}
	if (key == "dependency") {
		if (value.empty()) {
			context.diagnostics += "Target dependency requires a name.\n";
			return 0;
		}
		target->dependencies.push_back(std::move(value));
		return 1;
	}
	if (key == "includes" || key == "forced_includes" || key == "objects" || key == "library_paths" || key == "runtime_files" || key == "excludes" || key == "libraries") {
		auto values = kiln_option_values(value);
		if (value.starts_with("[") && values.empty() && value != "[]") {
			context.diagnostics += "Invalid path option list.\n";
			return 0;
		}
		for (auto& item : values) {
			auto path = std::filesystem::u8path(item);
			if (key != "libraries" || path.has_parent_path()) {
				item = project_path_utf8((context.directory / path).lexically_normal());
			}
		}
		value = nlohmann::json(values).dump();
	}
	target->options[std::move(key)] = std::move(value);
	return 1;
}

int kiln_configuration_source(void* opaque, std::size_t target_id, const char* folder_data, std::size_t folder_size) {
	auto& context = *static_cast<KilnConfigurationContext*>(opaque);
	std::string folder(folder_data, folder_size);
	auto target = target_id && target_id <= context.configuration.targets.size() ? context.configuration.targets.begin() + target_id - 1 : context.configuration.targets.end();
	std::error_code error;
	auto path = (context.directory / std::filesystem::u8path(folder)).lexically_normal();
	if (target == context.configuration.targets.end() || folder.empty() || folder.find('\0') != std::string::npos || !std::filesystem::is_directory(path, error) || error) {
		context.diagnostics += "Target sources must name an existing source folder: '" + folder + "' (resolved to '" + project_path_utf8(path) + "').\n";
		return 0;
	}
	if (std::find(target->source_folders.begin(), target->source_folders.end(), path) == target->source_folders.end()) {
		target->source_folders.push_back(path);
	}
	if (target->source.empty()) {
		target->source = std::move(path);
	}
	return 1;
}

std::FILE* kiln_configuration_open(const char* path, const char* mode) {
	return platform_open_file(std::filesystem::u8path(path), mode);
}

std::string kiln_configuration_diagnostics(std::FILE* file) {
	std::rewind(file);
	std::string result;
	char buffer[4096];
	while (std::size_t count = std::fread(buffer, 1, sizeof(buffer), file)) {
		result.append(buffer, count);
	}
	return result;
}

void kiln_configuration_cleanup(const std::filesystem::path& base) {
	for (const char* extension : { ".o", ".dll", ".so", ".log", ".lib", ".exp", ".rt" }) {
		auto path = base;
		path += extension;
		std::error_code error;
		std::filesystem::remove(path, error);
	}
}

bool kiln_configuration_program(const std::filesystem::path& root, const ProjectProgram& program, ProjectConfiguration& configuration, std::string& diagnostics, std::string_view profile) {
	std::error_code error;
	auto cache = root / ".kiln" / "configure";
	std::filesystem::create_directories(cache, error);
	if (error) {
		diagnostics = "Could not create configuration output folder.\n";
		return false;
	}
	auto sequence = std::chrono::steady_clock::now().time_since_epoch().count();
	auto base = cache / std::to_string(sequence);
	auto object = base;
	object += ".o";
	auto library = base;
#if defined(_WIN32)
	library += ".dll";
#else
	library += ".so";
#endif
	auto log = base;
	log += ".log";
	std::filesystem::path standard_library = std::filesystem::u8path(KILN_CONFIGURATION_STDLIB);
	std::filesystem::path api_source = std::filesystem::u8path(KILN_CONFIGURATION_API);
	char executable_path[32768];
	if (platform_executable_path(executable_path, sizeof(executable_path))) {
		auto location = std::filesystem::u8path(executable_path).parent_path();
		for (const auto& base : { location, location.parent_path() }) {
			auto resources = base / "share" / "kiln";
			if (std::filesystem::is_regular_file(resources / "api.rt", error) && !error) {
				standard_library = resources / "std";
				api_source = resources / "api.rt";
				break;
			}
		}
	}
	std::vector<std::string> paths;
	for (const char* name : { "unique.rt", "optional.rt", "buffer.rt", "string_view.rt", "string.rt", "hash.rt", "equal.rt", "unordered_map.rt" }) {
		paths.push_back(project_path_utf8(standard_library / name));
	}
	paths.push_back(project_path_utf8(api_source));
	paths.push_back(project_path_utf8(root / program.source));
	auto trampoline = base;
	trampoline += ".rt";
	std::ofstream wrapper(trampoline);
	wrapper << "fn kiln_configure(var kiln::Options& options) -> i32 {\n\tkiln::begin(@options);\n\t" << program.function << "(options);\n\treturn kiln::finish();\n};\n";
	wrapper.close();
	if (!wrapper) {
		kiln_configuration_cleanup(base);
		diagnostics = "Could not write configuration entry point.\n";
		return false;
	}
	paths.push_back(project_path_utf8(trampoline));
	std::vector<const char*> arguments;
	for (const auto& path : paths) {
		arguments.push_back(path.c_str());
	}
	std::unique_ptr<std::FILE, decltype(&std::fclose)> messages{ std::tmpfile(), std::fclose };
	if (!messages) {
		diagnostics = "Could not open configuration diagnostics.\n";
		return false;
	}
	int result = rutile_compile_library(arguments.data(), arguments.size(), project_path_utf8(object).c_str(), kiln_configuration_open, 0, messages.get());
	diagnostics = kiln_configuration_diagnostics(messages.get());
	if (!result) {
		std::vector<std::string> link_arguments;
#if defined(_WIN32)
		link_arguments = { "/DLL", "/EXPORT:kiln_configure", "/machine:x64", "/LIBPATH:" + project_path_utf8(std::filesystem::u8path(RUTILE_RUNTIME_KERNEL32).parent_path()), project_path_utf8(object), RUTILE_RUNTIME_LIBCMT, RUTILE_RUNTIME_LIBVCRUNTIME, RUTILE_RUNTIME_LIBUCRT, RUTILE_RUNTIME_OLDNAMES, RUTILE_RUNTIME_KERNEL32, "/out:" + project_path_utf8(library) };
#else
		link_arguments = { "-shared", project_path_utf8(object), "-o", project_path_utf8(library) };
#endif
		result = platform_run(std::filesystem::u8path(RUTILE_NATIVE_LINKER), link_arguments, log);
		std::unique_ptr<std::FILE, decltype(&std::fclose)> link_log{ platform_open_file(log, "rb"), std::fclose };
		if (link_log) {
			diagnostics += kiln_configuration_diagnostics(link_log.get());
		}
	}
	if (result) {
		kiln_configuration_cleanup(base);
		if (diagnostics.empty()) {
			diagnostics = "Could not compile the Rutile configuration program.\n";
		}
		return false;
	}
	void* handle = platform_library_open(library);
	if (!handle) {
		kiln_configuration_cleanup(base);
		diagnostics += "Could not load the Rutile configuration program.\n";
		return false;
	}
	auto configure = reinterpret_cast<int (*)(KilnConfigurationApi*)>(platform_library_symbol(handle, "kiln_configure"));
	KilnConfigurationContext context{ root, configuration, {}, program.source.parent_path(), std::string(profile) };
	std::string root_text = project_path_utf8(program.source.parent_path());
#if defined(_WIN32)
	std::string os = "windows";
#elif defined(__APPLE__)
	std::string os = "macos";
#else
	std::string os = "linux";
#endif
	KilnConfigurationApi api{ &context, kiln_configuration_target, kiln_configuration_option, kiln_configuration_source, { root_text.data(), root_text.size() }, { os.data(), os.size() }, { profile.data(), profile.size() }, kiln_configuration_profile };
	result = configure ? configure(&api) : 1;
	platform_library_close(handle);
	kiln_configuration_cleanup(base);
	diagnostics += context.diagnostics;
	if (result || !context.diagnostics.empty()) {
		diagnostics += configure ? "Rutile project configuration failed.\n" : "Configuration program must define kiln_configure.\n";
		return false;
	}
	configuration = std::move(context.configuration);
	return true;
}



bool kiln_configure_project(const std::filesystem::path& root, ProjectConfiguration& configuration, std::string& diagnostics, std::string_view profile) {
	ProjectConfiguration pending = configuration;
	for (const auto& program : configuration.projects) {
		if (!kiln_configuration_program(root, program, pending, diagnostics, profile)) {
			return false;
		}
	}
	for (const auto& target : pending.targets) {
		if (target.source.empty() && target.source_folders.empty()) {
			diagnostics += "Target '" + target.name + "' requires at least one source folder.\n";
			return false;
		}
	}
	configuration = std::move(pending);
	return true;
}
