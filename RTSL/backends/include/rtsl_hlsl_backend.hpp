#pragma once

#include "rutile.h"

#include <cstdint>

using u32 = std::uint32_t;
using u64 = std::uint64_t;

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

typedef struct rtdx_graphics_hlsl_compile_result {
	char* vertex_hlsl;
	u64 vertex_hlsl_size;
	char* fragment_hlsl;
	u64 fragment_hlsl_size;
	rtsl_hlsl_uniform_info* uniforms;
	u32 uniform_count;
} rtdx_graphics_hlsl_compile_result;

void rtdx_graphics_hlsl_result_clear(rtdx_graphics_hlsl_compile_result* result);
rtdx_graphics_hlsl_compile_result rtdx_shader_compile_graphics_rtslp(
	u64 program_size,
	const void* program_source
);

#ifdef __cplusplus
}
#endif
