#include "shader_compiler.h"
#include "context.h"
#include "error.h"

#include <Emit/SpirvEmitter.h>

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

void rtvk_shader_reflection_clear(rtvk_shader_reflection* reflection) {
	if (!reflection)
		return;
	free(reflection->uniform_blocks);
	free(reflection->textures);
	free(reflection->resources);
	memset(reflection, 0, sizeof(*reflection));
}

static bool rtvk_spirv_create_shader_module(struct rtvk_context* ctx, const uint32_t* words, size_t word_count, VkShaderModule* shader_out) {
	VkShaderModuleCreateInfo create_info = { VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO };
	create_info.codeSize = word_count * sizeof(uint32_t);
	create_info.pCode = words;
	const VkResult result = vkCreateShaderModule(ctx->vk_device, &create_info, VK_ALLOCATOR, shader_out);
	if (result != VK_SUCCESS) {
		rtvk_throwf(rtvk_error_from_vk(result), "Vulkan call returned %s", rtvk_vk_result_name(result));
		return false;
	}
	return true;
}

static bool rtvk_fill_reflection(const rtsl_uniform_info* uniforms, size_t uniform_count, rtvk_shader_reflection* reflection) {
	u32 uniform_block_count = 0;
	u32 texture_count = 0;
	for (size_t i = 0; i < uniform_count; ++i) {
		if (strcmp(uniforms[i].type, "Sampler2D") == 0)
			++texture_count;
		else
			++uniform_block_count;
	}

	if (uniform_block_count) {
		reflection->uniform_blocks = (rtvk_shader_uniform_block*)calloc(uniform_block_count, sizeof(rtvk_shader_uniform_block));
		if (!reflection->uniform_blocks)
			return false;
	}
	if (texture_count) {
		reflection->textures = (rtvk_shader_texture*)calloc(texture_count, sizeof(rtvk_shader_texture));
		if (!reflection->textures)
			return false;
	}

	reflection->uniform_block_count = uniform_block_count;
	reflection->texture_count = texture_count;
	reflection->resource_count = 0;

	u32 uniform_index = 0;
	u32 texture_index = 0;
	for (size_t i = 0; i < uniform_count; ++i) {
		if (strcmp(uniforms[i].type, "Sampler2D") == 0) {
			rtvk_shader_texture* dst = &reflection->textures[texture_index++];
			snprintf(dst->name, sizeof(dst->name), "%s", uniforms[i].name);
			dst->binding = uniforms[i].binding;
		} else {
			rtvk_shader_uniform_block* dst = &reflection->uniform_blocks[uniform_index++];
			snprintf(dst->name, sizeof(dst->name), "%s", uniforms[i].name);
			dst->binding = uniforms[i].binding;
		}
	}
	return true;
}

rtvk_graphics_shader_compile_result rtvk_shader_compile_graphics_rtslp(struct rtvk_context* ctx, u64 program_size, const void* program_source) {
	rtvk_graphics_shader_compile_result result = { 0 };
	rtsl_graphics_spirv spirv = { 0 };
	char error[512] = { 0 };

	const rtsl_compile_status compile_status = rtsl_compile_rtslp_graphics(program_size, program_source, &spirv);
	if (compile_status != RTSL_COMPILE_OK) {
		const char* message = "failed to lower RTSLP";
		if (compile_status == RTSL_COMPILE_INVALID_ARGUMENT) {
			message = "invalid shader translation arguments";
		} else if (compile_status == RTSL_COMPILE_OUT_OF_MEMORY) {
			message = "out of memory";
		}
		rtvk_throwf(RT_SHADER_COMPILATION_FAILED, "%s", message);
		return result;
	}

	// rtsl_compile_rtslp_graphics emits every global variable into every
	// stage's SPIR-V (and the entry-point interface), so each stage's
	// reflection must list the same uniforms. Otherwise the descriptor
	// set layout ends up with stage flags that omit a stage that actually
	// reads the binding, and Vulkan rejects pipeline creation.
	if (!rtvk_fill_reflection(spirv.uniforms, spirv.uniform_count, &result.vertex_reflection) || !rtvk_fill_reflection(spirv.uniforms, spirv.uniform_count, &result.fragment_reflection)) {
		rtvk_shader_reflection_clear(&result.vertex_reflection);
		rtvk_shader_reflection_clear(&result.fragment_reflection);
		rtsl_graphics_spirv_free(&spirv);
		rtvk_throwf(RT_OUT_OF_HOST_MEMORY, "failed to allocate RTSLP reflection");
		return result;
	}

	if (!rtvk_spirv_create_shader_module(ctx, spirv.vertex_words, spirv.vertex_word_count, &result.vertex_shader)) {
		rtvk_shader_reflection_clear(&result.vertex_reflection);
		rtvk_shader_reflection_clear(&result.fragment_reflection);
		rtsl_graphics_spirv_free(&spirv);
		rtvk_graphics_shader_compile_result empty = { 0 };
		return empty;
	}
	if (!rtvk_spirv_create_shader_module(ctx, spirv.fragment_words, spirv.fragment_word_count, &result.fragment_shader)) {
		vkDestroyShaderModule(ctx->vk_device, result.vertex_shader, VK_ALLOCATOR);
		rtvk_shader_reflection_clear(&result.vertex_reflection);
		rtvk_shader_reflection_clear(&result.fragment_reflection);
		rtsl_graphics_spirv_free(&spirv);
		rtvk_graphics_shader_compile_result empty = { 0 };
		return empty;
	}

	rtsl_graphics_spirv_free(&spirv);
	return result;
}
