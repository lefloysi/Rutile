#include "service.hpp"

#include "platform.hpp"
#include "version.hpp"

#include <nlohmann/json.hpp>
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

struct Compiler;
using RutileProgressCallback = void (*)(void*, const char*, std::size_t, std::size_t, const char*);
extern "C" Compiler* rutile_compiler_create();
extern "C" void rutile_compiler_destroy(Compiler* compiler);
extern "C" void rutile_compiler_set_progress(Compiler* compiler, RutileProgressCallback callback, void* context);
extern "C" int rutile_compile_sources(Compiler* compiler, const char* const* paths, const char* const* texts, const std::size_t* lengths, std::size_t count, const char* output, std::FILE* (*opener)(const char*, const char*), const char* entry, int optimization, std::FILE* diagnostics, int library);

std::string rutile_service_path(const std::filesystem::path& path) {
	auto text = path.generic_u8string();
	return std::string(reinterpret_cast<const char*>(text.data()), text.size());
}

std::string rutile_service_read(const std::filesystem::path& path) {
	std::ifstream file(path, std::ios::binary);
	if (!file) {
		throw std::runtime_error("Cannot read source input: " + rutile_service_path(path));
	}
	std::string result(std::istreambuf_iterator<char>(file), {});
	if (file.bad()) {
		throw std::runtime_error("Cannot finish reading source input: " + rutile_service_path(path));
	}
	return result;
}

void rutile_service_write(const std::filesystem::path& path, std::string_view text) {
	std::ofstream file(path, std::ios::binary | std::ios::trunc);
	file.write(text.data(), static_cast<std::streamsize>(text.size()));
	file.close();
	if (!file) {
		throw std::runtime_error("Cannot write compiler output: " + rutile_service_path(path));
	}
}

struct RutileServiceProgress {
	std::filesystem::path directory;
	std::string request_id;
	std::size_t sequence = 0;
	std::chrono::steady_clock::time_point last;
	std::string phase;
	std::string state = "running";
	~RutileServiceProgress();
};

void rutile_service_progress(void* context, const char* phase, std::size_t completed, std::size_t total, const char* detail) {
	auto& progress = *static_cast<RutileServiceProgress*>(context);
	auto now = std::chrono::steady_clock::now();
	if (progress.phase == phase && completed != total && now - progress.last < std::chrono::milliseconds(40)) {
		return;
	}
	progress.phase = phase;
	progress.last = now;
	try {
		auto text = nlohmann::json{ { "request_id", progress.request_id }, { "phase", phase }, { "completed", completed }, { "total", total }, { "detail", detail ? detail : "" }, { "sequence", ++progress.sequence }, { "state", progress.state }, { "pid", platform_process_id() } }.dump(-1, ' ', false, nlohmann::json::error_handler_t::replace);
		auto pending = progress.directory / "progress.pending.json";
		rutile_service_write(pending, text);
		platform_replace_file(pending, progress.directory / "progress.json");
	}
	catch (const std::exception&) {
		// Progress delivery must not alter compilation results.
	}
}

RutileServiceProgress::~RutileServiceProgress() {
	if (state == "running") {
		state = "failed";
		rutile_service_progress(this, "failed", 0, 0, "Compilation did not complete");
	}
}

std::FILE* rutile_service_open(const char* path, const char* mode) {
	return platform_open_file(std::filesystem::u8path(path), mode);
}

std::string rutile_service_diagnostics(std::FILE* file) {
	std::rewind(file);
	std::string result;
	char buffer[4096];
	while (true) {
		auto count = std::fread(buffer, 1, sizeof(buffer), file);
		if (!count) {
			break;
		}
		result.append(buffer, count);
		if (result.size() > 512 * 1024) {
			result += "\nFurther diagnostics omitted.\n";
			break;
		}
	}
	return result;
}

