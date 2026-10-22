# Private bootstrap seed

`windows-x64/rutilec.exe` is the Windows x64 bootstrap seed. Kiln controls the
persistent Rutile compiler process when building projects.

`compiler.rt` is the seed's complete compiler source snapshot. It contains the
standard library prelude followed by every Rutile source file under
`compiler/src` and `language`, recursively sorted by path. There are no source manifests.
The snapshot uses UTF-8 without a byte order mark and LF line endings.

`version.txt` records the release, and `source.sha256` identifies the snapshot.
Required runtime DLLs are included beside the private executable.

Build the compiler from the repository root:

```bash
bash tools/build.sh build release
```

The result is `build/release/native/rutilec.exe`. The native C++ toolchain and installed
dependencies are required. `RUTILE_COMPILER` may select the bootstrap compiler explicitly.

Refresh the distributed seed and its metadata together after verifying the new compiler.
The combined snapshot must reproduce the same object as the original source files before
publishing it beside the compiler binary.
