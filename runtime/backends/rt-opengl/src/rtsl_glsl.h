#ifndef RTSL_GLSL_H
#define RTSL_GLSL_H

#include "types.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum rtsl_glsl_status {
	RTSL_GLSL_SUCCESS,
	RTSL_GLSL_INVALID_PROGRAM,
	RTSL_GLSL_TRANSPILATION_FAILED,
	RTSL_GLSL_OUT_OF_MEMORY,
} rtsl_glsl_status;

typedef enum rtsl_glsl_stage {
	RTSL_GLSL_VERTEX,
	RTSL_GLSL_FRAGMENT,
} rtsl_glsl_stage;

typedef enum rtsl_glsl_resource_kind {
	RTSL_GLSL_UNIFORM_BUFFER,
	RTSL_GLSL_STORAGE_BUFFER,
	RTSL_GLSL_SAMPLER,
	RTSL_GLSL_SAMPLED_TEXTURE,
	RTSL_GLSL_STORAGE_IMAGE,
} rtsl_glsl_resource_kind;

typedef enum rtsl_glsl_storage_texture_format {
	RTSL_GLSL_STORAGE_TEXTURE_FORMAT_NONE,
	RTSL_GLSL_STORAGE_TEXTURE_FORMAT_RGBA32F,
	RTSL_GLSL_STORAGE_TEXTURE_FORMAT_RGBA32I,
	RTSL_GLSL_STORAGE_TEXTURE_FORMAT_RGBA32UI,
} rtsl_glsl_storage_texture_format;

typedef enum rtsl_glsl_storage_buffer_lowering {
	RTSL_GLSL_STORAGE_BUFFER_AUTO,
	RTSL_GLSL_STORAGE_BUFFER_NATIVE_SSBO,
	RTSL_GLSL_STORAGE_BUFFER_TEXTURE_BUFFER_READONLY_VEC4,
	RTSL_GLSL_STORAGE_BUFFER_UNSUPPORTED,
} rtsl_glsl_storage_buffer_lowering;

typedef struct rtsl_glsl_options {
	u32 version;
	bool separate_shader_objects;
	bool shader_storage_buffer;
	bool texture_buffer;
	rtsl_glsl_storage_buffer_lowering storage_buffer_lowering;
} rtsl_glsl_options;

typedef struct rtsl_glsl_resource_info {
	const char* name;
	u32 descriptor_set;
	u32 binding;
	u32 stages;
	rtsl_glsl_resource_kind kind;
	rtsl_glsl_storage_texture_format storage_texture_format;
} rtsl_glsl_resource_info;

typedef struct rtsl_glsl_translation rtsl_glsl_translation;



rtsl_glsl_status rtsl_glsl_translate(
	u64 size,
	const void* data,
	const rtsl_glsl_options* options,
	rtsl_glsl_translation** translation,
	char* message,
	u64 message_capacity
);
void rtsl_glsl_translation_destroy(rtsl_glsl_translation* translation);
const char* rtsl_glsl_stage_source(const rtsl_glsl_translation* translation, rtsl_glsl_stage stage);
u32 rtsl_glsl_resource_count(const rtsl_glsl_translation* translation);
bool rtsl_glsl_resource(const rtsl_glsl_translation* translation, u32 index, rtsl_glsl_resource_info* resource);

#ifdef __cplusplus
}
#endif

#endif
