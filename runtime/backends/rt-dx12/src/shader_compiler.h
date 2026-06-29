#ifndef RTDX_SHADER_COMPILER_H
#define RTDX_SHADER_COMPILER_H

#include "../../../../RTSL/backends/hlsl/include/rtsl_hlsl_backend.h"

typedef rtsl_graphics_hlsl_compile_result rtdx_graphics_hlsl_compile_result;
static inline void rtdx_graphics_hlsl_result_clear(rtdx_graphics_hlsl_compile_result* result) {
	rtsl_graphics_hlsl_result_clear(result);
}
static inline rtdx_graphics_hlsl_compile_result rtdx_shader_compile_graphics_rtslp(
	u64 program_size,
	const void* program_source
) {
	return rtsl_compile_graphics_rtslp_hlsl(program_size, program_source);
}

#endif /* RTDX_SHADER_COMPILER_H */
