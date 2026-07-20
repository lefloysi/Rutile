add_executable(rutile-04-galaxy
    main.cpp
    galaxy.rtsl
)
set_source_files_properties(galaxy.rtsl PROPERTIES HEADER_FILE_ONLY TRUE)

rutile_configure_example(rutile-04-galaxy)
rtsl_add_program(rutile-04-galaxy-shader
    SYMBOL galaxy_rtslp
    SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/galaxy.rtsl"
)
rtsl_embed_program(rutile-04-galaxy
    PROGRAMS rutile-04-galaxy-shader
)
