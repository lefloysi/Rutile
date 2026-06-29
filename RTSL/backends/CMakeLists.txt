cmake_minimum_required(VERSION 3.20)

project(rtsl-backends LANGUAGES CXX)

# Source-only backend tree.
# The runtime links to these interface targets for include paths and source
# ownership, but the backend code itself stays in the RTSL tree.

add_library(rtsl-backend-spirv INTERFACE)
target_include_directories(rtsl-backend-spirv INTERFACE
    "${CMAKE_CURRENT_SOURCE_DIR}/spirv/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/../src"
    "${CMAKE_CURRENT_SOURCE_DIR}/../../bindings/c/include"
)

if(WIN32)
    add_library(rtsl-backend-hlsl INTERFACE)
    target_include_directories(rtsl-backend-hlsl INTERFACE
        "${CMAKE_CURRENT_SOURCE_DIR}/hlsl/include"
        "${CMAKE_CURRENT_SOURCE_DIR}/../src"
        "${CMAKE_CURRENT_SOURCE_DIR}/../../bindings/c/include"
    )
endif()
