cmake_minimum_required(VERSION 3.28.0)

project(rt-dx12 C CXX)

if(NOT WIN32)
    return()
endif()

include(RutileTarget)

add_library(${PROJECT_NAME} SHARED)
target_compile_features(${PROJECT_NAME} PRIVATE
    c_std_11
    cxx_std_23
)
target_sources(${PROJECT_NAME} PRIVATE
    "src/atomic.cpp"
    "src/core.cpp"
    "src/error.cpp"
    "src/init.cpp"
    "src/shader_compiler.cpp"
    "src/resource/buffer.cpp"
    "src/resource/command_buffer.cpp"
    "src/resource/framebuffer.cpp"
    "src/resource/graphics_program.cpp"
    "src/resource/queue.cpp"
    "src/resource/resource.cpp"
    "src/resource/texture.cpp"
    "src/resource/swapchain.cpp"
    "src/resource/glfw/glfw.cpp"
    "src/error.h"
    "src/atomic.h"
    "src/core.h"
    "src/types.h"
    "src/context.h"
    "src/context.cpp"
    "src/shader_compiler.h"
    "src/resource/buffer.h"
    "src/resource/command_buffer.h"
    "src/resource/framebuffer.h"
    "src/resource/graphics_program.h"
    "src/resource/queue.h"
    "src/resource/resource.h"
    "src/resource/texture.h"
    "src/resource/swapchain.h"
    "src/resource/glfw/glfw.h"
)

target_include_directories(${PROJECT_NAME} PRIVATE "src")

rutile_configure_shared_library(${PROJECT_NAME})

target_link_libraries(${PROJECT_NAME}
    PRIVATE
        Rutile::backend-tools
        d3d12
        dxgi
        dxguid
        d3dcompiler
)
