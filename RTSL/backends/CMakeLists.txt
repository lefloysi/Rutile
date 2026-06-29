cmake_minimum_required(VERSION 3.20)

project(rtsl-backends LANGUAGES CXX)

set(_rtsl_src "${CMAKE_CURRENT_SOURCE_DIR}/../src")

function(_rtsl_backend target)
    target_compile_features(${target} PUBLIC cxx_std_23)
    set_target_properties(${target} PROPERTIES CXX_EXTENSIONS OFF)
    if(MSVC)
        set_target_properties(${target} PROPERTIES
            MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>"
        )
        target_compile_options(${target} PRIVATE /FS)
    endif()
    target_include_directories(${target}
        PUBLIC
            "${CMAKE_CURRENT_SOURCE_DIR}/../src"
            "${CMAKE_CURRENT_SOURCE_DIR}/../../bindings/c/include"
            "${CMAKE_CURRENT_SOURCE_DIR}/include"
    )
    target_link_libraries(${target} PUBLIC rtsl-core rtsl-package)
endfunction()

add_library(rtsl-backend-spirv STATIC
    "${CMAKE_CURRENT_SOURCE_DIR}/spirv/src/SpirvEmitter.cpp"
)
_rtsl_backend(rtsl-backend-spirv)
find_package(SPIRV-Headers CONFIG REQUIRED)
target_link_libraries(rtsl-backend-spirv PUBLIC SPIRV-Headers::SPIRV-Headers)

if(WIN32)
    add_library(rtsl-backend-hlsl STATIC
        "${CMAKE_CURRENT_SOURCE_DIR}/hlsl/src/shader_compiler.cpp"
    )
    _rtsl_backend(rtsl-backend-hlsl)
endif()
