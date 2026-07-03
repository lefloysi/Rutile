#include "rtsl_spirv.h"

#include <stdlib.h>

void rtsl_spirv_reflection_clear(rtsl_spirv_reflection* reflection) {
	if (!reflection) {
		return;
	}

	free(reflection->resources);
	free(reflection->uniform_blocks);
	free(reflection->textures);

	reflection->resources = NULL;
	reflection->resource_count = 0;
	reflection->uniform_blocks = NULL;
	reflection->uniform_block_count = 0;
	reflection->textures = NULL;
	reflection->texture_count = 0;
}
