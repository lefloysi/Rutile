cmake_minimum_required(VERSION 3.28)
project(Rutile VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_SCAN_FOR_MODULES OFF)

if(MSVC)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
    add_link_options("$<$<CONFIG:Debug>:/DEBUG:FULL>")
endif()

enable_testing()

set(RUTILE_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
include("${RUTILE_SOURCE_DIR}/cmake/rtsl_program.cmake")

# The examples embed RTSL program artifacts through the rtslc target.
set(RTSL_BUILD_CLI ON)
add_subdirectory("RTSL" EXCLUDE_FROM_ALL)

add_library(rutile STATIC
    "src/rutile.c"
    "src/rt_ext_swapchain.c"
    "src/rt_ext_glfw.c"
)
add_library(Rutile::rutile ALIAS rutile)
target_include_directories(rutile PUBLIC "include")

add_library(rutile-cpp INTERFACE)
add_library(Rutile::cpp ALIAS rutile-cpp)

target_link_libraries(rutile-cpp INTERFACE Rutile::rutile)
target_include_directories(rutile-cpp INTERFACE "include")

add_subdirectory("backends")
add_subdirectory("layers")
add_subdirectory("examples")
add_subdirectory("test")
