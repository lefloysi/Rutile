#include "server.hpp"

#include "compiler.hpp"
#include "../integrations/rutile.hpp"
#include "platform.hpp"
#include "project.hpp"
#include "version.hpp"

#include <CLI/CLI.hpp>
#include <cstdio>
#include <filesystem>
#include <memory>
#include <string>

int main(int argc, char* argv[]) {
	CLI::App app{ "Manage project builds across compiler integrations.", "kiln" };
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

	std::string target;
	std::string profile = "debug";
	auto* build = app.add_subcommand("build", "Build a configured target using its compiler integration.");
	build->add_option("target", target, "Target name; optional when only one target exists.");
	build->add_option("--profile", profile, "Build profile.")->default_val("debug");
	build->callback([&]() {
		exit_code = project_build(target, profile);
	});
	auto* run = app.add_subcommand("run", "Build and run a configured executable target.");
	run->add_option("target", target, "Target name; optional when only one target exists.");
	run->add_option("--profile", profile, "Build profile.")->default_val("debug");
	run->prefix_command();
	run->callback([&]() {
		exit_code = project_run(target, profile, run->remaining());
	});
	app.add_subcommand("list", "List configured targets.")->callback([&exit_code]() {
		exit_code = project_list();
	});

	std::string endpoint;
	auto* serve = app.add_subcommand("serve", "Serve a project session.")->group("");
	serve->add_option("endpoint", endpoint)->required();
	serve->callback([&exit_code, &endpoint]() {
		CompilerRegistry compilers;
		compilers.add(kiln_rutile_integration());
		exit_code = kiln_serve(compilers, endpoint.c_str());
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
		exit_code = kiln_rutile_language_server(root.c_str());
	});

	CLI11_PARSE(app, argc, argv);
	return exit_code;
}
