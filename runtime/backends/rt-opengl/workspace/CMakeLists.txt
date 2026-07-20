add_executable(rt-opengl-workspace
    main.cpp
)

target_compile_features(rt-opengl-workspace PRIVATE cxx_std_23)

rutile_set_common_output_dirs(rt-opengl-workspace)
set_target_properties(rt-opengl-workspace PROPERTIES
    VS_DEBUGGER_WORKING_DIRECTORY "$<TARGET_FILE_DIR:rt-opengl-workspace>"
    ENABLE_EXPORTS ON
)
if(MSVC)
    set_target_properties(rt-opengl-workspace PROPERTIES
        MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>"
    )
endif()

target_link_libraries(rt-opengl-workspace PRIVATE rutile)
target_link_libraries(rt-opengl-workspace PRIVATE glfw)
rutile_copy_runtime_dependencies(rt-opengl-workspace)
