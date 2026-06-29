cmake_minimum_required(VERSION 3.20)

project(rtsl LANGUAGES C CXX)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(Rtsl)

if(DEFINED VCPKG_INSTALLED_DIR AND DEFINED VCPKG_TARGET_TRIPLET AND NOT VCPKG_TARGET_TRIPLET STREQUAL "")
    list(PREPEND CMAKE_PREFIX_PATH "${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}")
endif()

# RTSL is split into sub-libraries so each piece can be reasoned about and
# linked independently.
#
#   rtsl-core         -> basic types, diagnostics, source management, AST,
#                        semantics, IR, serialization helpers, mangling
#   rtsl-compiler     -> source -> artifact compilation
#   rtsl-linker       -> object/library -> program/library linking
#   rtsl-backends     -> SPIR-V / HLSL emission backends
#   rtsl              -> public C API façade over compiler/linker/core

set(_rtsl_src "${CMAKE_CURRENT_SOURCE_DIR}/src")

function(_rtsl_configure_lib target)
    target_compile_features(${target} PUBLIC cxx_std_23)
    set_target_properties(${target} PROPERTIES CXX_EXTENSIONS OFF)
    if(MSVC)
        set_target_properties(${target} PROPERTIES
            MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>"
        )
        target_compile_options(${target} PRIVATE /FS)
    endif()
    target_include_directories(${target}
        PUBLIC
            "${_rtsl_src}"
            "${CMAKE_CURRENT_SOURCE_DIR}/bindings/c/include"
    )
endfunction()

add_library(rtsl-core STATIC
    ${_rtsl_src}/Basic/Diagnostics.cpp
    ${_rtsl_src}/Basic/SourceManager.cpp
    ${_rtsl_src}/Lex/Lexer.cpp
    ${_rtsl_src}/Parse/Parser.cpp
    ${_rtsl_src}/Sema/Sema.cpp
    ${_rtsl_src}/IR/IR.cpp
    ${_rtsl_src}/IR/UniformLowering.cpp
    ${_rtsl_src}/IR/StageBuiltins.cpp
    ${_rtsl_src}/Mangle/Mangler.cpp
    ${_rtsl_src}/Serialization/Artifact.cpp
    ${_rtsl_src}/Serialization/TextRTIR.cpp
)
_rtsl_configure_lib(rtsl-core)

# Linker: combines compiled objects/libraries into rtsll or rtslp.
add_library(rtsl-linker STATIC ${_rtsl_src}/Link/Linker.cpp)
_rtsl_configure_lib(rtsl-linker)
target_link_libraries(rtsl-linker PUBLIC rtsl-core)

# rtslp package reader: the in-memory loader for RTSL's linker output.
# Backends and runtimes link against this rather than reaching for the on-disk
# format directly.
add_subdirectory(package)

# Backend emitters live under RTSL, not the runtime.
add_subdirectory(backends)

# Compiler orchestrator: drives source -> rtslo / rtslm.
add_library(rtsl-compiler STATIC ${_rtsl_src}/Compiler/Compiler.cpp)
_rtsl_configure_lib(rtsl-compiler)
target_link_libraries(rtsl-compiler PUBLIC rtsl-core)

# Aggregate library that re-exports everything via the public rtsl.h API.
add_library(rtsl STATIC ${_rtsl_src}/rtsl.cpp)
_rtsl_configure_lib(rtsl)
target_link_libraries(rtsl
    PUBLIC
        rtsl-compiler
        rtsl-linker
        rtsl-core
)

add_executable(rtslc ${_rtsl_src}/Driver/rtslc.cpp)
target_link_libraries(rtslc PRIVATE rtsl)
find_package(CLI11 CONFIG REQUIRED)
target_link_libraries(rtslc PRIVATE CLI11::CLI11)
target_compile_features(rtslc PRIVATE cxx_std_23)
set_target_properties(rtslc PROPERTIES CXX_EXTENSIONS OFF)

add_executable(rtsl-tests tests/rtsl_tests.cpp)
target_link_libraries(rtsl-tests PRIVATE rtsl)
target_compile_features(rtsl-tests PRIVATE cxx_std_23)
set_target_properties(rtsl-tests PROPERTIES CXX_EXTENSIONS OFF)

if(MSVC)
    set_target_properties(rtslc rtsl-tests PROPERTIES
        MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>"
    )
endif()

enable_testing()
add_test(NAME rtsl-tests COMMAND rtsl-tests)

