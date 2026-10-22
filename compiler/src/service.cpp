#include "service.hpp"

#include "platform.hpp"
#include "version.hpp"

#include <nlohmann/json.hpp>
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <future>
#include <iostream>
#include <memory>
#include <mutex>
#include <set>
#include <stdexcept>
#include <string>
#include <thread>
#include <condition_variable>
#include <deque>
#include <unordered_map>
#include <vector>

struct Compiler;
using RutileProgressCallback = void (*)(void*, const char*, std::size_t, std::size_t, const char*);
extern "C" Compiler* rutile_compiler_create();
extern "C" void rutile_compiler_destroy(Compiler* compiler);
extern "C" void rutile_compiler_set_progress(Compiler* compiler, RutileProgressCallback callback, void* context);
extern "C" int rutile_compile_sources(Compiler* compiler, const char* const* paths, const char* const* texts, const std::size_t* lengths, std::size_t count, const char* output, std::FILE* (*opener)(const char*, const char*), const char* entry, int optimization, std::FILE* diagnostics, int library);
extern "C" int kiln_language_server(Compiler* compiler, std::FILE* input, std::FILE* output, const char* root);

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

std::mutex rutile_service_progress_mutex;
nlohmann::json rutile_service_progress_snapshots = nlohmann::json::object();
nlohmann::json rutile_service_latest_progress = nlohmann::json::object();

struct RutileServiceProgress {
	std::filesystem::path directory;
	std::string request_id;
	std::size_t sequence = 0;
	nlohmann::json metadata = nlohmann::json::object();
	std::chrono::steady_clock::time_point last;
	std::chrono::steady_clock::time_point last_snapshot;
	std::string phase;
	std::string state = "running";
	RutileServiceProgress(std::filesystem::path path, std::string id, nlohmann::json event_metadata = nlohmann::json::object());
	~RutileServiceProgress();
};

RutileServiceProgress::RutileServiceProgress(std::filesystem::path path, std::string id, nlohmann::json event_metadata) : directory(std::move(path)), request_id(std::move(id)), metadata(std::move(event_metadata)) {
}

