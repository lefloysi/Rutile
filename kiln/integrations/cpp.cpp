#include "cpp.hpp"

#include "platform.hpp"
#include "project.hpp"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cstdlib>
#include <cstdint>
#include <fstream>
#include <iterator>
#include <set>
#include <utility>

struct CppTools {
	std::filesystem::path compiler;
	std::filesystem::path linker;
	std::filesystem::path archiver;
	std::vector<std::filesystem::path> includes;
	std::vector<std::filesystem::path> libraries;
};

std::vector<std::string> cpp_list(std::string_view text) {
	if (text.empty()) {
		return {};
	}
	if (text.front() == '[') {
		return nlohmann::json::parse(text).get<std::vector<std::string>>();
	}
	std::vector<std::string> result;
	while (!text.empty()) {
		auto end = text.find(';');
		auto item = text.substr(0, end);
		if (!item.empty()) {
			result.emplace_back(item);
		}
		if (end == std::string_view::npos) {
			break;
		}
		text.remove_prefix(end + 1);
	}
	return result;
}

std::vector<std::string> cpp_option_list(const ProjectTarget& target, std::string_view name) {
	auto value = target.options.find(std::string(name));
	return value == target.options.end() ? std::vector<std::string>{} : cpp_list(value->second);
}

std::string cpp_option(const ProjectTarget& target, const char* name, const char* fallback = "") {
	auto value = target.options.find(name);
	return value == target.options.end() ? fallback : value->second;
}

std::filesystem::path cpp_path(const std::filesystem::path& root, std::string_view value) {
	auto path = std::filesystem::u8path(value);
	return path.is_absolute() ? path : root / path;
}

std::string cpp_read(const std::filesystem::path& path) {
	std::ifstream file(path, std::ios::binary);
	return std::string(std::istreambuf_iterator<char>(file), {});
}

std::string cpp_fingerprint(const std::filesystem::path& path) {
	std::ifstream file(path, std::ios::binary);
	if (!file) {
		return {};
	}
	std::uint64_t hash = 14695981039346656037ull;
	char bytes[8192];
	while (file) {
		file.read(bytes, sizeof(bytes));
		for (std::streamsize index = 0; index < file.gcount(); ++index) {
			hash = (hash ^ static_cast<unsigned char>(bytes[index])) * 1099511628211ull;
		}
	}
	return std::to_string(hash);
}

bool cpp_cached(const std::filesystem::path& object, const std::filesystem::path& cache, const std::vector<std::string>& command) {
	if (!std::filesystem::is_regular_file(object)) {
		return false;
	}
	try {
		auto record = nlohmann::json::parse(cpp_read(cache));
		if (record.at("command") != command || record.at("inputs").empty()) {
			return false;
		}
		for (const auto& [path, fingerprint] : record.at("inputs").items()) {
			if (fingerprint != cpp_fingerprint(std::filesystem::u8path(path))) {
				return false;
			}
		}
		return true;
	}
	catch (const nlohmann::json::exception&) {
		return false;
	}
}

void cpp_record(const std::filesystem::path& source, const std::filesystem::path& dependencies, const std::filesystem::path& cache, const std::vector<std::string>& command) {
	nlohmann::json inputs = nlohmann::json::object();
	inputs[project_path_utf8(source)] = cpp_fingerprint(source);
	try {
		auto record = nlohmann::json::parse(cpp_read(dependencies));
		for (const auto& include : record.at("Data").at("Includes")) {
			auto path = include.get<std::string>();
			inputs[path] = cpp_fingerprint(std::filesystem::u8path(path));
		}
	}
	catch (const nlohmann::json::exception&) {
		std::error_code error;
		std::filesystem::remove(cache, error);
		return;
	}
	std::ofstream(cache) << nlohmann::json{ { "command", command }, { "inputs", inputs } }.dump();
}

std::filesystem::path cpp_latest_directory(const std::filesystem::path& root) {
	std::error_code error;
	std::vector<std::filesystem::path> entries;
	for (std::filesystem::directory_iterator iterator(root, error), end; !error && iterator != end; iterator.increment(error)) {
		if (iterator->is_directory(error)) {
			entries.push_back(iterator->path());
		}
	}
	std::sort(entries.begin(), entries.end(), std::greater<>());
	return entries.empty() ? std::filesystem::path{} : entries.front();
}

