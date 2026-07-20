include_guard(GLOBAL)

function(rutile_find_vulkan_sdk_headers out_var)
    find_path(_rutile_vulkan_sdk_include
        NAMES vulkan/vulkan.h
        HINTS "$ENV{VULKAN_SDK}/Include"
    )
    set(${out_var} "${_rutile_vulkan_sdk_include}" PARENT_SCOPE)
endfunction()

function(rutile_find_spirv_headers)
    find_package(SPIRV-Headers CONFIG QUIET)
    if(TARGET SPIRV-Headers::SPIRV-Headers)
        return()
    endif()

    find_path(RUTILE_SPIRV_HEADERS_INCLUDE_DIR
        NAMES
            spirv/unified1/spirv.hpp11
            spirv/unified1/GLSL.std.450.h
            spirv-headers/spirv.hpp11
            spirv-headers/GLSL.std.450.h
        HINTS
            "$ENV{VULKAN_SDK}/Include"
            "${Vulkan_INCLUDE_DIRS}"
    )
    if(NOT RUTILE_SPIRV_HEADERS_INCLUDE_DIR)
        message(FATAL_ERROR
            "Could not find SPIR-V headers. Install vcpkg's spirv-headers package "
            "or install the Vulkan SDK and set VULKAN_SDK."
        )
    endif()

    add_library(SPIRV-Headers::SPIRV-Headers INTERFACE IMPORTED)
    target_include_directories(SPIRV-Headers::SPIRV-Headers INTERFACE
        "${RUTILE_SPIRV_HEADERS_INCLUDE_DIR}"
    )
endfunction()

function(rutile_find_vulkan_backend_dependencies)
    find_package(Vulkan QUIET)
    if(NOT Vulkan_FOUND)
        rutile_find_vulkan_sdk_headers(_rutile_vulkan_sdk_include)
        if(NOT _rutile_vulkan_sdk_include)
            message(FATAL_ERROR
                "RUTILE_BUILD_VK13 is ON, but Vulkan headers were not found. "
                "Install the Vulkan SDK and set VULKAN_SDK, or install vulkan-headers with vcpkg."
            )
        endif()

        add_library(Vulkan::Headers INTERFACE IMPORTED)
        target_include_directories(Vulkan::Headers INTERFACE "${_rutile_vulkan_sdk_include}")
    endif()

    find_package(VulkanMemoryAllocator CONFIG REQUIRED)
    find_package(volk CONFIG REQUIRED)
    find_package(Threads REQUIRED)
    rutile_find_spirv_headers()
endfunction()
