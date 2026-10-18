#include "server.hpp"

#include "compiler.hpp"
#include "platform.hpp"
#include "project.hpp"
#include "version.hpp"

#include <CLI/CLI.hpp>
#include <cstdio>
#include <filesystem>
#include <memory>
#include <string>

extern "C" int kiln_language_server(Compiler* compiler, std::FILE* input, std::FILE* output, const char* root);

int kiln_language_session(Compiler* compiler, void* context) {
	const auto* root = static_cast<const char*>(context);
	std::unique_ptr<EditorSession, decltype(&kiln_editor_stop)> session{ kiln_editor_start(compiler, root), kiln_editor_stop };
	if (!session) {
		return 1;
	}
	return kiln_language_server(compiler, platform_standard_input(), platform_standard_output(), root);
}

int main(int argc, char* argv[]) {
	CLI::App app{ "Manage the current Rutile project.", "kiln" };
	argv = app.ensure_utf8(argv);
	app.set_help_flag();
	app.failure_message([](const CLI::App* command, const CLI::Error& error) {
		if (!error.get_exit_code()) {
			return std::string{};
		}
		return std::string(error.what()) + "\nRun " + command->get_name() + " help for more information.\n";
	});
	app.set_version_flag("--version", std::string(software_version));
	app.require_subcommand(1, 1);
	int exit_code = 0;

	app.add_subcommand("init", "Initialize the current project and restart its session with the installed version.")->callback([&exit_code]() {
		exit_code = project_initialize();
	});
	app.add_subcommand("status", "Report the current project's state.")->callback([&exit_code]() {
		exit_code = project_request("status");
	});
	app.add_subcommand("exit", "End the current project's session.")->callback([&exit_code]() {
		exit_code = project_request("exit");
	});
	app.add_subcommand("help", "Show available operations.")->callback([&app]() {
		app.clear();
		throw CLI::CallForHelp();
	});

	std::string endpoint;
	auto* serve = app.add_subcommand("serve", "Serve a project session.")->group("");
	serve->add_option("endpoint", endpoint)->required();
	serve->callback([&exit_code, &endpoint]() {
		exit_code = kiln_serve(endpoint.c_str());
	});

	auto* language = app.add_subcommand("language-server", "Serve the current project's IDE connection.");
	bool stdio = false;
	language->add_flag("--stdio", stdio, "Use standard input and output for the IDE connection.")->required();
	language->callback([&exit_code]() {
		if (!platform_language_stdio()) {
			std::fputs("Could not configure the IDE connection streams.\n", stderr);
			exit_code = 1;
			return;
		}
		std::error_code error;
		const auto directory = std::filesystem::current_path(error);
		if (error) {
			std::fputs("Could not determine the project directory.\n", stderr);
			exit_code = 1;
			return;
		}
		auto root = project_path_utf8(directory);
		exit_code = rutile_compiler_session(kiln_language_session, root.data());
	});

	CLI11_PARSE(app, argc, argv);
	return exit_code;
}