CppTools cpp_tools(const ProjectTarget& target, const std::filesystem::path& root) {
	CppTools tools;
#if defined(_WIN32)
	std::filesystem::path linker = std::filesystem::u8path(RUTILE_NATIVE_LINKER);
	const char* installation = std::getenv("VCToolsInstallDir");
	auto msvc = installation && *installation ? std::filesystem::u8path(installation) : linker.parent_path().parent_path().parent_path().parent_path();
	if (!std::filesystem::is_regular_file(msvc / "bin/Hostx64/x64/cl.exe")) {
		const char* programs = std::getenv("ProgramFiles");
		auto visual_studio = std::filesystem::u8path(programs ? programs : "C:/Program Files") / "Microsoft Visual Studio";
		std::error_code error;
		for (std::filesystem::recursive_directory_iterator iterator(visual_studio, std::filesystem::directory_options::skip_permission_denied, error), end; !error && iterator != end; iterator.increment(error)) {
			if (iterator->path().filename() == "MSVC" && iterator->is_directory(error)) {
				auto candidate = cpp_latest_directory(iterator->path());
				if (std::filesystem::is_regular_file(candidate / "bin/Hostx64/x64/cl.exe")) {
					msvc = std::move(candidate);
					break;
				}
			}
		}
	}
	tools.compiler = msvc / "bin/Hostx64/x64/cl.exe";
	tools.linker = msvc / "bin/Hostx64/x64/link.exe";
	tools.archiver = msvc / "bin/Hostx64/x64/lib.exe";
	tools.includes.push_back(msvc / "include");
	tools.libraries.push_back(msvc / "lib/x64");
	auto sdk_version = std::filesystem::u8path(RUTILE_RUNTIME_LIBUCRT).parent_path().parent_path().parent_path();
	const char* sdk_root = std::getenv("WindowsSdkDir");
	const char* sdk_number = std::getenv("WindowsSDKVersion");
	auto sdk = sdk_root && *sdk_root ? std::filesystem::u8path(sdk_root) : sdk_version.parent_path().parent_path();
	auto number = sdk_number && *sdk_number ? std::filesystem::u8path(sdk_number).lexically_normal().filename() : sdk_version.filename();
	if (number.empty()) {
		number = cpp_latest_directory(sdk / "Include").filename();
	}
	for (const char* component : { "ucrt", "shared", "um", "winrt", "cppwinrt" }) {
		tools.includes.push_back(sdk / "Include" / number / component);
	}
	for (const char* component : { "ucrt", "um" }) {
		tools.libraries.push_back(sdk / "Lib" / number / component / "x64");
	}
#endif
	for (auto pair : { std::pair{ "compiler", &tools.compiler }, std::pair{ "linker", &tools.linker }, std::pair{ "archiver", &tools.archiver } }) {
		auto value = cpp_option(target, pair.first);
		if (!value.empty()) {
			*pair.second = cpp_path(root, value);
		}
	}
	return tools;
}

struct CppIntegration : CompilerIntegration {
	std::string_view name() const override;
	bool accepts_source(const std::filesystem::path& path) const override;
	CompilerArtifact artifact(const ProjectTarget& target) const override;
	bool validate(const ProjectTarget& target, const ProjectProfile& profile, std::string& diagnostics) const override;
	CompilerResult build(const CompilerBuild& request) override;
};

std::string_view CppIntegration::name() const {
	return "cpp";
}

bool CppIntegration::accepts_source(const std::filesystem::path& path) const {
	const auto extension = path.extension();
	return extension == ".cpp" || extension == ".cc" || extension == ".cxx" || extension == ".c";
}

CompilerArtifact CppIntegration::artifact(const ProjectTarget& target) const {
	auto usage = target.type == target_type::executable ? artifact_usage::executable : artifact_usage::module;
	const char* extension = target.type == target_type::static_lib ? ".lib" : target.type == target_type::shared_lib ? ".dll" : ".exe";
	return { target.output.empty() ? std::filesystem::u8path(target.name + extension) : target.output, usage };
}

