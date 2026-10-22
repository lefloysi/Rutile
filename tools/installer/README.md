# Command-line installers

Build from Git Bash with a prepared payload directory:

```sh
tools/installer/build.sh --name Kiln --version 0.0.95 \
    --payload build/package-kiln --output distro/windows-x64/KilnSetup.exe
tools/installer/build.sh --name Rutile --version 0.0.95 \
    --payload build/package-rutile --output distro/windows-x64/RutileSetup.exe
```

`CSC` may specify a Windows C# compiler. Otherwise the builder finds the Visual Studio 2022
Roslyn compiler or the installed .NET Framework compiler. The builder needs Git Bash, `cygpath`,
and .NET Framework with its compression assemblies. It does not use PowerShell or Python.

Each payload contains its own executable and runtime DLLs in `bin/`. Kiln contains
`share/kiln/api.rt` and `share/kiln/std/`. Rutile contains `share/rutile/std/`.
The builder rejects a payload containing both command-line tools.

Run from a command prompt:

```text
KilnSetup.exe install --user
RutileSetup.exe install --user
KilnSetup.exe install --machine
RutileSetup.exe install --user --prefix D:\Tools\Rutile
RutileSetup.exe uninstall --user --prefix D:\Tools\Rutile
```

`--user` is the default scope. Its default directory is `%LOCALAPPDATA%/Programs/NAME`.
`--machine` uses `%ProgramFiles%/NAME` and requires an administrator command prompt.
Each installer updates the matching persistent PATH and Windows uninstall registration.
Restart terminals and editors after installation to receive the new environment.

The installed `uninstall.exe` accepts the same uninstall arguments and finds its own directory.
Uninstall removes only files recorded in the installation manifest whose hashes still match.
Modified and unrelated files remain. A running uninstaller remains until deleted after it exits.
Upgrades refuse to overwrite files that do not belong to the preceding installation or have changed.
