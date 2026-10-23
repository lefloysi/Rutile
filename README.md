# Rutile

Rutile and its standard library are built here. Kiln is a separate project; this
repository provides the Rutile language extension and compiler service.

## Build Rutilec

On Windows, use Git Bash with Visual Studio 2022 C++ build tools, LLVM, and the
existing CLI11 and nlohmann/json dependencies available. The script does not fetch
dependencies. Set `RUTILE_DEPENDENCIES` to an existing dependency root if discovery
does not find them, or set `LLVM_ROOT` and `VCPKG_ROOT` as needed.

```bash
bash tools/build.sh help
bash tools/build.sh prepare
bash tools/build.sh build debug
bash tools/build.sh build release
```

`prepare` writes bootstrap files and copies dependencies beneath `build/<profile>/`.
`build` compiles the Rutile compiler and its language service, then links
`build/<profile>/native/rutilec.exe`. Set `RUTILE_COMPILER` to choose the Rutilec bootstrap
compiler; otherwise the script uses `distro/windows-x64/`, with `PATH` as a fallback.
All generated objects, executables, dependencies, and logs belong under `build/`.

The root `kiln.rt` describes the Rutile build as a Rutile language object target and
a native compiler target. Kiln can use that script to rebuild Rutilec after changes.
The standalone Bash entry point prepares the native Visual Studio toolchain and
bootstraps the same compiler sources.

## Visual Studio extension

The independent Rutile extension package is built with:

```bash
bash tools/Rutile.VisualStudio/Package.sh Release
```

The VSIX is copied to `distro/windows-x64/Rutile.VisualStudio.vsix`. It supplies
editor support and connects to the existing Rutilec service; Kiln owns project
configuration and starts that service.

`compiler/src` contains the compiler, `compiler/std` its standard library, and
`language` its editor-language support. The `base` directory contains native runtime
and platform code.
