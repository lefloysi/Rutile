cmake_minimum_required(VERSION 3.28.0)

project(rutile-examples C CXX)

find_package(glfw3 CONFIG REQUIRED)
find_package(glm CONFIG REQUIRED)
find_package(Stb REQUIRED)
include(RutileTarget)

function(rutile_configure_example target)
    target_compile_features(${target} PRIVATE cxx_std_23)

    rutile_set_common_output_dirs(${target})
    set_target_properties(${target} PROPERTIES
        VS_DEBUGGER_WORKING_DIRECTORY "$<TARGET_FILE_DIR:${target}>"
        ENABLE_EXPORTS ON
    )

    target_include_directories(${target} PRIVATE
        ${Stb_INCLUDE_DIR}
    )

    target_link_libraries(${target} PRIVATE
        glfw
        glm::glm
    )

    add_custom_command(TARGET ${target} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_RUNTIME_DLLS:${target}>
            $<TARGET_FILE_DIR:${target}>
        COMMAND_EXPAND_LISTS
    )

    rutile_configure_executable(${target})
endfunction()

add_subdirectory(first)
add_subdirectory(compute)
add_subdirectory(heat-diffusion)
add_subdirectory(heat-diffusion-edges)
add_subdirectory(minecraft)
add_subdirectory(galaxy)
add_subdirectory(plasma-lab)
add_subdirectory(rt-fluid-simulation)