bool CppIntegration::validate(const ProjectTarget& target, const ProjectProfile& profile, std::string& diagnostics) const {
	if (target.type == target_type::object_lib) {
		diagnostics = "C++ targets support executables, static libraries, and shared libraries.\n";
		return false;
	}
#if !defined(_WIN32)
	diagnostics = "The C++ integration currently requires the Windows x64 MSVC toolchain.\n";
	return false;
#endif
	const std::set<std::string> lists{ "includes", "defines", "forced_includes", "libraries", "library_paths", "compile_options", "link_options", "objects", "excludes", "runtime_files" };
	const std::set<std::string> scalars{ "standard", "runtime", "compiler", "linker", "archiver" };
	try {
		for (const auto& [name, value] : target.options) {
			if (lists.contains(name)) {
				cpp_list(value);
				continue;
			}
			if (!scalars.contains(name)) {
				diagnostics = "Unknown C++ compiler option '" + name + "'.\n";
				return false;
			}
		}
	}
	catch (const nlohmann::json::exception& error) {
		diagnostics = "C++ list options must contain a JSON string array or semicolon-separated values: " + std::string(error.what()) + "\n";
		return false;
	}
	auto runtime = cpp_option(target, "runtime", "MD");
	if (runtime != "MD" && runtime != "MDd" && runtime != "MT" && runtime != "MTd") {
		diagnostics = "C++ runtime must be MD, MDd, MT, or MTd.\n";
		return false;
	}
	if (profile.optimization < 0 || profile.optimization > 3) {
		diagnostics = "C++ optimization must be between 0 and 3.\n";
		return false;
	}
	return true;
}

