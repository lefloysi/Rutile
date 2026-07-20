add_executable(rutile-02-spinning-cube
    main.cpp
    cube.rtsl
)
set_source_files_properties(cube.rtsl PROPERTIES HEADER_FILE_ONLY TRUE)

rutile_configure_example(rutile-02-spinning-cube)
rtsl_add_program(rutile-02-spinning-cube-shader
    SYMBOL cube_rtslp
    SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/cube.rtsl"
)
rtsl_embed_program(rutile-02-spinning-cube
    PROGRAMS rutile-02-spinning-cube-shader
)
