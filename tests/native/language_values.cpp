#include "platform.hpp"

#include <catch2/catch_test_macros.hpp>

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <string_view>
#include <vector>

namespace language_tests {
	std::string path_text(const std::filesystem::path& path) {
		auto bytes = path.generic_u8string();
		return std::string(reinterpret_cast<const char*>(bytes.data()), bytes.size());
	}

	std::filesystem::path configured_path(const char* name, const std::filesystem::path& fallback) {
		const char* value = std::getenv(name);
		if (value && *value) {
			return std::filesystem::absolute(std::filesystem::u8path(value));
		}
		return std::filesystem::absolute(fallback);
	}

	std::string read_log(const std::filesystem::path& path) {
		std::ifstream file(path, std::ios::binary);
		return std::string(std::istreambuf_iterator<char>(file), {});
	}

	void check_fixture(std::string_view fixture, std::string_view rejection = {}) {
		auto source = configured_path("RUTILE_SOURCE_ROOT", RUTILE_SOURCE_ROOT);
		auto compiler = configured_path("RUTILE_TEST_COMPILER", KILN_TEST_EXECUTABLE);
		auto directory = configured_path("RUTILE_TEST_ROOT", source / "build/native-tests/language-values") / std::filesystem::u8path(fixture).stem();
		std::filesystem::create_directories(directory);
		auto object = directory / "case.obj";
		auto executable = directory / "case.exe";
		auto diagnostics = directory / "compile.log";
		std::vector<std::string> arguments;
		if (compiler.stem() == "kiln") {
			arguments.push_back("compile");
		}
		for (const auto* name : { "unique", "optional", "buffer", "string_view", "string", "hash", "equal", "unordered_map" }) {
			arguments.push_back(path_text(source / "kiln/compiler/std" / (std::string(name) + ".rt")));
		}
		arguments.push_back(path_text(source / std::filesystem::u8path(fixture)));
		arguments.insert(arguments.end(), { "--output", path_text(object), "--optimization", "2" });
		int compiled = platform_run(compiler, arguments, diagnostics);
		INFO("Fixture: " << fixture);
		INFO(read_log(diagnostics));
		if (!rejection.empty()) {
			REQUIRE(compiled == 1);
			REQUIRE(read_log(diagnostics).find(rejection) != std::string::npos);
			return;
		}
		REQUIRE(compiled == 0);
		auto link_log = directory / "link.log";
		arguments = {
			"/nologo", "/subsystem:console", "/entry:mainCRTStartup", "/machine:x64",
			"/libpath:" + path_text(std::filesystem::u8path(RUTILE_RUNTIME_KERNEL32).parent_path()),
			path_text(object), RUTILE_RUNTIME_LIBCMT, RUTILE_RUNTIME_LIBVCRUNTIME,
			RUTILE_RUNTIME_LIBUCRT, RUTILE_RUNTIME_OLDNAMES, RUTILE_RUNTIME_KERNEL32,
			"/out:" + path_text(executable),
		};
		int linked = platform_run(std::filesystem::u8path(RUTILE_NATIVE_LINKER), arguments, link_log);
		INFO(read_log(link_log));
		REQUIRE(linked == 0);
		auto run_log = directory / "run.log";
		int result = platform_run(executable, {}, run_log);
		INFO(read_log(run_log));
		REQUIRE(result == 42);
	}
}

TEST_CASE("String literals retain byte lengths and embedded NULs", "[language][values]") {
	language_tests::check_fixture("tests/compiler-new/std/string-literals.rt");
}

TEST_CASE("String literals do not implicitly become pointers", "[language][values]") {
	language_tests::check_fixture("tests/compiler-new/std/string-literals-pointer.rt", "cannot initialize");
}

TEST_CASE("Primitive values copy through readonly references", "[language][values]") {
	language_tests::check_fixture("tests/compiler-new/backend/cases/primitive-copy.rt");
}

TEST_CASE("Pointer overloads prefer void pointers over bool", "[language][values]") {
	language_tests::check_fixture("tests/compiler-new/backend/cases/pointer-overload-ranking.rt");
}

TEST_CASE("Module declarations do not depend on source discovery order", "[language][values]") {
	language_tests::check_fixture("tests/compiler-new/backend/cases/declaration-order.rt");
}

TEST_CASE("Default hashes preserve primitive and view representations", "[language][hash]") {
	language_tests::check_fixture("tests/compiler-new/std/hash.rt");
}

TEST_CASE("Default map hashes support primitive keys", "[language][hash]") {
	language_tests::check_fixture("tests/compiler-new/std/unordered-map-default-hash.rt");
}

TEST_CASE("Text maps compare view contents", "[language][hash]") {
	language_tests::check_fixture("tests/compiler-new/std/unordered-map.rt");
}

TEST_CASE("Function pointers accept explicit hash policies", "[language][hash]") {
	language_tests::check_fixture("tests/compiler-new/std/unordered-map-function-hash.rt");
}

TEST_CASE("Function pointers require explicit hash policies", "[language][hash]") {
	language_tests::check_fixture("tests/compiler-new/std/unordered-map-function-default-hash.rt", "no applicable function");
}
