# Rutile Shading Language (RTSL)

Rutile Shading Language is a shader language and compiler for the Rutile
graphics API.

RTSL keeps a shader contract in one source file: resources, stage payloads,
helper types, and entry points live together in a C-style language that lowers
to backend-neutral Rutile shader IR.

## Documentation

The specification is split by concern:

- [Language semantics](docs/language.md)
- [Compiler architecture](docs/compiler-architecture.md)
- [Artifact formats](docs/artifacts.md)
- [RTIR](docs/rtir.md)
- [Linking](docs/linking.md)
- [Backend contract](docs/backend-contract.md)

## Examples

The `workspace` directory contains sample shader files:

- `default.rtsl`
- `graphics.rtsl`

For the v0.1 release, only `graphics.rtsl` is a supported end-to-end sample.
The other files are exploratory or placeholder material and are not part of the
release promise.

## Current Status

RTSL v0.1 is a graphics-only release. It supports compilation to `rtslo`,
optional `rtslm` interfaces, linking to `rtsll` / `rtslp`, reflection for
uniforms and stage interfaces, and the current vertex/fragment backend path.

Editor tooling scaffolding lives under `tools/vs-rtsl-ext/`.

## CMake Integration

Downstream CMake projects can opt into shader compilation with the RTSL CMake
module:

```cmake
list(APPEND CMAKE_MODULE_PATH "/path/to/RTSL/cmake")
include(Rtsl)

rtsl_add_program(my_game_rtsl
    RTSLC rtslc
    SOURCES
        "${CMAKE_CURRENT_SOURCE_DIR}/shaders/world.rtsl"
        "${CMAKE_CURRENT_SOURCE_DIR}/shaders/ui.rtsl"
    EMBED
)
```

The helper:

- recompiles when any source changes
- relinks to fresh `rtslp` outputs
- can generate a C++ translation unit that embeds the `rtslp` bytes into the
  final executable
- keeps the build output visible to the normal compiler/IDE pipeline so RTSL
  failures show up as build errors

