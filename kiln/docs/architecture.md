# Kiln project manager

Kiln owns the persistent project process, configuration, requests, target selection,
build profiles, output publication, and project status. Compiler integrations own
language-specific validation, source selection, compiler lifetime, diagnostics, and
artifact production. Rutile is the first integration.

The core passes a folder-based target and its discovered sources to the selected
integration. Source files are an implementation detail of discovery, not individual
entries that users maintain in a target. Discovery recurses into nested folders and
excludes Kiln's `.rt` working directory. The Rutile integration selects `.rt` files.

## Configuration

New projects use `kiln.yaml`. Existing `rutile.yaml` projects remain supported when
`kiln.yaml` is absent. A target selects a compiler by name:

```yaml
targets:
  app:
    compiler: rutile
    source: app
    options:
      entry: main
profiles:
  debug:
    optimization: 0
```

The omitted compiler defaults to Rutile for existing projects. Compiler options are
validated by the selected integration. Legacy `entry` remains accepted; new configuration
can place it in `options`. An unknown compiler is diagnosed when its target is built.

YAML supplies high-level configuration and selects named project functions:

```yaml
projects:
  project_name:
profiles:
  debug:
    optimization: 0
```

By default these functions are defined in `kiln.rt`. A mapping value can select a
specific file. Each function receives `kiln::Options`, exposing the project root,
host OS, and selected profile:

```rutile
fn project_name(kiln::Options& options) {
	var target = kiln::add_target("some_exe.exe", kiln::TargetType::executable);
	@target.sources.add("src");
};
```

The [Rutile API](../configuration/README.md) adds folder-based targets and options to
the same project model. It does not generate YAML. Configuration runs before target
listing, builds, and editor source discovery. Failed configuration is not published.
`TargetType` also records `shared_lib` and `static_lib`; building those Rutile modules
is deferred along with module linking.

## Integration boundary

`CompilerIntegration` defines an integration's name, source acceptance, target validation,
artifact description, and build operation. `CompilerRegistry` owns integrations for the
life of a project session. Compiler state is not part of the server API.

The core validates artifact paths and publishes a successful staged artifact. Integrations
choose artifact filenames and whether an artifact can be executed. This lets a later C++
integration produce its own output without changing the server's request loop.

The Rutile integration lazily creates its standalone compiler. An IDE session borrows the
editor's existing compiler so builds continue to use unsaved analysis. Only Rutile source
groups enter its language server; targets belonging to other integrations remain separate.

`kiln init`, `list`, `build`, `run`, `status`, and `exit` expose the project workflow.
The existing Rutile commands remain compatible clients of the project process.

## Module direction

A module is defined by source folders. Its eventual outputs include a native artifact
(such as a static library or DLL) and an `.rti` interface carrying type information.
Dependencies will pair the binary with its interface. Rutile declarations can describe
C++ code for that interface. This does not require a new `export` keyword.

Module linking and `.rti` generation/loading are deliberately deferred. The current Rutile
integration preserves the existing executable build behavior; this refactor does not add
library or cross-language linking.
