#ifndef RTSL_SPIRV_BACKEND_H
#define RTSL_SPIRV_BACKEND_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct rtsl_uniform_info {
	char name[64];
	char type[64];
	uint32_t set;
	uint32_t binding;
} rtsl_uniform_info;

typedef struct rtsl_graphics_spirv {
	uint32_t* vertex_words;
	size_t vertex_word_count;
	uint32_t* fragment_words;
	size_t fragment_word_count;
	rtsl_uniform_info* uniforms;
	size_t uniform_count;
} rtsl_graphics_spirv;

typedef enum rtsl_compile_status {
	RTSL_COMPILE_OK = 0,
	RTSL_COMPILE_INVALID_ARGUMENT = 1,
	RTSL_COMPILE_OUT_OF_MEMORY = 2,
	RTSL_COMPILE_FAILED = 3,
} rtsl_compile_status;

rtsl_compile_status rtsl_compile_rtslp_graphics(size_t program_size, const void* program_source, rtsl_graphics_spirv* out);

void rtsl_graphics_spirv_free(rtsl_graphics_spirv* module);

#ifdef __cplusplus
}
#endif

#endif /* RTSL_SPIRV_BACKEND_H */
