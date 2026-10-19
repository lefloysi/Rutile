# Rutile project configuration API

Declare project configuration functions in `kiln.yaml`:

```yaml
projects:
  demo:
profiles:
  debug:
    optimization: 0
```

The empty `demo` value selects `kiln.rt`. A scalar value selects another Rutile file.
Kiln calls the named function when loading the project, passing typed options:

```rutile
fn demo(kiln::Options& options) {
	var target = kiln::add_target("app.exe", kiln::TargetType::executable);
	@target.sources.add("app");
	@target.option("entry", "start");
};
```

`options.path()`, `options.os()`, and `options.profile()` return string views containing
the project root, host OS (`windows`, `macos`, or `linux`), and selected build profile.
The default profile for target discovery is `debug`.

`kiln::add_target(output, type)` declares a target with an output filename and one of
`TargetType::executable`, `TargetType::shared_lib`, or `TargetType::static_lib`.
Library target declarations are represented in the project model; building libraries
awaits module linking support.

`target.sources.add(folder)` adds a source folder relative to the project root. Kiln
collects sources recursively through the selected compiler integration. It accepts folders,
not individual files. Add several folders by calling it several times.

`target.compiler(name)` selects a compiler integration; the default is `rutile`.
`target.option(key, value)` assigns an integration-specific string option. These methods
and `sources.add` return `bool` and record configuration failures even if ignored.

YAML supplies high-level configuration. Rutile executes project declarations through the
API rather than generating YAML. Names must be unique across all declarations. Failed
callbacks or invalid declarations reject the entire configuration update. Configuration
programs are compiled again when loaded, so edits affect later requests in a running Kiln.
