# C++ integration

Select `@target.compiler("cpp")` for a target. Source folders include `.cpp`, `.cc`, `.cxx`, and `.c`
files recursively. The initial toolchain is Visual Studio MSVC for Windows x64, including
the Windows SDK. Kiln discovers its tool paths without requiring a developer terminal.
Targets can produce executables, static `.lib` libraries, or shared `.dll` libraries.

Compiler options are available through Rutile `target.option` in the root `kiln.rt` program.
List values accept JSON string arrays or semicolon-separated strings. JSON preserves
semicolons inside an individual argument.

| Option | Meaning |
| --- | --- |
| `includes` | Include directories, relative to the project root or absolute. |
| `defines` | Preprocessor definitions, such as `FEATURE=1`. |
| `forced_includes` | Header files included before each source file. |
| `excludes` | Exact root-relative source paths or source filenames to omit. |
| `objects` | Additional compiled object files. |
| `libraries` | Library filenames or root-relative/absolute paths. |
| `library_paths` | Library search directories. |
| `compile_options` | Additional individual compiler arguments. |
| `link_options` | Additional individual linker or librarian arguments. |
| `runtime_files` | Files to copy alongside the produced executable or library. |
| `standard` | C++ standard, default `c++latest`. |
| `runtime` | MSVC runtime: `MD` (default), `MDd`, `MT`, or `MTd`. |
| `compiler` | Optional explicit compiler executable path. |
| `linker` | Optional explicit linker executable path. |
| `archiver` | Optional explicit librarian executable path. |

Target dependencies supply their built object or library artifacts to the linker.
Shared-library dependencies use the generated import library. DLL runtime deployment
can be specified with `runtime_files`.

Kiln keeps object files and compiler-reported include dependencies in the target output
directory. It recompiles when compiler arguments, source contents, or included header
contents change. A failed compilation never reuses the failed object as a successful
result. Linking runs after the objects are ready. Debug information is embedded in objects
and linked into a PDB beside the output. Each C++ target also writes a standard
`compile_commands.json` containing its actual compiler arguments.
