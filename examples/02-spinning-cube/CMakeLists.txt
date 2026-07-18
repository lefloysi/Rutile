add_executable(rutile-02-spinning-cube
    main.cpp
    cube.rtsl
)
set_source_files_properties(cube.rtsl PROPERTIES HEADER_FILE_ONLY TRUE)

rutile_configure_example(rutile-02-spinning-cube)
set(_cube_rtsl_dir "${CMAKE_CURRENT_BINARY_DIR}/rtsl")
rtsl_add_program(rutile-02-spinning-cube
    RTSLC rtslc
    OUTPUT_DIR "${_cube_rtsl_dir}"
    EMBED
    SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/cube.rtsl"
)
target_include_directories(rutile-02-spinning-cube PRIVATE "${_cube_rtsl_dir}")
