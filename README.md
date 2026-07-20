# Rutile

Rutile is a user-space graphics API built around portability and ease of use.
Backends are explicitly loaded DLLs to support various features such as layers and extensions.
The core Rutile API is quite small, supporting only things that *every* api is expected to support.
Rutile currently supports Windows only.

At this point only C bindings exist. Other languages are theoretically supported by the architecture,
just not implemented at this point.

## Build

Install:

- Visual Studio 2022 with the **Desktop development with C++** workload
- CMake 3.28 or newer
- vcpkg, either from Visual Studio or from a standalone checkout with `VCPKG_ROOT` set
- Vulkan SDK, only if you build the Vulkan backend

The easiest Windows build uses the repository script. It enters the MSVC x64
developer environment, uses vcpkg manifest mode, and defaults to the static
`x64-windows-static` triplet:

```bat
scripts\build.bat Debug
```

For command-line CMake with a standalone vcpkg checkout:

```bat
cmake --preset windows-debug
cmake --build --preset windows-debug
ctest --preset windows-debug
```

The vcpkg manifest is split into features so optional dependencies stay behind
the targets that use them:

- `vulkan`: `rt-vk13`, VMA, volk, Vulkan headers, SPIR-V headers
- `gl33`: `rt-gl33` support dependencies
- `examples`: GLFW, GLM, CLI11, stb
- `tests`: Catch2 and test CLI support

For example, a headers/layers-only configure can use `windows-core`, while a
Vulkan examples configure can use `windows-vulkan-examples`.

## Run Examples

Build the examples and run them against the Vulkan backend:

```bat
scripts\test-examples.bat Debug out\build\examples rt-vk13
```

Run the built examples manually from the build output directory:

```bat
out\build\examples\bin\rutile-01-triangle.exe --backend rt-vk13
out\build\examples\bin\rutile-05-voxel-renderer.exe --backend rt-vk13 --frames 300
```

If you use a multi-config generator, the executables are under
`out\build\examples\bin\Debug` instead.

`scripts\test.bat Debug` configures, builds, and runs the test tree with CTest.
It returns an error if configuration fails, compilation fails, a test fails, or
no tests are registered.

Here is a screenshot from the voxel renderer.
<p align="center">
  <img src="examples/05-voxel-renderer/image.png" alt="Rutile Minecraft example" width="720">
</p>

## Project Shape

- `bindings/c/include/rutile.h` is the public loader and core API.
- `bindings/c/include/rt_ext_*.h` files are optional extension packages.
- `rt-vk13` is the Vulkan backend.
- `rt-dx12` is the DirectX 12 backend (Windows only).
- `rt-validation-layer` is a validation layer.
- `rt-logging-layer` is a logging layer.
- `examples` is a collection of small projects that show how to use Rutile. All examples use RTSL shaders and can be built and run with `scripts\test-examples.bat`.

## Loading

Applications load a backend by name, optionally with layers:

```c
const char* layers[] = {
    "RT_VALIDATION_LAYER",
    "RT_LOGGING_LAYER",
};
if (rtLoad("rt-vk13", layers, 2) != RT_SUCCESS) {
    /* handle load failure */
}
```

After `rtLoad`, extension headers can resolve their own procs:

```c
if (!rtLoad_RT_EXT_SWAPCHAIN()) {
    /* the loaded backend/layer chain does not support the swapchain extension */
}

if (!rtLoad_RT_EXT_GLFW()) {
    /* the loaded backend/layer chain does not support the GLFW extension */
}
```

For ad-hoc feature checks, ask the loaded chain for a proc:

```c
if (rtGetProc("rtMyFeatureDoThing")) {
    /* the loaded chain exposes this feature */
}
```

## Extensions

Rutile extensions are functions exported by a backend that are not part of the core.
These features should come with headers declaring what functions are exposed and what
to load.

These headers contain things like:
- public extension types
- proc typedefs
- private proc slots
- inline API wrappers
- a function (like `rtLoad_EXT_SWAPCHAIN`) that resolves names through `rtGetProc`


The application includes the extension header and loads it after the backend:

```c
#include "rt_ext_my_feature.h"

rtLoad("rt-vk13", layers, layer_count);

if (rtLoad_RT_EXT_MY_FEATURE()) {
    rtMyFeatureDoThing(...);
}
```

This is one of Rutile's core philosophies: features should be able to be screwed onto forked backends 
with ease.

## Layers
Layers are a feature that enable easy cross platform validation, logging, profiling or other features
enabled by intercepting calls. For example the official validation layer tracks object lifetimes and
errors when things remain alive at termination.
