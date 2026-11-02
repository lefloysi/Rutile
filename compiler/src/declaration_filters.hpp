#pragma once

#include <cstddef>

struct NativeDeclarationFilters;

struct NativeDeclarationAttribute {
	const char* key;
	std::size_t key_size;
	const char* value;
	std::size_t value_size;
};

extern "C" NativeDeclarationFilters* declaration_filters_open(const char* json, std::size_t size);
extern "C" void declaration_filters_close(NativeDeclarationFilters* filters);
extern "C" int declaration_filters_apply(NativeDeclarationFilters* filters, const NativeDeclarationAttribute* attributes, std::size_t count);
extern "C" const char* declaration_filters_error();

struct Compiler;
extern "C" int rutile_compiler_set_declaration_filters(Compiler* compiler, NativeDeclarationFilters* filters);
extern "C" int rutile_compiler_excluded_declarations(Compiler* compiler, void (*visit)(void*, std::size_t, std::size_t, std::size_t), void* context);
