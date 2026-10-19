#include "project.hpp"
#include "version.hpp"

#include <CLI/CLI.hpp>
#include <string>

int main(int argc, char* argv[]) {
	CLI::App app{ "Manage the current Rutile project.", "rutile" };
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

	std::string target;
	std::string profile = "debug";
	auto* run = app.add_subcommand("run", "Build and run a configured project target.");
	run->add_option("target", target, "Target from the project configuration; optional when only one target exists.");
	run->add_option("--profile", profile, "Build profile from the project configuration.")->default_val("debug");
	run->prefix_command();
	run->callback([&]() {
		exit_code = project_run(target, profile, run->remaining());
	});
	app.add_subcommand("list", "List executables the current project can build.")->callback([&exit_code]() {
		exit_code = project_list();
	});
	app.add_subcommand("init", "Initialize the current folder and restart its project session.")->callback([&exit_code]() {
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

	CLI11_PARSE(app, argc, argv);
	return exit_code;
}
