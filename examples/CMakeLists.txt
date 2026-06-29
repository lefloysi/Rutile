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
    if(MSVC)
        set_target_properties(${target} PROPERTIES
            MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>"
        )
    endif()

    target_include_directories(${target} PRIVATE
        ${Stb_INCLUDE_DIR}
    )

    target_link_libraries(${target} PRIVATE
        glfw
        glm::glm
    )

    rutile_configure_executable(${target})
endfunction()

add_subdirectory("00-triangle")
add_subdirectory("01-textured-quads")
# Temporarily disabled — these examples still use the pre-RTSL split shader
# APIs (rtGraphicsProgramVertexShader / rtGraphicsProgramFragmentShader) that
# were removed during the RTSL migration. Re-enable as each is ported.
# add_subdirectory("02-compute-buffer")
# add_subdirectory("03-heat-diffusion")
# add_subdirectory("04-heat-diffusion-edges")
# add_subdirectory("05-voxel-engine")
# add_subdirectory("06-galaxy")
# add_subdirectory("07-plasma-lab")
