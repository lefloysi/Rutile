#include "platform.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_message.hpp>

#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

struct KilnTestResult {
	int code;
	std::string output;
};

struct KilnTestProject {
	std::filesystem::path executable;
	std::filesystem::path root;
	std::filesystem::path previous;
	std::size_t invocation = 0;

	KilnTestProject(const std::filesystem::path& executable_path, const std::filesystem::path& project_root) : executable(executable_path), root(project_root), previous(std::filesystem::current_path()) {
		std::filesystem::create_directories(root / ".kiln");
		std::filesystem::current_path(root);
	}

	~KilnTestProject() {
		platform_run(executable, { "exit" }, root / "shutdown.log");
		std::error_code error;
		std::filesystem::current_path(previous, error);
	}

	void write(const std::filesystem::path& path, const std::string& text) {
		std::filesystem::create_directories((root / path).parent_path());
		std::ofstream file(root / path);
		file << text;
		file.close();
		REQUIRE(file.good());
	}

	KilnTestResult run(const std::vector<std::string>& arguments) {
		auto log = root / ("command-" + std::to_string(++invocation) + ".log");
		int code = platform_run(executable, arguments, log);
		std::ifstream file(log);
		return { code, std::string(std::istreambuf_iterator<char>(file), {}) };
	}
};

std::filesystem::path kiln_test_setting(const char* name, const char* fallback) {
	if (const char* value = std::getenv(name)) {
		return std::filesystem::absolute(std::filesystem::u8path(value));
	}
	return std::filesystem::absolute(std::filesystem::u8path(fallback));
}

std::string kiln_test_configuration(const std::string& name = "app.exe", const std::string& compiler = "rutile", bool duplicate = false) {
	std::string text = "fn main(kiln::Options& options) {\n\tkiln::profile(\"debug\", 0);\n\tkiln::profile(\"release\", 2);\n\tvar target = kiln::add_target(\"" + name + "\", kiln::TargetType::executable);\n\t@target.sources.add(\"app\");\n";
	if (duplicate) {
		text += "\tkiln::add_target(\"" + name + "\", kiln::TargetType::executable);\n";
	}
	text += "\t@target.option(\"entry\", \"start\");\n\t@target.compiler(\"" + compiler + "\");\n";
	text += "\tif (options.profile() == \"release\") {\n\t\t@target.option(\"entry\", \"release_start\");\n\t};\n";
	text += "\tif (options.path().empty() || options.os().empty()) {\n\t\t@target.sources.add(\"missing-options\");\n\t};\n};\n";
	return text;
}

