# Bootstrap compiler

The Windows x64 compiler in `windows-x64/compiler.exe` is built from
`compiler/src/compiler.rt`. Its runtime DLLs are included beside it.
`version.txt` identifies the release; `source.sha256` identifies its source.

CMake uses this compiler by default. Building the project does not require the original C compiler.
The existing LLVM SDK, C++ toolchain, and vcpkg dependencies are still required.

After a verified compiler change, refresh the distribution and release archive with:

```powershell
cmake --build --preset release --target rutile-distro
```

This first verifies self-hosting, then refreshes this folder and writes
`build/release/distro/rutile-VERSION-windows-x64.zip`.
Commit the refreshed distribution alongside its source and version change.

For another native host, set `RUTILE_BOOTSTRAP_COMPILER` to a compatible compiler executable.
`RUTILE_BOOTSTRAP_FROM_C=ON` enables the historical C seed as an explicit recovery option.
