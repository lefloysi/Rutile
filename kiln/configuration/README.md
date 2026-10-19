# Kiln project configuration

A project has one entry file, `kiln.rt`. Kiln compiles it and executes its `main`:

```rutile
fn main(kiln::Options& options) {
	kiln::profile("debug", 0);
	kiln::profile("release", 2);
	var app = kiln::add_target("app.exe", kiln::TargetType::executable);
	@app.sources.add("src");
	kiln::add_subdirectory("engine");
};
```

`add_subdirectory("engine")` compiles `engine/kiln.rt` as a separate configuration
program and executes its `main`. It contributes targets to the same project. Nested
configuration paths and source folders resolve relative to their own `kiln.rt`.
Duplicate targets and repeated or cyclic subdirectory configuration are errors.
A failed configuration update is rejected as a whole.

`target.sources.add(folder)` recursively includes sources accepted by its compiler.
It accepts folders, not individual files. There is no source manifest or YAML setup.
Subfolders do not need configuration files unless they contribute build configuration.

`options.path()`, `options.os()`, and `options.profile()` return views containing the
configuration directory, host OS, and selected profile. Debug and release profiles
are available by default; `kiln::profile(name, optimization)` declares or customizes
profiles, with optimization from 0 through 3.

Targets support `executable`, `static_lib`, `shared_lib`, and `object_lib` types.
`target.compiler("cpp")` selects C++; the default is Rutile. C++ builds executables
and libraries; Rutile currently builds executables and compile-only objects.
Rutile module linking and `.rti` generation remain deferred.

`target.depends_on("library.lib")` declares a target dependency. Kiln builds dependencies
first, rejects dependency cycles, and supplies their native artifacts to the compiler.
`target.option(key, value)` supplies compiler options. See [C++ options](../integrations/cpp.md).
Path options resolve relative to the declaring configuration file. Target names are unique
within the single project, including targets declared by subdirectories.

Kiln caches configuration until a configuration file or its API changes. After the first
build, the continuous manager watches sources, headers, configuration, and dependencies,
rebuilding affected targets. `kiln describe --json` exposes targets, profiles, and build results.
