#include "rutile.hpp"

#include "platform.hpp"
#include "project.hpp"
#include "../src/linker.hpp"
#include "../src/server.hpp"

std::vector<std::filesystem::path> kiln_rutile_standard_sources() {
	auto directory = std::filesystem::u8path(KILN_CONFIGURATION_STDLIB);
	char executable[32768];
	if (platform_executable_path(executable, sizeof(executable))) {
		auto installed = std::filesystem::u8path(executable).parent_path().parent_path() / "share" / "kiln" / "std";
		std::error_code error;
		if (std::filesystem::is_regular_file(installed / "string_view.rt", error) && !error) {
			directory = std::move(installed);
		}
	}
	std::vector<std::filesystem::path> result;
	for (const char* name : { "unique", "optional", "buffer", "string_view", "string", "hash", "equal", "unordered_map" }) {
		result.push_back(directory / (std::string(name) + ".rt"));
	}
	return result;
}

#include <utility>

struct RutileIntegration : CompilerIntegration {
	Compiler* compiler;
	Compiler* borrowed;

	explicit RutileIntegration(Compiler* supplied);
	~RutileIntegration() override;
	std::string_view name() const override;
	bool accepts_source(const std::filesystem::path& path) const override;
	CompilerArtifact artifact(const ProjectTarget& target) const override;
	bool validate(const ProjectTarget& target, const ProjectProfile& profile, std::string& diagnostics) const override;
	CompilerResult build(const CompilerBuild& request) override;
};

std::string rutile_target_entry(const ProjectTarget& target) {
	auto option = target.options.find("entry");
	if (option != target.options.end()) {
		return option->second;
	}
	return target.entry.empty() ? "main" : target.entry;
}

bool rutile_entry_identifier(std::string_view name) {
	while (true) {
		auto separator = name.find("::");
		auto part = name.substr(0, separator);
		if (part.empty()) {
			return false;
		}
		for (std::size_t index = 0; index < part.size(); ++index) {
			char value = part[index];
			bool letter = (value >= 'a' && value <= 'z') || (value >= 'A' && value <= 'Z');
			if (!letter && value != '_' && !(index && value >= '0' && value <= '9')) {
				return false;
			}
		}
		if (separator == std::string_view::npos) {
			return true;
		}
		name.remove_prefix(separator + 2);
	}
}

std::string rutile_diagnostics(std::FILE* file) {
	std::rewind(file);
	std::string text;
	char buffer[4096];
	while (true) {
		std::size_t count = std::fread(buffer, 1, sizeof(buffer), file);
		if (!count) {
			break;
		}
		text.append(buffer, count);
		if (text.size() >= 128 * 1024) {
			text += "\nFurther diagnostics omitted.\n";
			break;
		}
	}
	return text;
}

extern "C" std::FILE* compiler_open_file(const char* path, const char* mode) {
	return platform_open_file(std::filesystem::u8path(path), mode);
}

RutileIntegration::RutileIntegration(Compiler* supplied) : compiler(supplied), borrowed(supplied) {
}

RutileIntegration::~RutileIntegration() {
	if (compiler && !borrowed) {
		rutile_compiler_destroy(compiler);
	}
}

std::string_view RutileIntegration::name() const {
	return "rutile";
}

bool RutileIntegration::accepts_source(const std::filesystem::path& path) const {
	return path.extension() == ".rt";
}

CompilerArtifact RutileIntegration::artifact(const ProjectTarget& target) const {
	if (!target.output.empty()) {
		return { target.output, artifact_usage::executable };
	}
#if defined(_WIN32)
	return { std::filesystem::u8path(target.name + ".exe"), artifact_usage::executable };
#else
	return { std::filesystem::u8path(target.name), artifact_usage::executable };
#endif
}

