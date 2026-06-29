# RTSL architecture

RTSL compiles source files into backend-neutral binary artifacts that the
[Rutile](https://www.github.com/lefloys/Rutile) runtime consumes:

```
.rtsl  source        --[ compiler ]--> .rtslo  object
                                        .rtslm  module/interface (optional)
.rtslo + .rtsll      --[ linker  ]--> .rtsll  library
                                        .rtslp  final program (consumed by Rutile backends)
```

`rtslc` is the CLI driver wrapping the library.

## Repository layout

```
RTSL/
  bindings/c/             # public C ABI surface (header today, lib later)
    include/rtsl.h
    CMakeLists.txt
  src/                    # clang-style internal layout
    Basic/                # diagnostics, source manager, types
    Lex/                  # lexer, tokens
    AST/                  # AST node types (header-only today)
    Parse/                # parser
    Sema/                 # semantic analysis
    IR/                   # RTIR data model + lowering passes
    Mangle/               # name mangling
    Serialization/        # rtslo/rtsll/rtslm/rtslp + text RTIR
    Compiler/             # orchestrates source -> rtslo
    Link/                 # rtslo + rtsll -> rtslp
    Backend/              # compiler-side backends (currently GlslBackend)
    Driver/               # rtslc CLI entry
    rtsl.cpp              # public C ABI implementation
  tests/                  # rtsl-tests + workspace shaders
  cmake/                  # RunGlslE2E.cmake helper
  docs/                   # language, RTIR, artifacts, linking specs
  scripts/                # build.{sh,bat}, test.{sh,bat}
  tools/                  # vs-rtsl-ext, etc.
  workspace/              # scratch shaders for development
```

## CMake target split

The monolithic `rtsl` static library has been split into focused sub-libs.
Source files keep their existing locations under `src/`; only the build
graph is split. Each library declares its dependencies explicitly so
boundary violations cause link errors rather than silent regressions.

```
rtsl-frontend       Basic + Lex + Parse + Sema       no internal deps
rtsl-ir             IR + Mangle                       -> rtsl-frontend  (see note)
rtsl-serialization  Serialization                     -> rtsl-ir
rtsl-linker         Link                              -> rtsl-serialization
rtsl-backend-glsl   Backend/GlslBackend               -> rtsl-serialization
rtsl-compiler       Compiler                          -> rtsl-frontend, rtsl-serialization
rtsl                rtsl.cpp (public ABI impl)        -> everything above
rtslc               Driver/rtslc.cpp                  -> rtsl
```

### Why `rtsl-ir` depends on `rtsl-frontend` today

The IR currently includes `AST/AST.h` and `Sema/Sema.h` directly, so it
cannot be a "pure" IR library yet. Long-term we want the IR to stand alone
so the in-memory model and the rtslp reader/writer can be linked by Rutile
backends and tools without dragging in the parser. The refactor target:

1. Move IR-relevant types out of `AST/Sema` into `IR/` (or split AST into
   `AST-syntax` vs `AST-ir-bridge`).
2. Strip `#include "AST/AST.h"` and `#include "Sema/Sema.h"` from
   `IR/*.h`.
3. Drop `target_link_libraries(rtsl-ir PUBLIC rtsl-frontend)`.

Once that lands, `rtsl-ir` + `rtsl-serialization` form a clean contract
that the Rutile repo can link against without pulling in the RTSL frontend.

## Boundary rules

- Backends in Rutile must **never** depend on `rtsl-compiler`,
  `rtsl-frontend`, or `rtsl-linker`. They only consume `.rtslp` packages.
- `rtsl-ir` and `rtsl-serialization` are the only RTSL libs that should
  ever be linked by code outside this repo.
- Nothing in `bindings/` may include from `src/`.

## Build strategy

CMake stays scoped to the C/C++ library + CLI. Top-level orchestration
lives in `scripts/`:

- `scripts/build.sh` / `build.bat` — configures and builds via CMake.
- `scripts/test.sh` / `test.bat` — runs ctest (`rtsl-tests` and the
  glslangValidator end-to-end check).

Future language bindings (`bindings/zig/`, `bindings/rust/`, ...) bring
their own build systems and slot into `scripts/build.{sh,bat}` as
additional phases.

## Related specs

- [docs/compiler-architecture.md](docs/compiler-architecture.md)
- [docs/language.md](docs/language.md)
- [docs/artifacts.md](docs/artifacts.md)
- [docs/rtir.md](docs/rtir.md)
- [docs/linking.md](docs/linking.md)
- [docs/backend-contract.md](docs/backend-contract.md)
