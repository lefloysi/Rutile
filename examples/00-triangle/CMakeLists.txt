add_executable(rutile-00-triangle main.cpp)
rutile_configure_example(rutile-00-triangle)

add_custom_command(TARGET rutile-00-triangle POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/triangle.rtslp"
        "$<TARGET_FILE_DIR:rutile-00-triangle>/triangle.rtslp"
)
