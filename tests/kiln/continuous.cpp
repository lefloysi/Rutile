#include "platform.hpp"

#include <catch2/catch_message.hpp>
#include <catch2/catch_test_macros.hpp>
#include <nlohmann/json.hpp>

#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

namespace continuous_tests {
	std::filesystem::path setting(const char* name, const char* fallback) {
		const char* value = std::getenv(name);
		return std::filesystem::absolute(std::filesystem::u8path(value && *value ? value : fallback));
	}

	std::string read(const std::filesystem::path& path) {
		std::ifstream file(path, std::ios::binary);
		return std::string(std::istreambuf_iterator<char>(file), {});
	}

	struct Project {
		std::filesystem::path executable;
		std::filesystem::path root;
		std::filesystem::path previous;
		std::size_t invocation = 0;

		Project() : executable(setting("KILN_TEST_EXECUTABLE", KILN_TEST_EXECUTABLE)), root(setting("KILN_TEST_ROOT", KILN_TEST_ROOT) / ("continuous-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()))), previous(std::filesystem::current_path()) {
			std::filesystem::create_directories(root / ".kiln");
			std::filesystem::current_path(root);
		}

		~Project() {
			platform_run(executable, { "exit" }, root / "shutdown.log");
			std::error_code error;
			std::filesystem::current_path(previous, error);
		}

		void write(const std::filesystem::path& path, std::string_view value) {
			std::filesystem::create_directories((root / path).parent_path());
			std::ofstream file(root / path, std::ios::binary);
			file << value;
			file.close();
			REQUIRE(file.good());
		}

		std::pair<int, std::string> run(const std::vector<std::string>& arguments) {
			auto log = root / ("command-" + std::to_string(++invocation) + ".log");
			int code = platform_run(executable, arguments, log);
			return { code, read(log) };
		}

		bool wait_for_exit(const std::filesystem::path& artifact, int expected) {
			auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(30);
			while (std::chrono::steady_clock::now() < deadline) {
				if (platform_run(artifact, {}, root / "program.log") == expected) {
					return true;
				}
				platform_sleep(200);
			}
			return false;
		}
	};

