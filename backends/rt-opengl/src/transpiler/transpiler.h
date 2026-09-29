#ifndef RTGL_SPIRV_TRANSPILER_H
#define RTGL_SPIRV_TRANSPILER_H

#include "../config.h"
#include "../rutile.h"

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum rt_spirv_status {
	RT_SPIRV_SUCCESS,
	RT_SPIRV_INVALID_ARTIFACT,
	RT_SPIRV_INVALID_MODULE,
	RT_SPIRV_UNSUPPORTED_IR,
	RT_SPIRV_OUT_OF_MEMORY,
} rt_spirv_status;

typedef enum rt_spirv_stage {
	RT_SPIRV_VERTEX,
	RT_SPIRV_TESSELLATION_CONTROL,
	RT_SPIRV_TESSELLATION_EVALUATION,
	RT_SPIRV_GEOMETRY,
	RT_SPIRV_FRAGMENT,
	RT_SPIRV_COMPUTE,
	RT_SPIRV_STAGE_COUNT,
} rt_spirv_stage;

typedef enum rt_spirv_location_kind {
	RT_SPIRV_UNIFORM_DATA,
	RT_SPIRV_STORAGE_DATA,
	RT_SPIRV_UNIFORM_BUFFER,
	RT_SPIRV_STORAGE_BUFFER,
	RT_SPIRV_SAMPLED_TEXTURE,
	RT_SPIRV_STORAGE_TEXTURE,
	RT_SPIRV_SAMPLER,
	RT_SPIRV_INPUT_ATTACHMENT,
} rt_spirv_location_kind;

typedef struct rt_spirv_location_info {
	const char* name;
	rt_spirv_location_kind kind;
	u32 stages;
	u32 descriptor_set;
	u32 binding;
	usize offset;
	usize size;
	usize block_size;
} rt_spirv_location_info;

typedef struct rt_spirv_program rt_spirv_program;

RTGL_API rt_spirv_status rt_spirv_transpile(const u08* bytes, usize byte_size, const char* entry_name, rt_spirv_program** program, char* message, usize message_size);
RTGL_API int rt_spirv_validate(const u32* words, usize word_count, char* message, usize message_size);
RTGL_API void rt_spirv_program_destroy(rt_spirv_program* program);
RTGL_API const u32* rt_spirv_stage_words(const rt_spirv_program* program, rt_spirv_stage stage, usize* word_count);
RTGL_API const char* rt_spirv_stage_entry_point(const rt_spirv_program* program, rt_spirv_stage stage);
RTGL_API u32 rt_spirv_location_count(const rt_spirv_program* program);
RTGL_API int rt_spirv_location(const rt_spirv_program* program, u32 index, rt_spirv_location_info* location);

#ifdef __cplusplus
}
#endif

#endif


