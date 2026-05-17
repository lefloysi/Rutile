add_executable(rutile-example
    main.cpp
    ../shared/stb_image.cpp
    ../shared/stb_image_write.cpp
    ../shared/util.h
)

rutile_configure_example(rutile-example)

add_custom_command(TARGET rutile-example POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${RUTILE_SOURCE_DIR}/rutile.png"
        $<TARGET_FILE_DIR:rutile-example>
)