TEST_CASE("Kiln executes its Rutile configuration through its persistent project process", "[kiln][configuration]") {
	auto executable = kiln_test_setting("KILN_TEST_EXECUTABLE", KILN_TEST_EXECUTABLE);
	auto output = kiln_test_setting("KILN_TEST_ROOT", KILN_TEST_ROOT);
	auto root = output / ("configuration-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
	REQUIRE(std::filesystem::is_regular_file(executable));
	KilnTestProject project(executable, root);
	project.write("kiln.rt", kiln_test_configuration());
	project.write("app/start.rt", "fn start() -> i32 { return answer(); };\nfn release_start() -> i32 { return 7; };\n");
	project.write("app/nested/answer.rt", "fn answer() -> i32 { return 42; };\n");
	project.write("app/nested/notes.txt", "This is not Rutile source.\n");

	auto result = project.run({ "init" });
	INFO(result.output);
	REQUIRE(result.code == 0);
	result = project.run({ "list" });
	INFO(result.output);
	REQUIRE(result.code == 0);
	REQUIRE(result.output == "app.exe\n");
	result = project.run({ "build", "app.exe" });
	INFO(result.output);
	REQUIRE(result.code == 0);
	auto artifact = result.output;
	while (!artifact.empty() && (artifact.back() == '\n' || artifact.back() == '\r')) {
		artifact.pop_back();
	}
	REQUIRE(std::filesystem::is_regular_file(std::filesystem::u8path(artifact)));
	result = project.run({ "run", "app.exe" });
	INFO(result.output);
	REQUIRE(result.code == 42);
	result = project.run({ "run", "app.exe", "--profile", "release" });
	INFO(result.output);
	REQUIRE(result.code == 7);

	project.write("kiln.rt", kiln_test_configuration("renamed-application.exe"));
	result = project.run({ "list" });
	INFO(result.output);
	REQUIRE(result.code == 0);
	REQUIRE(result.output == "renamed-application.exe\n");
	REQUIRE(project.run({ "run", "renamed-application.exe" }).code == 42);

	project.write("kiln.rt", kiln_test_configuration("partial.exe", "rutile", true));
	result = project.run({ "list" });
	INFO(result.output);
	REQUIRE(result.code == 1);
	REQUIRE(result.output.find("duplicate target") != std::string::npos);
	project.write("kiln.rt", kiln_test_configuration("recovered.exe"));
	result = project.run({ "list" });
	INFO(result.output);
	REQUIRE(result.code == 0);
	REQUIRE(result.output == "recovered.exe\n");

	project.write("kiln.rt", "fn main(kiln::Options& options) { kiln::profile(\"debug\", 0); kiln::add_target(\"empty.exe\", kiln::TargetType::executable); };\n");
	result = project.run({ "list" });
	INFO(result.output);
	REQUIRE(result.code == 1);
	REQUIRE(result.output.find("requires at least one source folder") != std::string::npos);
	project.write("kiln.rt", "fn main(kiln::Options& options) { kiln::profile(\"debug\", 0); var target = kiln::add_target(\"file.exe\", kiln::TargetType::executable); @target.sources.add(\"app/start.rt\"); };\n");
	result = project.run({ "list" });
	INFO(result.output);
	REQUIRE(result.code == 1);
	REQUIRE(result.output.find("existing source folder") != std::string::npos);

	for (const std::string kind : { "shared_lib", "static_lib" }) {
		project.write("kiln.rt", "fn main(kiln::Options& options) { kiln::profile(\"debug\", 0); var target = kiln::add_target(\"library.bin\", kiln::TargetType::" + kind + "); @target.sources.add(\"app\"); };\n");
		result = project.run({ "list" });
		INFO(result.output);
		REQUIRE(result.code == 0);
		REQUIRE(result.output == "library.bin\n");
		result = project.run({ "build", "library.bin" });
		INFO(result.output);
		REQUIRE(result.code == 1);
		REQUIRE(result.output.find("library module builds are not implemented") != std::string::npos);
	}
	project.write("kiln.rt", kiln_test_configuration("native.exe", "missing"));
	result = project.run({ "list" });
	INFO(result.output);
	REQUIRE(result.code == 0);
	REQUIRE(result.output == "native.exe\n");
	result = project.run({ "build", "native.exe" });
	INFO(result.output);
	REQUIRE(result.code == 1);
	REQUIRE(result.output.find("Unknown compiler integration") != std::string::npos);
}


TEST_CASE("Kiln initializes only the Rutile configuration file", "[kiln][configuration]") {
	auto executable = kiln_test_setting("KILN_TEST_EXECUTABLE", KILN_TEST_EXECUTABLE);
	auto output = kiln_test_setting("KILN_TEST_ROOT", KILN_TEST_ROOT);
	auto root = output / ("initialize-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
	KilnTestProject project(executable, root);
	project.write("rutile.yaml", "this legacy file is ignored\n");
	auto result = project.run({ "init" });
	INFO(result.output);
	REQUIRE(result.code == 0);
	REQUIRE(std::filesystem::is_regular_file(root / "kiln.rt"));
	REQUIRE_FALSE(std::filesystem::exists(root / "kiln.yaml"));
}

TEST_CASE("Kiln subdirectories contribute recursive folder targets to the same project", "[kiln][configuration][subdirectory]") {
	auto executable = kiln_test_setting("KILN_TEST_EXECUTABLE", KILN_TEST_EXECUTABLE);
	auto output = kiln_test_setting("KILN_TEST_ROOT", KILN_TEST_ROOT);
	auto root = output / ("subdirectories-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
	KilnTestProject project(executable, root);
	project.write("kiln.rt", "fn main(kiln::Options& options) { kiln::profile(\"debug\", 0); kiln::profile(\"checked\", 1); kiln::add_subdirectory(\"child\"); };\n");
	project.write("child/kiln.rt", "fn main(kiln::Options& options) { var target = kiln::add_target(\"child.exe\", kiln::TargetType::executable); @target.sources.add(\"src\"); @target.option(\"entry\", \"start\"); };\n");
	project.write("child/src/main.rt", "fn start() -> i32 { return answer(); };\n");
	project.write("child/src/nested/value.rt", "fn answer() -> i32 { return 42; };\n");
	auto result = project.run({ "init" });
	INFO(result.output);
	REQUIRE(result.code == 0);
	result = project.run({ "list" });
	INFO(result.output);
	REQUIRE(result.code == 0);
	REQUIRE(result.output == "child.exe\n");
	result = project.run({ "run", "child.exe", "--profile", "checked" });
	INFO(result.output);
	REQUIRE(result.code == 42);

	project.write("child/kiln.rt", "fn main(kiln::Options& options) { kiln::add_subdirectory(\"..\"); };\n");
	result = project.run({ "list" });
	INFO(result.output);
	REQUIRE(result.code == 1);
	REQUIRE(result.output.find("cycle") != std::string::npos);

	project.write("child/kiln.rt", "fn main(kiln::Options& options) { var target = kiln::add_target(\"child.exe\", kiln::TargetType::executable); @target.sources.add(\"src\"); @target.option(\"entry\", \"start\"); };\n");
	project.write("kiln.rt", "fn main(kiln::Options& options) { kiln::profile(\"debug\", 0); kiln::add_subdirectory(\"child\"); kiln::add_subdirectory(\"child\"); };\n");
	result = project.run({ "list" });
	INFO(result.output);
	REQUIRE(result.code == 1);
	REQUIRE(result.output.find("Duplicate") != std::string::npos);

	project.write("kiln.rt", "fn main(kiln::Options& options) { kiln::profile(\"debug\", 0); kiln::add_subdirectory(\"child\"); };\n");
	result = project.run({ "list" });
	INFO(result.output);
	REQUIRE(result.code == 0);
	REQUIRE(result.output == "child.exe\n");
	result = project.run({ "run", "child.exe" });
	INFO(result.output);
	REQUIRE(result.code == 42);
}
