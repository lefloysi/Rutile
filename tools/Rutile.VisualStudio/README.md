# Rutile Visual Studio extension

This project produces the independent `Rutile.VisualStudio.vsix`. It provides `.rt` content
types, server-provided syntax and semantic colors, diagnostics, completion, hover, navigation,
signature help, and editable template argument contexts. Grammar and binding remain in Rutilec;
rename is not advertised by the language server.

Rutilec owns highlighting rules, including keywords, comments, strings, numbers, and operators.
The extension renders the token roles advertised by the connected server. Updating Rutilec and
restarting its service refreshes the legend and open documents without reinstalling the extension.
Compiler and extension versions do not need to match. VSIX updates are for editor integration
changes.

Typing support pairs parentheses, square brackets, braces, and quotes using Visual Studio's
brace completion sessions. Closing characters step over their matching pair; Backspace removes
empty pairs and Tab moves past the closing character. Enter inside empty braces creates an
indented body using the editor's indentation settings. Pairing is suppressed in comments and
strings.

Completion ranks `this` first, followed by names in the nearest scope and then outer scopes.
Member access uses the compiler's accessible member candidates. Keywords and built-in types
come from the compiler's own inventories and follow visible symbols. Completion edits replace
the current identifier, including when the caret is in the middle of it.

Comments, strings, numbers, and operators receive basic fallback colors while disconnected.
The extension resolves each focused source file to its nearest `kiln.rt` project definition and
connects using that directory's identity. If the service is unavailable, it runs `kiln configure`
for the project before connecting. Kiln owns project configuration and the Rutilec lifecycle.
An existing service is reused without reconfiguration. Kiln is found through `KILN_EXECUTABLE`,
`PATH`, its Windows application registration, standard installation folders, or the project's
`distro/windows-x64` directory.

Customize Rutile classifications in Fonts and Colors. Language-server errors appear in the
Rutile Output pane.

## Build and install

Install Visual Studio 2022 with the extension development workload, then package this extension:

```bash
bash ./tools/Rutile.VisualStudio/Package.sh Release
```

The package is written to `distro/windows-x64/Rutile.VisualStudio.vsix`. It contains the editor
component only: no Kiln executable, Rutilec executable, LLVM runtime, or compiler toolchain.
Install the VSIX and open a `.rt` document beneath a `kiln.rt` project definition. The extension
prepares that file's project when needed and reconnects when you switch between projects.
