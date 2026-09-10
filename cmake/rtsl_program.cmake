function(rtsl_add_program name)
	cmake_parse_arguments(PROGRAM "" "" "SOURCES" ${ARGN})
	if(NOT PROGRAM_SOURCES)
		message(FATAL_ERROR "rtsl_add_program requires SOURCES")
	endif()
	list(LENGTH PROGRAM_SOURCES source_count)
	if(NOT source_count EQUAL 1)
		message(FATAL_ERROR "rtsl_add_program currently requires one aggregate RTSL source")
	endif()
	list(GET PROGRAM_SOURCES 0 source)
	set(program_artifact "${CMAKE_CURRENT_BINARY_DIR}/${name}.rtslp")
	get_filename_component(module_name "${source}" NAME_WE)

	add_custom_command(
		OUTPUT "${program_artifact}"
		COMMAND "$<TARGET_FILE:rtslc>" "${source}" --module "${module_name}" --emit-program -o "${program_artifact}"
		DEPENDS rtslc "${source}"
		VERBATIM
	)
	add_custom_target("${name}" DEPENDS "${program_artifact}")
	set_property(TARGET "${name}" PROPERTY RTSL_PROGRAM_ARTIFACT "${program_artifact}")
endfunction()

function(rtsl_embed_program target)
	cmake_parse_arguments(EMBED "" "SYMBOL" "PROGRAMS" ${ARGN})
	if(NOT DEFINED EMBED_SYMBOL OR "${EMBED_SYMBOL}" STREQUAL "" OR NOT DEFINED EMBED_PROGRAMS OR "${EMBED_PROGRAMS}" STREQUAL "")
		message(FATAL_ERROR "rtsl_embed_program requires SYMBOL and PROGRAMS")
	endif()
	list(LENGTH EMBED_PROGRAMS program_count)
	if(NOT program_count EQUAL 1)
		message(FATAL_ERROR "rtsl_embed_program requires one aggregate program")
	endif()
	list(GET EMBED_PROGRAMS 0 program)
	set(symbol "${EMBED_SYMBOL}")
	set(embed_program_script "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/embed_program.cmake")
	if(NOT TARGET "${program}")
		message(FATAL_ERROR "rtsl_embed_program references unknown RTSL program '${program}'")
	endif()
	get_target_property(program_artifact "${program}" RTSL_PROGRAM_ARTIFACT)
	set(program_source "${CMAKE_CURRENT_BINARY_DIR}/${symbol}.cpp")

	add_custom_command(
		OUTPUT "${program_source}"
		COMMAND "${CMAKE_COMMAND}"
			"-DINPUT=${program_artifact}"
			"-DOUTPUT=${program_source}"
			"-DSYMBOL=${symbol}"
			"-DHEADER=leaf/graphics/graphics_program"
			"-DTYPE=rt::program_bytes"
			-P "${embed_program_script}"
		DEPENDS "${program_artifact}" "${embed_program_script}"
		VERBATIM
	)

	set_source_files_properties("${program_source}" PROPERTIES GENERATED TRUE)
	target_sources("${target}" PRIVATE "${program_source}")
	set(embed_target "${target}-${symbol}-embed")
	add_custom_target("${embed_target}" DEPENDS "${program_source}")
	add_dependencies("${target}" "${embed_target}" "${program}")
endfunction()
