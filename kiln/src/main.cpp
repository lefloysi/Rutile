#include "server.hpp"

#include "project.hpp"
#include "version.hpp"

#include <CLI/CLI.hpp>
#include <string>

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

	CLI11_PARSE(app, argc, argv);
	return exit_code;
}
