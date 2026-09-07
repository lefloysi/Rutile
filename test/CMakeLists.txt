add_executable(rutile-test
	"src/main.cpp"
	"src/reflected_program_data.rtsl"
)

set_source_files_properties("src/reflected_program_data.rtsl" PROPERTIES HEADER_FILE_ONLY TRUE)
target_compile_features(rutile-test PRIVATE cxx_std_23)
target_include_directories(rutile-test PRIVATE "src")
target_link_libraries(rutile-test PRIVATE rutile)
set_target_properties(rutile-test PROPERTIES
	RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

set(program_artifact "${CMAKE_CURRENT_BINARY_DIR}/reflected_program_data.rtslp")
set(program_source "${CMAKE_CURRENT_BINARY_DIR}/reflected_program_data.cpp")
add_custom_command(
	OUTPUT "${program_artifact}"
	COMMAND "$<TARGET_FILE:rtslc>"
		"${CMAKE_CURRENT_SOURCE_DIR}/src/reflected_program_data.rtsl"
		--module reflected_program_data
		--emit-program
		-o "${program_artifact}"
	DEPENDS rtslc "src/reflected_program_data.rtsl"
	VERBATIM
)
add_custom_command(
	OUTPUT "${program_source}"
	COMMAND "${CMAKE_COMMAND}"
		"-DINPUT=${program_artifact}"
		"-DOUTPUT=${program_source}"
		"-DSYMBOL=reflected_program_data_rtslp"
		"-DHEADER=embedded_program"
		"-DTYPE=rt_example_program"
		-P "${RUTILE_SOURCE_DIR}/cmake/embed_program.cmake"
	DEPENDS "${program_artifact}" "${RUTILE_SOURCE_DIR}/cmake/embed_program.cmake"
	VERBATIM
)
set_source_files_properties("${program_source}" PROPERTIES GENERATED TRUE)
target_sources(rutile-test PRIVATE "${program_source}")

foreach(backend IN ITEMS rt-vulkan rt-d3d12 rt-opengl)
	if(TARGET "${backend}")
		add_dependencies(rutile-test "${backend}")
		add_test(
			NAME "rutile.${backend}.reflected-program-data"
			COMMAND "$<TARGET_FILE:rutile-test>" "${backend}"
		)
		set_tests_properties("rutile.${backend}.reflected-program-data" PROPERTIES
			WORKING_DIRECTORY "$<TARGET_FILE_DIR:rutile-test>"
		)
	endif()
endforeach()
