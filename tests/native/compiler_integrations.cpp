#include "compiler.hpp"
#include "../../../Kiln/configuration/execution.hpp"
#include "project.hpp"
#include "status.hpp"

#include <nlohmann/json.hpp>
#include <chrono>
#include <fstream>
#include <iterator>
#include <memory>
#include <stdexcept>

std::string project_path_utf8(const std::filesystem::path& path) {
	auto text = path.generic_u8string();
	return std::string(reinterpret_cast<const char*>(text.data()), text.size());
}

void project_record_compilation(const std::filesystem::path&, const std::filesystem::path&, std::size_t, std::int64_t, bool) {
}

bool kiln_configure_project(const std::filesystem::path& root, ProjectConfiguration& configuration, std::string&, std::string_view profile) {
	if (!profile.empty() && profile != "debug") {
		throw std::runtime_error("incorrect configuration profile");
	}
	for (const auto& name : { "app", "native", "library", "unknown" }) {
		ProjectTarget target;
		target.name = name;
		target.source = root / "sources";
		target.compiler = target.name == "app" ? "rutile" : target.name == "unknown" ? "absent" : "cpp";
		target.options["mode"] = target.name;
		target.type = target.name == "library" ? target_type::shared_lib : target_type::executable;
		configuration.targets.push_back(std::move(target));
	}
	return true;
}

enum class FakeBehavior {
	succeed,
	fail,
	invalid,
};

struct FakeCompiler : CompilerIntegration {
	std::string id;
	std::string extension;
	FakeBehavior behavior = FakeBehavior::succeed;
	artifact_usage usage = artifact_usage::executable;
	target_type requested_type = target_type::executable;
	std::filesystem::path artifact_name;
	int calls = 0;
	std::filesystem::path staged;
	build_input input = build_input::disk;
	std::string option;

	FakeCompiler(std::string name, std::string suffix);
	std::string_view name() const override;
	bool accepts_source(const std::filesystem::path& path) const override;
	CompilerArtifact artifact(const ProjectTarget& target) const override;
	bool validate(const ProjectTarget&, const ProjectProfile&, std::string& diagnostics) const override;
	CompilerResult build(const CompilerBuild& request) override;
};

FakeCompiler::FakeCompiler(std::string name, std::string suffix) : id(std::move(name)), extension(std::move(suffix)) {
}

std::string_view FakeCompiler::name() const {
	return id;
}

bool FakeCompiler::accepts_source(const std::filesystem::path& path) const {
	return path.extension() == extension;
}

CompilerArtifact FakeCompiler::artifact(const ProjectTarget& target) const {
	if (!artifact_name.empty()) {
		return { artifact_name, usage };
	}
	return { target.name + (usage == artifact_usage::module ? ".lib" : ".exe"), usage };
}

bool FakeCompiler::validate(const ProjectTarget&, const ProjectProfile&, std::string& diagnostics) const {
	if (behavior == FakeBehavior::invalid) {
		diagnostics = "fake validation failed";
		return false;
	}
	return true;
}

CompilerResult FakeCompiler::build(const CompilerBuild& request) {
	++calls;
	staged = request.output;
	input = request.input;
	option = request.target.options.at("mode");
	requested_type = request.target.type;
	if (request.sources.size() != 2 || request.profile.name != "debug" || request.root != std::filesystem::current_path()) {
		throw std::runtime_error("incorrect compiler build request");
	}
	for (const auto& source : request.sources) {
		if (!accepts_source(source)) {
			throw std::runtime_error("mixed compiler source leaked");
		}
	}
	std::ofstream(request.output) << id << ':' << calls;
	if (behavior == FakeBehavior::fail) {
		return { false, "fake compilation failed" };
	}
	return { true, {} };
}

std::string read_output(const std::filesystem::path& path) {
	std::ifstream file(path);
	return std::string(std::istreambuf_iterator<char>(file), {});
}

nlohmann::json build_target(CompilerRegistry& registry, std::string_view target, build_input input = build_input::disk) {
	return nlohmann::json::parse(kiln_build(registry, nlohmann::json{ { "target", target }, { "profile", "debug" } }.dump(), input));
}

