add_executable(rutile-05-voxel-renderer
    main.cpp
    terrain.rtsl
    water.rtsl
    world.cpp
)
set_source_files_properties(terrain.rtsl water.rtsl PROPERTIES HEADER_FILE_ONLY TRUE)

rutile_configure_example(rutile-05-voxel-renderer)
set(_voxel_rtsl_dir "${CMAKE_CURRENT_BINARY_DIR}/rtsl")
rtsl_add_program(rutile-05-voxel-renderer
    RTSLC rtslc
    OUTPUT_DIR "${_voxel_rtsl_dir}"
    EMBED
    SOURCES
        "${CMAKE_CURRENT_SOURCE_DIR}/terrain.rtsl"
        "${CMAKE_CURRENT_SOURCE_DIR}/water.rtsl"
)
