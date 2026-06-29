cmake_minimum_required(VERSION 3.28.0)

# The rtslp package format is RTSL's linker output. This target provides the
# in-memory reader (RTArtifactModule + friends) that backend emitters and
# runtimes consume; the matching writer lives in RTSL/src/Serialization.
add_library(rtsl-package STATIC
    "src/rtslp_package.cpp"
    "include/rtslp_package.hpp"
)
target_compile_features(rtsl-package PUBLIC cxx_std_23)
if(MSVC)
    set_target_properties(rtsl-package PROPERTIES
        MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>"
    )
endif()
target_include_directories(rtsl-package
    PUBLIC
        "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>"
)
target_link_libraries(rtsl-package PUBLIC rutile)
