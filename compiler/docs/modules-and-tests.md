# Modules and test targets

Kiln selects the application entry function for each executable target:

```rutile
var game = kiln::add_target("game", "game::client");
@game.sources.add("src");
@game.import("std");

var tests = kiln::add_tests("game-tests", game);
@tests.sources.add("tests");
```

A target without an entry function is a static library. Executables also provide
a companion static library so their tests can link their code without a process
entry point. Test targets inherit the tested target's imports and dependencies.
Run a test target with `kiln test game-tests --profile debug`.
Use `--jobs 4` to run four worker processes. Each worker runs its harness once
and executes a distinct subset of the discovered tests. Child registrations
execute within their parent test's worker. Any failed worker fails the command.

## Interfaces and visibility

Each static library is accompanied by a `.rti` interface. The interface retains
declarations, attributes, implementation information needed for analysis, and
template definitions. Importing it does not require the original source files.
Concrete imported definitions are linked from the library; required template
instances can be emitted by the consumer with shared native identities.

Interfaces include private declarations. Ordinary consumers must use declarations
qualified with `export`. Tests additionally have private access to the target
they test, including private members. This access does not extend to that
target's dependencies. Exporting a namespace does not export its children.

Both `template<typename T> export` and `export template<typename T>` are valid;
the former is the preferred ordering.

Interfaces require the same compiler version and target pointer width as their
consumer. `std.lib` and `std.rti` are built and distributed together with the
compiler. Targets request them explicitly using `target.import("std")`.

## Application entry

The runtime calls the selected function with `rt::span<rt::string_view>` and
expects `rt::ExitCode`. Arguments exclude the executable name. The compiler owns
the CRT-facing `main` and converts the returned exit code for the operating system.
The selected function can have any name, including an overload named `main`.

## Tests and harnesses

Attributes are arbitrary declaration metadata. `[[key]]` records presence;
`[[key : value]]` retains a value, name, expression, or type. They do not require
registration or an attribute declaration. The test environment interprets `test`
and `harness`; other consumers can inspect the syntax metadata independently.

The compiler discovers `[[test]]` functions in the test target's own sources and
generates its runner. It does not run tests found inside imported interfaces.
The test environment uses the standard library for its runtime and reporting.

```rutile
[[harness]]
fn run_tests() {
	var environment = game::Environment();
	rt::test::run(@environment);
	@environment.shutdown();
};

[[test]]
fn example(var game::Environment& environment) {
	rt::test::expect(environment.ready());
	rt::test::expect(environment.assets_loaded());
};
```

There can be at most one harness. It owns its local environment and controls when
to call `rt::test::run`. That call forwards the same references to every discovered
test. Without a harness, the generated entry calls `run()` with no arguments.
Incompatible test signatures are compile errors.

`expect` records a failed check and continues. A failed check makes the process
result unsuccessful even if later checks pass. The harness shuts down after the
runner returns; environment ownership follows ordinary Rutile lifetime rules.

Tests can synchronously register and execute named child tests using
`rt::test::register(name, &function, arguments...)`. Child tests can do the same.
The child receives explicitly supplied references and completes before
registration returns. No lambda or new iteration syntax is needed.

DLL modules and explicitly loaded library interfaces are outside this change.
