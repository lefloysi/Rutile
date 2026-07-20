cmake_minimum_required(VERSION 3.28.0)

project(rt-dx12 CXX)

if(NOT WIN32)
    return()
endif()

include(RutileTarget)

add_library(${PROJECT_NAME} SHARED)
target_sources(${PROJECT_NAME} PRIVATE
    "src/atomic.hpp"
    "src/config.hpp"
    "src/context.cpp"
    "src/context.hpp"
    "src/core.cpp"
    "src/core.hpp"
    "src/error.cpp"
    "src/error.hpp"
    "src/init.cpp"
    "src/types.hpp"
    "src/resource/buffer.cpp"
    "src/resource/buffer.hpp"
    "src/resource/command_buffer.cpp"
    "src/resource/command_buffer.hpp"
    "src/resource/framebuffer.cpp"
    "src/resource/framebuffer.hpp"
    "src/resource/graphics_program.cpp"
    "src/resource/graphics_program.hpp"
    "src/resource/queue.cpp"
    "src/resource/queue.hpp"
    "src/resource/resource.cpp"
    "src/resource/resource.hpp"
    "src/resource/swapchain.cpp"
    "src/resource/swapchain.hpp"
    "src/resource/texture.cpp"
    "src/resource/texture.hpp"
    "src/resource/glfw/glfw.cpp"
    "src/resource/glfw/glfw.hpp"
)

target_include_directories(${PROJECT_NAME} PRIVATE "src")
target_compile_definitions(${PROJECT_NAME} PRIVATE
    $<$<CONFIG:Debug>:RTDX_ENABLE_D3D12_VALIDATION>
)

rutile_configure_shared_library(${PROJECT_NAME})

target_link_libraries(${PROJECT_NAME}
    PRIVATE
        d3d12
        dxgi
        dxguid
        RTSL::hlsl
)

find_library(_rtdx_dxcompiler_lib dxcompiler)
if(_rtdx_dxcompiler_lib)
    target_link_libraries(${PROJECT_NAME} PRIVATE "${_rtdx_dxcompiler_lib}")
else()
    target_link_libraries(${PROJECT_NAME} PRIVATE dxcompiler)
endif()

find_file(_rtdx_dxcompiler_dll dxcompiler.dll HINTS
    "${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/bin"
    "${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/debug/bin"
    "${CMAKE_WINDOWS_KITS_10_DIR}/Redist/D3D/x64"
    "C:/Program Files (x86)/Windows Kits/10/Redist/D3D/x64"
    "$ENV{VULKAN_SDK}/Bin"
)
find_file(_rtdx_dxil_dll dxil.dll HINTS
    "${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/bin"
    "${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/debug/bin"
    "${CMAKE_WINDOWS_KITS_10_DIR}/Redist/D3D/x64"
    "C:/Program Files (x86)/Windows Kits/10/Redist/D3D/x64"
    "$ENV{VULKAN_SDK}/Bin"
)
if(_rtdx_dxcompiler_dll AND _rtdx_dxil_dll)
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${_rtdx_dxcompiler_dll}"
            "${_rtdx_dxil_dll}"
            "$<TARGET_FILE_DIR:${PROJECT_NAME}>"
    )
    if(RUTILE_INSTALL)
        install(FILES
            "${_rtdx_dxcompiler_dll}"
            "${_rtdx_dxil_dll}"
            DESTINATION "${CMAKE_INSTALL_BINDIR}"
        )
    endif()
else()
    message(FATAL_ERROR "rt-dx12 requires redistributable dxcompiler.dll and dxil.dll")
endif()
