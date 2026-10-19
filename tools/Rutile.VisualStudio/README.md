# Rutile for Visual Studio 2022

Rutile language support installs separately from the generic Kiln extension. Install
`Kiln.VisualStudio.vsix` first, then `Rutile.VisualStudio.vsix`.

The Rutile extension imports Kiln's workspace service through MEF. Kiln supplies the active
folder or solution, workspace change notifications, and the installed host executable. Rutile
owns its language-server connection and editor subscriptions. Changing the workspace restarts
that connection; closing it releases its process. The native language server currently runs
through `kiln.exe language-server --stdio` supplied by the Kiln extension.

Kiln owns build, run, debug, profile and target selection, and C++ workspace configuration.
The Rutile package supplies `.rt` content types, diagnostics, completion, hover, navigation,
signature help, semantic colors, and editable template argument contexts. Grammar and binding
remain in the compiler. Rename is not advertised by the server.

Comments, strings and numbers receive immediate lexical colors. Compiler classifications supply
user types, aliases, functions and contextual syntax. Cached semantic spans move with unchanged
text during analysis. Customize the Rutile classifications in Fonts and Colors. Language-server
errors appear in the Rutile Output pane.

## Build and install

Install Visual Studio 2022 with the extension development workload, then build each package:

```powershell
../Kiln/tools/Kiln.VisualStudio/Package.ps1 -Configuration Release
./tools/Rutile.VisualStudio/Package.ps1 -Configuration Release
```

The first script requires the bootstrapped Kiln executable and native runtime. The Rutile script
builds the managed language extension and verifies its separate manifest, dependency and payload.
It does not bundle Kiln or duplicate its native runtime. Rutile's package and assembly versions
come from `version/release.txt`.

Release packaging copies each verified VSIX into its project's `distro/windows-x64` folder.
Install Kiln's `Kiln.VisualStudio.vsix`, then Rutile's `Rutile.VisualStudio.vsix`. Open a folder or
solution, then a `.rt` document. Build and F5 continue to use the Kiln extension's selection.
Use the template context pencil to enter sample arguments; trailing defaulted arguments may
remain empty. Apply resolves the samples through the compiler without changing build diagnostics.

## Verification

```powershell
./tools/Rutile.VisualStudio/tests/Run.ps1
./tools/Rutile.VisualStudio/tests/RunHighlighting.ps1
./tools/Rutile.VisualStudio/tests/RunClassifierLifetime.ps1
./tools/Rutile.VisualStudio/tests/RunTemplateArguments.ps1
```

These checks cover process ownership, lexical and semantic classifications, late document
attachment, server-ready callback ordering, and editable template controls. Build and workspace
checks live with `Kiln/tools/Kiln.VisualStudio/tests`.

The opt-in Visual Studio host check accepts both separately built packages:

```powershell
powershell.exe -NoProfile -STA -ExecutionPolicy Bypass `
    -File tools/Rutile.VisualStudio/tests/VerifyVisualStudio.ps1 `
    -Vsix tools/Rutile.VisualStudio/bin/Release/Rutile.VisualStudio.vsix `
    -KilnVsix ../Kiln/tools/Kiln.VisualStudio/bin/Release/Kiln.VisualStudio.vsix -Install
```

It uses an isolated `RutileValidation<number>` profile. Logs are written under `build/vsix-tests`.
SDK builds and standalone tests do not establish interactive editor behavior; verify completion,
hover, F12, diagnostics and template controls in the installed editor as well.
