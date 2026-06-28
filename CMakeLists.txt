cmake_minimum_required(VERSION 3.28.0)

if(MSVC)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
    set(CMAKE_SYSTEM_VERSION "10.0.26100.0" CACHE STRING "Windows SDK version" FORCE)
    set(CMAKE_VS_WINDOWS_TARGET_PLATFORM_VERSION "10.0.26100.0" CACHE STRING "Windows SDK version" FORCE)
endif()

project(Rutile VERSION 0.1.0 LANGUAGES C CXX)

include(CMakePackageConfigHelpers)
include(GNUInstallDirs)

option(RUTILE_BUILD_EXAMPLES "Build Rutile examples" ${PROJECT_IS_TOP_LEVEL})
option(RUTILE_INSTALL "Generate Rutile install rules" ${PROJECT_IS_TOP_LEVEL})
option(RUTILE_BUILD_VULKAN "Build the Vulkan Rutile runtime." ON)
option(RUTILE_BUILD_DX12 "Build the DirectX 12 Rutile runtime on Windows." ON)
option(RUTILE_BUILD_GL33 "Build the OpenGL 3.3 Rutile runtime on Windows." ON)
option(RUTILE_BUILD_VALIDATION_LAYER "Build the Rutile validation layer." ON)
option(RUTILE_BUILD_LOGGING_LAYER "Build the Rutile logging layer." ON)

set(RUTILE_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
set(RUTILE_INSTALL_CMAKEDIR "${CMAKE_INSTALL_LIBDIR}/cmake/Rutile" CACHE STRING "Rutile CMake package install directory")

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(RutileTarget)

if(DEFINED VCPKG_INSTALLED_DIR AND DEFINED VCPKG_TARGET_TRIPLET AND NOT VCPKG_TARGET_TRIPLET STREQUAL "")
    list(PREPEND CMAKE_PREFIX_PATH "${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}")
endif()

add_subdirectory("RTSL")

# The Rutile::rutile interface target lives in the C binding so that
# downstream C/C++ consumers can also pull it in directly with
# add_subdirectory(bindings/c) if they want headers without the runtime.
add_subdirectory("bindings/c")

add_subdirectory("runtime")


if(RUTILE_BUILD_EXAMPLES)
    add_subdirectory("examples")
endif()

rutile_get_runtime_targets(RUTILE_REGISTERED_RUNTIME_TARGETS)
if(RUTILE_REGISTERED_RUNTIME_TARGETS)
    add_custom_target(rutile-runtimes DEPENDS ${RUTILE_REGISTERED_RUNTIME_TARGETS})
else()
    add_custom_target(rutile-runtimes)
endif()

if(RUTILE_INSTALL)
    install(RUNTIME_DEPENDENCY_SET RutileRuntimeDependencies
        DESTINATION "${CMAKE_INSTALL_BINDIR}"
        PRE_EXCLUDE_REGEXES
            "api-ms-"
            "ext-ms-"
        POST_EXCLUDE_REGEXES
            ".*[Ww][Ii][Nn][Dd][Oo][Ww][Ss][\\\\/][Ss][Yy][Ss][Tt][Ee][Mm]32[\\\\/].*"
    )

    # Headers and the rutile interface target are installed by bindings/c.

    install(EXPORT RutileTargets
        NAMESPACE Rutile::
        DESTINATION "${RUTILE_INSTALL_CMAKEDIR}"
    )

    configure_package_config_file(
        "${CMAKE_CURRENT_SOURCE_DIR}/cmake/RutileConfig.cmake.in"
        "${CMAKE_CURRENT_BINARY_DIR}/RutileConfig.cmake"
        INSTALL_DESTINATION "${RUTILE_INSTALL_CMAKEDIR}"
    )

    write_basic_package_version_file(
        "${CMAKE_CURRENT_BINARY_DIR}/RutileConfigVersion.cmake"
        VERSION "${PROJECT_VERSION}"
        COMPATIBILITY SameMajorVersion
    )

    install(FILES
        "${CMAKE_CURRENT_BINARY_DIR}/RutileConfig.cmake"
        "${CMAKE_CURRENT_BINARY_DIR}/RutileConfigVersion.cmake"
        "${CMAKE_CURRENT_SOURCE_DIR}/cmake/RutileTarget.cmake"
        DESTINATION "${RUTILE_INSTALL_CMAKEDIR}"
    )
endif()
