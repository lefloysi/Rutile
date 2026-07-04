cmake_minimum_required(VERSION 3.20)

project(rtsl LANGUAGES C CXX)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(Rtsl)

if(DEFINED VCPKG_INSTALLED_DIR AND DEFINED VCPKG_TARGET_TRIPLET AND NOT VCPKG_TARGET_TRIPLET STREQUAL "")
    list(PREPEND CMAKE_PREFIX_PATH "${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}")
endif()

# All RTSL sources live flat in src/. Headers and .cpp sit next to each other;
# sibling includes use plain "foo.hpp". The public C ABI header is the only
# split-out surface and lives at bindings/c/include/rtsl.h.

set(_rtsl_src "${CMAKE_CURRENT_SOURCE_DIR}/src")

set(RTSL_SOURCES
    ${_rtsl_src}/basic_diagnostics.cpp
    ${_rtsl_src}/basic_source_manager.cpp
    ${_rtsl_src}/lexer.cpp
    ${_rtsl_src}/parser.cpp
    ${_rtsl_src}/sema.cpp
    ${_rtsl_src}/stage_builtins.cpp
    ${_rtsl_src}/ir.cpp
    ${_rtsl_src}/mangler.cpp
    ${_rtsl_src}/uniform_lowering.cpp
    ${_rtsl_src}/artifact.cpp
    ${_rtsl_src}/text_rtir.cpp
    ${_rtsl_src}/package.cpp
    ${_rtsl_src}/compiler.cpp
    ${_rtsl_src}/language_service.cpp
    ${_rtsl_src}/linker.cpp
    ${_rtsl_src}/rtsl.cpp
)

add_library(rtsl STATIC ${RTSL_SOURCES})
target_compile_features(rtsl PUBLIC cxx_std_23)
set_target_properties(rtsl PROPERTIES CXX_EXTENSIONS OFF)
if(MSVC)
    set_target_properties(rtsl PROPERTIES
        MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>"
    )
    target_compile_options(rtsl PRIVATE /FS)
endif()
target_include_directories(rtsl
    PUBLIC
        "${_rtsl_src}"
        "${CMAKE_CURRENT_SOURCE_DIR}/bindings/c/include"
)
# package.cpp pulls in rutile.h (from the runtime layout/IDs).
target_link_libraries(rtsl PUBLIC rutile)

add_executable(rtslc ${_rtsl_src}/rtslc.cpp)
find_package(CLI11 CONFIG QUIET)
if(CLI11_FOUND)
    target_link_libraries(rtslc PRIVATE rtsl CLI11::CLI11)
    target_compile_features(rtslc PRIVATE cxx_std_23)
    set_target_properties(rtslc PROPERTIES CXX_EXTENSIONS OFF)
else()
    message(WARNING "CLI11 not found; skipping rtslc target")
    set_target_properties(rtslc PROPERTIES EXCLUDE_FROM_ALL TRUE)
endif()

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
