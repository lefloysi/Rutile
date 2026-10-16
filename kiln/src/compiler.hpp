#pragma once

#include <string>
#include <string_view>
#include <cstddef>
#include <cstdio>

struct Compiler;

extern "C" int rutile_compiler_session(int (*callback)(struct Compiler*, void*), void* data);
extern "C" int rutile_compile(struct Compiler* compiler, const char* const* source_paths, std::size_t source_count, const char* output_path, std::FILE* (*open_file)(const char*, const char*), const char* entry, int optimization, std::FILE* diagnostics);

std::string kiln_build(struct Compiler* compiler, std::string_view request);
std::string kiln_list();
