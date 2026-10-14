#pragma once

#include <stddef.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

struct Compiler;

struct Compiler* rutile_compiler_create(void);
void rutile_compiler_destroy(struct Compiler* compiler);
int rutile_compile(struct Compiler* compiler, const char* const* source_paths, size_t source_count, const char* output_path, FILE* (*open_file)(const char*, const char*), const char* entry, int optimization, FILE* diagnostics);
int rutile_compile_library(struct Compiler* compiler, const char* const* source_paths, size_t source_count, const char* output_path, FILE* (*open_file)(const char*, const char*), int optimization, FILE* diagnostics);

#ifdef __cplusplus
}
#endif
