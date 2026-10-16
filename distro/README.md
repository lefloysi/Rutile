# Bootstrap compiler

The Windows x64 compiler in `windows-x64/compiler.exe` is built from the included
`compiler.rt` snapshot of `compiler/src/old.rt`. Its runtime DLLs are included beside it.
`version.txt` identifies the release; `source.sha256` identifies the included source.

CMake uses this compiler by default. The compiler implementation is written entirely in Rutile.
The existing LLVM SDK, C++ toolchain, and vcpkg dependencies are still required.
The single C++ driver in `compiler/src/main.cpp` supplies the command-line entry point.
Directory inputs concatenate every regular file recursively in sorted relative-path order,
preserving the bytes exactly without adding separators.

After a verified compiler change, refresh the distribution and release archive with:

```powershell
cmake --build --preset release --target rutile-distro
```

This first verifies self-hosting, then replaces the current compiler and source snapshot and writes
`build/release/distro/rutile-VERSION-windows-x64.zip`.
Commit the refreshed distribution alongside its source and version change.

For another native host, set `RUTILE_BOOTSTRAP_COMPILER` to a compatible compiler executable.

Add language support using syntax the current Rutile compiler already accepts, then compile that
compiler before adopting the new features in its source. Use `RUTILE_BOOTSTRAP_COMPILER` to select
the intermediate executable until the refreshed distribution is ready.
