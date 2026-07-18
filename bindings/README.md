# Rutile language bindings

Each subdirectory is a binding of the Rutile API to one host language. Every
binding is built with its native toolchain.

| Directory | Language | Build system | Status     |
| --------- | -------- | ------------ | ---------- |
| `c/`      | C99      | CMake        | Source of truth (defines the ABI) |
| `zig/`    | Zig      | `build.zig`  | Planned    |
| `rust/`   | Rust     | `cargo`      | Planned    |

The C binding is the source of truth. `rutile.h` defines the ABI; the runtime
in `runtime/` implements it; every other binding wraps `rutile.h` either by
hand-translation or by FFI.

CMake is intentionally used only here and in `runtime/` — the Windows build is
orchestrated from `scripts/build.bat`.
