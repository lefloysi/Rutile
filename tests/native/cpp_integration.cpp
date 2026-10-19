#include "../../kiln/integrations/cpp.hpp"
#include "platform.hpp"

#include <catch2/catch_message.hpp>
#include <catch2/catch_test_macros.hpp>

#include <chrono>
#include <filesystem>
#include <fstream>

TEST_CASE("C++ integration validates options and selects artifact types", "[kiln][cpp]") {
	auto integration = kiln_cpp_integration();
	ProjectTarget target;
	target.name = "native";
	ProjectProfile profile{ "debug", 0 };
	std::string diagnostics;
	REQUIRE(integration->name() == "cpp");
	REQUIRE(integration->accepts_source("source.cpp"));
	REQUIRE(integration->accepts_source("source.c"));
	REQUIRE_FALSE(integration->accepts_source("source.hpp"));
	REQUIRE(integration->artifact(target).filename == "native.exe");
	target.type = target_type::static_lib;
	REQUIRE(integration->artifact(target).filename == "native.lib");
	target.type = target_type::shared_lib;
	REQUIRE(integration->artifact(target).filename == "native.dll");
	target.options["includes"] = "[42]";
	REQUIRE_FALSE(integration->validate(target, profile, diagnostics));
	target.options.clear();
	target.options["runtime"] = "wrong";
	REQUIRE_FALSE(integration->validate(target, profile, diagnostics));
	target.options.clear();
	target.options["invented"] = "value";
	REQUIRE_FALSE(integration->validate(target, profile, diagnostics));
}

TEST_CASE("MSVC builds C++ executables static libraries DLLs and tracks configuration", "[kiln][cpp][native]") {
#if defined(_WIN32)
	auto root = std::filesystem::absolute(std::filesystem::u8path(RUTILE_SOURCE_ROOT)) / "build/cpp-tests" / std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
	std::filesystem::create_directories(root / "include");
	std::ofstream(root / "include/value.hpp") << "#pragma once\n#define HEADER_VALUE 40\n";
	std::ofstream(root / "helper.cpp") << "#include <value.hpp>\nint helper() { return HEADER_VALUE + EXTRA_VALUE; }\n";
	std::ofstream(root / "main.cpp") << "#include <vector>\nint helper(); int main() { std::vector<int> value{helper()}; return value.front(); }\n";
	std::ofstream(root / "ignored.cpp") << "#error This file must be excluded\n";
	auto integration = kiln_cpp_integration();
	ProjectProfile profile{ "debug", 0 };
	ProjectTarget library;
	library.name = "helper";
	library.type = target_type::static_lib;
	library.options = { { "includes", "include" }, { "defines", "EXTRA_VALUE=2" } };
	std::vector<std::filesystem::path> sources{ root / "helper.cpp" };
	auto output = root / "helper.lib";
	auto result = integration->build({ root, library, profile, sources, output, build_input::disk });
	INFO(result.diagnostics);
	REQUIRE(result.succeeded);
	REQUIRE(std::filesystem::is_regular_file(output));
	auto object_time = std::filesystem::last_write_time(root / "objects/0.obj");
	result = integration->build({ root, library, profile, sources, output, build_input::disk });
	INFO(result.diagnostics);
	REQUIRE(result.succeeded);
	REQUIRE(std::filesystem::last_write_time(root / "objects/0.obj") == object_time);
	std::ofstream(root / "include/value.hpp") << "#pragma once\n#define HEADER_VALUE 39\n";
	result = integration->build({ root, library, profile, sources, output, build_input::disk });
	INFO(result.diagnostics);
	REQUIRE(result.succeeded);
	REQUIRE(std::filesystem::last_write_time(root / "objects/0.obj") != object_time);
	std::ofstream(root / "include/value.hpp") << "#pragma once\n#define HEADER_VALUE 40\n";
	result = integration->build({ root, library, profile, sources, output, build_input::disk });
	INFO(result.diagnostics);
	REQUIRE(result.succeeded);
	ProjectTarget app;
	app.name = "app";
	std::filesystem::create_directories(root / "runtime");
	std::ofstream(root / "runtime/asset.txt") << "runtime dependency";
	app.options = { { "library_paths", "." }, { "excludes", "ignored.cpp" }, { "runtime_files", "[\"runtime/asset.txt\"]" } };
	sources = { root / "main.cpp", root / "ignored.cpp" };
	output = root / "app.exe";
	result = integration->build({ root, app, profile, sources, output, build_input::disk, { root / "helper.lib" } });
	INFO(result.diagnostics);
	REQUIRE(result.succeeded);
	REQUIRE(platform_run(output, {}, root / "run.log") == 42);
	REQUIRE(std::filesystem::is_regular_file(root / "app.pdb"));
	REQUIRE(std::filesystem::is_regular_file(root / "asset.txt"));
	REQUIRE(std::filesystem::is_regular_file(root / "compile_commands.json"));
	std::ofstream(root / "export.cpp") << "extern \"C\" __declspec(dllexport) int answer() { return 42; }\n";
	ProjectTarget dll;
	dll.name = "module";
	dll.type = target_type::shared_lib;
	sources = { root / "export.cpp" };
	output = root / "module.dll";
	result = integration->build({ root, dll, profile, sources, output, build_input::disk });
	INFO(result.diagnostics);
	REQUIRE(result.succeeded);
	REQUIRE(std::filesystem::is_regular_file(root / "module.lib"));
	auto loaded = platform_library_open(output);
	REQUIRE(loaded != nullptr);
	auto answer = reinterpret_cast<int (*)()>(platform_library_symbol(loaded, "answer"));
	REQUIRE(answer != nullptr);
	REQUIRE(answer() == 42);
	platform_library_close(loaded);
	std::ofstream(root / "main.cpp") << "this is not valid C++;\n";
	sources = { root / "main.cpp" };
	output = root / "broken.exe";
	result = integration->build({ root, app, profile, sources, output, build_input::disk });
	REQUIRE_FALSE(result.succeeded);
	REQUIRE_FALSE(std::filesystem::exists(output));
#endif
}
