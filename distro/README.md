# Private bootstrap seed

`windows-x64/rutilec.exe` is the verified Windows x64 bootstrap compiler. Its matching
standard-library interface and native library are in `windows-x64/share/rutile/lib`.
Keep these files together: compiler versions and interface versions must agree.
Required runtime DLLs remain beside the executable.

`compiler.rt` contains all Rutile sources under `compiler/src`, recursively sorted by
path in the same order used by Kiln. The standard library lives in `std` and is compiled
separately. The UTF-8 snapshot preserves each source file's bytes, with a newline between files.
`version.txt` records the compiler version, and `source.sha256` identifies the snapshot.

Build the compiler and run its tests from the repository root with Kiln:

```text
kiln build rutilec --profile debug
kiln test compiler --profile debug
kiln test std --profile debug
```

Kiln discovers an installed Rutile compiler through PATH or the installation directory.
For a bootstrap build, its standard `RUTILE_COMPILER` override can select the absolute path
of `distro/windows-x64/rutilec.exe`. Kiln discovers the matching library directory beside
that compiler. Test targets receive their compiler and module paths from Kiln automatically.
The native C++ toolchain and the project's declared dependencies are also required.

Refresh the seed, its standard-library pair, the source snapshot, and metadata together
after verifying the compiler. Before replacing the seed, compile the combined snapshot
and the exact separate input files using the same compiler, standard-library interface,
module identity, and optimization level. Their native objects must be byte-identical.
