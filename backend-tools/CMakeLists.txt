cmake_minimum_required(VERSION 3.50)

project(rutile-backend-tools CXX)

find_package(glslang CONFIG REQUIRED)
find_package(spirv_cross_core CONFIG REQUIRED)
find_package(spirv_cross_glsl CONFIG REQUIRED)
find_package(spirv_cross_hlsl CONFIG REQUIRED)

add_library(${PROJECT_NAME} STATIC)
add_library(Rutile::backend-tools ALIAS ${PROJECT_NAME})
target_compile_features(${PROJECT_NAME} PUBLIC cxx_std_23)
target_sources(${PROJECT_NAME} PRIVATE
    "src/shader_translation.cpp"
    "include/rutile/backend_tools/shader_translation.hpp"
)
target_include_directories(${PROJECT_NAME}
    PUBLIC
        "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>"
)
target_link_libraries(${PROJECT_NAME}
    PUBLIC
        rutile
    PRIVATE
        glslang::glslang
        glslang::SPIRV
        glslang::glslang-default-resource-limits
        spirv-cross-core
        spirv-cross-glsl
        spirv-cross-hlsl
)
