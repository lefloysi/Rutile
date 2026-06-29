cmake_minimum_required(VERSION 3.20)

# The C binding for the RTSL compiler library.
#
# Today this only carries the public header. The compiled C-ABI library is
# still emitted by the top-level CMakeLists as `rtsl`. Long-term we expect
# this directory to host an `rtsl_c` interface (or wrapper) so downstream C
# consumers can depend on the C ABI without pulling in the C++ headers.

add_library(rtsl-headers INTERFACE)
add_library(RTSL::headers ALIAS rtsl-headers)

target_include_directories(rtsl-headers
    INTERFACE
        "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>"
)