CompilerResult CppIntegration::build(const CompilerBuild& request) {
	auto tools = cpp_tools(request.target, request.root);
	for (const auto& tool : { tools.compiler, request.target.type == target_type::static_lib ? tools.archiver : tools.linker }) {
		if (!std::filesystem::is_regular_file(tool)) {
			return { false, "Cannot find the MSVC x64 toolchain. Install Visual Studio C++ build tools or set the C++ compiler/linker/archiver paths.\n" };
		}
	}
	const auto directory = request.output.parent_path();
	auto objects_directory = directory / "objects";
	std::filesystem::create_directories(objects_directory);
	std::string diagnostics;
	std::vector<std::string> objects;
	nlohmann::json commands = nlohmann::json::array();
	for (std::size_t index = 0; index < request.sources.size(); ++index) {
		const auto& source = request.sources[index];
		bool excluded = false;
		for (const auto& exclusion : cpp_option_list(request.target, "excludes")) {
			auto path = std::filesystem::u8path(exclusion);
			if ((path.has_parent_path() && cpp_path(request.root, exclusion).lexically_normal() == source.lexically_normal()) || (!path.has_parent_path() && path == source.filename())) {
				excluded = true;
				break;
			}
		}
		if (excluded) {
			continue;
		}
		auto object = objects_directory / (std::to_string(index) + ".obj");
		auto dependencies = objects_directory / (std::to_string(index) + ".dependencies.json");
		auto cache = objects_directory / (std::to_string(index) + ".cache.json");
		std::vector<std::string> arguments{ "/nologo", "/c", "/EHsc", "/utf-8", "/Z7", "/" + cpp_option(request.target, "runtime", "MD"), request.profile.optimization ? "/O2" : "/Od", "/Fo" + project_path_utf8(object) };
		arguments.insert(arguments.end(), { "/sourceDependencies", project_path_utf8(dependencies) });
		if (source.extension() != ".c") {
			arguments.push_back("/std:" + cpp_option(request.target, "standard", "c++latest"));
			arguments.push_back("/Zc:__cplusplus");
		}
		for (const auto& include : tools.includes) {
			arguments.push_back("/I" + project_path_utf8(include));
		}
		for (const auto& include : cpp_option_list(request.target, "includes")) {
			arguments.push_back("/I" + project_path_utf8(cpp_path(request.root, include)));
		}
		for (const auto& define : cpp_option_list(request.target, "defines")) {
			arguments.push_back("/D" + define);
		}
		for (const auto& include : cpp_option_list(request.target, "forced_includes")) {
			arguments.push_back("/FI" + project_path_utf8(cpp_path(request.root, include)));
		}
		for (const auto& option : cpp_option_list(request.target, "compile_options")) {
			arguments.push_back(option);
		}
		arguments.push_back(project_path_utf8(source));
		auto command = arguments;
		command.insert(command.begin(), project_path_utf8(tools.compiler));
		commands.push_back({ { "directory", project_path_utf8(request.root) }, { "file", project_path_utf8(source) }, { "output", project_path_utf8(object) }, { "arguments", command } });
		objects.push_back(project_path_utf8(object));
		if (cpp_cached(object, cache, command)) {
			continue;
		}
		std::error_code cache_error;
		std::filesystem::remove(cache, cache_error);
		auto log = objects_directory / (std::to_string(index) + ".log");
		int result = platform_run(tools.compiler, arguments, log);
		diagnostics += cpp_read(log);
		if (result) {
			return { false, diagnostics.empty() ? "C++ compilation failed.\n" : diagnostics };
		}
		cpp_record(source, dependencies, cache, command);
	}
	std::ofstream(directory / "compile_commands.json") << commands.dump(2);
	for (const auto& object : cpp_option_list(request.target, "objects")) {
		objects.push_back(project_path_utf8(cpp_path(request.root, object)));
	}
	if (objects.empty()) {
		return { false, "C++ target has no source or object files after exclusions.\n" };
	}
	std::vector<std::string> arguments{ "/nologo", "/machine:x64", "/out:" + project_path_utf8(request.output) };
	arguments.insert(arguments.end(), objects.begin(), objects.end());
	if (request.target.type != target_type::static_lib) {
		arguments.push_back("/DEBUG");
		arguments.push_back("/INCREMENTAL:NO");
		auto pdb = request.output;
		pdb.replace_extension(".pdb");
		arguments.push_back("/PDB:" + project_path_utf8(pdb));
		auto pdb_name = project_path_utf8(request.output.stem());
		if (pdb_name.ends_with(".pending")) {
			pdb_name.resize(pdb_name.size() - std::string_view(".pending").size());
		}
		arguments.push_back("/PDBALTPATH:" + pdb_name + ".pdb");
		if (request.target.type == target_type::shared_lib) {
			arguments.push_back("/DLL");
			auto library = request.output;
			library.replace_extension(".lib");
			arguments.push_back("/IMPLIB:" + project_path_utf8(library));
		}
		for (const auto& path : tools.libraries) {
			arguments.push_back("/LIBPATH:" + project_path_utf8(path));
		}
		for (const auto& path : cpp_option_list(request.target, "library_paths")) {
			arguments.push_back("/LIBPATH:" + project_path_utf8(cpp_path(request.root, path)));
		}
		for (const auto& library : cpp_option_list(request.target, "libraries")) {
			auto path = std::filesystem::u8path(library);
			arguments.push_back(path.has_parent_path() ? project_path_utf8(cpp_path(request.root, library)) : library);
		}
		for (auto dependency : request.dependency_artifacts) {
			if (dependency.extension() == ".dll") {
				dependency.replace_extension(".lib");
			}
			arguments.push_back(project_path_utf8(dependency));
		}
	}
	for (const auto& option : cpp_option_list(request.target, "link_options")) {
		arguments.push_back(option);
	}
	auto log = directory / "link.log";
	int result = platform_run(request.target.type == target_type::static_lib ? tools.archiver : tools.linker, arguments, log);
	diagnostics += cpp_read(log);
	if (result) {
		return { false, diagnostics };
	}
	for (const auto& file : cpp_option_list(request.target, "runtime_files")) {
		auto source = cpp_path(request.root, file);
		auto destination = directory / source.filename();
		std::error_code error;
		if (std::filesystem::equivalent(source, destination, error)) {
			continue;
		}
		if (std::filesystem::is_regular_file(destination) && cpp_fingerprint(source) == cpp_fingerprint(destination)) {
			continue;
		}
		error.clear();
		std::filesystem::copy_file(source, destination, std::filesystem::copy_options::overwrite_existing, error);
		if (error) {
			return { false, diagnostics + "Could not copy runtime dependency '" + file + "': " + error.message() + "\n" };
		}
	}
	return { true, diagnostics };
}

std::unique_ptr<CompilerIntegration> kiln_cpp_integration() {
	return std::make_unique<CppIntegration>();
}
