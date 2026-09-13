file(READ "${INPUT}" program_hex HEX)
string(REGEX REPLACE "([0-9a-fA-F][0-9a-fA-F])" "0x\\1, " program_bytes "${program_hex}")
file(WRITE "${OUTPUT}" "static const unsigned char rt_antialias_program[] = { ${program_bytes} };\n")
