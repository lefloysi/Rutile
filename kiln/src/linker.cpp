#include "linker.hpp"

#include "platform.hpp"
#include "project.hpp"

#include <string>
#include <vector>

int kiln_link(const std::filesystem::path& object, const std::filesystem::path& output, const std::filesystem::path& diagnostics) {
	std::vector<std::string> arguments;
#if defined(_WIN32)
	arguments = {
		"/subsystem:console",
		"/entry:mainCRTStartup",
		"/machine:x64",
		"/libpath:" + project_path_utf8(std::filesystem::u8path(RUTILE_RUNTIME_KERNEL32).parent_path()),
		project_path_utf8(object),
		RUTILE_RUNTIME_LIBCMT,
		RUTILE_RUNTIME_LIBVCRUNTIME,
		RUTILE_RUNTIME_LIBUCRT,
		RUTILE_RUNTIME_OLDNAMES,
		RUTILE_RUNTIME_KERNEL32,
		"/out:" + project_path_utf8(output),
	};
#else
	arguments = { project_path_utf8(object), "-o", project_path_utf8(output) };
#endif
	return platform_run(std::filesystem::u8path(RUTILE_NATIVE_LINKER), arguments, diagnostics);
}
