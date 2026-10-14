cmake_minimum_required(VERSION 3.25)

set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT "Embedded")
unset(RUTILE_VERSION CACHE)
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/version/release.txt")
file(READ "${CMAKE_CURRENT_SOURCE_DIR}/version/release.txt" RUTILE_VERSION)
string(STRIP "${RUTILE_VERSION}" RUTILE_VERSION)
if(NOT RUTILE_VERSION MATCHES "^0\\.0\\.[1-9][0-9]*$")
	message(FATAL_ERROR "version/release.txt must contain a release version in the form 0.0.patch.")
endif()
set(RUTILE_PROTOCOL_VERSION "1" CACHE STRING "Rutile communication protocol version")
if(NOT RUTILE_PROTOCOL_VERSION MATCHES "^[1-9][0-9]*$" OR RUTILE_PROTOCOL_VERSION GREATER "4294967295")
	message(FATAL_ERROR "RUTILE_PROTOCOL_VERSION must be a positive 32-bit integer.")
endif()
project("Rutile" VERSION "${RUTILE_VERSION}" LANGUAGES "C" "CXX")

set(CMAKE_CXX_STANDARD "23")
set(CMAKE_CXX_STANDARD_REQUIRED "ON")
set(CMAKE_CXX_EXTENSIONS "OFF")
find_package("CLI11" CONFIG REQUIRED)
find_package("nlohmann_json" CONFIG REQUIRED)
find_package("yaml-cpp" CONFIG REQUIRED)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")

