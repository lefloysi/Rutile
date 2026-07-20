add_executable(rt-gl33-workspace
    main.cpp
)

target_compile_features(rt-gl33-workspace PRIVATE cxx_std_23)

rutile_set_common_output_dirs(rt-gl33-workspace)
set_target_properties(rt-gl33-workspace PROPERTIES
    VS_DEBUGGER_WORKING_DIRECTORY "$<TARGET_FILE_DIR:rt-gl33-workspace>"
    ENABLE_EXPORTS ON
)
if(MSVC)
    set_target_properties(rt-gl33-workspace PROPERTIES
        MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>"
    )
endif()

target_link_libraries(rt-gl33-workspace PRIVATE rutile)
target_link_libraries(rt-gl33-workspace PRIVATE glfw)
rutile_copy_runtime_dependencies(rt-gl33-workspace)
