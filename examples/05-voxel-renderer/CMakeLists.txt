add_executable(rutile-05-voxel-renderer
    main.cpp
    terrain.rtsl
    water.rtsl
    world.cpp
)
set_source_files_properties(terrain.rtsl water.rtsl PROPERTIES HEADER_FILE_ONLY TRUE)

rutile_configure_example(rutile-05-voxel-renderer)
rtsl_add_program(rutile-05-voxel-terrain
    SYMBOL terrain_rtslp
    SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/terrain.rtsl"
)
rtsl_add_program(rutile-05-voxel-water
    SYMBOL water_rtslp
    SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/water.rtsl"
)
rtsl_embed_program(rutile-05-voxel-renderer
    PROGRAMS
        rutile-05-voxel-terrain
        rutile-05-voxel-water
)
