#ifndef RTDX_SHADER_COMPILER_H
#define RTDX_SHADER_COMPILER_H

#include "config.h"
#include "resource/graphics_program.h"
#include "types.h"

RTDX_EXTERN_C_ENTER

typedef struct rtdx_graphics_hlsl_compile_result {
	char *vertex_hlsl;
	u64 vertex_hlsl_size;
	char *fragment_hlsl;
	u64 fragment_hlsl_size;
	rtdx_uniform_location *uniform_locations;
	u32 uniform_location_count;
} rtdx_graphics_hlsl_compile_result;

void rtdx_graphics_hlsl_result_clear(rtdx_graphics_hlsl_compile_result *result);

// Lower an RTSLP package to HLSL for the vertex and fragment stages.
// Throws std::exception on failure; the caller is expected to translate this
// into the rtdx error system.
rtdx_graphics_hlsl_compile_result rtdx_shader_compile_graphics_rtslp(
	struct rtdx_graphics_program *program,
	u64 program_size,
	const void *program_source
);

RTDX_EXTERN_C_EXIT

#endif /* RTDX_SHADER_COMPILER_H */
