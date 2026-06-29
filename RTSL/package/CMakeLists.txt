cmake_minimum_required(VERSION 3.28.0)

project(rutile-backend-tools CXX)

add_library(${PROJECT_NAME} STATIC)
add_library(Rutile::backend-tools ALIAS ${PROJECT_NAME})
target_compile_features(${PROJECT_NAME} PUBLIC cxx_std_23)
if(MSVC)
    set_target_properties(${PROJECT_NAME} PROPERTIES
        MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>"
    )
endif()
target_sources(${PROJECT_NAME} PRIVATE
    "src/rtslp_package.cpp"
    "include/rtslp_package.hpp"
)
target_include_directories(${PROJECT_NAME}
    PUBLIC
        "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>"
)
target_link_libraries(${PROJECT_NAME}
    PUBLIC
        rutile
)
