# Rutile for Visual Studio 2022

The extension registers `.rt` files and connects Visual Studio's built-in language client to
`kiln.exe language-server --stdio`. Kiln starts in the open folder workspace or solution directory
supplied by Visual Studio. The client does not parse Rutile.

The Rutile server port is in development. Diagnostics and the compiler navigation model are
being integrated. Completion, signature help, and semantic-token support remain pending.
Hover, F12, and references must also pass protocol tests before they are considered available.

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

## Verification

Build and inspect the VSIX payload before installing. Protocol tests exercise the server
separately; a successful extension build does not establish language feature correctness.
An interactive Visual Studio test should open a `.rt` file and check completion, hover,
F12, signature help, and diagnostics, including edits and multiple open documents.
