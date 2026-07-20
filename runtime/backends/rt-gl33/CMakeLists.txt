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
    "src/config.h"
    "src/types.h"
    "src/core.c"
    "src/core.h"
    "src/context.c"
    "src/context.h"
    "src/error.c"
    "src/error.h"
    "src/resource/swapchain.c"
    "src/resource/swapchain.h"
    "src/resource/glfw/glfw.c"
    
    "src/platform/proc.h"
    "src/platform/win32/proc.c"
    "src/platform/win32/glfw.c"
    "src/platform/context.h"
    "src/platform/wgl/context.c"
    "src/glad/gl.c"
    "src/glad/gl.h"
    "src/types.h")

rutile_configure_shared_library(${PROJECT_NAME})

target_link_libraries(${PROJECT_NAME}
    PRIVATE
        glfw
        opengl32
        gdi32
        user32
        psapi
)

add_subdirectory("workspace")
