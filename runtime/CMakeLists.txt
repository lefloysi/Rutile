# Rutile runtime: the C/C++ implementation of the Rutile API.
#
# The runtime is split into:
#   backends/*      vulkan, dx12, gl33 backends
#   layers/*        validation, logging
#
# Shared shader/artifact support — the rtslp package reader, compiler, and
# linker — live under RTSL/ and are exposed by the single `rtsl` target.
# Backends link `rtsl` when they need to consume rtslp artifacts.
#
# All targets defined here link against the Rutile::rutile interface target
# from bindings/c. They must NOT depend on each other except through the
# rules documented in ARCHITECTURE.md.

add_subdirectory("backends")
add_subdirectory("layers")
