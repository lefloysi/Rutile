#ifndef RTSL_HLSL_BACKEND_H
#define RTSL_HLSL_BACKEND_H

#include <stddef.h>
#include <stdint.h>

typedef uint32_t u32;
typedef uint64_t u64;

#ifndef RTSL_HLSL_MAX_UNIFORM_NAME
#define RTSL_HLSL_MAX_UNIFORM_NAME 64
#endif

#ifdef __cplusplus
extern "C" {
#endif

typedef enum rtsl_hlsl_uniform_kind {
	RTSL_HLSL_UNIFORM_BUFFER = 0,
	RTSL_HLSL_UNIFORM_TEXTURE_SAMPLED = 1,
} rtsl_hlsl_uniform_kind;

typedef struct rtsl_hlsl_uniform_info {
	char name[RTSL_HLSL_MAX_UNIFORM_NAME];
	u32 binding;
	rtsl_hlsl_uniform_kind kind;
} rtsl_hlsl_uniform_info;

typedef struct rtsl_graphics_hlsl_compile_result {
	char* vertex_hlsl;
	u64 vertex_hlsl_size;
	char* fragment_hlsl;
	u64 fragment_hlsl_size;
	rtsl_hlsl_uniform_info* uniforms;
	u32 uniform_count;
} rtsl_graphics_hlsl_compile_result;

void rtsl_graphics_hlsl_result_clear(rtsl_graphics_hlsl_compile_result* result);
rtsl_graphics_hlsl_compile_result rtsl_compile_graphics_rtslp_hlsl(u64 program_size, const void* program_source);

#ifdef __cplusplus
}
#endif

#endif /* RTSL_HLSL_BACKEND_H */
