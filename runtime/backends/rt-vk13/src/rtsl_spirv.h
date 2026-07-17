#ifndef RTSL_SPIRV_H
#define RTSL_SPIRV_H

#include "types.h"

typedef enum rtsl_spirv_status {
	RTSL_SPIRV_SUCCESS,
	RTSL_SPIRV_INVALID_PROGRAM,
	RTSL_SPIRV_TRANSPILATION_FAILED,
	RTSL_SPIRV_OUT_OF_MEMORY,
} rtsl_spirv_status;

typedef enum rtsl_spirv_stage {
	RTSL_SPIRV_VERTEX,
	RTSL_SPIRV_FRAGMENT,
} rtsl_spirv_stage;

typedef enum rtsl_spirv_stage_flags {
	RTSL_SPIRV_STAGE_VERTEX = 1 << 0,
	RTSL_SPIRV_STAGE_FRAGMENT = 1 << 1,
} rtsl_spirv_stage_flags;

typedef enum rtsl_spirv_resource_kind {
	RTSL_SPIRV_UNIFORM_BUFFER,
	RTSL_SPIRV_STORAGE_BUFFER,
	RTSL_SPIRV_SAMPLER,
	RTSL_SPIRV_SAMPLED_TEXTURE,
	RTSL_SPIRV_STORAGE_IMAGE,
} rtsl_spirv_resource_kind;

typedef struct rtsl_spirv_resource_info {
	const char* name;
	u32 descriptor_set;
	u32 binding;
	u32 stages;
	rtsl_spirv_resource_kind kind;
} rtsl_spirv_resource_info;

typedef struct rtsl_spirv_translation rtsl_spirv_translation;

#ifdef __cplusplus
extern "C" {
#endif

rtsl_spirv_status rtsl_spirv_translate(
	u64 size,
	const void* data,
	rtsl_spirv_translation** translation,
	char* message,
	u64 message_capacity
);
void rtsl_spirv_translation_destroy(rtsl_spirv_translation* translation);
const u32* rtsl_spirv_stage_words(
	const rtsl_spirv_translation* translation,
	rtsl_spirv_stage stage,
	u64* word_count
);
u32 rtsl_spirv_resource_count(const rtsl_spirv_translation* translation);
bool rtsl_spirv_resource(
	const rtsl_spirv_translation* translation,
	u32 index,
	rtsl_spirv_resource_info* resource
);

#ifdef __cplusplus
}
#endif

#endif
