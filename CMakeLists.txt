cmake_minimum_required(VERSION 3.24)

project(Rutile VERSION 0.1.0 LANGUAGES C CXX)

include(CMakePackageConfigHelpers)
include(GNUInstallDirs)

option(RUTILE_BUILD_EXAMPLES "Build Rutile examples" ${PROJECT_IS_TOP_LEVEL})
option(RUTILE_INSTALL "Generate Rutile install rules" ${PROJECT_IS_TOP_LEVEL})
option(RUTILE_BUILD_VULKAN "Build the Vulkan Rutile runtime." ON)
option(RUTILE_BUILD_DX12 "Build the DirectX 12 Rutile runtime on Windows." ON)
option(RUTILE_BUILD_VALIDATION_LAYER "Build the Rutile validation layer." ON)
option(RUTILE_BUILD_LOGGING_LAYER "Build the Rutile logging layer." ON)

set(RUTILE_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
set(RUTILE_INSTALL_CMAKEDIR "${CMAKE_INSTALL_LIBDIR}/cmake/Rutile" CACHE STRING "Rutile CMake package install directory")

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(RutileTarget)

add_library(rutile INTERFACE)
add_library(Rutile::rutile ALIAS rutile)
target_include_directories(rutile
    INTERFACE
        "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>"
        "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>"
)

add_subdirectory("backend-tools")

if(RUTILE_BUILD_VULKAN)
    add_subdirectory("rt-vulkan")
endif()
if(RUTILE_BUILD_DX12 AND WIN32)
    add_subdirectory("rt-dx12")
endif()
if(RUTILE_BUILD_VALIDATION_LAYER)
    add_subdirectory("rt-validation")
endif()
if(RUTILE_BUILD_LOGGING_LAYER)
    add_subdirectory("rt-logging-layer")
endif()

if(RUTILE_BUILD_EXAMPLES)
    add_subdirectory("example")
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

    install(DIRECTORY include/ DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}")

    install(TARGETS rutile EXPORT RutileTargets)

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
