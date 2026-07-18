add_executable(rutile-03-plasma-lab
    main.cpp
    plasma.rtsl
)
set_source_files_properties(plasma.rtsl PROPERTIES HEADER_FILE_ONLY TRUE)

rutile_configure_example(rutile-03-plasma-lab)
set(_plasma_rtsl_dir "${CMAKE_CURRENT_BINARY_DIR}/rtsl")
rtsl_add_program(rutile-03-plasma-lab
    RTSLC rtslc
    OUTPUT_DIR "${_plasma_rtsl_dir}"
    EMBED
    SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/plasma.rtsl"
)
target_include_directories(rutile-03-plasma-lab PRIVATE "${_plasma_rtsl_dir}")
