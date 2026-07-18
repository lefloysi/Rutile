cmake_minimum_required(VERSION 3.28.0)

project(rutile-examples C CXX)

find_package(glfw3 CONFIG REQUIRED)
find_package(glm CONFIG REQUIRED)
find_package(CLI11 CONFIG REQUIRED)
find_package(Stb REQUIRED)
include(RutileTarget)

function(rutile_configure_example target)
    target_compile_features(${target} PRIVATE cxx_std_23)

    rutile_set_common_output_dirs(${target})
    set_target_properties(${target} PROPERTIES
        VS_DEBUGGER_WORKING_DIRECTORY "$<TARGET_FILE_DIR:${target}>"
        ENABLE_EXPORTS ON
    )
    if(MSVC)
        set_target_properties(${target} PROPERTIES
            MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>"
        )
    endif()

    target_include_directories(${target} PRIVATE
        ${Stb_INCLUDE_DIR}
        "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/shared"
    )

    target_link_libraries(${target} PRIVATE
        glfw
        glm::glm
        CLI11::CLI11
    )

    rutile_configure_executable(${target})
endfunction()

add_subdirectory("01-triangle")
add_subdirectory("02-spinning-cube")
add_subdirectory("03-plasma-lab")
add_subdirectory("04-galaxy")
add_subdirectory("05-voxel-renderer")
