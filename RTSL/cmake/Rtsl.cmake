function(rtsl_add_program target_name)
    set(options EMBED)
    set(oneValueArgs RTSLC OUTPUT_DIR NAMESPACE)
    set(multiValueArgs SOURCES DEPENDS INCLUDE_DIRS)
    cmake_parse_arguments(RTSL "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

    if(NOT RTSL_RTSLC)
        message(FATAL_ERROR "rtsl_add_program(${target_name}) requires RTSLC to be set")
    endif()
    if(NOT RTSL_SOURCES)
        message(FATAL_ERROR "rtsl_add_program(${target_name}) requires at least one source")
    endif()
    if(NOT RTSL_OUTPUT_DIR)
        set(RTSL_OUTPUT_DIR "${CMAKE_CURRENT_BINARY_DIR}/rtsl/${target_name}")
    endif()
    if(NOT RTSL_NAMESPACE)
        set(RTSL_NAMESPACE "${target_name}")
    endif()

    file(MAKE_DIRECTORY "${RTSL_OUTPUT_DIR}")

    set(outputs)
    foreach(source IN LISTS RTSL_SOURCES)
        get_filename_component(source_name "${source}" NAME_WE)
        set(object_path "${RTSL_OUTPUT_DIR}/${source_name}.rtslo")
        set(program_path "${RTSL_OUTPUT_DIR}/${source_name}.rtslp")

        add_custom_command(
            OUTPUT "${object_path}" "${program_path}"
            COMMAND "$<TARGET_FILE:rtslc>" compile "${source}" -o "${object_path}"
            COMMAND "$<TARGET_FILE:rtslc>" link-program "${object_path}" -o "${program_path}"
            DEPENDS "${source}" rtslc ${RTSL_DEPENDS}
            VERBATIM
            COMMENT "RTSL ${source_name} -> ${target_name}"
        )

        list(APPEND outputs "${program_path}")
    endforeach()

    add_custom_target("${target_name}-rtsl" DEPENDS ${outputs})
    add_dependencies("${target_name}" "${target_name}-rtsl")

    if(RTSL_EMBED)
        set(embed_cpp "${RTSL_OUTPUT_DIR}/${target_name}_rtsl_embed.cpp")
        string(JOIN ";" embed_inputs ${outputs})
        add_custom_command(
            OUTPUT "${embed_cpp}"
            COMMAND "${CMAKE_COMMAND}"
                -DRTSL_EMBED_INPUTS=${embed_inputs}
                -DRTSL_EMBED_OUTPUT=${embed_cpp}
                -DRTSL_EMBED_HEADER_NAME=rtsl_embed.hpp
                -DRTSL_EMBED_NAMESPACE=${RTSL_NAMESPACE}
                -P "${CMAKE_CURRENT_FUNCTION_LIST_FILE}"
            DEPENDS ${outputs}
            VERBATIM
            COMMENT "Embedding RTSL programs for ${target_name}"
        )
        target_sources("${target_name}" PRIVATE "${embed_cpp}")
        target_include_directories("${target_name}" PRIVATE "${RTSL_OUTPUT_DIR}")
    endif()
endfunction()

if(DEFINED RTSL_EMBED_INPUTS AND DEFINED RTSL_EMBED_OUTPUT AND DEFINED RTSL_EMBED_HEADER_NAME AND DEFINED RTSL_EMBED_NAMESPACE)
    file(WRITE "${RTSL_EMBED_OUTPUT}" "#include <cstddef>\n#include <cstdint>\n#include \"${RTSL_EMBED_HEADER_NAME}\"\n\n")

    foreach(input_path IN LISTS RTSL_EMBED_INPUTS)
        get_filename_component(input_name "${input_path}" NAME_WE)
        file(READ "${input_path}" _hex HEX)

        file(APPEND "${RTSL_EMBED_OUTPUT}" "namespace ${RTSL_EMBED_NAMESPACE} {\n")
        file(APPEND "${RTSL_EMBED_OUTPUT}" "alignas(16) const std::uint8_t ${input_name}_rtslp[] = {\n")

        string(LENGTH "${_hex}" _hex_length)
        math(EXPR _byte_count "${_hex_length} / 2")
        if(_byte_count GREATER 0)
            math(EXPR _last_index "${_byte_count} - 1")
            foreach(index RANGE 0 "${_last_index}")
                math(EXPR _offset "${index} * 2")
                math(EXPR _next_index "${index} + 1")
                math(EXPR _column "${_next_index} % 12")
                string(SUBSTRING "${_hex}" "${_offset}" 2 _byte)
                if(_next_index EQUAL _byte_count)
                    file(APPEND "${RTSL_EMBED_OUTPUT}" "    0x${_byte}\n")
                elseif(_column EQUAL 0)
                    file(APPEND "${RTSL_EMBED_OUTPUT}" "    0x${_byte},\n")
                else()
                    file(APPEND "${RTSL_EMBED_OUTPUT}" "    0x${_byte}, ")
                endif()
            endforeach()
        endif()

        file(APPEND "${RTSL_EMBED_OUTPUT}" "};\n")
        file(APPEND "${RTSL_EMBED_OUTPUT}" "const std::size_t ${input_name}_rtslp_size = sizeof(${input_name}_rtslp);\n")
        file(APPEND "${RTSL_EMBED_OUTPUT}" "}\n\n")
    endforeach()
endif()
