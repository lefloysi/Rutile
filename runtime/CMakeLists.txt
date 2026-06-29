# Rutile runtime: the C/C++ implementation of the Rutile API.
#
# The runtime is split into:
#   backends/*      vulkan, dx12, gl33 backends
#   layers/*        validation, logging
#
# Shared backend helpers — notably the rtslp package reader (rtsl-package) —
# live under RTSL/, since they are produced by RTSL and consumed by both
# runtimes and external shader tools.
#
# All targets defined here link against the Rutile::rutile interface target
# from bindings/c. They must NOT depend on each other except through the
# rules documented in ARCHITECTURE.md.

add_subdirectory("backends")
add_subdirectory("layers")
