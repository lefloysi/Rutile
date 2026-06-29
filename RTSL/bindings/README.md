# RTSL language bindings

Each subdirectory is a binding of the RTSL public API to one host language.

| Directory | Language | Build system | Status |
| --------- | -------- | ------------ | ------ |
| `c/`      | C99      | CMake        | Header today; library binding planned |
| `zig/`    | Zig      | `build.zig`  | Planned |
| `rust/`   | Rust     | `cargo`      | Planned |

RTSL's primary user is `rtslc` (the compiler CLI). The C binding exists so
embedding apps can drive compilation from C/C++ host code without depending
on the internal C++ headers. For v0.1, this binding is the supported host API;
other language bindings remain planned work.

CMake is used only here and at the repository root for the compiler
library and CLI; everything else is orchestrated from
`scripts/build.{sh,bat}`.
