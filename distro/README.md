# Private bootstrap seed

`windows-x64/rutilec.exe` is the Windows x64 bootstrap seed. Kiln controls the
persistent Rutile compiler process when building projects.

`compiler.rt` is the seed's compiler source snapshot. It contains every Rutile
source file under `compiler/src` and `language`, recursively sorted by path.
The standard library is compiled separately and is not embedded in this snapshot.
Compiling the snapshot imports the matching shipped `std.rti`; native linking uses
its companion `std.lib`. There are no source manifests. The snapshot uses UTF-8
without a byte order mark and LF line endings.

`version.txt` records the release, and `source.sha256` identifies the snapshot.
Required runtime DLLs are included beside the private executable.

`windows-x64/share/rutile/lib/std.lib` and `std.rti` are the precompiled standard
library and its interface for this compiler version. Consumer targets import
`std` explicitly; they do not load the standard-library source directory.
Installed payloads place the compiler in `bin/` and the library pair in
`share/rutile/lib/` under the installation prefix.

Build the standard-library pair with:

```powershell
./tools/package-stdlib.ps1 -Compiler ./build/release/native/rutilec.exe
```

Build the compiler from the repository root:

```bash
bash tools/build.sh build release
```

The result is `build/release/native/rutilec.exe`. The native C++ toolchain and installed
dependencies are required. `RUTILE_COMPILER` may select the bootstrap compiler explicitly.

Refresh the distributed seed and its metadata together after verifying the new compiler.
The combined snapshot must reproduce the same object as the original source files,
using the same standard-library interface, module identity, and compiler options,
before publishing it beside the compiler binary and standard-library pair.
