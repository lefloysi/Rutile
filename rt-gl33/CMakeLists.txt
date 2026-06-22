cmake_minimum_required(VERSION 3.28.0)

project(rt-gl33 C CXX)

if(NOT WIN32)
    return()
endif()

include(RutileTarget)

find_package(glfw3 CONFIG REQUIRED)

add_library(${PROJECT_NAME} SHARED)
target_compile_features(${PROJECT_NAME} PRIVATE
    c_std_11
    cxx_std_23
)

target_sources(${PROJECT_NAME} PRIVATE
    "src/core.c"
    "src/context.c"
    "src/error.c"
    "src/system_win32.c"
    "src/init.c"
    "src/resource/swapchain.c"
    "src/resource/glfw/glfw.c"
    "src/core.h"
    "src/context.h"
    "src/error.h"
    "src/types.h"
    "src/system.h"
    "src/system_win32.h"
    "src/resource/swapchain.h"
    "src/resource/glfw/glfw.h"
)

rutile_configure_shared_library(${PROJECT_NAME})

target_link_libraries(${PROJECT_NAME}
    PRIVATE
        glfw
        opengl32
        gdi32
        user32
)