if(CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
	if(WIN32 AND DEFINED ENV{LOCALAPPDATA})
		set(CMAKE_INSTALL_PREFIX "$ENV{LOCALAPPDATA}/Programs/Rutile" CACHE PATH "Rutile installation directory" FORCE)
	elseif(UNIX AND DEFINED ENV{HOME})
		set(CMAKE_INSTALL_PREFIX "$ENV{HOME}/.local" CACHE PATH "Rutile installation directory" FORCE)
	endif()
endif()

add_subdirectory("base")
include("cmake/llvm.cmake")
include("cmake/linker.cmake")

add_library("rutile-compiler" STATIC)
rutile_configure_llvm("rutile-compiler")

option(RUTILE_BOOTSTRAP_FROM_C "Use the historical C compiler instead of the distributed compiler" OFF)
set(RUTILE_BOOTSTRAP_COMPILER "${CMAKE_CURRENT_SOURCE_DIR}/distro/windows-x64/compiler.exe" CACHE FILEPATH "Native bootstrap compiler")

if(RUTILE_BOOTSTRAP_FROM_C)
	add_library("rutile-bootstrap-compiler" STATIC)
	target_sources("rutile-bootstrap-compiler" PRIVATE "compiler/src/compiler.c")
	target_include_directories("rutile-bootstrap-compiler" PUBLIC "compiler/include")
	set_target_properties("rutile-bootstrap-compiler" PROPERTIES C_STANDARD "17" C_STANDARD_REQUIRED "ON" C_EXTENSIONS "OFF")
	target_link_libraries("rutile-bootstrap-compiler" PRIVATE "rutile-llvm")
	if(MSVC)
		target_compile_definitions("rutile-bootstrap-compiler" PRIVATE "_CRT_SECURE_NO_WARNINGS")
	endif()

	add_executable("rutile-bootstrap")
	target_sources("rutile-bootstrap" PRIVATE "compiler/bootstrap.cpp")
	target_link_libraries("rutile-bootstrap" PRIVATE "rutile-bootstrap-compiler" "rutile-version" "base" "CLI11::CLI11")
	if(WIN32)
		add_custom_command(TARGET "rutile-bootstrap" POST_BUILD COMMAND "${CMAKE_COMMAND}" "-E" "copy_if_different" "$<TARGET_FILE:rutile-llvm>" "$<TARGET_FILE_DIR:rutile-bootstrap>" VERBATIM)
	endif()

	set(rutile_seed_target "rutile-bootstrap")
else()
	if(NOT EXISTS "${RUTILE_BOOTSTRAP_COMPILER}")
		message(FATAL_ERROR "Missing distributed compiler: ${RUTILE_BOOTSTRAP_COMPILER}. Supply a native RUTILE_BOOTSTRAP_COMPILER.")
	endif()
	if(RUTILE_BOOTSTRAP_COMPILER STREQUAL "${CMAKE_CURRENT_SOURCE_DIR}/distro/windows-x64/compiler.exe" AND (NOT WIN32 OR NOT CMAKE_SIZEOF_VOID_P EQUAL "8" OR NOT CMAKE_SYSTEM_PROCESSOR MATCHES "^(AMD64|amd64|x86_64|X86_64)$"))
		message(FATAL_ERROR "The bundled compiler requires Windows x64. Supply a native RUTILE_BOOTSTRAP_COMPILER.")
	endif()
	add_executable("rutile-distro-compiler" IMPORTED GLOBAL)
	set_target_properties("rutile-distro-compiler" PROPERTIES IMPORTED_LOCATION "${RUTILE_BOOTSTRAP_COMPILER}")
	set(rutile_seed_target "rutile-distro-compiler")
endif()

if(CMAKE_CROSSCOMPILING)
	message(FATAL_ERROR "Bootstrapping the Rutile compiler currently requires a native build.")
endif()
if(WIN32)
	set(rutile_compiler_object "${CMAKE_BINARY_DIR}/compiler/compiler.obj")
else()
	set(rutile_compiler_object "${CMAKE_BINARY_DIR}/compiler/compiler.o")
endif()
add_custom_command(OUTPUT "${rutile_compiler_object}"
	COMMAND "$<TARGET_FILE:${rutile_seed_target}>" "${CMAKE_CURRENT_SOURCE_DIR}/compiler/src/compiler.rt" "--output" "${rutile_compiler_object}" "--optimization" "$<IF:$<CONFIG:Debug>,0,2>"
	DEPENDS "${rutile_seed_target}" "$<TARGET_FILE:${rutile_seed_target}>" "compiler/src/compiler.rt"
	VERBATIM)
set_source_files_properties("${rutile_compiler_object}" PROPERTIES GENERATED "ON" EXTERNAL_OBJECT "ON")
target_sources("rutile-compiler" PRIVATE "${rutile_compiler_object}")
target_include_directories("rutile-compiler" PUBLIC "compiler/include")
target_link_libraries("rutile-compiler" PRIVATE "rutile-llvm")
set_target_properties("rutile-compiler" PROPERTIES LINKER_LANGUAGE "C")

add_executable("rutile-selfhost" EXCLUDE_FROM_ALL)
target_sources("rutile-selfhost" PRIVATE "compiler/bootstrap.cpp")
target_link_libraries("rutile-selfhost" PRIVATE "rutile-compiler" "rutile-version" "base" "CLI11::CLI11")

if(WIN32)
	set(rutile_selfhost_object "${CMAKE_BINARY_DIR}/compiler/selfhost.obj")
	set(rutile_stage3_object "${CMAKE_BINARY_DIR}/compiler/stage3.obj")
else()
	set(rutile_selfhost_object "${CMAKE_BINARY_DIR}/compiler/selfhost.o")
	set(rutile_stage3_object "${CMAKE_BINARY_DIR}/compiler/stage3.o")
endif()
add_custom_command(OUTPUT "${rutile_selfhost_object}"
	COMMAND "$<TARGET_FILE:rutile-selfhost>" "${CMAKE_CURRENT_SOURCE_DIR}/compiler/src/compiler.rt" "--output" "${rutile_selfhost_object}" "--optimization" "$<IF:$<CONFIG:Debug>,0,2>"
	DEPENDS "rutile-selfhost" "compiler/src/compiler.rt"
	VERBATIM)
set_source_files_properties("${rutile_selfhost_object}" PROPERTIES GENERATED "ON" EXTERNAL_OBJECT "ON")
add_executable("rutile-stage2" EXCLUDE_FROM_ALL)
target_sources("rutile-stage2" PRIVATE "compiler/bootstrap.cpp" "${rutile_selfhost_object}")
target_include_directories("rutile-stage2" PRIVATE "compiler/include")
target_link_libraries("rutile-stage2" PRIVATE "rutile-llvm" "rutile-version" "base" "CLI11::CLI11")

add_custom_command(OUTPUT "${rutile_stage3_object}"
	COMMAND "$<TARGET_FILE:rutile-stage2>" "${CMAKE_CURRENT_SOURCE_DIR}/compiler/src/compiler.rt" "--output" "${rutile_stage3_object}" "--optimization" "$<IF:$<CONFIG:Debug>,0,2>"
	DEPENDS "rutile-stage2" "compiler/src/compiler.rt"
	VERBATIM)
add_custom_target("rutile-bootstrap-check"
	COMMAND "${CMAKE_COMMAND}" "-E" "compare_files" "${rutile_selfhost_object}" "${rutile_stage3_object}"
	DEPENDS "${rutile_stage3_object}"
	VERBATIM)

if(WIN32)
	foreach(compiler_target IN ITEMS "rutile-selfhost" "rutile-stage2")
		add_custom_command(TARGET "${compiler_target}" POST_BUILD COMMAND "${CMAKE_COMMAND}" "-E" "copy_if_different" "$<TARGET_FILE:rutile-llvm>" "$<TARGET_FILE_DIR:${compiler_target}>" VERBATIM)
	endforeach()
endif()
include("cmake/distro.cmake")
rutile_configure_distro("rutile-stage2")

add_library("rutile-version" STATIC)
target_sources("rutile-version" PRIVATE "version/runtime.cpp")
target_include_directories("rutile-version" PUBLIC "version")
target_compile_definitions("rutile-version" PRIVATE "RUTILE_VERSION=\"${PROJECT_VERSION}\"" "RUTILE_PROTOCOL_VERSION=${RUTILE_PROTOCOL_VERSION}")

add_library("rutile-project" STATIC)
target_sources("rutile-project" PRIVATE "project/session.cpp" "project/status.cpp" "project/compilation.cpp" "project/execution.cpp" "project/configuration.cpp")
target_include_directories("rutile-project" PUBLIC "project")
target_link_libraries("rutile-project" PRIVATE "base" "rutile-version" "nlohmann_json::nlohmann_json" "yaml-cpp::yaml-cpp")
target_compile_definitions("rutile-project" PRIVATE "KILN_EXECUTABLE_NAME=\"kiln${CMAKE_EXECUTABLE_SUFFIX}\"")

add_executable("rutile")
target_sources("rutile" PRIVATE "rutile/src/main.cpp")
target_link_libraries("rutile" PRIVATE "rutile-project" "rutile-version" "base" "CLI11::CLI11")

add_executable("kiln")
target_sources("kiln" PRIVATE "kiln/src/main.cpp" "kiln/src/server.cpp" "kiln/src/compiler.cpp" "kiln/src/linker.cpp")
target_link_libraries("kiln" PRIVATE "base" "rutile-project" "rutile-version" "rutile-compiler" "CLI11::CLI11" "nlohmann_json::nlohmann_json")
target_compile_definitions("kiln" PRIVATE "RUTILE_EXECUTABLE_SUFFIX=\"${CMAKE_EXECUTABLE_SUFFIX}\"")
rutile_configure_linker("kiln")
if(WIN32)
	add_custom_command(TARGET "kiln" POST_BUILD COMMAND "${CMAKE_COMMAND}" "-E" "copy_if_different" "$<TARGET_FILE:rutile-llvm>" "$<TARGET_FILE_DIR:kiln>" VERBATIM)
endif()

add_dependencies("rutile" "kiln")

install(TARGETS "rutile" "kiln" RUNTIME DESTINATION "bin")
if(WIN32)
	install(FILES "$<TARGET_FILE:rutile-llvm>" DESTINATION "bin")
	get_target_property(YAML_CPP_LIBRARY_TYPE "yaml-cpp::yaml-cpp" TYPE)
	if(YAML_CPP_LIBRARY_TYPE STREQUAL "SHARED_LIBRARY")
		install(FILES "$<TARGET_FILE:yaml-cpp::yaml-cpp>" DESTINATION "bin")
	endif()
endif()