nlohmann::json rutile_service_compile(Compiler* compiler, const nlohmann::json& request, const std::filesystem::path& directory) {
	RutileServiceProgress progress{ directory, request.at("request_id").get<std::string>() };
	auto names = request.at("sources").get<std::vector<std::string>>();
	if (names.empty()) {
		throw std::runtime_error("No compiler sources were supplied.");
	}
	auto output = std::filesystem::absolute(std::filesystem::u8path(request.at("output").get<std::string>())).lexically_normal();
	auto bundle_path = output;
	bundle_path += ".sources.rt";
	auto map_path = output;
	map_path += ".sources.json";
	for (const auto& name : names) {
		auto source = std::filesystem::absolute(std::filesystem::u8path(name)).lexically_normal();
		for (const auto& destination : { output, bundle_path, map_path }) {
			std::error_code error;
			if (source == destination || std::filesystem::equivalent(source, destination, error)) {
				throw std::runtime_error("Compiler output would overwrite a source file.");
			}
		}
	}
	std::filesystem::create_directories(output.parent_path());
	nlohmann::json overlays = nlohmann::json::object();
	if (request.contains("overlays_path")) {
		overlays = nlohmann::json::parse(rutile_service_read(std::filesystem::u8path(request.at("overlays_path").get<std::string>())));
		if (!overlays.is_object()) {
			throw std::runtime_error("Editor source snapshot is invalid.");
		}
	}
	std::string bundle;
	std::vector<std::size_t> offsets;
	std::vector<std::size_t> lengths;
	auto source_map = nlohmann::json::array();
	rutile_service_progress(&progress, "combining", 0, names.size(), "Combining source files into one mapped input");
	for (std::size_t index = 0; index < names.size(); ++index) {
		const auto& name = names[index];
		auto text = overlays.contains(name) ? overlays.at(name).get<std::string>() : rutile_service_read(std::filesystem::u8path(name));
		bundle += "// source " + nlohmann::json(name).dump() + "\n";
		offsets.push_back(bundle.size());
		lengths.push_back(text.size());
		source_map.push_back({ { "path", name }, { "offset", bundle.size() }, { "length", text.size() } });
		bundle += text;
		bundle += '\n';
		rutile_service_progress(&progress, "combining", index + 1, names.size(), name.c_str());
	}
	rutile_service_write(bundle_path, bundle);
	rutile_service_write(map_path, nlohmann::json{ { "bundle", rutile_service_path(bundle_path) }, { "sources", source_map } }.dump(2));
	auto bundle_text = rutile_service_path(bundle_path);
	rutile_service_progress(&progress, "combined", names.size(), names.size(), bundle_text.c_str());
	std::vector<const char*> paths;
	std::vector<const char*> texts;
	for (std::size_t index = 0; index < names.size(); ++index) {
		paths.push_back(names[index].c_str());
		texts.push_back(bundle.data() + offsets[index]);
	}
	std::unique_ptr<std::FILE, decltype(&std::fclose)> diagnostics{ std::tmpfile(), std::fclose };
	if (!diagnostics) {
		throw std::runtime_error("Cannot create compiler diagnostics buffer.");
	}
	auto entry = request.value("entry", std::string("main"));
	auto kind = request.at("kind").get<std::string>();
	if (kind != "object" && kind != "executable") {
		throw std::runtime_error("Unknown compiler output kind.");
	}
	auto output_text = rutile_service_path(output);
	rutile_compiler_set_progress(compiler, rutile_service_progress, &progress);
	int result = rutile_compile_sources(compiler, paths.data(), texts.data(), lengths.data(), paths.size(), output_text.c_str(), rutile_service_open, entry.c_str(), request.at("optimization").get<int>(), diagnostics.get(), kind == "object");
	rutile_compiler_set_progress(compiler, nullptr, nullptr);
	progress.state = result ? "failed" : "succeeded";
	rutile_service_progress(&progress, "compiled", 1, 1, result ? "Compilation failed" : "Compilation completed");
	return { { "succeeded", !result }, { "diagnostics", rutile_service_diagnostics(diagnostics.get()) }, { "bundle", rutile_service_path(bundle_path) }, { "source_map", rutile_service_path(map_path) }, { "pid", platform_process_id() } };
}

