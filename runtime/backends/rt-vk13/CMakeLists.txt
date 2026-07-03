cmake_minimum_required(VERSION 3.28.0)

project(rt-vk13 C CXX)

find_package(Vulkan REQUIRED)
find_package(VulkanMemoryAllocator CONFIG REQUIRED)
find_package(Threads REQUIRED)
include(RutileTarget)

set(RUTILE_VULKAN_LIBRARY Vulkan::Vulkan)
if(UNIX AND NOT APPLE)
    if(EXISTS "/usr/lib/x86_64-linux-gnu/libvulkan.so")
        set(RUTILE_VULKAN_LIBRARY "/usr/lib/x86_64-linux-gnu/libvulkan.so")
    elseif(EXISTS "/lib/x86_64-linux-gnu/libvulkan.so")
        set(RUTILE_VULKAN_LIBRARY "/lib/x86_64-linux-gnu/libvulkan.so")
    else()
        message(FATAL_ERROR "Could not find the Linux system Vulkan loader. Install libvulkan-dev.")
    endif()
endif()

add_library(${PROJECT_NAME} SHARED)
target_compile_features(${PROJECT_NAME} PRIVATE
    c_std_11
    cxx_std_23
)
target_compile_definitions(${PROJECT_NAME} PRIVATE
    "$<$<CONFIG:Debug>:RTVK_ENABLE_VULKAN_VALIDATION>"
)

target_sources(${PROJECT_NAME} PRIVATE
    "src/core.c"
    "src/error.c"
    
    "src/atomic.cpp"
    
    
    "src/vma.cpp"
    "src/atomic.h"
    "src/resource/buffer.c"
    "src/resource/buffer.h"
    
    
    "src/resource/command_buffer.c"
    "src/resource/command_buffer.h"
    "src/resource/framebuffer.c"
    "src/resource/framebuffer.h"
    "src/resource/graphics_program.c"
    "src/resource/graphics_program.h"
    "src/rtsl_spirv.c"
    "src/rtsl_spirv.h"
    "src/resource/queue.c"
    "src/resource/queue.h"
    "src/resource/resource.c"
    "src/resource/resource.h"
    "src/resource/texture.c"
    "src/resource/texture.h"
    "src/resource/swapchain.c"
    "src/resource/swapchain.h"
    "src/resource/glfw/glfw.c"
    "src/resource/glfw/glfw.h"
    "src/error.h"
    "src/core.h"
    "src/types.h"
    "src/context.h"
    "src/context.c"
)
target_include_directories(${PROJECT_NAME} PRIVATE
    "src"
)

rutile_configure_shared_library(${PROJECT_NAME})

target_link_libraries(${PROJECT_NAME}
    PUBLIC
        ${RUTILE_VULKAN_LIBRARY}
    PRIVATE
        GPUOpen::VulkanMemoryAllocator
        ${CMAKE_DL_LIBS}
        Threads::Threads
        rtsl
)
if(WIN32)
    target_link_libraries(${PROJECT_NAME} PRIVATE psapi)
endif()
