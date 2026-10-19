# Rutile

Rutile and its standard library live here. The build system lives in the sibling `Kiln`
folder. Keep the projects beside one another:

```text
Projects/
  Kiln/
  Rutile/
```

On Windows x64, install Visual Studio's Desktop development with C++ workload and prepare
Kiln once:

```powershell
./tools/bootstrap/bootstrap.ps1
../Kiln/build/bootstrap/bin/kiln.exe build compiler.exe
```

The forwarding bootstrap uses the distributed compiler in `distro/` to build Kiln.
The root `kiln.rt` declares the Rutile compiler object and native compiler driver.
It includes this checkout's standard library when rebuilding the compiler.
Kiln discovers their sources recursively and builds their dependencies first.
Use `--profile release` for an optimized build. Build outputs are under `.kiln/build`.

`compiler/src` contains the compiler; `compiler/std` contains the standard library.
`language` contains the Rutile language service. Kiln's Rutile integration builds and
connects that service. See [Kiln](../Kiln/README.md) for build operations and extension points.

After changing the compiler, refresh the distributed seed only after staged verification:

```powershell
./tools/bootstrap/refresh-seed.ps1
```

That script verifies matching native objects from the source files, the newly built compiler,
and its complete source snapshot before publishing the seed.
