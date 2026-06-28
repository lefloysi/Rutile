add_executable(rutile-01-textured-quads
    main.cpp
    textured_quads.rtsl
    ../shared/stb_image.cpp
    ../shared/stb_image_write.cpp
    ../shared/util.h
)
set_source_files_properties(textured_quads.rtsl PROPERTIES HEADER_FILE_ONLY TRUE)

rutile_configure_example(rutile-01-textured-quads)
set(_textured_quads_rtsl_dir "${CMAKE_CURRENT_BINARY_DIR}/rtsl")
rtsl_add_program(rutile-01-textured-quads
    RTSLC rtslc
    OUTPUT_DIR "${_textured_quads_rtsl_dir}"
    NAMESPACE rutile_01_textured_quads
    EMBED
    SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/textured_quads.rtsl"
)
target_include_directories(rutile-01-textured-quads PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}" "${_textured_quads_rtsl_dir}")

add_custom_command(TARGET rutile-01-textured-quads POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${RUTILE_SOURCE_DIR}/rutile.png"
        $<TARGET_FILE_DIR:rutile-01-textured-quads>
)
