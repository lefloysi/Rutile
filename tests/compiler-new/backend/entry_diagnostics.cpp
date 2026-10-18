#include <cstddef>
#include <cstdio>

struct Compiler;

extern "C" int rutile_compiler_session(int (*callback)(Compiler*, void*), void* data);
extern "C" int rutile_compile(Compiler* compiler, const char* const* paths, std::size_t count, const char* output, std::FILE* (*opener)(const char*, const char*), const char* entry, int optimization, std::FILE* diagnostics);

struct Request {
	const char* source;
	const char* output;
	const char* entry;
};

int compile_entry(Compiler* compiler, void* data) {
	const auto& request = *static_cast<Request*>(data);
	const char* paths[] = { request.source };

	return rutile_compile(compiler, paths, 1, request.output, nullptr, request.entry, 0, stderr);
}

int main(int argc, char** argv) {
	if (argc != 4) {
		std::fputs("Usage: entry_diagnostics source output entry\n", stderr);
		return 2;
	}

	Request request{ argv[1], argv[2], argv[3] };
	return rutile_compiler_session(compile_entry, &request);
}
