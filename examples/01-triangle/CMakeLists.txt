add_executable(rutile-01-triangle
    main.cpp
    triangle.rtsl
)
set_source_files_properties(triangle.rtsl PROPERTIES HEADER_FILE_ONLY TRUE)
rutile_configure_example(rutile-01-triangle)
set(_triangle_rtsl_dir "${CMAKE_CURRENT_BINARY_DIR}/rtsl")
rtsl_add_program(rutile-01-triangle
    RTSLC rtslc
    OUTPUT_DIR "${_triangle_rtsl_dir}"
    EMBED
    SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/triangle.rtsl"
)
target_include_directories(rutile-01-triangle PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}" "${_triangle_rtsl_dir}")
