add_executable(rutile-03-plasma-lab
    main.cpp
    plasma.rtsl
)
set_source_files_properties(plasma.rtsl PROPERTIES HEADER_FILE_ONLY TRUE)

rutile_configure_example(rutile-03-plasma-lab)
rtsl_add_program(rutile-03-plasma-lab-shader
    SYMBOL plasma_rtslp
    SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/plasma.rtsl"
)
rtsl_embed_program(rutile-03-plasma-lab
    PROGRAMS rutile-03-plasma-lab-shader
)
