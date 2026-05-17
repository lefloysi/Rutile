function(rutile_set_common_output_dirs target_name)
    set_target_properties(${target_name} PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
        LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
        ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
    )

    if(CMAKE_CONFIGURATION_TYPES)
        foreach(cfg ${CMAKE_CONFIGURATION_TYPES})
            string(TOUPPER "${cfg}" cfg_upper)
            set_target_properties(${target_name} PROPERTIES
                RUNTIME_OUTPUT_DIRECTORY_${cfg_upper} "${CMAKE_BINARY_DIR}/bin/${cfg}"
                LIBRARY_OUTPUT_DIRECTORY_${cfg_upper} "${CMAKE_BINARY_DIR}/bin/${cfg}"
                ARCHIVE_OUTPUT_DIRECTORY_${cfg_upper} "${CMAKE_BINARY_DIR}/lib/${cfg}"
            )
        endforeach()
    endif()
endfunction()

function(rutile_register_runtime_target target_name)
    if(NOT TARGET "${target_name}")
        message(FATAL_ERROR "Rutile runtime target '${target_name}' does not exist")
    endif()

    set_property(GLOBAL APPEND PROPERTY RUTILE_RUNTIME_TARGETS "${target_name}")

    if(NOT TARGET "Rutile::${target_name}")
        add_library("Rutile::${target_name}" ALIAS "${target_name}")
    endif()

    if(RUTILE_INSTALL)
        install(TARGETS "${target_name}"
            EXPORT RutileTargets
            RUNTIME_DEPENDENCY_SET RutileRuntimeDependencies
            RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
            LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}"
            ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}"
        )
        install(FILES
            "$<TARGET_RUNTIME_DLLS:${target_name}>"
            DESTINATION "${CMAKE_INSTALL_BINDIR}"
        )
    endif()
endfunction()

function(rutile_configure_shared_library target_name)
    rutile_set_common_output_dirs(${target_name})
    target_compile_definitions(${target_name} PRIVATE RT_BUILD_DLL)
    target_include_directories(${target_name}
        PUBLIC
            "$<BUILD_INTERFACE:${RUTILE_SOURCE_DIR}/include>"
            "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>"
        PRIVATE
            src
    )
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/include")
        target_include_directories(${target_name} PUBLIC "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>")
    endif()

    rutile_register_runtime_target(${target_name})
endfunction()

function(rutile_get_runtime_targets out_var)
    get_property(runtime_targets GLOBAL PROPERTY RUTILE_RUNTIME_TARGETS)
    set(${out_var} ${runtime_targets} PARENT_SCOPE)
endfunction()

function(rutile_copy_runtime_dependencies target_name)
    if(NOT TARGET "${target_name}")
        message(FATAL_ERROR "Rutile target '${target_name}' does not exist")
    endif()

    cmake_parse_arguments(RUTILE_COPY_RUNTIME "" "DESTINATION" "" ${ARGN})
    if(NOT RUTILE_COPY_RUNTIME_DESTINATION)
        set(RUTILE_COPY_RUNTIME_DESTINATION "$<TARGET_FILE_DIR:${target_name}>")
    endif()

    rutile_get_runtime_targets(runtime_targets)
    if(NOT runtime_targets AND NOT RUTILE_RUNTIME_FILES)
        return()
    endif()

    set(build_runtime_targets)
    set(runtime_files ${RUTILE_RUNTIME_FILES})
    foreach(runtime_target IN LISTS runtime_targets)
        get_target_property(runtime_target_imported "${runtime_target}" IMPORTED)
        if(NOT runtime_target_imported)
            list(APPEND build_runtime_targets "${runtime_target}")
            list(APPEND runtime_files
                "$<TARGET_FILE:${runtime_target}>"
                "$<TARGET_RUNTIME_DLLS:${runtime_target}>"
            )
        endif()
    endforeach()

    if(build_runtime_targets)
        add_dependencies("${target_name}" ${build_runtime_targets})
    endif()

    add_custom_command(TARGET "${target_name}" POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory
            "${RUTILE_COPY_RUNTIME_DESTINATION}"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${runtime_files}
            "${RUTILE_COPY_RUNTIME_DESTINATION}"
        COMMAND_EXPAND_LISTS
    )
endfunction()

function(rutile_configure_executable target_name)
    if(NOT TARGET "${target_name}")
        message(FATAL_ERROR "Rutile executable target '${target_name}' does not exist")
    endif()

    target_link_libraries("${target_name}" PRIVATE rutile)
    rutile_copy_runtime_dependencies("${target_name}")
endfunction()

function(rutile_install_runtime_dependencies target_name)
    cmake_parse_arguments(RUTILE_INSTALL_RUNTIME "" "DESTINATION" "" ${ARGN})
    if(NOT RUTILE_INSTALL_RUNTIME_DESTINATION)
        set(RUTILE_INSTALL_RUNTIME_DESTINATION "${CMAKE_INSTALL_BINDIR}")
    endif()

    rutile_get_runtime_targets(runtime_targets)
    if(RUTILE_RUNTIME_FILES)
        install(FILES
            ${RUTILE_RUNTIME_FILES}
            DESTINATION "${RUTILE_INSTALL_RUNTIME_DESTINATION}"
        )
    endif()

    foreach(runtime_target IN LISTS runtime_targets)
        get_target_property(runtime_target_imported "${runtime_target}" IMPORTED)
        if(NOT runtime_target_imported)
            install(FILES
                "$<TARGET_FILE:${runtime_target}>"
                "$<TARGET_RUNTIME_DLLS:${runtime_target}>"
                DESTINATION "${RUTILE_INSTALL_RUNTIME_DESTINATION}"
            )
        endif()
    endforeach()
endfunction()
