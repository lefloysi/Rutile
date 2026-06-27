# Rutile runtime: the C/C++ implementation of the Rutile API.
#
# The runtime is split into:
#   backend-tools   shared shader translation helper (glslang + spirv-cross)
#   backends/*      vulkan, dx12, gl33 backends
#   layers/*        validation, logging
#
# All targets defined here link against the Rutile::rutile interface target
# from bindings/c. They must NOT depend on each other except through the
# rules documented in ARCHITECTURE.md.

add_subdirectory("backend-tools")
add_subdirectory("backends")
add_subdirectory("layers")
