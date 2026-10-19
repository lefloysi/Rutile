#pragma once

#include "../src/compiler.hpp"

#include <cstdio>

struct Compiler;

extern "C" Compiler* rutile_compiler_create();
extern "C" void rutile_compiler_destroy(Compiler* compiler);
extern "C" int rutile_compiler_session(int (*callback)(Compiler*, void*), void* data);
extern "C" int rutile_compile(Compiler* compiler, const char* const* paths, std::size_t count, const char* output, std::FILE* (*opener)(const char*, const char*), const char* entry, int optimization, std::FILE* diagnostics);
extern "C" int rutile_compile_library(const char* const* paths, std::size_t count, const char* output, std::FILE* (*opener)(const char*, const char*), int optimization, std::FILE* diagnostics);
extern "C" int kiln_emit_target(Compiler* compiler, const char* root, const char* name, const char* output, const char* entry, int optimization, std::FILE* diagnostics);
extern "C" int kiln_language_server(Compiler* compiler, std::FILE* input, std::FILE* output, const char* root);

std::unique_ptr<CompilerIntegration> kiln_rutile_integration(Compiler* borrowed = nullptr);
std::vector<std::filesystem::path> kiln_rutile_standard_sources();
int kiln_rutile_language_server(const char* root);
int kiln_compile_objects(const std::vector<std::string>& inputs, const std::string& output, int optimization);
