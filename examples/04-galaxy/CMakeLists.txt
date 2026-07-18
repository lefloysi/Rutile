add_executable(rutile-04-galaxy
    main.cpp
    galaxy.rtsl
)
set_source_files_properties(galaxy.rtsl PROPERTIES HEADER_FILE_ONLY TRUE)

rutile_configure_example(rutile-04-galaxy)
set(_galaxy_rtsl_dir "${CMAKE_CURRENT_BINARY_DIR}/rtsl")
rtsl_add_program(rutile-04-galaxy
    RTSLC rtslc
    OUTPUT_DIR "${_galaxy_rtsl_dir}"
    EMBED
    SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/galaxy.rtsl"
)
target_include_directories(rutile-04-galaxy PRIVATE "${_galaxy_rtsl_dir}")