int rutile_service_serve(const char* endpoint) {
	auto root = std::filesystem::current_path();
	auto directory = root / ".kiln" / "rutile";
	std::filesystem::create_directories(directory);
	char expected[256];
	if (!platform_endpoint(rutile_service_path(directory).c_str(), expected, sizeof(expected)) || std::string_view(endpoint) != expected) {
		std::fputs("Compiler connection does not belong to this project.\n", stderr);
		return 1;
	}
	std::unique_ptr<PlatformLock, decltype(&platform_lock_release)> lock{ platform_lock_acquire(rutile_service_path(directory / "process.lock").c_str()), platform_lock_release };
	if (!lock) {
		std::fputs("The project compiler is already running.\n", stderr);
		return 1;
	}
	std::unique_ptr<Compiler, decltype(&rutile_compiler_destroy)> compiler{ rutile_compiler_create(), rutile_compiler_destroy };
	std::unique_ptr<PlatformServer, decltype(&platform_server_close)> server{ platform_server_open(endpoint), platform_server_close };
	if (!compiler || !server || !platform_write_file(rutile_service_path(directory / "connection").c_str(), endpoint)) {
		std::fputs("Cannot initialize the compiler service.\n", stderr);
		return 1;
	}
	std::size_t compilations = 0;
	char executable_buffer[32768];
	if (!platform_executable_path(executable_buffer, sizeof(executable_buffer))) {
		return 1;
	}
	auto executable = std::filesystem::absolute(std::filesystem::u8path(executable_buffer)).lexically_normal();
	auto binary_stamp = std::to_string(std::filesystem::last_write_time(executable).time_since_epoch().count());
	std::vector<char> command(16 * 1024 * 1024);
	while (true) {
		if (!platform_server_accept(server.get())) {
			return 1;
		}
		bool stop = false;
		if (platform_server_read(server.get(), command.data(), command.size())) {
			std::string_view text(command.data());
			std::string response;
			if (text == "hello") {
				response = nlohmann::json{ { "service", "rutile" }, { "protocol", 1 }, { "pid", platform_process_id() }, { "version", software_version }, { "compilations", compilations }, { "executable", rutile_service_path(executable) }, { "binary_stamp", binary_stamp } }.dump();
			}
			else if (text == "ping") {
				response = "ready";
			}
			else if (text == "exit") {
				response = "Compiler stopped.";
				stop = true;
			}
			else if (text.starts_with("compile ")) {
				try {
					auto result = rutile_service_compile(compiler.get(), nlohmann::json::parse(text.substr(8)), directory);
					++compilations;
					result["compilations"] = compilations;
					response = result.dump(-1, ' ', false, nlohmann::json::error_handler_t::replace);
				}
				catch (const std::exception& error) {
					rutile_compiler_set_progress(compiler.get(), nullptr, nullptr);
					response = nlohmann::json{ { "succeeded", false }, { "diagnostics", std::string(error.what()) + "\n" }, { "pid", platform_process_id() } }.dump();
				}
			}
			else {
				response = "Unknown compiler operation.";
			}
			platform_server_write(server.get(), response.c_str());
		}
		platform_server_disconnect(server.get());
		if (stop) {
			return 0;
		}
	}
}

int rutile_service_control(const char* operation) {
	auto root = std::filesystem::current_path();
	while (true) {
		char endpoint[256];
		if (platform_read_file(rutile_service_path(root / ".kiln" / "rutile" / "connection").c_str(), endpoint, sizeof(endpoint))) {
			char response[4096];
			if (!platform_request(endpoint, std::string_view(operation) == "status" ? "hello" : "exit", response, sizeof(response), 5000)) {
				break;
			}
			std::cout << response << '\n';
			return 0;
		}
		auto parent = root.parent_path();
		if (parent == root) {
			break;
		}
		root = std::move(parent);
	}
	std::fputs("The project compiler is not running. Build a Rutile target through Kiln first.\n", stderr);
	return 1;
}
