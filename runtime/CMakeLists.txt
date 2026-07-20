# Rutile runtime: the C/C++ implementation of the Rutile API.
#
# The runtime is split into:
#   backends/*      vulkan, directx12, opengl backends
#   layers/*        validation, logging
#
# Runtime shader/artifact support lives behind RTSL::sdk and target-specific
# transpilers such as RTSL::spirv. Backends never link the compiler target.
#
# All targets defined here link against the Rutile::rutile interface target
# from bindings/c. They must NOT depend on each other except through the
# rules documented in ARCHITECTURE.md.

add_subdirectory("backends")
add_subdirectory("layers")