void rutile_service_progress(void* context, const char* phase, std::size_t completed, std::size_t total, const char* detail) {
	auto& progress = *static_cast<RutileServiceProgress*>(context);
	auto now = std::chrono::steady_clock::now();
	auto previous_phase = progress.phase;
	progress.phase = phase;
	progress.last = now;
	if (previous_phase == progress.phase && progress.state == "running" && now - progress.last_snapshot < std::chrono::milliseconds(100)) {
		return;
	}
	try {
		auto event = nlohmann::json{ { "request_id", progress.request_id }, { "phase", phase }, { "completed", completed }, { "total", total }, { "detail", detail ? detail : "" }, { "sequence", ++progress.sequence }, { "state", progress.state }, { "pid", platform_process_id() } };
		event.update(progress.metadata);
		progress.last_snapshot = now;
		std::lock_guard guard(rutile_service_progress_mutex);
		rutile_service_latest_progress = event;
		rutile_service_progress_snapshots[progress.request_id] = std::move(event);
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

struct RutileServiceArtifactFingerprint {
	bool exists = false;
	std::uintmax_t size = 0;
	std::uint64_t hash = 0;
	bool operator==(const RutileServiceArtifactFingerprint&) const = default;
};

RutileServiceArtifactFingerprint rutile_service_artifact_fingerprint(const std::filesystem::path& path) {
	std::ifstream file(path, std::ios::binary);
	if (!file) {
		return {};
	}
	RutileServiceArtifactFingerprint fingerprint;
	fingerprint.exists = true;
	fingerprint.hash = UINT64_C(14695981039346656037);
	char buffer[64 * 1024];
	while (file) {
		file.read(buffer, sizeof(buffer));
		const auto count = file.gcount();
		fingerprint.size += static_cast<std::uintmax_t>(count);
		for (std::streamsize index = 0; index < count; ++index) {
			fingerprint.hash = (fingerprint.hash ^ static_cast<unsigned char>(buffer[index])) * UINT64_C(1099511628211);
		}
	}
	if (file.bad()) {
		return {};
	}
	return fingerprint;
}

bool rutile_service_artifacts_exist(const nlohmann::json& request) {
	const auto output = std::filesystem::u8path(request.at("output").get<std::string>());
	auto bundle = output;
	bundle += ".sources.rt";
	auto map = output;
	map += ".sources.json";
	for (const auto& path : { output, bundle, map }) {
		std::error_code error;
		if (!std::filesystem::is_regular_file(path, error)) {
			return false;
		}
	}
	return true;
}

nlohmann::json rutile_service_compile(Compiler* compiler, const nlohmann::json& request, const std::filesystem::path& directory, std::unordered_map<std::string, std::string>& source_snapshot) {
	nlohmann::json metadata{ { "session", request.value("session", std::string{}) }, { "source_revision", request.value("source_revision", std::size_t{}) }, { "artifact_revision", request.value("artifact_revision", std::size_t{}) }, { "artifacts_current", request.value("artifacts_current", false) } };
	if (request.contains("target")) metadata["target"] = request.at("target");
	if (request.contains("profile")) metadata["profile"] = request.at("profile");
	RutileServiceProgress progress{ directory, request.at("request_id").get<std::string>(), std::move(metadata) };
	auto names = request.at("sources").get<std::vector<std::string>>();
	if (names.empty()) {
		throw std::runtime_error("No compiler sources were supplied.");
	}
	if (request.contains("changed_sources")) {
		const auto changed = request.at("changed_sources").get<std::vector<std::string>>();
		for (std::size_t index = 0; index < changed.size(); ++index) {
			rutile_service_progress(&progress, "changed-source", index + 1, changed.size(), changed[index].c_str());
		}
	}
	auto incremental = request.value("incremental", false);
	std::set<std::string> changed_sources;
	if (request.contains("changed_sources")) {
		for (const auto& name : request.at("changed_sources")) {
			changed_sources.insert(name.get<std::string>());
		}
	}
	std::set<std::string> current_sources(names.begin(), names.end());
	for (auto source = source_snapshot.begin(); source != source_snapshot.end();) {
		if (!current_sources.contains(source->first)) {
			source = source_snapshot.erase(source);
		}
		else {
			++source;
		}
	}
	auto output = std::filesystem::absolute(std::filesystem::u8path(request.at("output").get<std::string>())).lexically_normal();
	auto staged_filename = output.filename();
	auto output_extension = staged_filename.extension();
	staged_filename.replace_extension();
	std::string request_suffix;
	for (const auto character : request.at("request_id").get<std::string>()) {
		const bool safe = (character >= 'a' && character <= 'z') || (character >= 'A' && character <= 'Z') || (character >= '0' && character <= '9') || character == '-';
		request_suffix += safe ? character : '_';
		if (request_suffix.size() == 48) {
			break;
		}
	}
	staged_filename += ".pending-" + std::to_string(platform_process_id()) + "-" + request_suffix;
	staged_filename += output_extension;
	auto staged_output = output.parent_path() / staged_filename;
	auto bundle_path = output;
	bundle_path += ".sources.rt";
	auto map_path = output;
	map_path += ".sources.json";
	for (const auto& name : names) {
		auto source = std::filesystem::absolute(std::filesystem::u8path(name)).lexically_normal();
		for (const auto& destination : { output, staged_output, bundle_path, map_path }) {
			std::error_code error;
			if (source == destination || std::filesystem::equivalent(source, destination, error)) {
				throw std::runtime_error("Compiler output would overwrite a source file.");
			}
		}
	}
	std::filesystem::create_directories(output.parent_path());
	std::error_code staging_error;
	std::filesystem::remove(staged_output, staging_error);
	if (staging_error) {
		throw std::runtime_error("Cannot prepare the staged compiler output: " + staging_error.message());
	}
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
		std::string text;
		if (overlays.contains(name)) {
			text = overlays.at(name).get<std::string>();
		}
		else if (!incremental || changed_sources.contains(name) || !source_snapshot.contains(name)) {
			text = rutile_service_read(std::filesystem::u8path(name));
		}
		else {
			text = source_snapshot.at(name);
		}
		if (!overlays.contains(name)) {
			source_snapshot.insert_or_assign(name, text);
		}
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
	auto staged_output_text = rutile_service_path(staged_output);
	rutile_compiler_set_progress(compiler, rutile_service_progress, &progress);
	int result = rutile_compile_sources(compiler, paths.data(), texts.data(), lengths.data(), paths.size(), staged_output_text.c_str(), rutile_service_open, entry.c_str(), request.at("optimization").get<int>(), diagnostics.get(), kind == "object");
	rutile_compiler_set_progress(compiler, nullptr, nullptr);
	if (!result) {
		rutile_service_progress(&progress, "publishing", 0, 1, output_text.c_str());
		if (!platform_replace_file(staged_output, output)) {
			result = 1;
			std::fputs("Cannot publish the completed compiler artifact.\n", diagnostics.get());
		}
		else {
			rutile_service_progress(&progress, "publishing", 1, 1, output_text.c_str());
		}
	}
	if (result) {
		std::error_code ignored;
		std::filesystem::remove(staged_output, ignored);
	}
	auto diagnostic_text = rutile_service_diagnostics(diagnostics.get());
	auto source_revision = request.value("source_revision", std::size_t{});
	auto artifact_revision = result ? request.value("artifact_revision", std::size_t{}) : source_revision;
	auto fingerprint = !result ? rutile_service_artifact_fingerprint(output) : RutileServiceArtifactFingerprint{};
	progress.metadata["artifact_revision"] = artifact_revision;
	progress.metadata["artifacts_current"] = !result && artifact_revision == source_revision && fingerprint.exists;
	progress.state = result ? "failed" : "succeeded";
	rutile_service_progress(&progress, result ? "failed" : "compiled", 1, 1, result ? diagnostic_text.c_str() : "Compilation completed");
	return { { "succeeded", !result }, { "diagnostics", diagnostic_text }, { "output", output_text }, { "bundle", rutile_service_path(bundle_path) }, { "source_map", rutile_service_path(map_path) }, { "source_revision", source_revision }, { "artifact_revision", artifact_revision }, { "artifacts_current", !result && artifact_revision == source_revision && fingerprint.exists }, { "artifact_fingerprint", { { "exists", fingerprint.exists }, { "size", fingerprint.size }, { "hash", fingerprint.hash } } }, { "pid", platform_process_id() } };
}

struct RutileServiceFileStamp {
	bool exists = false;
	std::filesystem::file_time_type modified{};
	std::uintmax_t size = 0;
	bool operator==(const RutileServiceFileStamp&) const = default;
};

struct RutileServicePendingChange {
	RutileServiceFileStamp stamp;
	std::chrono::steady_clock::time_point since;
};

struct RutileServiceCompileJob {
	std::string session;
	nlohmann::json request;
	std::promise<nlohmann::json> completion;
	std::shared_future<nlohmann::json> result;
	RutileServiceCompileJob(std::string session_id, nlohmann::json input)
		: session(std::move(session_id)), request(std::move(input)), result(completion.get_future().share()) {}
};

struct RutileServiceWatch {
	nlohmann::json request;
	std::unordered_map<std::string, RutileServiceFileStamp> sources;
	std::unordered_map<std::string, RutileServicePendingChange> pending;
	std::vector<std::string> deferred_changes;
	std::shared_ptr<RutileServiceCompileJob> active_job;
	nlohmann::json last_result;
	RutileServiceArtifactFingerprint artifact_fingerprint;
	bool build_requested = false;
	bool initial_pending = false;
	bool compile_failed = false;
	std::size_t revision = 0;
	std::size_t source_revision = 0;
	std::size_t artifact_revision = 0;
};

RutileServiceFileStamp rutile_service_stamp(const std::string& name) {
	auto path = std::filesystem::u8path(name);
	std::error_code error;
	RutileServiceFileStamp stamp;
	stamp.exists = std::filesystem::is_regular_file(path, error) && !error;
	if (stamp.exists) {
		stamp.modified = std::filesystem::last_write_time(path, error);
		if (error) {
			stamp.exists = false;
			return stamp;
		}
		stamp.size = std::filesystem::file_size(path, error);
		if (error) {
			stamp.exists = false;
		}
	}
	return stamp;
}

std::string rutile_service_session(const nlohmann::json& request) {
	if (request.contains("session")) {
		const auto session = request.at("session").get<std::string>();
		if (!session.empty()) {
			return session;
		}
	}
	return rutile_service_path(std::filesystem::absolute(std::filesystem::u8path(request.at("output").get<std::string>())).lexically_normal());
}

bool rutile_service_configure_watch(const nlohmann::json& request, RutileServiceWatch& watch, bool activate) {
	auto configuration = request;
	for (const auto* field : { "request_id", "incremental", "changed_sources", "overlays_path", "source_revision", "artifact_revision", "artifacts_current", "build" }) {
		configuration.erase(field);
	}
	auto previous_configuration = watch.request;
	if (previous_configuration.is_object()) {
		for (const auto* field : { "request_id", "incremental", "changed_sources", "overlays_path", "source_revision", "artifact_revision", "artifacts_current", "build" }) {
			previous_configuration.erase(field);
		}
	}
	const bool changed_configuration = !previous_configuration.is_object() || previous_configuration != configuration;
	const bool newly_activated = activate && !watch.build_requested;
	watch.build_requested = watch.build_requested || activate;
	watch.request = request;
	watch.request.erase("overlays_path");
	if (changed_configuration || !watch.source_revision) {
		++watch.source_revision;
		watch.sources.clear();
		watch.pending.clear();
		watch.deferred_changes.clear();
		watch.compile_failed = false;
		for (const auto& source : request.at("sources").get<std::vector<std::string>>()) {
			watch.sources.insert_or_assign(source, rutile_service_stamp(source));
		}
	}
	const bool missing_artifacts = watch.artifact_revision && !watch.active_job && !rutile_service_artifacts_exist(request);
	if (watch.build_requested && (changed_configuration || newly_activated || missing_artifacts)) {
		watch.initial_pending = true;
	}
	watch.request["source_revision"] = watch.source_revision;
	watch.request["artifact_revision"] = watch.artifact_revision;
	watch.request["artifacts_current"] = watch.artifact_revision == watch.source_revision && !watch.compile_failed && !missing_artifacts;
	return changed_configuration;
}

std::vector<std::string> rutile_service_changed_sources_now(RutileServiceWatch& watch) {
	std::vector<std::string> changed;
	for (const auto& source : watch.request.at("sources").get<std::vector<std::string>>()) {
		auto current = rutile_service_stamp(source);
		auto previous = watch.sources.find(source);
		if (previous == watch.sources.end() || !(previous->second == current)) {
			watch.sources.insert_or_assign(source, current);
			watch.pending.erase(source);
			changed.push_back(source);
		}
	}
	return changed;
}

std::vector<std::string> rutile_service_changed_sources(RutileServiceWatch& watch, std::chrono::steady_clock::time_point now) {
	constexpr auto debounce = std::chrono::milliseconds(200);
	std::vector<std::string> changed;
	const auto sources = watch.request.at("sources").get<std::vector<std::string>>();
	for (const auto& source : sources) {
		auto current = rutile_service_stamp(source);
		auto previous = watch.sources.find(source);
		auto candidate = watch.pending.find(source);
		if (previous != watch.sources.end() && previous->second == current) {
			watch.pending.erase(source);
			continue;
		}
		if (candidate == watch.pending.end() || !(candidate->second.stamp == current)) {
			watch.pending.insert_or_assign(source, RutileServicePendingChange{ current, now });
			continue;
		}
		if (now - candidate->second.since >= debounce) {
			changed.push_back(source);
			watch.sources.insert_or_assign(source, current);
			watch.pending.erase(candidate);
		}
	}
	return changed;
}

void rutile_service_watch_failure(const std::filesystem::path& directory, const nlohmann::json& request, const std::string& message) {
	nlohmann::json metadata{ { "session", request.value("session", std::string{}) }, { "source_revision", request.value("source_revision", std::size_t{}) }, { "artifact_revision", request.value("artifact_revision", std::size_t{}) }, { "artifacts_current", false } };
	if (request.contains("target")) metadata["target"] = request.at("target");
	if (request.contains("profile")) metadata["profile"] = request.at("profile");
	RutileServiceProgress progress{ directory, request.value("request_id", std::string{}), std::move(metadata) };
	progress.state = "failed";
	rutile_service_progress(&progress, "failed", 0, 0, message.c_str());
}

int rutile_service_serve(const char* endpoint) {
	auto root = std::filesystem::current_path();
	char expected[256];
	if (!platform_endpoint(rutile_service_path(root).c_str(), expected, sizeof(expected)) || std::string_view(endpoint) != std::string(expected) + "-rutilec") {
		std::fputs("Compiler connection does not belong to this project.\n", stderr);
		return 1;
	}
	std::string runtime_key;
	for (const unsigned char character : std::string(endpoint)) {
		constexpr char digits[] = "0123456789abcdef";
		runtime_key += digits[character >> 4];
		runtime_key += digits[character & 15];
	}
	auto directory = std::filesystem::temp_directory_path() / "rutilec" / runtime_key;
	std::filesystem::create_directories(directory);
	std::unique_ptr<PlatformLock, decltype(&platform_lock_release)> lock{ platform_lock_acquire(rutile_service_path(directory / "process.lock").c_str()), platform_lock_release };
	if (!lock) {
		std::fputs("The project compiler is already running.\n", stderr);
		return 1;
	}
	std::unique_ptr<Compiler, decltype(&rutile_compiler_destroy)> compiler{ rutile_compiler_create(), rutile_compiler_destroy };
	std::unique_ptr<PlatformServer, decltype(&platform_server_close)> server{ platform_server_open(endpoint), platform_server_close };
	const auto language_endpoint = std::string(endpoint) + "-lsp";
	std::unique_ptr<PlatformStreamServer, decltype(&platform_stream_server_close)> language_server{ platform_stream_server_open(language_endpoint.c_str()), platform_stream_server_close };
	if (!compiler || !server || !language_server) {
		std::fputs("Cannot initialize the compiler service.\n", stderr);
		return 1;
	}
	char executable_buffer[32768];
	if (!platform_executable_path(executable_buffer, sizeof(executable_buffer))) {
		return 1;
	}
	auto executable = std::filesystem::absolute(std::filesystem::u8path(executable_buffer)).lexically_normal();
	std::error_code executable_error;
	auto executable_time = std::filesystem::last_write_time(executable, executable_error);
	if (executable_error) {
		return 1;
	}
	auto binary_stamp = std::to_string(executable_time.time_since_epoch().count());
	std::atomic<std::size_t> compilations = 0;
	std::mutex queue_mutex;
	std::condition_variable queue_ready;
	std::deque<std::shared_ptr<RutileServiceCompileJob>> compile_queue;
	std::unordered_map<std::string, std::shared_ptr<RutileServiceCompileJob>> request_jobs;
	bool stop_compile_worker = false;
	std::thread compile_worker([&]() {
		std::unordered_map<std::string, std::unordered_map<std::string, std::string>> source_snapshots;
		while (true) {
			std::shared_ptr<RutileServiceCompileJob> job;
			{
				std::unique_lock guard(queue_mutex);
				queue_ready.wait(guard, [&]() { return stop_compile_worker || !compile_queue.empty(); });
				if (compile_queue.empty() && stop_compile_worker) {
					return;
				}
				job = std::move(compile_queue.front());
				compile_queue.pop_front();
			}
			try {
				auto result = rutile_service_compile(compiler.get(), job->request, directory, source_snapshots[job->session]);
				result["compilations"] = ++compilations;
				result["request_id"] = job->request.value("request_id", std::string{});
				job->completion.set_value(std::move(result));
			}
			catch (const std::exception& error) {
				rutile_compiler_set_progress(compiler.get(), nullptr, nullptr);
				if (job->request.contains("request_id")) {
					rutile_service_watch_failure(directory, job->request, error.what());
				}
				job->completion.set_value(nlohmann::json{ { "succeeded", false }, { "diagnostics", std::string(error.what()) + "\n" }, { "compilations", compilations.load() }, { "request_id", job->request.value("request_id", std::string{}) }, { "source_revision", job->request.value("source_revision", std::size_t{}) }, { "artifact_revision", job->request.value("artifact_revision", std::size_t{}) }, { "artifacts_current", false }, { "pid", platform_process_id() } });
			}
		}
	});
	auto enqueue_compile = [&](const std::string& session, nlohmann::json request) {
		auto job = std::make_shared<RutileServiceCompileJob>(session, std::move(request));
		{
			std::lock_guard guard(queue_mutex);
			compile_queue.push_back(job);
		}
		queue_ready.notify_one();
		return job;
	};
	auto update_artifact_revision = [](RutileServiceWatch& watch, const nlohmann::json& result) {
		if (result.value("source_revision", std::size_t{}) == watch.source_revision) {
			if (result.value("succeeded", false)) {
				watch.artifact_revision = result.value("artifact_revision", watch.source_revision);
				watch.compile_failed = false;
				watch.last_result = result;
				const auto fingerprint = result.value("artifact_fingerprint", nlohmann::json::object());
				watch.artifact_fingerprint = { fingerprint.value("exists", false), fingerprint.value("size", std::uintmax_t{}), fingerprint.value("hash", std::uint64_t{}) };
			}
			else {
				watch.compile_failed = true;
			}
		}
		watch.request["source_revision"] = watch.source_revision;
		watch.request["artifact_revision"] = watch.artifact_revision;
		watch.request["artifacts_current"] = watch.artifact_revision == watch.source_revision && !watch.compile_failed && rutile_service_artifacts_exist(watch.request);
	};
	std::unordered_map<std::string, RutileServiceWatch> watches;
	std::atomic<bool> stop_language_server = false;
	auto root_text = rutile_service_path(root);
	std::thread language_worker([&]() {
		while (!stop_language_server.load()) {
			if (!rutile_project_sources_ready()) {
				platform_sleep(20);
				continue;
			}
			PlatformStream* connection = nullptr;
			auto accepted = platform_stream_server_accept_for(language_server.get(), 100, &connection);
			if (accepted == platform_accept::timeout) {
				continue;
			}
			if (accepted == platform_accept::error) {
				if (stop_language_server.load()) {
					break;
				}
				platform_sleep(50);
				continue;
			}
			std::unique_ptr<PlatformStream, decltype(&platform_stream_close)> client{ connection, platform_stream_close };
			try {
				kiln_language_server(compiler.get(), platform_stream_input(client.get()), platform_stream_output(client.get()), root_text.c_str());
			}
			catch (...) {
				// A disconnected or malformed editor client must not stop Rutilec.
			}
		}
	});
	auto shutdown_workers = [&]() {
		stop_language_server.store(true);
		platform_stream_server_interrupt(language_server.get());
		if (language_worker.joinable()) {
			language_worker.join();
		}
		{
			std::lock_guard guard(queue_mutex);
			stop_compile_worker = true;
		}
		queue_ready.notify_all();
		if (compile_worker.joinable()) {
			compile_worker.join();
		}
	};
	std::vector<char> command(16 * 1024 * 1024);
	auto poll_watches = [&]() {
		auto now = std::chrono::steady_clock::now();
		for (auto& entry : watches) {
			auto& watch = entry.second;
			if (!watch.build_requested) {
				continue;
			}
			if (watch.active_job && watch.active_job->result.wait_for(std::chrono::milliseconds(0)) == std::future_status::ready) {
				auto result = watch.active_job->result.get();
				update_artifact_revision(watch, result);
				watch.active_job.reset();
			}
			if (watch.initial_pending && watch.active_job) {
				continue;
			}
			if (watch.initial_pending) {
				for (const auto& source : watch.request.at("sources").get<std::vector<std::string>>()) {
					watch.sources.insert_or_assign(source, rutile_service_stamp(source));
				}
				watch.pending.clear();
				auto request = watch.request;
				request["incremental"] = false;
				request.erase("changed_sources");
				watch.initial_pending = false;
				watch.active_job = enqueue_compile(entry.first, std::move(request));
				continue;
			}
			auto changed = rutile_service_changed_sources(watch, now);
			for (auto& source : changed) {
				if (std::find(watch.deferred_changes.begin(), watch.deferred_changes.end(), source) == watch.deferred_changes.end()) {
					watch.deferred_changes.push_back(std::move(source));
				}
			}
			if (watch.active_job || watch.deferred_changes.empty()) {
				continue;
			}
			auto request = watch.request;
			request["incremental"] = true;
			request["changed_sources"] = watch.deferred_changes;
			watch.deferred_changes.clear();
			request["source_revision"] = ++watch.source_revision;
			watch.compile_failed = false;
			watch.request["source_revision"] = watch.source_revision;
			watch.request["artifact_revision"] = watch.artifact_revision;
			watch.request["artifacts_current"] = false;
			request["artifact_revision"] = watch.artifact_revision;
			request["artifacts_current"] = false;
			request["request_id"] = "watch-" + std::to_string(platform_process_id()) + "-" + std::to_string(++watch.revision);
			watch.active_job = enqueue_compile(entry.first, std::move(request));
		}
	};
	while (true) {
		poll_watches();
		auto accepted = platform_server_accept_for(server.get(), 100);
		if (accepted == platform_accept::error) {
			shutdown_workers();
			return 1;
		}
		if (accepted == platform_accept::timeout) {
			continue;
		}
		bool stop = false;
		if (platform_server_read(server.get(), command.data(), command.size())) {
			std::string_view text(command.data());
			std::string response;
			if (text == "hello") {
				auto sessions = nlohmann::json::array();
				for (const auto& [session, watch] : watches) {
						sessions.push_back({ { "session", session }, { "target", watch.request.value("target", std::string{}) }, { "profile", watch.request.value("profile", std::string{}) }, { "source_revision", watch.source_revision }, { "artifact_revision", watch.artifact_revision }, { "artifacts_current", watch.source_revision == watch.artifact_revision && !watch.compile_failed && rutile_service_artifacts_exist(watch.request) }, { "compiling", static_cast<bool>(watch.active_job) || watch.initial_pending } });
				}
				response = nlohmann::json{ { "service", "rutile" }, { "protocol", 2 }, { "pid", platform_process_id() }, { "version", software_version }, { "compilations", compilations.load() }, { "executable", rutile_service_path(executable) }, { "binary_stamp", binary_stamp }, { "language_endpoint", language_endpoint }, { "sessions", std::move(sessions) } }.dump();
			}
			else if (text == "ping") {
				response = "ready";
			}
			else if (text.starts_with("sources ")) {
				try {
					auto sources = nlohmann::json::parse(text.substr(8));
					if (!sources.is_object() || !sources.contains("sources") || !sources.at("sources").is_array()) {
						throw std::runtime_error("Invalid project source configuration.");
					}
					rutile_project_sources_update(sources);
					response = nlohmann::json{ { "accepted", true }, { "pid", platform_process_id() } }.dump();
				}
				catch (const std::exception& error) {
					response = nlohmann::json{ { "accepted", false }, { "diagnostics", error.what() } }.dump();
				}
			}
			else if (text == "progress" || text.starts_with("progress ")) {
				std::lock_guard guard(rutile_service_progress_mutex);
				auto request_id = text.size() > 9 ? std::string(text.substr(9)) : std::string{};
				auto found = rutile_service_progress_snapshots.find(request_id);
				response = (request_id.empty() ? rutile_service_latest_progress : found != rutile_service_progress_snapshots.end() ? *found : nlohmann::json::object()).dump(-1, ' ', false, nlohmann::json::error_handler_t::replace);
			}
			else if (text == "exit") {
				response = "Compiler stopped.";
				stop = true;
			}
			else if (text.starts_with("configure ")) {
				nlohmann::json request;
				try {
					request = nlohmann::json::parse(text.substr(10));
					if (!request.contains("sources") || request.at("sources").get<std::vector<std::string>>().empty()) {
						throw std::runtime_error("No Rutile sources were supplied in the target manifest.");
					}
					if (request.value("kind", std::string{}) != "object" && request.value("kind", std::string{}) != "executable") {
						throw std::runtime_error("Unknown compiler output kind in the target manifest.");
					}
					if (!request.contains("optimization") || request.at("optimization").get<int>() < 0 || request.at("optimization").get<int>() > 3) {
						throw std::runtime_error("Invalid optimization level in the target manifest.");
					}
					if (!request.contains("request_id")) {
						request["request_id"] = "configure-" + std::to_string(platform_process_id()) + "-" + std::to_string(compilations.load() + watches.size() + 1);
					}
					auto session = rutile_service_session(request);
					rutile_service_configure_watch(request, watches[session], request.value("build", false));
					response = nlohmann::json{ { "accepted", true }, { "session", session }, { "request_id", request.at("request_id") }, { "source_revision", watches[session].source_revision }, { "pid", platform_process_id() } }.dump();
				}
				catch (const std::exception& error) {
					response = nlohmann::json{ { "accepted", false }, { "diagnostics", std::string(error.what()) + "\n" }, { "pid", platform_process_id() } }.dump();
				}
			}
			else if (text.starts_with("compile ")) {
				nlohmann::json request;
				try {
					request = nlohmann::json::parse(text.substr(8));
					if (!request.contains("request_id")) {
						request["request_id"] = "build-" + std::to_string(platform_process_id()) + "-" + std::to_string(compilations.load() + 1);
					}
					auto request_id = request.at("request_id").get<std::string>();
					auto session = rutile_service_session(request);
					auto& watch = watches[session];
					const bool changed_configuration = rutile_service_configure_watch(request, watch, true);
					auto changed_sources = changed_configuration ? std::vector<std::string>{} : rutile_service_changed_sources_now(watch);
					if (!changed_configuration && !changed_sources.empty()) {
						++watch.source_revision;
						watch.compile_failed = false;
						watch.request["source_revision"] = watch.source_revision;
						watch.request["artifact_revision"] = watch.artifact_revision;
						watch.request["artifacts_current"] = false;
					}
					request["source_revision"] = watch.source_revision;
					request["artifact_revision"] = watch.artifact_revision;
					request["artifacts_current"] = watch.artifact_revision == watch.source_revision && !watch.compile_failed && rutile_service_artifacts_exist(request);
					if (changed_configuration) {
						request["incremental"] = false;
						request.erase("changed_sources");
					}
					else if (!changed_sources.empty()) {
						request["incremental"] = true;
						request["changed_sources"] = changed_sources;
					}
					else {
						request["incremental"] = true;
						request.erase("changed_sources");
					}
					std::shared_ptr<RutileServiceCompileJob> job;
					if (!changed_configuration && changed_sources.empty() && watch.active_job
						&& watch.active_job->request.value("source_revision", std::size_t{}) == watch.source_revision) {
						job = watch.active_job;
					}
					else if (!changed_configuration && changed_sources.empty() && watch.artifact_revision == watch.source_revision
						&& !watch.compile_failed && watch.last_result.is_object()) {
						const auto artifact_path = std::filesystem::u8path(request.at("output").get<std::string>());
						if (watch.artifact_fingerprint.exists && rutile_service_artifacts_exist(request) && rutile_service_artifact_fingerprint(artifact_path) == watch.artifact_fingerprint) {
							job = std::make_shared<RutileServiceCompileJob>(session, request);
							auto cached = watch.last_result;
							cached["request_id"] = request_id;
							cached["compilations"] = compilations.load();
							cached["cached"] = true;
							job->completion.set_value(std::move(cached));
						}
					}
					if (!job) {
						job = enqueue_compile(session, request);
					}
					watch.active_job = job;
					watch.initial_pending = false;
					request_jobs.insert_or_assign(request_id, job);
					response = nlohmann::json{ { "accepted", true }, { "request_id", request_id }, { "worker_request_id", job->request.value("request_id", request_id) }, { "session", session }, { "source_revision", watch.source_revision }, { "pid", platform_process_id() } }.dump();
				}
				catch (const std::exception& error) {
					rutile_service_watch_failure(directory, request, error.what());
					response = nlohmann::json{ { "accepted", false }, { "request_id", request.value("request_id", std::string{}) }, { "diagnostics", std::string(error.what()) + "\n" }, { "pid", platform_process_id() } }.dump();
				}
			}
			else if (text.starts_with("result ")) {
				const auto request_id = std::string(text.substr(7));
				auto found = request_jobs.find(request_id);
				if (found == request_jobs.end()) {
					response = nlohmann::json{ { "ready", true }, { "succeeded", false }, { "request_id", request_id }, { "diagnostics", "Unknown or expired compilation request.\n" } }.dump();
				}
				else {
					auto job = found->second;
					auto watch = watches.find(job->session);
					if (watch != watches.end() && watch->second.active_job && watch->second.active_job != job) {
						job = watch->second.active_job;
						found->second = job;
					}
					if (job->result.wait_for(std::chrono::milliseconds(0)) != std::future_status::ready) {
						response = nlohmann::json{ { "ready", false }, { "request_id", request_id }, { "session", job->session }, { "source_revision", job->request.value("source_revision", std::size_t{}) } }.dump();
					}
					else {
						auto result = job->result.get();
						if (watch != watches.end()) {
							auto& session = watch->second;
							bool artifact_changed = false;
							if (result.value("succeeded", false)) {
								const auto expected = result.value("artifact_fingerprint", nlohmann::json::object());
								const auto artifact_path = std::filesystem::u8path(session.request.at("output").get<std::string>());
								artifact_changed = !expected.value("exists", false) || !rutile_service_artifacts_exist(session.request)
									|| rutile_service_artifact_fingerprint(artifact_path) != RutileServiceArtifactFingerprint{ expected.value("exists", false), expected.value("size", std::uintmax_t{}), expected.value("hash", std::uint64_t{}) };
								if (artifact_changed) {
									session.compile_failed = true;
								}
							}
							auto changed = rutile_service_changed_sources_now(session);
							bool newly_changed = !changed.empty();
							for (const auto& source : session.deferred_changes) {
								if (std::find(changed.begin(), changed.end(), source) == changed.end()) {
									changed.push_back(source);
								}
							}
							if (newly_changed || (!changed.empty() && result.value("source_revision", std::size_t{}) == session.source_revision)) {
								++session.source_revision;
								session.compile_failed = false;
							}
							if (!changed.empty() || artifact_changed || result.value("source_revision", std::size_t{}) != session.source_revision) {
								auto request = session.request;
								auto previous_configuration = job->request;
								for (const auto* field : { "request_id", "incremental", "changed_sources", "overlays_path", "source_revision", "artifact_revision", "artifacts_current", "build" }) {
									previous_configuration.erase(field);
								}
								auto current_configuration = session.request;
								for (const auto* field : { "request_id", "incremental", "changed_sources", "overlays_path", "source_revision", "artifact_revision", "artifacts_current", "build" }) {
									current_configuration.erase(field);
								}
								const bool same_configuration = previous_configuration == current_configuration;
								request["source_revision"] = session.source_revision;
								request["artifact_revision"] = session.artifact_revision;
								request["artifacts_current"] = false;
								request["incremental"] = same_configuration;
								if (same_configuration) {
									request["changed_sources"] = changed;
								}
								else {
									request.erase("changed_sources");
								}
								request["request_id"] = "watch-" + std::to_string(platform_process_id()) + "-" + std::to_string(++session.revision);
								session.request["source_revision"] = session.source_revision;
								session.request["artifact_revision"] = session.artifact_revision;
								session.request["artifacts_current"] = false;
								session.initial_pending = false;
								session.deferred_changes.clear();
								auto latest = enqueue_compile(job->session, std::move(request));
								session.active_job = latest;
								found->second = latest;
								response = nlohmann::json{ { "ready", false }, { "request_id", request_id }, { "session", job->session }, { "source_revision", session.source_revision } }.dump();
							}
							else {
								update_artifact_revision(session, result);
								if (session.active_job == job) {
									session.active_job.reset();
								}
								result["ready"] = true;
								response = result.dump(-1, ' ', false, nlohmann::json::error_handler_t::replace);
								request_jobs.erase(found);
							}
						}
						else {
							result["ready"] = true;
							response = result.dump(-1, ' ', false, nlohmann::json::error_handler_t::replace);
							request_jobs.erase(found);
						}
					}
				}
			}
			else {
				response = "Unknown compiler operation.";
			}
			platform_server_write(server.get(), response.c_str());
		}
		platform_server_disconnect(server.get());
		if (stop) {
			shutdown_workers();
			return 0;
		}
	}
}

int rutile_service_control(const char* operation) {
	auto root = std::filesystem::current_path();
	while (true) {
		char endpoint[256];
		if (platform_endpoint(rutile_service_path(root).c_str(), endpoint, sizeof(endpoint))) {
			auto service_endpoint = std::string(endpoint) + "-rutilec";
			std::vector<char> response(1024 * 1024);
			if (platform_request(service_endpoint.c_str(), std::string_view(operation) == "status" ? "hello" : operation, response.data(), response.size(), 1000)) {
				std::cout << response.data() << '\n';
				return 0;
			}
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
