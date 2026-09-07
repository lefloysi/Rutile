function(rtsl_embed_program target symbol source header type)
	set(embed_program_script "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/embed_program.cmake")
	get_filename_component(module_name "${source}" NAME_WE)
	set(program_artifact "${CMAKE_CURRENT_BINARY_DIR}/${symbol}.rtslp")
	set(program_source "${CMAKE_CURRENT_BINARY_DIR}/${symbol}.cpp")

	add_custom_command(
		OUTPUT "${program_artifact}"
		COMMAND "$<TARGET_FILE:rtslc>" "${source}" --module "${module_name}" --emit-program -o "${program_artifact}"
		DEPENDS rtslc "${source}"
		VERBATIM
	)
	add_custom_command(
		OUTPUT "${program_source}"
		COMMAND "${CMAKE_COMMAND}"
			"-DINPUT=${program_artifact}"
			"-DOUTPUT=${program_source}"
			"-DSYMBOL=${symbol}"
			"-DHEADER=${header}"
			"-DTYPE=${type}"
			-P "${embed_program_script}"
		DEPENDS "${program_artifact}" "${embed_program_script}"
		VERBATIM
	)

	set_source_files_properties("${program_source}" PROPERTIES GENERATED TRUE)
	target_sources("${target}" PRIVATE "${program_source}")
	add_custom_target("${target}-${symbol}" DEPENDS "${program_source}")
	add_dependencies("${target}" "${target}-${symbol}")
endfunction()
