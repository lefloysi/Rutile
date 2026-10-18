# Kiln language support

The Visual Studio client starts `kiln language-server --stdio` in the open folder or solution
folder. Kiln owns the compiler, project targets, editor overlays, and background analysis. CLI
builds connect to that same project process and emit the current target analysis, including
unsaved editor text.

## Editor features

- Definition and declaration navigation, references, hover, document and workspace symbols.
- Lexical, member, pointer-member, namespace, and type-qualified completion.
- Compiler access checks, inherited member lookup, overloads, aliases, and template bindings.
- Signature help for selected calls and ordinary overload candidates in incomplete calls.
- Compiler diagnostics with source ranges, related candidate notes, and document versions.
- Incremental UTF-16 editor updates over UTF-8 sources, including Unicode paths.
- Project source and `rutile.yaml` change notifications, with target rediscovery.

A source location in a template can represent several instantiated calls. Signature help retains
those signatures rather than choosing an arbitrary instantiation. Member candidates likewise come
from the compiler's resolved receiver and scope; the client does not infer types from spelling.

## Analysis and latency

Analysis runs on a worker. Requests use a published immutable index containing compiler symbol
identities, locations, references, scopes, calls, and receiver candidates. F12 uses indexed source
occurrences and symbol lookup. It does not parse files or wait for the analysis worker.

Edits invalidate overlapping bindings. Unaffected locations can be mapped through pending edits;
a request for an unindexed or invalidated location returns no target until analysis publishes it.
This avoids navigating to a stale or guessed declaration. Background indexing time and indexed
request time are separate measurements.

Editor parsing preserves incomplete member accesses, calls, expressions, and later declarations.
Recovery diagnostics still make those programs invalid for compilation. Semantic failures retain
the bindings found before the failure; general continuation past arbitrary semantic errors is
not yet supported. Rename, formatting, semantic tokens, and debugging are not advertised.

## Verification

Run the following against a built Kiln; use the corresponding configuration for all binaries:

```powershell
python tests/language-server/protocol.py --server build/release/bin/kiln.exe
python tests/language-server/editor.py --server build/release/bin/kiln.exe `
    --compiler build/release/bin/rutile-selfhost.exe
python tests/language-server/navigation.py --server build/release/bin/kiln.exe
python tests/language-server/build.py --server build/release/bin/kiln.exe `
    --command build/release/bin/rutile.exe
python tests/language-server/lifecycle.py --server build/release/bin/kiln.exe
```

The navigation check reports end-to-end F12 latency for a project with 10,000 functions in 101
files. It also checks navigation after external file creation, modification, deletion, and project
configuration changes. Protocol checks additionally cover responsiveness during reanalysis.

See the [Visual Studio extension](../../tools/Rutile.VisualStudio/README.md) for packaging and
installation instructions. Extension build success and standalone protocol tests do not establish
that the installed Visual Studio client activates correctly; verify that separately.
