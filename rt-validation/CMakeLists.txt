cmake_minimum_required(VERSION 3.50)

project(rt-validation C)

include(RutileTarget)

add_library(${PROJECT_NAME} SHARED)
target_compile_features(${PROJECT_NAME} PRIVATE c_std_11)

target_sources(${PROJECT_NAME} PRIVATE
    "src/buffer.c"
    "src/command_buffer.c"
    "src/core.c"
    "src/entry.c"
    "src/framebuffer.c"
    "src/graphics_program.c"
    "src/logger.c"
    "src/queue.c"
    "src/state.c"
    "src/swapchain.c"
    "src/texture.c"
    "src/logger.h"
    "src/procs.h"
    "src/types.h"
)

rutile_configure_shared_library(${PROJECT_NAME})
