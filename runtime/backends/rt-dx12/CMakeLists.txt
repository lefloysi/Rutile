cmake_minimum_required(VERSION 3.28.0)

project(rt-dx12 CXX)

if(NOT WIN32)
    return()
endif()

include(RutileTarget)

add_library(${PROJECT_NAME} SHARED)
target_compile_features(${PROJECT_NAME} PRIVATE cxx_std_23)
target_sources(${PROJECT_NAME} PRIVATE
    "src/atomic.hpp"
    "src/config.hpp"
    "src/context.cpp"
    "src/context.hpp"
    "src/core.cpp"
    "src/core.hpp"
    "src/error.cpp"
    "src/error.hpp"
    "src/init.cpp"
    "src/rtsl-hlsl.cpp"
    "src/rtsl-hlsl.hpp"
    "src/types.hpp"
    "src/resource/buffer.cpp"
    "src/resource/buffer.hpp"
    "src/resource/command_buffer.cpp"
    "src/resource/command_buffer.hpp"
    "src/resource/framebuffer.cpp"
    "src/resource/framebuffer.hpp"
    "src/resource/graphics_program.cpp"
    "src/resource/graphics_program.hpp"
    "src/resource/queue.cpp"
    "src/resource/queue.hpp"
    "src/resource/resource.cpp"
    "src/resource/resource.hpp"
    "src/resource/swapchain.cpp"
    "src/resource/swapchain.hpp"
    "src/resource/texture.cpp"
    "src/resource/texture.hpp"
    "src/resource/glfw/glfw.cpp"
    "src/resource/glfw/glfw.hpp"
)

target_include_directories(${PROJECT_NAME} PRIVATE "src")

rutile_configure_shared_library(${PROJECT_NAME})

target_link_libraries(${PROJECT_NAME}
    PRIVATE
        rtsl
        d3d12
        dxgi
        dxguid
)
