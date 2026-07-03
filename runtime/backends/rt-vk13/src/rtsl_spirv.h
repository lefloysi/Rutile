#ifndef RTSL_SPIRV_H
#define RTSL_SPIRV_H

#include "types.h"

struct rtsl_spirv_resource {
	const char* name;
	u32 binding;
};

struct rtsl_spirv_uniform_block {
	const char* name;
	u32 binding;
};

struct rtsl_spirv_texture {
	const char* name;
	u32 binding;
};

typedef struct rtsl_spirv_reflection {
	struct rtsl_spirv_resource* resources;
	u32 resource_count;

	struct rtsl_spirv_uniform_block* uniform_blocks;
	u32 uniform_block_count;

	struct rtsl_spirv_texture* textures;
	u32 texture_count;
} rtsl_spirv_reflection;

void rtsl_spirv_reflection_clear(rtsl_spirv_reflection* reflection);

#endif