int main(int count, char** arguments) {
	if (count != 2) {
		return 1;
	}
	auto root = std::filesystem::absolute(std::filesystem::u8path(arguments[1])) / ("case-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
	std::filesystem::create_directories(root / "sources/nested");
	std::filesystem::current_path(root);
	for (const auto& name : { "main.rt", "main.cpp", "nested/helper.rt", "nested/helper.cpp" }) {
		std::ofstream(root / "sources" / name) << "fake source";
	}
	std::ofstream(root / "kiln.rt") << "fn main(kiln::Options& options) {};";
	CompilerRegistry registry;
	auto rutile = std::make_unique<FakeCompiler>("rutile", ".rt");
	auto cpp = std::make_unique<FakeCompiler>("cpp", ".cpp");
	auto* rutile_state = rutile.get();
	auto* cpp_state = cpp.get();
	if (!registry.add(std::move(rutile)) || !registry.add(std::move(cpp)) || registry.add(std::make_unique<FakeCompiler>("rutile", ".rt")) || registry.add(nullptr) || registry.add(std::make_unique<FakeCompiler>("", ".rt"))) {
		return 2;
	}
	if (registry.find("rutile") != rutile_state || registry.find("cpp") != cpp_state || registry.find("absent")) {
		return 3;
	}
	auto first = build_target(registry, "app");
	if (!first.at("succeeded").get<bool>() || rutile_state->calls != 1 || cpp_state->calls || rutile_state->option != "app" || std::filesystem::exists(rutile_state->staged)) {
		return 4;
	}
	auto output = std::filesystem::u8path(first.at("executable").get<std::string>());
	if (read_output(output) != "rutile:1" || rutile_state->staged.parent_path().filename() != ".pending") {
		return 5;
	}
	auto native = build_target(registry, "native", build_input::editor);
	if (!native.at("succeeded").get<bool>() || cpp_state->calls != 1 || cpp_state->input != build_input::editor || cpp_state->option != "native") {
		return 6;
	}
	rutile_state->behavior = FakeBehavior::fail;
	std::ofstream(root / "sources/main.rt") << "changed fake source";
	auto failed = build_target(registry, "app");
	if (failed.at("succeeded").get<bool>() || failed.at("diagnostics") != "fake compilation failed" || read_output(output) != "rutile:1" || std::filesystem::exists(rutile_state->staged)) {
		return 7;
	}
	rutile_state->behavior = FakeBehavior::succeed;
	auto retry = build_target(registry, "app");
	if (!retry.at("succeeded").get<bool>() || rutile_state->calls != 3 || read_output(output) != "rutile:3") {
		return 8;
	}
	auto unknown = build_target(registry, "unknown");
	if (unknown.at("succeeded").get<bool>() || unknown.at("diagnostics").get<std::string>().find("Unknown compiler integration 'absent'") == std::string::npos || rutile_state->calls != 3 || cpp_state->calls != 1) {
		return 9;
	}
	rutile_state->behavior = FakeBehavior::invalid;
	auto invalid = build_target(registry, "app");
	if (invalid.at("succeeded").get<bool>() || invalid.at("diagnostics") != "fake validation failed" || rutile_state->calls != 3 || read_output(output) != "rutile:3") {
		return 10;
	}
	cpp_state->usage = artifact_usage::module;
	auto module = build_target(registry, "library");
	if (!module.at("succeeded").get<bool>() || module.contains("executable") || cpp_state->requested_type != target_type::shared_lib || std::filesystem::u8path(module.at("artifact").get<std::string>()).extension() != ".lib" || read_output(std::filesystem::u8path(module.at("artifact").get<std::string>())) != "cpp:2") {
		return 11;
	}
	cpp_state->artifact_name = "../escape.lib";
	auto traversal = build_target(registry, "native");
	if (traversal.at("succeeded").get<bool>() || traversal.at("diagnostics").get<std::string>().find("invalid artifact filename") == std::string::npos || cpp_state->calls != 2) {
		return 12;
	}
	return 42;
}
