#ifndef RTVK_COMPUTE_PROGRAM_H
#define RTVK_COMPUTE_PROGRAM_H

#include "config.h"
#include "resource.h"
#include "shader_compiler.h"

#include <rt_ext_compute.h>
#include <vulkan/vulkan.h>

/*===============================================================================================*/
/*                                                                                               */
/*===============================================================================================*/

RTVK_API rt_compute_program rtComputeProgramCreate(void);
RTVK_API void rtComputeProgramDestroy(rt_compute_program program);
RTVK_API void rtComputeProgramShader(rt_compute_program program, u64 size, const void* data);
RTVK_API void rtComputeProgramLink(rt_compute_program program);

/*===============================================================================================*/
/*                                                                                               */
/*===============================================================================================*/

typedef enum rtvk_compute_binding_kind {
	RTVK_COMPUTE_BINDING_EMPTY,
	RTVK_COMPUTE_BINDING_STORAGE_BUFFER,
	RTVK_COMPUTE_BINDING_STORAGE_TEXTURE,
} rtvk_compute_binding_kind;

struct rtvk_compute_program {
	struct rtvk_resource_base base;

	VkShaderModule vk_shader;
	VkDescriptorSetLayout vk_descriptor_set_layout;
	VkPipelineLayout vk_pipeline_layout;
	VkPipeline vk_pipeline;

	char* shader_source;
	u64 shader_size;
	rtvk_compute_binding_kind bindings[RTVK_MAX_COMPUTE_BINDINGS];
	u32 binding_count;
	rtvk_shader_reflection reflection;
};
RTVK_DECLARE_NEW_RESOURCE(compute_program)

void rtvk_compute_program_shader(struct rtvk_context* ctx, struct rtvk_compute_program* program, u64 size, const void* data);
void rtvk_compute_program_link(struct rtvk_context* ctx, struct rtvk_compute_program* program);
void rtvk_compute_program_destroy_shader(struct rtvk_context* ctx, struct rtvk_compute_program* program);
void rtvk_compute_program_destroy_pipeline(struct rtvk_context* ctx, struct rtvk_compute_program* program);
void rtvk_compute_program_destroy_layout(struct rtvk_context* ctx, struct rtvk_compute_program* program);
void rtvk_compute_program_set_reflected_binding(struct rtvk_compute_program* program, u32 binding, rtvk_compute_binding_kind kind, const char* name);
void rtvk_compute_program_apply_reflection(struct rtvk_compute_program* program, const rtvk_shader_reflection* reflection);
void rtvk_compute_program_create_descriptor_set_layout(struct rtvk_context* ctx, struct rtvk_compute_program* program);
void rtvk_compute_program_create_pipeline_layout(struct rtvk_context* ctx, struct rtvk_compute_program* program);
void rtvk_compute_program_create_pipeline(struct rtvk_context* ctx, struct rtvk_compute_program* program);

#endif