bool RutileIntegration::validate(const ProjectTarget& target, const ProjectProfile& profile, std::string& diagnostics) const {
	if (target.type != target_type::executable) {
		diagnostics = "Rutile library module builds are not implemented yet.\n";
		return false;
	}
	if (!rutile_entry_identifier(rutile_target_entry(target))) {
		diagnostics = "Rutile entry must be an identifier or a namespace-qualified identifier.\n";
		return false;
	}
	for (const auto& option : target.options) {
		if (option.first != "entry") {
			diagnostics = "Unknown Rutile compiler option '" + option.first + "'.\n";
			return false;
		}
	}
	if (profile.optimization < 0 || profile.optimization > 3) {
		diagnostics = "Rutile optimization must be between 0 and 3.\n";
		return false;
	}
	return true;
}

CompilerResult RutileIntegration::build(const CompilerBuild& request) {
	auto object = request.output;
	object.replace_extension(".o");
	auto log = request.output.parent_path() / "diagnostics.log";
	for (const auto& source : request.sources) {
		for (const auto& destination : { request.output, object, log }) {
			std::error_code error;
			if (std::filesystem::equivalent(source, destination, error)) {
				return { false, "Build output would overwrite a source file.\n" };
			}
			if (error && error != std::errc::no_such_file_or_directory) {
				return { false, "Could not verify the build output paths.\n" };
			}
		}
	}
	std::unique_ptr<std::FILE, decltype(&std::fclose)> diagnostics{ std::tmpfile(), std::fclose };
	if (!diagnostics) {
		return { false, "Could not create compiler diagnostics buffer.\n" };
	}
	if (!compiler) {
		compiler = rutile_compiler_create();
		if (!compiler) {
			return { false, "Could not create the Rutile compiler.\n" };
		}
	}
	std::vector<std::string> paths;
	for (const auto& standard : kiln_rutile_standard_sources()) {
		bool provided = false;
		for (const auto& source : request.sources) {
			if (source.filename() == standard.filename()) {
				provided = true;
				break;
			}
		}
		if (!provided) {
			paths.push_back(project_path_utf8(standard));
		}
	}
	for (const auto& source : request.sources) {
		paths.push_back(project_path_utf8(source));
	}
	std::vector<const char*> arguments;
	for (const auto& path : paths) {
		arguments.push_back(path.c_str());
	}
	auto entry = rutile_target_entry(request.target);
	int result;
	if (request.input == build_input::editor) {
		result = kiln_emit_target(compiler, project_path_utf8(request.root).c_str(), request.target.name.c_str(), project_path_utf8(object).c_str(), entry.c_str(), request.profile.optimization, diagnostics.get());
	}
	else {
		result = rutile_compile(compiler, arguments.data(), arguments.size(), project_path_utf8(object).c_str(), compiler_open_file, entry.c_str(), request.profile.optimization, diagnostics.get());
	}
	std::string messages = rutile_diagnostics(diagnostics.get());
	if (!result) {
		result = kiln_link(object, request.output, log);
		std::unique_ptr<std::FILE, decltype(&std::fclose)> backend_log{ platform_open_file(log, "rb"), std::fclose };
		if (backend_log) {
			messages += rutile_diagnostics(backend_log.get());
		}
	}
	if (result && messages.empty()) {
		messages = "Rutile compilation failed.\n";
	}
	return { !result, std::move(messages) };
}

std::unique_ptr<CompilerIntegration> kiln_rutile_integration(Compiler* borrowed) {
	return std::make_unique<RutileIntegration>(borrowed);
}

int kiln_rutile_language_session(Compiler* compiler, void* context) {
	const auto* root = static_cast<const char*>(context);
	CompilerRegistry integrations;
	if (!integrations.add(kiln_rutile_integration(compiler))) {
		return 1;
	}
	std::unique_ptr<EditorSession, decltype(&kiln_editor_stop)> session{ kiln_editor_start(integrations, root), kiln_editor_stop };
	if (!session) {
		return 1;
	}
	return kiln_language_server(compiler, platform_standard_input(), platform_standard_output(), root);
}

int kiln_rutile_language_server(const char* root) {
	return rutile_compiler_session(kiln_rutile_language_session, const_cast<char*>(root));
}
