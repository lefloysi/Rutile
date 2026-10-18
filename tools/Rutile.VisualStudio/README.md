# Rutile for Visual Studio 2022

The extension registers `.rt` files and connects Visual Studio's built-in language client to
`kiln.exe language-server --stdio`. Kiln starts in the open folder workspace or solution directory
supplied by Visual Studio. Grammar and name binding remain in the compiler.

The server provides diagnostics, hover, definitions, declarations, references, completion,
signature help, document and workspace symbols, and compiler-backed semantic tokens.
Rename is not advertised by the server.

Highlighting uses Rutile-specific editor styles. Functions, methods, and enum values default to
pale yellow; namespaces to light gray; user types to green; ordinary keywords and primitive types
to blue; and control-flow words to purple. The **Rutile** entries in Visual Studio's Fonts and
Colors
settings can customize these defaults without changing C++ or other languages.
Comments, strings, and numbers are colored immediately. User types and aliases come from compiler
bindings, and contextual words such as access labels and casts come from compiler syntax.

Compiler tokens are rendered by Rutile's classifier rather than relying on Visual Studio's optional
C++ classifications. Bound spans remain visible and move with unchanged text while an edit is
being analyzed. The compiler's completed-index notification refreshes their semantic
classifications.

Visual Studio watches `.rt`, `rutile.yaml`, and `.list` files throughout the active workspace and
forwards external changes to Kiln. Opening a different folder or solution replaces the owned
Kiln process. Closing the workspace stops it. These subscriptions are released with the client.

Server errors appear in the **Rutile** Output pane. Closing the language connection releases
its server process. Cancellation also terminates it; stderr is drained independently of LSP.

## Build

Install the Visual Studio 2022 extension development workload. The project targets .NET
Framework 4.7.2 and the cached Visual Studio 17 SDK packages.

Build the CMake `kiln` target first, then package from the repository root:

```powershell
cmake --build build/release --target kiln
./tools/Rutile.VisualStudio/Package.ps1 -Configuration Release
```

The package script locates the installed Visual Studio SDK and x64 MSVC redistributables.
It uses the SDK's VSIX packaging target and does not install or launch the extension.
The result is `tools/Rutile.VisualStudio/bin/Release/Rutile.VisualStudio.vsix`.
Its manifest and assembly versions come from `version/release.txt`.
The source manifest is a template. Packaging rejects a Kiln binary with a different version.

For other build directories, pass `-RutileServerDirectory <absolute-path>`. The matching
MSBuild property has the same name. Debug defaults to `build/debug/bin`; Release defaults
to `build/release/bin`. `RutileCrtDirectory` can select an explicit x64 CRT payload directory.

The VSIX includes the extension assembly, `kiln.exe`, `LLVM-C.dll`, and the matching
`yaml-cpp.dll` or `yaml-cppd.dll`, plus the x64 MSVC runtime libraries.
Packaging fails if required native inputs are missing.
Debug packages also include the developer Debug CRT and debug Universal CRT from the installed SDK.

## Install and use

Open `tools/Rutile.VisualStudio/bin/Release/Rutile.VisualStudio.vsix` to start the VSIX installer.
Select Visual Studio 2022 and restart it if the installer requests it. Open the Rutile folder
or its solution, then open a `.rt` file. Use F12 on a symbol to navigate to its definition.
Completion and signature help use Visual Studio's normal editor commands.

Place the caret inside a template to show an argument field above its declaration. Type the
arguments, then press Enter or **Apply**. The compiler resolves those arguments even when the
program has no existing instantiation. **Generic** restores the unselected context. Argument
errors appear beside the field and do not change build diagnostics. Generic hover stays concise.

## Verification

Run the process ownership checks without starting Visual Studio:

```powershell
./tools/Rutile.VisualStudio/tests/Run.ps1
```

Run lexical and semantic-color mapping checks without starting Visual Studio:

```powershell
./tools/Rutile.VisualStudio/tests/RunHighlighting.ps1
```

These compile against the installed SDK and check keyword categories, comments, literals,
UTF-16 spans, document synchronization, and explicit semantic classification delivery.

Check late document attachment and the editable template control after building the extension:

```powershell
./tools/Rutile.VisualStudio/tests/RunClassifierLifetime.ps1
./tools/Rutile.VisualStudio/tests/RunTemplateArguments.ps1
```

These check function classification and its yellow default format, measured WPF layout,
pending responses, and stale snapshots. They do not inspect the installed editor's rendering.

The process checks exercise the actual stream implementation with an isolated child:
working directory, stderr drainage beyond pipe capacity, connection disposal, cancellation,
concurrent cleanup, and unexpected exit reporting.

The opt-in Visual Studio check uses a dedicated `RutileValidation<number>` profile:

```powershell
powershell.exe -NoProfile -STA -ExecutionPolicy Bypass `
    -File tools/Rutile.VisualStudio/tests/VerifyVisualStudio.ps1 `
    -Vsix tools/Rutile.VisualStudio/bin/Debug/Rutile.VisualStudio.vsix `
    -Install -Kiln build/debug/bin/kiln.exe
```

It verifies matching package versions before installation, starts a hidden test instance,
and closes that instance afterward. The normal Visual Studio profile is not used.
Logs and completed checks are saved under `build/vsix-tests/RutileValidation<number>`.
The default smoke checks activation and solution ownership changes, with an outer process deadline.
It does not invoke F12 or inspect the Error List. Hidden navigation blocked during verification;
external-change diagnostics were not observed reliably through hidden Error List automation.
Check navigation and external source/configuration diagnostics manually in an interactive editor.

Add `-Palette` to install a separate test-only MEF client into that isolated profile. It checks
aggregate editor classifications and effective foreground brushes, then inserts a newline to check
that cached semantic spans move immediately. The probe is excluded from the shipping VSIX.

Build and inspect the VSIX payload before installing. Protocol tests exercise the server
separately; a successful extension build does not establish language feature correctness.
An interactive Visual Studio test should open a `.rt` file and check completion, hover,
F12, signature help, and diagnostics, including edits and multiple open documents.

Version 0.0.67 was built. Its Release package payload matched the selected binaries.
The isolated Debug profile loaded the extension and started its packaged IDE-owned Kiln process.
It also passed solution close, replacement project startup, and replacement owner shutdown.
Editor navigation and watcher notifications remain interactive verification steps.
Process ownership checks passed.

The 0.0.70 palette probe could not complete in the hidden isolated host: solution automation
was rejected with `RPC_E_CALL_REJECTED`. Effective host brush and visual palette checks remain
unverified. The extension SDK build and lexical/semantic classifier tests passed.
