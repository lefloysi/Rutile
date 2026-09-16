find_package(glfw3 CONFIG REQUIRED)
find_package(glm CONFIG REQUIRED)
find_package(CLI11 CONFIG REQUIRED)
find_package(Stb REQUIRED)

set(RUTILE_EXAMPLE_RUNTIMES "rt-vulkan;rt-d3d12;rt-opengl" CACHE STRING "Runtime backends built and deployed with examples")
set(RUTILE_EXAMPLE_LAYERS "rt-logging-layer;rt-validation-layer;rt-debug" CACHE STRING "Runtime layers built and deployed with examples")

function(rutile_embed_program target symbol source)
    get_filename_component(module_name "${source}" NAME_WE)
    set(program_artifact "${CMAKE_CURRENT_BINARY_DIR}/${symbol}.rtslp")
    set(program_source "${CMAKE_CURRENT_BINARY_DIR}/${symbol}.cpp")

    add_custom_command(
        OUTPUT "${program_artifact}"
        COMMAND "$<TARGET_FILE:rtslc>" "${source}" --module "${module_name}" --emit-program -o "${program_artifact}"
        DEPENDS rtslc "${source}"
        VERBATIM
    )
    add_custom_command(
        OUTPUT "${program_source}"
        COMMAND "${CMAKE_COMMAND}"
            "-DINPUT=${program_artifact}"
            "-DOUTPUT=${program_source}"
            "-DSYMBOL=${symbol}"
			"-DHEADER=embedded_program"
			"-DTYPE=rt_example_program"
            -P "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/../cmake/embed_program.cmake"
        DEPENDS "${program_artifact}" "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/../cmake/embed_program.cmake"
        VERBATIM
    )
    set_source_files_properties("${program_source}" PROPERTIES GENERATED TRUE)
    target_sources("${target}" PRIVATE "${program_source}")
endfunction()

function(rutile_configure_example target)
    target_compile_features("${target}" PRIVATE cxx_std_23)
    set_target_properties("${target}" PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
        VS_DEBUGGER_WORKING_DIRECTORY "$<TARGET_FILE_DIR:${target}>"
        ENABLE_EXPORTS ON
    )

    target_include_directories("${target}" PRIVATE
        ${Stb_INCLUDE_DIR}
        "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/shared"
    )

    target_link_libraries("${target}" PRIVATE
        glfw
        glm::glm
        CLI11::CLI11
    )

    target_link_libraries("${target}" PRIVATE rutile)
    set(_runtime_targets)
    foreach(_runtime IN LISTS RUTILE_EXAMPLE_RUNTIMES)
        if(TARGET "${_runtime}")
            list(APPEND _runtime_targets "${_runtime}")
        endif()
    endforeach()

    foreach(_layer IN LISTS RUTILE_EXAMPLE_LAYERS)
        if(TARGET "${_layer}")
            list(APPEND _runtime_targets "${_layer}")
        endif()
    endforeach()

    list(REMOVE_DUPLICATES _runtime_targets)
    if(_runtime_targets)
        add_dependencies("${target}" ${_runtime_targets})
    endif()
endfunction()

add_subdirectory("01-triangle")
add_subdirectory("02-spinning-cube")
add_subdirectory("03-plasma-lab")
add_subdirectory("05-temperature-simulation")
add_subdirectory("06-tessellation")
add_subdirectory("07-geometry")
add_subdirectory("08-voxel-renderer")

set_property(GLOBAL APPEND PROPERTY RUTILE_EXAMPLE_TARGETS
    "rutile-01-triangle"
    "rutile-02-spinning-cube"
    "rutile-03-plasma-lab"
    "rutile-05-temperature-simulation"
    "rutile-06-tessellation"
    "rutile-07-geometry"
    "rutile-08-voxel-renderer"
)