	std::string configuration(bool cycle = false) {
		return "fn main(kiln::Options& options) {\n"
			"var foundation = kiln::add_target(\"foundation.lib\", kiln::TargetType::static_lib);\n"
			"@foundation.compiler(\"cpp\"); @foundation.sources.add(\"foundation\");\n"
			"@foundation.option(\"includes\", \"include\");\n"
			"var helper = kiln::add_target(\"helper.lib\", kiln::TargetType::static_lib);\n"
			"@helper.compiler(\"cpp\"); @helper.sources.add(\"helper\");\n"
			"@helper.depends_on(\"foundation.lib\");\n" + std::string(cycle ? "@helper.depends_on(\"app.exe\");\n" : "") +
			"var app = kiln::add_target(\"app.exe\", kiln::TargetType::executable);\n"
			"@app.compiler(\"cpp\"); @app.sources.add(\"app\");\n"
			"@app.option(\"runtime_files\", \"runtime/asset.txt\");\n"
			"@app.depends_on(\"helper.lib\");\n};\n";
	}
}

TEST_CASE("Kiln keeps C++ dependencies building after saves without build requests", "[kiln][continuous][cpp]") {
#if defined(_WIN32)
	continuous_tests::Project project;
	REQUIRE(std::filesystem::is_regular_file(project.executable));
	project.write("kiln.rt", continuous_tests::configuration());
	project.write("include/value.hpp", "#pragma once\n#define VALUE 40\n");
	project.write("foundation/base.cpp", "#include <value.hpp>\nint base() { return VALUE; }\n");
	project.write("helper/value.cpp", "int base(); int answer() { return base() + 2; }\n");
	project.write("app/main.cpp", "int answer(); int main() { return answer(); }\n");
	project.write("runtime/asset.txt", "initial asset\n");
	// Building starts the project process automatically; no explicit init is required.
	auto [code, output] = project.run({ "build", "app.exe", "--json" });
	INFO(output);
	REQUIRE(code == 0);
	auto built = nlohmann::json::parse(output);
	REQUIRE(built.at("succeeded").get<bool>());
	REQUIRE(built.at("link_artifacts").size() == 3);
	auto artifact = std::filesystem::u8path(built.at("executable").get<std::string>());
	REQUIRE(platform_run(artifact, {}, project.root / "initial.log") == 42);
	REQUIRE(std::filesystem::is_regular_file(project.root / ".kiln/connection"));
	REQUIRE(continuous_tests::read(artifact.parent_path() / "asset.txt") == "initial asset\n");
	project.write("runtime/asset.txt", "changed asset\n");
	auto asset_deadline = std::chrono::steady_clock::now() + std::chrono::seconds(30);
	while (continuous_tests::read(artifact.parent_path() / "asset.txt") != "changed asset\n" && std::chrono::steady_clock::now() < asset_deadline) {
		platform_sleep(200);
	}
	REQUIRE(continuous_tests::read(artifact.parent_path() / "asset.txt") == "changed asset\n");

	project.write("helper/value.cpp", "int base(); int answer() { return base() + 3; }\n");
	REQUIRE(project.wait_for_exit(artifact, 43));
	project.write("include/value.hpp", "#pragma once\n#define VALUE 41\n");
	REQUIRE(project.wait_for_exit(artifact, 44));

	project.write("helper/value.cpp", "this is invalid C++;\n");
	bool failed = false;
	auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(30);
	while (std::chrono::steady_clock::now() < deadline) {
		auto [described, model_text] = project.run({ "describe", "--json" });
		REQUIRE(described == 0);
		auto model = nlohmann::json::parse(model_text);
		for (const auto& build : model.at("builds")) {
			if (build.at("target") == "helper.lib" && !build.at("succeeded").get<bool>()) {
				failed = true;
			}
		}
		if (failed) {
			break;
		}
		platform_sleep(200);
	}
	REQUIRE(failed);
	REQUIRE(platform_run(artifact, {}, project.root / "preserved.log") == 44);
	project.write("helper/value.cpp", "int base(); int answer() { return base() + 4; }\n");
	REQUIRE(project.wait_for_exit(artifact, 45));

	project.write("kiln.rt", continuous_tests::configuration(true));
	auto cycle = project.run({ "build", "app.exe", "--json" });
	INFO(cycle.second);
	REQUIRE(cycle.first == 1);
	REQUIRE(nlohmann::json::parse(cycle.second).at("diagnostics").get<std::string>().find("cycle") != std::string::npos);
	REQUIRE(platform_run(artifact, {}, project.root / "cycle-preserved.log") == 45);
	project.write("kiln.rt", continuous_tests::configuration());
	auto cleaned = project.run({ "clean" });
	INFO(cleaned.second);
	REQUIRE(cleaned.first == 0);
	REQUIRE_FALSE(std::filesystem::exists(artifact));
	platform_sleep(1500);
	REQUIRE_FALSE(std::filesystem::exists(artifact));
#endif
}

TEST_CASE("Kiln links and deploys shared C++ target dependencies", "[kiln][continuous][cpp][shared]") {
#if defined(_WIN32)
	continuous_tests::Project project;
	project.write("kiln.rt", "fn main(kiln::Options& options) {\n"
		"var module = kiln::add_target(\"answer.dll\", kiln::TargetType::shared_lib);\n"
		"@module.compiler(\"cpp\"); @module.sources.add(\"module\");\n"
		"var app = kiln::add_target(\"app.exe\", kiln::TargetType::executable);\n"
		"@app.compiler(\"cpp\"); @app.sources.add(\"app\"); @app.depends_on(\"answer.dll\");\n};\n");
	project.write("module/answer.cpp", "extern \"C\" __declspec(dllexport) int answer() { return 42; }\n");
	project.write("app/main.cpp", "extern \"C\" __declspec(dllimport) int answer(); int main() { return answer(); }\n");
	auto [code, output] = project.run({ "build", "app.exe", "--json" });
	INFO(output);
	REQUIRE(code == 0);
	auto built = nlohmann::json::parse(output);
	REQUIRE(built.at("succeeded").get<bool>());
	auto artifact = std::filesystem::u8path(built.at("executable").get<std::string>());
	REQUIRE(std::filesystem::is_regular_file(artifact.parent_path() / "answer.dll"));
	REQUIRE(platform_run(artifact, {}, project.root / "initial.log") == 42);
	project.write("module/answer.cpp", "extern \"C\" __declspec(dllexport) int answer() { return 43; }\n");
	REQUIRE(project.wait_for_exit(artifact, 43));
#endif
}
