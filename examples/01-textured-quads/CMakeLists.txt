add_executable(rutile-01-textured-quads
    main.cpp
    ../shared/stb_image.cpp
    ../shared/stb_image_write.cpp
    ../shared/util.h
)

rutile_configure_example(rutile-01-textured-quads)

set(rtslc_exe "${RUTILE_SOURCE_DIR}/../RTSL/out/build/rtslc.exe")
set(textured_quads_object "${CMAKE_CURRENT_BINARY_DIR}/textured_quads.rtslo")
set(textured_quads_program "${CMAKE_CURRENT_SOURCE_DIR}/textured_quads.rtslp")

add_custom_command(
    OUTPUT "${textured_quads_program}"
    COMMAND "${rtslc_exe}"
        -c "${CMAKE_CURRENT_SOURCE_DIR}/textured_quads.rtsl"
        -o "${textured_quads_object}"
    COMMAND "${rtslc_exe}"
        --link-program "${textured_quads_object}"
        -o "${textured_quads_program}"
    DEPENDS
        "${CMAKE_CURRENT_SOURCE_DIR}/textured_quads.rtsl"
        "${rtslc_exe}"
    VERBATIM
)

add_custom_target(rutile-01-textured-quads-shader DEPENDS "${textured_quads_program}")
add_dependencies(rutile-01-textured-quads rutile-01-textured-quads-shader)

add_custom_command(TARGET rutile-01-textured-quads POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${textured_quads_program}"
        "$<TARGET_FILE_DIR:rutile-01-textured-quads>/textured_quads.rtslp"
)

add_custom_command(TARGET rutile-01-textured-quads POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${RUTILE_SOURCE_DIR}/rutile.png"
        $<TARGET_FILE_DIR:rutile-01-textured-quads>
)
