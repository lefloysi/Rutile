cmake_minimum_required(VERSION 3.28.0)

# The C language binding for Rutile.
#
# This file defines the public, header-only `rutile` interface target that
# downstream C and C++ consumers depend on. Sibling directories under
# `bindings/` host bindings for other languages (Zig, Rust, ...). Each
# binding is built with its native toolchain; only this one is CMake-driven.

add_library(rutile INTERFACE)
add_library(Rutile::rutile ALIAS rutile)

target_include_directories(rutile
    INTERFACE
        "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>"
        "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>"
)

if(RUTILE_INSTALL)
    install(DIRECTORY include/ DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}")
    install(TARGETS rutile EXPORT RutileTargets)
endif()
