# Rutile architecture

Rutile is a graphics stack split along three axes that stay decoupled:

1. **API** — the C ABI defined in `bindings/c/include/rutile.h`. This is the
   contract every backend implements and every host language binds to.
2. **Runtime** — the C/C++ implementation of that API: loader-like dispatch,
   the backends (Vulkan, DX12, GL33), and optional layers (validation,
   logging).
3. **Shader programs** — RTSL compiles source into linked `.rtslp` artifacts.
   Runtime backends load the normalized IR through `RTSL::sdk` and lower it
   through a target transpiler such as `RTSL::spirv`.

```
+---------------------------------------------------------------+
|                       application                              |
+----------------+--------------+-------------------+------------+
|  bindings/c    |  bindings/zig|  bindings/rust    |    ...     |
+----------------+--------------+-------------------+------------+
                 |  same C ABI underneath
+----------------v-----------------------------------------------+
|                     runtime/ (C/C++)                            |
|                                                                 |
|  layers/                         backends/                       |
|  validation, logging            vulkan, dx12, gl33              |
+-----------------------------------------------------------------+
                 ^
                 |  rtsl produces rtslp packages that backends consume
+----------------+----------------------------------------------+
|                    RTSL (in-tree)                              |
+----------------------------------------------------------------+
```

## Repository layout

```
rutile/
  bindings/             # language-side API surface
    c/                  # source of truth — defines the ABI
      include/rutile.h
      include/rt_ext_*.h
      CMakeLists.txt    # exposes Rutile::rutile interface target
    zig/                # planned
    rust/               # planned

  runtime/              # C/C++ implementation
    backends/
      rt-vk13/          # target: rt-vk13 -> Rutile::rt-vk13
      rt-dx12/          # target: rt-dx12 -> Rutile::rt-dx12
      rt-gl33/          # target: rt-gl33 -> Rutile::rt-gl33
    layers/
      rt-validation-layer/  # target: rt-validation-layer
      rt-logging-layer/     # target: rt-logging-layer

  examples/             # sample apps that link the C binding + runtime
  spec/                 # SPEC.html, the core API specification
  scripts/              # Windows build and test entry points
  cmake/                # RutileTarget.cmake, RutileConfig.cmake.in
```

## Build strategy

CMake is intentionally scoped to the C/C++ subtree. Windows development is
driven by two batch entry points:

- `scripts/build.bat` — configures dependencies and builds the repository.
- `scripts/test.bat` — configures a dedicated test tree, builds it, and runs
  every registered CTest test.
- `scripts/test-examples.bat` — configures and builds the example tree.
- `bindings/c/CMakeLists.txt` — importable on its own. Downstream C/C++
  consumers that only need headers can `add_subdirectory(rutile/bindings/c)`
  without pulling in the runtime.
- `CMakeLists.txt` (top-level) — a thin coordinator that adds the C binding
  and the runtime so a single configure works for in-tree development.

Each language binding under `bindings/` is built with its native toolchain
(`build.zig`, `Cargo.toml`, ...). They do not appear in CMake.

## Dependency rules

These are the only allowed dependencies. Anything else is a layering bug.

```
Allowed
-------
runtime/backends/*      ->  Rutile::rutile, runtime-facing RTSL SDK/transpiler
runtime/layers/*        ->  Rutile::rutile
bindings/c              ->  (none — header-only interface target)
examples/*              ->  Rutile::rutile, Rutile::rt-*

Forbidden
---------
bindings/* depending on runtime/*
runtime/* depending on each other
runtime/* depending on the RTSL compiler/frontend
```

The last rule is the load-bearing one: a Rutile backend is a runtime component,
not a compiler. It may depend on the immutable artifact SDK and a target
transpiler, but never on the RTSL frontend, parser, semantic analysis, linker,
or driver. RTSL lives in-tree under `Rutile/RTSL/`.

## RTIR and the Rutile/RTSL split

RTIR is the normalized representation stored in linked RTSL program artifacts.
`RTSL::sdk` owns artifact loading, validation, immutable IR access, and
reflection. Target transpilers live under `RTSL/transpilers/`; the Vulkan
backend links `RTSL::spirv`, which lowers vertex and fragment stages directly
to SPIR-V words. The DX12 backend links `RTSL::hlsl`, compiles the generated
HLSL to DXIL with DXC, and derives its root signature and input layout from
RTSL reflection. Neither runtime backend links the source-language compiler;
Vulkan has no intermediate GLSL path.

## Future bindings

When adding `bindings/zig/` or `bindings/rust/`:

- The binding wraps the C ABI defined in `bindings/c/include/rutile.h`. It
  does not reinvent the ABI.
- Its build system is native to the language. No CMake.
- Its build is invoked from `scripts/build.bat` as a separate phase
  after the C/C++ build.
- The README at `bindings/README.md` tracks the binding inventory.
