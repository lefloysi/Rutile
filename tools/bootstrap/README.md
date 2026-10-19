# Building with Kiln

Kiln owns this project's build. It keeps a project session running and invokes its
Rutile and C++ integrations when targets need rebuilding. The public executable is
`kiln.exe`; the distributed compiler under `distro/` is only a bootstrap seed.

On Windows x64, install Visual Studio's Desktop development with C++ workload and
provide the dependencies from `vcpkg.json`. Then run from the repository root:

```powershell
./tools/bootstrap/bootstrap.ps1
./build/bootstrap/bin/kiln.exe build kiln.exe
```

The bootstrap discovers Visual Studio and an existing `x64-windows` dependency
installation. Set `VCPKG_ROOT` to let it install missing dependencies, or pass
`-Dependencies <installed-triplet-directory>`. The LLVM C API DLL defaults to the
bundled version; `-Llvm <dll-or-install-directory>` selects another installation.
`-PrepareOnly` prepares toolchain settings and dependencies without rebuilding Kiln.

The initial bootstrap compiles the private seed once, then compiles the native host.
Subsequent builds call `fn main(kiln::Options& options)` in the root `kiln.rt`.
Profiles are declared with `kiln::profile`; `kiln::add_subdirectory` loads another
folder's `kiln.rt` into the same project. Source folders include nested files.
The language object target builds the compiler and language service. The C++ target depends on
that object. A normal rebuild therefore includes changes to both languages.

```powershell
./build/bootstrap/bin/kiln.exe list
./build/bootstrap/bin/kiln.exe status
./build/bootstrap/bin/kiln.exe build kiln.exe --profile release
./build/bootstrap/bin/kiln.exe run kiln-tests.exe
./build/bootstrap/bin/kiln.exe exit
```

Generated headers, copied dependencies, bootstrap objects, and executables stay in
`build/bootstrap`. Target artifacts are published under `.kiln` by the running session.
Visual Studio folder integration uses the same session and project declarations.
