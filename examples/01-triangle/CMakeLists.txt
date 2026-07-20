add_executable(rutile-01-triangle
    main.cpp
    triangle.rtsl
)
set_source_files_properties(triangle.rtsl PROPERTIES HEADER_FILE_ONLY TRUE)
rutile_configure_example(rutile-01-triangle)
rtsl_add_program(rutile-01-triangle-shader
    SYMBOL triangle_rtslp
    SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/triangle.rtsl"
)
rtsl_embed_program(rutile-01-triangle
    PROGRAMS rutile-01-triangle-shader
)
target_include_directories(rutile-01-triangle PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
