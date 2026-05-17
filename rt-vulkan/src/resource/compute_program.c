#include "compute_program.h"
#include "context.h"
#include "error.h"
#include "shader_compiler.h"

#include <stdlib.h>
#include <string.h>

rt_compute_program rtComputeProgramCreate(void) {
	return rtvk_compute_program_to_handle(rtvk_compute_program_create(rtvk_get_current_context()));
}

void rtComputeProgramDestroy(rt_compute_program program) {
	rtvk_compute_program_destroy(rtvk_get_current_context(), rtvk_compute_program_from_handle(program));
}

void rtComputeProgramShader(rt_compute_program program, u64 size, const void* data) {
	rtvk_compute_program_shader(rtvk_get_current_context(), rtvk_compute_program_from_handle(program), size, data);
}

void rtComputeProgramLink(rt_compute_program program) {
	rtvk_compute_program_link(rtvk_get_current_context(), rtvk_compute_program_from_handle(program));
}

RTVK_DEFINE_RESOURCE_PRIVATE(compute_program)

void rtvk_compute_program_init(struct rtvk_context* ctx, struct rtvk_compute_program* program) {
	rtvk_init_resource_base(ctx, RTVK_RESOURCE_BASE(program), RT_RESOURCE_COMPUTE_PROGRAM);
}

static void rtvk_compute_program_destroy_shader(struct rtvk_context* ctx, struct rtvk_compute_program* program) {
	if (program->vk_shader) {
		vkDestroyShaderModule(ctx->vk_device, program->vk_shader, VK_ALLOCATOR);
		program->vk_shader = VK_NULL_HANDLE;
	}
}

static void rtvk_compute_program_destroy_pipeline(struct rtvk_context* ctx, struct rtvk_compute_program* program) {
	if (program->vk_pipeline) {
		vkDestroyPipeline(ctx->vk_device, program->vk_pipeline, VK_ALLOCATOR);
		program->vk_pipeline = VK_NULL_HANDLE;
	}
}

static void rtvk_compute_program_destroy_layout(struct rtvk_context* ctx, struct rtvk_compute_program* program) {
	rtvk_compute_program_destroy_pipeline(ctx, program);
	if (program->vk_pipeline_layout) {
		vkDestroyPipelineLayout(ctx->vk_device, program->vk_pipeline_layout, VK_ALLOCATOR);
		program->vk_pipeline_layout = VK_NULL_HANDLE;
	}
	if (program->vk_descriptor_set_layout) {
		vkDestroyDescriptorSetLayout(ctx->vk_device, program->vk_descriptor_set_layout, VK_ALLOCATOR);
		program->vk_descriptor_set_layout = VK_NULL_HANDLE;
	}
}

void rtvk_compute_program_finish(struct rtvk_context* ctx, struct rtvk_compute_program* program) {
	rtvk_compute_program_destroy_layout(ctx, program);
	rtvk_compute_program_destroy_shader(ctx, program);
	free(program->shader_source);
	program->shader_source = NULL;
	program->shader_size = 0;
	rtvk_shader_reflection_clear(&program->reflection);
	rtvk_finish_resource_base(ctx, RTVK_RESOURCE_BASE(program));
}

void rtvk_compute_program_shader(struct rtvk_context* ctx, struct rtvk_compute_program* program, u64 size, const void* data) {
	if (!program) {
		rtvk_throwf(RT_IMPROPER_USAGE, "compute program is NULL");
		return;
	}
	if (!data || size == 0) {
		rtvk_compute_program_destroy_layout(ctx, program);
		rtvk_compute_program_destroy_shader(ctx, program);
		free(program->shader_source);
		program->shader_source = NULL;
		program->shader_size = 0;
		return;
	}
	if (size > SIZE_MAX) {
		rtvk_throwf(RT_IMPROPER_USAGE, "compute shader source is too large");
		return;
	}
	char* source = malloc((size_t)size);
	if (!source) {
		rtvk_throwf(RT_OUT_OF_HOST_MEMORY, "failed to allocate compute shader source");
		return;
	}
	memcpy(source, data, (size_t)size);
	rtvk_compute_program_destroy_layout(ctx, program);
	rtvk_compute_program_destroy_shader(ctx, program);
	rtvk_shader_reflection_clear(&program->reflection);
	memset(program->bindings, 0, sizeof(program->bindings));
	program->binding_count = 0;
	free(program->shader_source);
	program->shader_source = source;
	program->shader_size = size;
}

static bool rtvk_compute_program_set_reflected_binding(struct rtvk_compute_program* program, u32 binding, rtvk_compute_binding_kind kind, const char* name) {
	if (binding >= RTVK_MAX_COMPUTE_BINDINGS) {
		rtvk_throwf(RT_SHADER_LINK_FAILED, "compute resource %s binding %u exceeds limit %u", name ? name : "<unnamed>", binding, RTVK_MAX_COMPUTE_BINDINGS);
		return false;
	}
	if (program->bindings[binding] && program->bindings[binding] != kind) {
		rtvk_throwf(RT_SHADER_LINK_FAILED, "compute resource %s binding %u conflicts with a different resource kind", name ? name : "<unnamed>", binding);
		return false;
	}
	program->bindings[binding] = kind;
	if (program->binding_count <= binding) {
		program->binding_count = binding + 1;
	}
	return true;
}

