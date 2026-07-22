cmake_minimum_required(VERSION 3.28.0)

project(rt-opengl C CXX)

if(NOT WIN32)
    return()
endif()

include(RutileTarget)

find_package(glfw3 CONFIG REQUIRED)

add_library(${PROJECT_NAME} SHARED)

target_sources(${PROJECT_NAME} PRIVATE
    "src/config.h"
    "src/types.h"
    "src/core.c"
    "src/core.h"
    "src/context.c"
    "src/context.h"
    "src/execution.cpp"
    "src/execution_commands.cpp"
    "src/execution.h"
    "src/execution_internal.hpp"
    "src/error.c"
    "src/error.h"
    "../../common/sync.h"
    "$<$<PLATFORM_ID:Windows>:${CMAKE_CURRENT_SOURCE_DIR}/../../common/win32/sync.c>"
    "$<$<NOT:$<PLATFORM_ID:Windows>>:${CMAKE_CURRENT_SOURCE_DIR}/../../common/posix/sync.c>"
    "src/ext/glfw/swapchain.c"
    "src/ext/glfw/swapchain.h"
    "src/glfw/glfw.c"
    "src/glfw/glfw.h"
    "src/resource/resource.c"
    "src/resource/resource.h"
    "src/resource/buffer.c"
    "src/resource/buffer.h"
    "src/resource/command_buffer.cpp"
    "src/resource/command_buffer.h"
    "src/resource/framebuffer.c"
    "src/resource/framebuffer.h"
    "src/resource/graphics_program.c"
    "src/resource/graphics_program.h"
    "../rt-vulkan/src/rtsl_spirv.cpp"
    "../rt-vulkan/src/rtsl_spirv.h"
    "src/resource/queue.c"
    "src/resource/queue.h"
    "src/resource/swapchain.c"
    "src/resource/swapchain.h"
    "src/resource/texture.c"
    "src/resource/texture.h"
    
    "src/platform/proc.h"
    "src/platform/context.h"
    "src/platform/wgl/context.c"
    "src/glad/gl.c"
    "src/glad/gl.h"
    "src/KHR/khrplatform.h"
    "src/types.h")

rutile_configure_shared_library(${PROJECT_NAME})

target_include_directories(${PROJECT_NAME} PRIVATE
    "../../common"
    "../rt-vulkan/src"
)

target_link_libraries(${PROJECT_NAME}
    PRIVATE
        glfw
        opengl32
        gdi32
        user32
        psapi
        RTSL::spirv
)

add_subdirectory("workspace")
