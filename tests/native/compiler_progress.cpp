#include <cstdio>
#include <cstring>
#include <filesystem>
#include <map>
#include <string>
#include <vector>

struct Compiler;
using ProgressCallback = void (*)(void*, const char*, std::size_t, std::size_t, const char*);
extern "C" Compiler* rutile_compiler_create();
extern "C" void rutile_compiler_destroy(Compiler*);
extern "C" void rutile_compiler_set_progress(Compiler*, ProgressCallback, void*);
extern "C" int rutile_compile_sources(Compiler*, const char* const*, const char* const*, const std::size_t*, std::size_t, const char*, std::FILE* (*)(const char*, const char*), const char*, int, std::FILE*, int);

struct Event {
	std::string phase;
	std::size_t completed;
	std::size_t total;
	std::string detail;
};

void record_progress(void* context, const char* phase, std::size_t completed, std::size_t total, const char* detail) {
	auto& events = *static_cast<std::vector<Event>*>(context);
	events.push_back({ phase, completed, total, detail });
}

std::string read_diagnostics(std::FILE* stream) {
	std::fflush(stream);
	std::rewind(stream);
	std::string text;
	char chunk[4096];
	while (const auto count = std::fread(chunk, 1, sizeof(chunk), stream)) {
		text.append(chunk, count);
	}
	return text;
}

int main(int argc, char** argv) {
	if (argc != 2) {
		return 1;
	}
	const auto directory = std::filesystem::path(argv[1]);
	std::filesystem::create_directories(directory);
	auto* compiler = rutile_compiler_create();
	if (!compiler) {
		return 2;
	}
	std::vector<Event> events;
	rutile_compiler_set_progress(compiler, record_progress, &events);
	const char* paths[] = { "original/first.rt", "original/second.rt" };
	const char* texts[] = { "fn answer() -> i32 { return value(); };", "fn value() -> i32 { return 42; };" };
	std::size_t lengths[] = { std::strlen(texts[0]), std::strlen(texts[1]) };
	for (const auto optimization : { 0, 2 }) {
		events.clear();
		const auto object = (directory / ("valid-" + std::to_string(optimization) + ".obj")).string();
		auto* diagnostics = std::tmpfile();
		const auto result = rutile_compile_sources(compiler, paths, texts, lengths, 2, object.c_str(), nullptr, nullptr, optimization, diagnostics, 1);
		const auto errors = read_diagnostics(diagnostics);
		std::fclose(diagnostics);
		if (result || !errors.empty() || !std::filesystem::file_size(object)) {
			std::fputs(errors.c_str(), stderr);
			return 3;
		}
		std::map<std::string, Event> final_events;
		for (const auto& event : events) {
			if (event.completed > event.total || (final_events.contains(event.phase) && event.completed < final_events.at(event.phase).completed)) {
				return 4;
			}
			final_events.insert_or_assign(event.phase, event);
		}
		for (const auto* phase : { "reading", "parsing", "collecting", "declarations", "validating", "resolving", "analyzing", "lifetimes", "declaring", "functions", "dispatch", "initializing", "optimizing", "object", "writing" }) {
			if (!final_events.contains(phase) || final_events.at(phase).completed != final_events.at(phase).total) {
				std::fprintf(stderr, "Incomplete progress phase: %s\n", phase);
				return 5;
			}
		}
		if (final_events.at("reading").total != 2 || final_events.at("reading").detail != paths[1] || final_events.at("parsing").detail != paths[1] || !final_events.at("writing").total) {
			return 6;
		}
		if (final_events.at("functions").total != 2 || final_events.at("functions").completed != 2 || final_events.at("functions").detail.empty()) {
			return 10;
		}
		bool reported_first_function = false;
		bool reported_second_function = false;
		for (const auto& event : events) {
			if (event.phase != "functions" || event.completed == 0) {
				continue;
			}
			reported_first_function = reported_first_function || event.detail.find("original/first.rt :: answer") != std::string::npos;
			reported_second_function = reported_second_function || event.detail.find("original/second.rt :: value") != std::string::npos;
		}
		if (!reported_first_function || !reported_second_function) {
			std::fputs("Function progress did not include each function's source file.\n", stderr);
			return 11;
		}
	}
	// Separate original files retain parser boundaries and exact diagnostic locations.
	const char* broken_texts[] = { "fn incomplete() -> i32 {", "return 42; };" };
	std::size_t broken_lengths[] = { std::strlen(broken_texts[0]), std::strlen(broken_texts[1]) };
	auto* diagnostics = std::tmpfile();
	const auto invalid_object = (directory / "invalid.obj").string();
	events.clear();
	const auto invalid = rutile_compile_sources(compiler, paths, broken_texts, broken_lengths, 2, invalid_object.c_str(), nullptr, nullptr, 0, diagnostics, 1);
	const auto errors = read_diagnostics(diagnostics);
	std::fclose(diagnostics);
	if (!invalid || errors.find("original/first.rt:1:") == std::string::npos || errors.find("original/second.rt:1:") == std::string::npos) {
		std::fputs(errors.c_str(), stderr);
		return 7;
	}
	for (const auto& event : events) {
		if (event.phase == "writing") {
			return 8;
		}
	}
	// A later request on the same compiler has no declarations from the previous build.
	const char* last_paths[] = { "original/third.rt" };
	const char* last_texts[] = { "fn next() -> i32 { return answer(); };" };
	std::size_t last_lengths[] = { std::strlen(last_texts[0]) };
	diagnostics = std::tmpfile();
	const auto stale = rutile_compile_sources(compiler, last_paths, last_texts, last_lengths, 1, invalid_object.c_str(), nullptr, nullptr, 0, diagnostics, 1);
	const auto stale_errors = read_diagnostics(diagnostics);
	std::fclose(diagnostics);
	if (!stale || stale_errors.find("original/third.rt:1:") == std::string::npos || stale_errors.find("answer") == std::string::npos) {
		std::fputs(stale_errors.c_str(), stderr);
		return 9;
	}
	rutile_compiler_set_progress(compiler, nullptr, nullptr);
	rutile_compiler_destroy(compiler);
	std::puts("compiler-progress: PASS");
	return 0;
}
