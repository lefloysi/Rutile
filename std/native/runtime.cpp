#include <cstddef>
#include <cstdio>

extern "C" void rutile_runtime_report_error(const char* bytes, std::size_t size) {
	std::fwrite(bytes, sizeof(char), size, stderr);
	if (!size || bytes[size - 1] != '\n') {
		std::fputc('\n', stderr);
	}
}