static bool rtvk_compute_program_apply_reflection(struct rtvk_compute_program* program, const rtvk_shader_reflection* reflection) {
	memset(program->bindings, 0, sizeof(program->bindings));
	program->binding_count = 0;
	for (u32 i = 0; i < reflection->resource_count; i++) {
		const rtvk_shader_resource* resource = &reflection->resources[i];
		switch (resource->kind) {
		case RTVK_SHADER_RESOURCE_STORAGE_BUFFER:
			if (!rtvk_compute_program_set_reflected_binding(program, resource->binding, RTVK_COMPUTE_BINDING_STORAGE_BUFFER, resource->name)) {
				return false;
			}
			break;
		case RTVK_SHADER_RESOURCE_STORAGE_TEXTURE:
			if (!rtvk_compute_program_set_reflected_binding(program, resource->binding, RTVK_COMPUTE_BINDING_STORAGE_TEXTURE, resource->name)) {
				return false;
			}
			break;
		case RTVK_SHADER_RESOURCE_UNIFORM_BUFFER:
			rtvk_throwf(RT_SHADER_LINK_FAILED, "compute resource %s is a uniform buffer; Rutile compute currently supports storage buffers and storage textures only", resource->name);
			return false;
		case RTVK_SHADER_RESOURCE_SAMPLED_TEXTURE:
			rtvk_throwf(RT_SHADER_LINK_FAILED, "compute resource %s is a sampled texture; Rutile compute currently supports storage buffers and storage textures only", resource->name);
			return false;
		}
	}
	return true;
}

static bool rtvk_compute_program_create_descriptor_set_layout(struct rtvk_context* ctx, struct rtvk_compute_program* program) {
	VkDescriptorSetLayoutBinding bindings[RTVK_MAX_COMPUTE_BINDINGS];
	u32 count = 0;
	for (u32 i = 0; i < program->binding_count; i++) {
		if (!program->bindings[i]) {
			continue;
		}
		bindings[count] = (VkDescriptorSetLayoutBinding){ 0 };
		bindings[count].binding = i;
		bindings[count].descriptorType = program->bindings[i] == RTVK_COMPUTE_BINDING_STORAGE_TEXTURE ? VK_DESCRIPTOR_TYPE_STORAGE_IMAGE : VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
		bindings[count].descriptorCount = 1;
		bindings[count].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
		count++;
	}

	VkDescriptorSetLayoutCreateInfo layout_info = { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO };
	layout_info.bindingCount = count;
	layout_info.pBindings = count ? bindings : NULL;
	VkResult result = vkCreateDescriptorSetLayout(ctx->vk_device, &layout_info, VK_ALLOCATOR, &program->vk_descriptor_set_layout);
	if (result != VK_SUCCESS) {
		rtvk_throwf(rtvk_error_from_vk(result), NULL);
		return false;
	}
	return true;
}

static bool rtvk_compute_program_create_pipeline_layout(struct rtvk_context* ctx, struct rtvk_compute_program* program) {
	if (!program->vk_descriptor_set_layout && !rtvk_compute_program_create_descriptor_set_layout(ctx, program)) {
		return false;
	}
	VkPipelineLayoutCreateInfo layout_info = { VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO };
	layout_info.setLayoutCount = 1;
	layout_info.pSetLayouts = &program->vk_descriptor_set_layout;
	VkResult result = vkCreatePipelineLayout(ctx->vk_device, &layout_info, VK_ALLOCATOR, &program->vk_pipeline_layout);
	if (result != VK_SUCCESS) {
		rtvk_throwf(rtvk_error_from_vk(result), NULL);
		return false;
	}
	return true;
}

static bool rtvk_compute_program_create_pipeline(struct rtvk_context* ctx, struct rtvk_compute_program* program) {
	VkPipelineShaderStageCreateInfo stage = { VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO };
	stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
	stage.module = program->vk_shader;
	stage.pName = "main";

	VkComputePipelineCreateInfo pipeline_info = { VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO };
	pipeline_info.stage = stage;
	pipeline_info.layout = program->vk_pipeline_layout;
	VkResult result = vkCreateComputePipelines(ctx->vk_device, VK_NULL_HANDLE, 1, &pipeline_info, VK_ALLOCATOR, &program->vk_pipeline);
	if (result != VK_SUCCESS) {
		rtvk_throwf(rtvk_error_from_vk(result), NULL);
		return false;
	}
	return true;
}

void rtvk_compute_program_link(struct rtvk_context* ctx, struct rtvk_compute_program* program) {
	if (!program) {
		rtvk_throwf(RT_IMPROPER_USAGE, "compute program is NULL");
		return;
	}
	if (!program->shader_source) {
		rtvk_throwf(RT_SHADER_LINK_FAILED, "compute program link requires a compute shader");
		return;
	}
	rtvk_shader_reflection reflection = {};
	VkShaderModule shader = rtvk_shader_compile(ctx, VK_SHADER_STAGE_COMPUTE_BIT, program->shader_size, program->shader_source, &reflection, NULL, NULL);
	if (!shader) {
		return;
	}
	if (!rtvk_compute_program_apply_reflection(program, &reflection)) {
		rtvk_shader_reflection_clear(&reflection);
		if (shader) {
			vkDestroyShaderModule(ctx->vk_device, shader, VK_ALLOCATOR);
		}
		return;
	}
	rtvk_compute_program_destroy_layout(ctx, program);
	rtvk_compute_program_destroy_shader(ctx, program);
	rtvk_shader_reflection_clear(&program->reflection);
	program->vk_shader = shader;
	program->reflection = reflection;
	if (!rtvk_compute_program_create_pipeline_layout(ctx, program)) {
		return;
	}
	rtvk_compute_program_create_pipeline(ctx, program);
}
