#pragma once

#include <CLI/CLI.hpp>

#include <cstdlib>
#include <string>

struct ExampleOptions {
	std::string backend;
};

inline ExampleOptions parse_cli(int argc, char** argv) {
	CLI::App cli;
	ExampleOptions options;
	cli.add_option("-b,--backend", options.backend, "Rendering backend")->default_val("rt-vk13");
	try {
		cli.parse(argc, argv);
	} catch (const CLI::ParseError& error) {
		std::exit(cli.exit(error));
	}
	return options;
}
