# Private bootstrap seed

`windows-x64/compiler.exe` is the private Windows x64 bootstrap seed. Kiln embeds
its Rutile compiler; users build and run projects through `kiln.exe`.

`compiler.rt` is the seed's complete compiler source snapshot. It contains the
standard library prelude followed by every Rutile source file under
`kiln/compiler/src`, recursively sorted by path. There are no source manifests.
The snapshot uses UTF-8 without a byte order mark and LF line endings.

`version.txt` records the release, and `source.sha256` identifies the snapshot.
Required runtime DLLs are included beside the private executable.

After bootstrapping the updated Kiln host, refresh the seed from the repository root:

```powershell
./tools/bootstrap/refresh-seed.ps1
```

The refresh compiles current sources with Kiln, links the private driver, and verifies
that the new seed reproduces identical objects from both the original sources and
the combined snapshot. It publishes the seed and metadata only after both checks pass.
The native C++ toolchain and the dependencies in `vcpkg.json` are required.
