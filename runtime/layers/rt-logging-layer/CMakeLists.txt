cmake_minimum_required(VERSION 3.28.0)

project(rt-logging-layer C)

include(RutileTarget)

add_library(${PROJECT_NAME} SHARED)

target_sources(${PROJECT_NAME} PRIVATE
    "src/buffer.c"
    "src/command_buffer.c"
    "src/core.c"
    "src/entry.c"
    "src/framebuffer.c"
    "src/graphics_program.c"
    "src/logger.c"
    "src/procs.h"
    "src/queue.c"
    "src/state.c"
    "src/swapchain.c"
    "src/texture.c"
)

rutile_configure_shared_library(${PROJECT_NAME})
