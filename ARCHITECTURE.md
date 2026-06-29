# Rutile architecture

Rutile is a graphics stack split along three axes that stay decoupled:

1. **API** — the C ABI defined in `bindings/c/include/rutile.h`. This is the
   contract every backend implements and every host language binds to.
2. **Runtime** — the C/C++ implementation of that API: loader-like dispatch,
   the backends (Vulkan, DX12, GL33), and optional layers (validation,
   logging).
3. **Shader IR (RTIR)** — the portable shader representation backends
   consume. RTIR is part of the Rutile contract; RTSL is one frontend that
   produces it.

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
|  layers/      backend-tools/      backends/                     |
|  validation,  shader translation  vulkan, dx12, gl33            |
|  logging      (glslang + spirv-cross)                           |
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
    backend-tools/      # shared shader translation (glslang + spirv-cross)
    backends/
      vk13/             # target: rt-vk13 -> Rutile::rt-vk13
      dx12/             # target: rt-dx12   -> Rutile::rt-dx12
      gl33/             # target: rt-gl33   -> Rutile::rt-gl33
    layers/
      validation/       # target: rt-validation
      logging/          # target: rt-logging-layer

  examples/             # sample apps that link the C binding + runtime
  spec/                 # SPEC.html, SHADERS.md, RTIR contract docs
  scripts/              # build.sh, build.bat, test.sh, test.bat
  cmake/                # RutileTarget.cmake, RutileConfig.cmake.in
```

## Build strategy

CMake is intentionally scoped to the C/C++ subtree. Everything outside that
subtree is driven by `scripts/build.sh` and `scripts/build.bat`:

- `scripts/build.sh` — POSIX entry point; invokes CMake on the runtime and
  C binding, and will later invoke `zig build` / `cargo build` for the
  other language bindings.
- `scripts/build.bat` — Windows equivalent.
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
runtime/backends/*      ->  Rutile::rutile, Rutile::backend-tools
runtime/layers/*        ->  Rutile::rutile
runtime/backend-tools   ->  Rutile::rutile
bindings/c              ->  (none — header-only interface target)
examples/*              ->  Rutile::rutile, Rutile::rt-*

Forbidden
---------
bindings/* depending on runtime/*
runtime/* depending on each other (except via backend-tools)
runtime/* depending on RTSL or any compiler
```

The last rule is the load-bearing one: a Rutile backend is a runtime
component, not a compiler. It consumes RTIR; it never depends on the RTSL
frontend, parser, or linker. RTSL now lives in-tree under `Rutile/RTSL/`.

## RTIR and the Rutile/RTSL split

RTIR is the portable shader representation. Long-term, the IR data model,
reader/writer, and verifier should live in Rutile (likely under
`runtime/ir/`) because it is part of the Rutile contract — RTSL produces
it; backends consume it. Today, `runtime/backend-tools/` is a stopgap that
performs GLSL → HLSL / SPIR-V translation via glslang and SPIRV-Cross.

When the RTIR library lands, the migration path is:

1. Add `runtime/ir/` defining the IR model, package reader/writer,
   verifier, and reflection.
2. Backends gain an RTIR lowering pass alongside the existing glslang path.
3. `backend-tools/` shrinks to whatever shared utilities remain (or
   disappears).

Until then, treat `backend-tools/` as the placeholder for shared shader
infrastructure.

## Future bindings

When adding `bindings/zig/` or `bindings/rust/`:

- The binding wraps the C ABI defined in `bindings/c/include/rutile.h`. It
  does not reinvent the ABI.
- Its build system is native to the language. No CMake.
- Its build is invoked from `scripts/build.{sh,bat}` as a separate phase
  after the C/C++ build.
- The README at `bindings/README.md` tracks the binding inventory.
