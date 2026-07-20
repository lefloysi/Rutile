# Building Rutile On Linux

Verified on 2026-07-20 in a clean Ubuntu 24.04 environment.

## Install System Packages

```bash
sudo apt update
sudo apt install -y \
  ca-certificates curl git zip unzip tar pkg-config \
  build-essential ninja-build cmake
```

## Build And Test

From the Rutile repository:

```bash
scripts/build-linux.sh
```

The script uses `~/.cache/rutile/vcpkg` by default. If vcpkg is not there yet,
it clones and bootstraps it before running CMake. Keep vcpkg on the Linux
filesystem; a full vcpkg checkout on `/mnt/c` is much slower because it contains
many small files.

The script configures with `cmake --fresh`, so stale build directories from
Docker or another checkout path are regenerated automatically.

To use an existing vcpkg checkout instead:

```bash
VCPKG_ROOT="$HOME/vcpkg" scripts/build-linux.sh
```

The verified result is:

```text
100% tests passed, 0 tests failed out of 6
```

## Manual vcpkg Setup

Use a normal clone, not a shallow clone. Rutile pins a vcpkg
`builtin-baseline`, so vcpkg needs enough git history to resolve that commit.

```bash
git clone https://github.com/microsoft/vcpkg ~/vcpkg
~/vcpkg/bootstrap-vcpkg.sh -disableMetrics
export VCPKG_ROOT="$HOME/vcpkg"
```

For a persistent shell setup, add this to `~/.bashrc`:

```bash
export VCPKG_ROOT="$HOME/vcpkg"
```

Then build manually:

```bash
cmake --preset linux-ci
cmake --build --preset linux-ci
ctest --preset linux-ci
```

## What This Builds

The `linux-ci` preset builds the currently portable Linux target set:

- RTSL library, CLI, SDK, and tests
- Rutile validation layer
- Rutile logging layer

It intentionally disables examples and the graphics backends:

- `RUTILE_BUILD_EXAMPLES=OFF`
- `RUTILE_BUILD_OPENGL=OFF`
- `RUTILE_BUILD_VULKAN=OFF`
- `RUTILE_BUILD_DIRECTX12=OFF`

This keeps the Linux setup small and avoids pulling GLFW, Vulkan, and X11
development packages into the baseline compiler/test workflow.

## Notes From Docker Verification

Docker was only used to discover the clean Ubuntu dependency list. The
recommended user workflow is the normal CMake preset workflow above.

The important vcpkg detail is that `linux-ci` must set both:

```text
VCPKG_MANIFEST_FEATURES=tests
VCPKG_MANIFEST_NO_DEFAULT_FEATURES=ON
```

Without `VCPKG_MANIFEST_NO_DEFAULT_FEATURES=ON`, vcpkg also enables the default
features in `vcpkg.json`, which pulls in examples, OpenGL, Vulkan, GLFW, and
their system dependencies.

GCC currently reports format warnings in
`runtime/layers/rt-logging-layer/src/logger.c` because `u64` maps to
`unsigned long` on Linux while the format strings use `%llu` / `%llX`. These are
warnings only in the verified build.
