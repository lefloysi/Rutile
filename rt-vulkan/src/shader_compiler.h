#ifndef RTVK_SHADER_COMPILER_H
#define RTVK_SHADER_COMPILER_H

#include "config.h"
#include "types.h"

#include <stdbool.h>
#include <vulkan/vulkan.h>

RTVK_EXTERN_C_ENTER

typedef struct rtvk_shader_uniform_block {
	char name[RTVK_MAX_SHADER_UNIFORM_NAME];
	u32 binding;
} rtvk_shader_uniform_block;

typedef struct rtvk_shader_texture {
	char name[RTVK_MAX_SHADER_UNIFORM_NAME];
	u32 binding;
} rtvk_shader_texture;

typedef enum rtvk_shader_resource_kind {
	RTVK_SHADER_RESOURCE_UNIFORM_BUFFER,
	RTVK_SHADER_RESOURCE_SAMPLED_TEXTURE,
	RTVK_SHADER_RESOURCE_STORAGE_BUFFER,
	RTVK_SHADER_RESOURCE_STORAGE_TEXTURE,
} rtvk_shader_resource_kind;

typedef struct rtvk_shader_resource {
	char name[RTVK_MAX_SHADER_UNIFORM_NAME];
	u32 binding;
	rtvk_shader_resource_kind kind;
} rtvk_shader_resource;

typedef struct rtvk_shader_reflection {
	rtvk_shader_uniform_block* uniform_blocks;
	rtvk_shader_texture* textures;
	rtvk_shader_resource* resources;
	u32 uniform_block_count;
	u32 texture_count;
	u32 resource_count;
} rtvk_shader_reflection;

void rtvk_shader_reflection_clear(rtvk_shader_reflection* reflection);

/*===============================================================================================*/
/*                                                                                               */
/*===============================================================================================*/

void rtvk_shader_compiler_init(void);
void rtvk_shader_compiler_finish(void);

typedef struct rtvk_graphics_shader_compile_result {
	VkShaderModule vertex_shader;
	VkShaderModule fragment_shader;
	rtvk_shader_reflection vertex_reflection;
	rtvk_shader_reflection fragment_reflection;
} rtvk_graphics_shader_compile_result;

VkShaderModule rtvk_shader_compile(struct rtvk_context* ctx, VkShaderStageFlagBits stage, u64 size, const void* source, rtvk_shader_reflection* reflection, u32** spirv_source, u64* spirv_size);
rtvk_graphics_shader_compile_result rtvk_shader_compile_graphics(
	struct rtvk_context* ctx,
	const rt_vertex_layout* vertex_layout,
	u64 vertex_size,
	const void* vertex_source,
	u64 fragment_size,
	const void* fragment_source);

RTVK_EXTERN_C_EXIT

#endif


