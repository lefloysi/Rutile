# Editor source groups

Kiln analyzes each configured target with all of that target's Rutile source files.
Files outside those targets otherwise belong to a loose group of open documents.
Declarations in unopened, unrelated files are not available in that group.

A project built by another build system can declare additional editor-only source groups:

```yaml
analysis:
  kiln:
    source: kiln
```

Each source folder is scanned recursively for `.rt` files, using the same discovery rules as
runnable targets. An analysis group has no entry point and cannot be selected by `rutile run`.
Its source files are analyzed together, including unopened files, so diagnostics and navigation
use their actual cross-file declarations. Group names are independent of runnable target names.

This repository's `kiln` group includes the compiler, standard library, and Kiln Rutile sources.
CMake continues to own the native build and C++ entry point. The `hello` and `greet` runnable
targets remain separate.

An ordered group uses `sources` instead of `source`:

```yaml
analysis:
  kiln:
    sources:
      - kiln/compiler/std/unique.rt
      - kiln/compiler/std/optional.rt
      - kiln/compiler/std/buffer.rt
      - kiln/compiler/std/string_view.rt
      - kiln/compiler/std/string.rt
      - kiln/compiler/std/hash.rt
      - kiln/compiler/std/equal.rt
      - kiln/compiler/std/unordered_map.rt
      - kiln/compiler/std/json.rt
      - manifest: kiln/compiler/sources.list
        root: kiln/compiler/src
      - manifest: kiln/sources.list
        root: kiln/src
```

Each plain path names a Rutile file or a recursively scanned folder. Folder entries are sorted.
A manifest preserves its listed file order; its entries are resolved against the supplied `root`.
Both paths in a manifest mapping are relative to the project configuration. Blank manifest lines
and lines beginning with `#` are ignored. Missing source files report the manifest and line number.
Repeated source files are included once, at their first position.

The compiler's qualified declarations require their owning declarations to have been collected.
This repository reuses its CMake source manifests to preserve that order for editor analysis.
`source` and `sources` cannot appear together in one group.
