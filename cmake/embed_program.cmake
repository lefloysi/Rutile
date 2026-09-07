if(NOT DEFINED INPUT OR NOT DEFINED OUTPUT OR NOT DEFINED SYMBOL OR NOT DEFINED HEADER OR NOT DEFINED TYPE)
	message(FATAL_ERROR "INPUT, OUTPUT, SYMBOL, HEADER, and TYPE are required")
endif()

file(READ "${INPUT}" program_hex HEX)
string(REGEX REPLACE "([0-9a-fA-F][0-9a-fA-F])" "0x\\1, " program_bytes "${program_hex}")
file(WRITE "${OUTPUT}"
	"#include <${HEADER}.hpp>\n\n"
	"static const u08 ${SYMBOL}_data[] = { ${program_bytes} };\n"
	"extern \"C\" const ${TYPE} ${SYMBOL} = { ${SYMBOL}_data, sizeof(${SYMBOL}_data) };\n"
)
