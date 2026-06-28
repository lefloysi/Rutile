add_executable(rutile-00-triangle
    main.cpp
    triangle.rtsl
)
set_source_files_properties(triangle.rtsl PROPERTIES HEADER_FILE_ONLY TRUE)
rutile_configure_example(rutile-00-triangle)

set(rtslc_exe "${RUTILE_SOURCE_DIR}/../RTSL/out/build/rtslc.exe")
set(triangle_object "${CMAKE_CURRENT_BINARY_DIR}/triangle.rtslo")
set(triangle_program "${CMAKE_CURRENT_BINARY_DIR}/triangle.rtslp")
set(triangle_runtime_program "${CMAKE_BINARY_DIR}/bin/$<CONFIG>/triangle.rtslp")

add_custom_command(
    OUTPUT "${triangle_program}"
    COMMAND "${rtslc_exe}"
        compile "${CMAKE_CURRENT_SOURCE_DIR}/triangle.rtsl"
        -o "${triangle_object}"
    COMMAND "${rtslc_exe}"
        link-program "${triangle_object}"
        -o "${triangle_program}"
    DEPENDS
        "${CMAKE_CURRENT_SOURCE_DIR}/triangle.rtsl"
        "${rtslc_exe}"
    VERBATIM
)

add_custom_command(
    OUTPUT "${triangle_runtime_program}"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${triangle_program}"
        "${triangle_runtime_program}"
    DEPENDS "${triangle_program}"
    VERBATIM
)

add_custom_target(rutile-00-triangle-shader DEPENDS "${triangle_runtime_program}")
add_dependencies(rutile-00-triangle rutile-00-triangle-shader)
