#include "graphics_program.h"
#include "context.h"
#include "error.h"
#include "shader_compiler.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

/*===============================================================================================*/
/*                                                                                               */
/*===============================================================================================*/

rt_graphics_program rtGraphicsProgramCreate(void) {
	struct rtvk_graphics_program* program = rtvk_graphics_program_create(rtvk_get_current_context());
	return rtvk_graphics_program_to_handle(program);
}

void rtGraphicsProgramDestroy(rt_graphics_program program) {
	rtvk_graphics_program_destroy(rtvk_get_current_context(), rtvk_graphics_program_from_handle(program));
}

void rtGraphicsProgramVertexLayout(rt_graphics_program program, const rt_vertex_layout* layout) {
	rtvk_graphics_program_vertex_layout(rtvk_get_current_context(), rtvk_graphics_program_from_handle(program), layout);
}

void rtGraphicsProgramVertexShader(rt_graphics_program program, u64 size, const void* data) {
	rtvk_graphics_program_vertex_shader(rtvk_get_current_context(), rtvk_graphics_program_from_handle(program), size, data);
}

void rtGraphicsProgramFragmentShader(rt_graphics_program program, u64 size, const void* data) {
	rtvk_graphics_program_fragment_shader(rtvk_get_current_context(), rtvk_graphics_program_from_handle(program), size, data);
}

void rtGraphicsProgramRasterState(rt_graphics_program program, enum rt_cull_mode cull_mode, enum rt_front_face front_face, enum rt_fill_mode fill_mode) {
	rtvk_graphics_program_raster_state(rtvk_get_current_context(), rtvk_graphics_program_from_handle(program), cull_mode, front_face, fill_mode);
}

void rtGraphicsProgramBlendState(
	rt_graphics_program program,
	bool enabled,
	enum rt_blend_factor src_color,
	enum rt_blend_factor dst_color,
	enum rt_blend_op color_op,
	enum rt_blend_factor src_alpha,
	enum rt_blend_factor dst_alpha,
	enum rt_blend_op alpha_op) {
	rtvk_graphics_program_blend_state(rtvk_get_current_context(), rtvk_graphics_program_from_handle(program),
		enabled, src_color, dst_color, color_op, src_alpha, dst_alpha, alpha_op);
}

void rtGraphicsProgramLink(rt_graphics_program program) {
	rtvk_graphics_program_link(rtvk_get_current_context(), rtvk_graphics_program_from_handle(program));
}

rt_uniform_location rtGraphicsProgramUniformLocation(rt_graphics_program program, const char* name) {
	struct rtvk_uniform_location* location = rtvk_graphics_program_uniform_location(
		rtvk_get_current_context(),
		rtvk_graphics_program_from_handle(program),
		name);
	return rtvk_uniform_location_to_handle(location);
}

/*===============================================================================================*/
/*                                                                                               */
/*===============================================================================================*/

RTVK_DEFINE_RESOURCE_PRIVATE(graphics_program)

void rtvk_graphics_program_init(struct rtvk_context* ctx, struct rtvk_graphics_program* program) {
	rtvk_init_resource_base(ctx, RTVK_RESOURCE_BASE(program), RT_RESOURCE_GRAPHICS_PROGRAM);
	program->cull_mode = RT_CULL_NONE;
	program->front_face = RT_FRONT_FACE_CCW;
	program->fill_mode = RT_FILL_SOLID;
	program->blend_enabled = false;
	program->src_color_blend = RT_BLEND_ONE;
	program->dst_color_blend = RT_BLEND_ZERO;
	program->color_blend_op = RT_BLEND_OP_ADD;
	program->src_alpha_blend = RT_BLEND_ONE;
	program->dst_alpha_blend = RT_BLEND_ZERO;
	program->alpha_blend_op = RT_BLEND_OP_ADD;
}

static void rtvk_graphics_program_destroy_shader(struct rtvk_context* ctx, VkShaderModule* shader) {
	if (*shader) {
		vkDestroyShaderModule(ctx->vk_device, *shader, VK_ALLOCATOR);
		*shader = VK_NULL_HANDLE;
	}
}

static void rtvk_graphics_program_destroy_shader_source(char** source, u64* size) {
	free(*source);
	*source = NULL;
	*size = 0;
}

static void rtvk_graphics_program_clear_uniform_locations(struct rtvk_graphics_program* program) {
	free(program->uniform_locations);
	program->uniform_locations = NULL;
	program->uniform_location_count = 0;
	program->uniform_location_capacity = 0;
}

static void rtvk_graphics_program_retire_pipeline(struct rtvk_graphics_program* program, VkPipeline pipeline) {
	if (!pipeline) {
		return;
	}

	rtvk_retired_graphics_pipeline* retired = malloc(sizeof(*retired));
	if (!retired) {
		rtvk_throwf(RT_OUT_OF_HOST_MEMORY, "failed to retire graphics pipeline");
		return;
	}
	retired->vk_pipeline = pipeline;
	retired->next = program->retired_pipelines;
	program->retired_pipelines = retired;
}

static void rtvk_graphics_program_destroy_pipeline(struct rtvk_context* ctx, struct rtvk_graphics_program* program) {
	(void)ctx;
	if (program->vk_pipeline) {
		rtvk_graphics_program_retire_pipeline(program, program->vk_pipeline);
		program->vk_pipeline = VK_NULL_HANDLE;
		program->vk_pipeline_format = VK_FORMAT_UNDEFINED;
		program->vk_pipeline_depth_format = VK_FORMAT_UNDEFINED;
	}
}

static void rtvk_graphics_program_destroy_pipeline_layout(struct rtvk_context* ctx, struct rtvk_graphics_program* program) {
	rtvk_graphics_program_destroy_pipeline(ctx, program);
	if (program->vk_pipeline_layout) {
		vkDestroyPipelineLayout(ctx->vk_device, program->vk_pipeline_layout, VK_ALLOCATOR);
		program->vk_pipeline_layout = VK_NULL_HANDLE;
	}
	if (program->vk_descriptor_set_layout) {
		vkDestroyDescriptorSetLayout(ctx->vk_device, program->vk_descriptor_set_layout, VK_ALLOCATOR);
		program->vk_descriptor_set_layout = VK_NULL_HANDLE;
	}
}

static VkCullModeFlags rtvk_cull_mode(enum rt_cull_mode mode);
static VkFrontFace rtvk_front_face(enum rt_front_face face);
static VkPolygonMode rtvk_fill_mode(enum rt_fill_mode mode);
static VkBlendFactor rtvk_blend_factor(enum rt_blend_factor factor);
static VkBlendOp rtvk_blend_op(enum rt_blend_op op);

static VkFormat rtvk_vertex_format(enum rt_format format) {
	switch (format) {
	case RT_R32_SFLOAT: return VK_FORMAT_R32_SFLOAT;
	case RT_RG32_SFLOAT: return VK_FORMAT_R32G32_SFLOAT;
	case RT_RGB32_SFLOAT: return VK_FORMAT_R32G32B32_SFLOAT;
	case RT_RGBA32_SFLOAT: return VK_FORMAT_R32G32B32A32_SFLOAT;
	case RT_R32_SINT: return VK_FORMAT_R32_SINT;
	case RT_RG32_SINT: return VK_FORMAT_R32G32_SINT;
	case RT_RGB32_SINT: return VK_FORMAT_R32G32B32_SINT;
	case RT_RGBA32_SINT: return VK_FORMAT_R32G32B32A32_SINT;
	case RT_R32_UINT: return VK_FORMAT_R32_UINT;
	case RT_RG32_UINT: return VK_FORMAT_R32G32_UINT;
	case RT_RGB32_UINT: return VK_FORMAT_R32G32B32_UINT;
	case RT_RGBA32_UINT: return VK_FORMAT_R32G32B32A32_UINT;
	default: return VK_FORMAT_UNDEFINED;
	}
}

void rtvk_graphics_program_finish(struct rtvk_context* ctx, struct rtvk_graphics_program* program) {
	rtvk_graphics_program_destroy_pipeline_layout(ctx, program);
	while (program->retired_pipelines) {
		rtvk_retired_graphics_pipeline* retired = program->retired_pipelines;
		program->retired_pipelines = retired->next;
		vkDestroyPipeline(ctx->vk_device, retired->vk_pipeline, VK_ALLOCATOR);
		free(retired);
	}
	rtvk_graphics_program_destroy_shader(ctx, &program->vk_vertex_shader);
	rtvk_graphics_program_destroy_shader(ctx, &program->vk_fragment_shader);
	rtvk_graphics_program_destroy_shader_source(&program->vertex_shader_source, &program->vertex_shader_size);
	rtvk_graphics_program_destroy_shader_source(&program->fragment_shader_source, &program->fragment_shader_size);
	rtvk_shader_reflection_clear(&program->vertex_reflection);
	rtvk_shader_reflection_clear(&program->fragment_reflection);
	rtvk_graphics_program_clear_uniform_locations(program);
	rtvk_finish_resource_base(ctx, RTVK_RESOURCE_BASE(program));
}

static bool rtvk_graphics_program_create_descriptor_set_layout(struct rtvk_context* ctx, struct rtvk_graphics_program* program) {
	if (program->uniform_location_count == 0) {
		return true;
	}

	size_t binding_bytes = sizeof(VkDescriptorSetLayoutBinding) * program->uniform_location_count;
	VkDescriptorSetLayoutBinding* bindings = calloc(program->uniform_location_count, sizeof(*bindings));
	if (!bindings) {
		rtvk_throwf(RT_OUT_OF_HOST_MEMORY, "failed to allocate descriptor set layout bindings");
		return false;
	}

	for (u32 i = 0; i < program->uniform_location_count; i++) {
		bindings[i].binding = program->uniform_locations[i].binding;
		bindings[i].descriptorType = program->uniform_locations[i].kind == RTVK_UNIFORM_LOCATION_TEXTURE ?
			VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER :
			VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		bindings[i].descriptorCount = 1;
		bindings[i].stageFlags = program->uniform_locations[i].stages;
		bindings[i].pImmutableSamplers = NULL;
	}

	VkDescriptorSetLayoutCreateInfo layout_info = { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO };
	layout_info.pNext = NULL;
	layout_info.flags = 0;
	layout_info.bindingCount = program->uniform_location_count;
	layout_info.pBindings = bindings;

	VkResult result = vkCreateDescriptorSetLayout(ctx->vk_device, &layout_info, VK_ALLOCATOR, &program->vk_descriptor_set_layout);
	free(bindings);
	if (result != VK_SUCCESS) {
		rtvk_throwf(rtvk_error_from_vk(result), NULL);
		return false;
	}

	return true;
}

static bool rtvk_graphics_program_create_pipeline_layout(struct rtvk_context* ctx, struct rtvk_graphics_program* program) {
	VkPipelineLayoutCreateInfo layout_info = { VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO };

	if (!program->vk_descriptor_set_layout && !rtvk_graphics_program_create_descriptor_set_layout(ctx, program)) {
		return false;
	}

	layout_info.pNext = NULL;
	layout_info.flags = 0;
	layout_info.setLayoutCount = program->vk_descriptor_set_layout ? 1 : 0;
	layout_info.pSetLayouts = program->vk_descriptor_set_layout ? &program->vk_descriptor_set_layout : NULL;
	layout_info.pushConstantRangeCount = 0;
	layout_info.pPushConstantRanges = NULL;

	VkResult result = vkCreatePipelineLayout(ctx->vk_device, &layout_info, VK_ALLOCATOR, &program->vk_pipeline_layout);
	if (result != VK_SUCCESS) {
		rtvk_throwf(rtvk_error_from_vk(result), NULL);
		return false;
	}

	return true;
}

static void rtvk_graphics_program_shader_stages(struct rtvk_graphics_program* program, VkPipelineShaderStageCreateInfo stages[2]) {
	stages[0] = (VkPipelineShaderStageCreateInfo){ VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO };
	stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
	stages[0].module = program->vk_vertex_shader;
	stages[0].pName = "main";

	stages[1] = (VkPipelineShaderStageCreateInfo){ VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO };
	stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
	stages[1].module = program->vk_fragment_shader;
	stages[1].pName = "main";
}

static void rtvk_graphics_program_viewport_state(VkViewport* viewport, VkRect2D* scissor, VkPipelineViewportStateCreateInfo* viewport_info) {
	viewport->x = 0.0f;
	viewport->y = 0.0f;
	viewport->width = 1.0f;
	viewport->height = 1.0f;
	viewport->minDepth = 0.0f;
	viewport->maxDepth = 1.0f;

	scissor->offset.x = 0;
	scissor->offset.y = 0;
	scissor->extent.width = 1;
	scissor->extent.height = 1;

	*viewport_info = (VkPipelineViewportStateCreateInfo){ VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO };
	viewport_info->viewportCount = 1;
	viewport_info->pViewports = viewport;
	viewport_info->scissorCount = 1;
	viewport_info->pScissors = scissor;
}

static void rtvk_graphics_program_color_blend_state(
	struct rtvk_graphics_program* program,
	VkPipelineColorBlendAttachmentState* attachment,
	VkPipelineColorBlendStateCreateInfo* color_blend_info) {
	attachment->blendEnable = program->blend_enabled ? VK_TRUE : VK_FALSE;
	attachment->srcColorBlendFactor = rtvk_blend_factor(program->src_color_blend);
	attachment->dstColorBlendFactor = rtvk_blend_factor(program->dst_color_blend);
	attachment->colorBlendOp = rtvk_blend_op(program->color_blend_op);
	attachment->srcAlphaBlendFactor = rtvk_blend_factor(program->src_alpha_blend);
	attachment->dstAlphaBlendFactor = rtvk_blend_factor(program->dst_alpha_blend);
	attachment->alphaBlendOp = rtvk_blend_op(program->alpha_blend_op);
	attachment->colorWriteMask =
		VK_COLOR_COMPONENT_R_BIT |
		VK_COLOR_COMPONENT_G_BIT |
		VK_COLOR_COMPONENT_B_BIT |
		VK_COLOR_COMPONENT_A_BIT;

	*color_blend_info = (VkPipelineColorBlendStateCreateInfo){ VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO };
	color_blend_info->logicOpEnable = VK_FALSE;
	color_blend_info->logicOp = VK_LOGIC_OP_COPY;
	color_blend_info->attachmentCount = 1;
	color_blend_info->pAttachments = attachment;
}

static bool rtvk_graphics_program_create_pipeline(
	struct rtvk_context* ctx,
	struct rtvk_graphics_program* program,
	VkFormat color_format,
	VkFormat depth_format) {
	VkPipelineShaderStageCreateInfo stages[2];
	rtvk_graphics_program_shader_stages(program, stages);

	VkVertexInputBindingDescription binding = { 0 };
	VkVertexInputAttributeDescription attributes[RTVK_MAX_VERTEX_ATTRIBUTES] = { 0 };
	VkPipelineVertexInputStateCreateInfo vertex_input_info = { VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO };
	if (program->vertex_layout.attribute_count) {
		binding.binding = 0;
		binding.stride = program->vertex_layout.stride;
		binding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

		for (u32 i = 0; i < program->vertex_layout.attribute_count; i++) {
			attributes[i].location = program->vertex_attributes[i].location;
			attributes[i].binding = 0;
			attributes[i].format = rtvk_vertex_format(program->vertex_attributes[i].format);
			attributes[i].offset = program->vertex_attributes[i].offset;
			if (attributes[i].format == VK_FORMAT_UNDEFINED) {
				rtvk_throwf(RT_UNSUPPORTED_FEATURE, "unsupported vertex attribute format");
				return false;
			}
		}

		vertex_input_info.vertexBindingDescriptionCount = 1;
		vertex_input_info.pVertexBindingDescriptions = &binding;
		vertex_input_info.vertexAttributeDescriptionCount = program->vertex_layout.attribute_count;
		vertex_input_info.pVertexAttributeDescriptions = attributes;
	} else {
		vertex_input_info.vertexBindingDescriptionCount = 0;
		vertex_input_info.pVertexBindingDescriptions = NULL;
		vertex_input_info.vertexAttributeDescriptionCount = 0;
		vertex_input_info.pVertexAttributeDescriptions = NULL;
	}

	VkPipelineInputAssemblyStateCreateInfo input_assembly_info = { VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO };
	input_assembly_info.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
	input_assembly_info.primitiveRestartEnable = VK_FALSE;

	VkViewport viewport = { 0 };
	VkRect2D scissor = { 0 };
	VkPipelineViewportStateCreateInfo viewport_info;
	rtvk_graphics_program_viewport_state(&viewport, &scissor, &viewport_info);

	VkPipelineRasterizationStateCreateInfo raster_info = { VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO };
	raster_info.pNext = NULL;
	raster_info.flags = 0;
	raster_info.depthClampEnable = VK_FALSE;
	raster_info.rasterizerDiscardEnable = VK_FALSE;
	raster_info.polygonMode = rtvk_fill_mode(program->fill_mode);
	raster_info.cullMode = rtvk_cull_mode(program->cull_mode);
	raster_info.frontFace = rtvk_front_face(program->front_face);
	raster_info.depthBiasEnable = VK_FALSE;
	raster_info.depthBiasConstantFactor = 0.0f;
	raster_info.depthBiasClamp = 0.0f;
	raster_info.depthBiasSlopeFactor = 0.0f;
	raster_info.lineWidth = 1.0f;

	VkPipelineMultisampleStateCreateInfo multisample_info = { VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO };
	multisample_info.pNext = NULL;
	multisample_info.flags = 0;
	multisample_info.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
	multisample_info.sampleShadingEnable = VK_FALSE;
	multisample_info.minSampleShading = 1.0f;
	multisample_info.pSampleMask = NULL;
	multisample_info.alphaToCoverageEnable = VK_FALSE;
	multisample_info.alphaToOneEnable = VK_FALSE;

	VkPipelineColorBlendAttachmentState color_blend_attachment = { 0 };
	VkPipelineColorBlendStateCreateInfo color_blend_info;
	rtvk_graphics_program_color_blend_state(program, &color_blend_attachment, &color_blend_info);

	VkPipelineDepthStencilStateCreateInfo depth_info = { VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO };
	depth_info.pNext = NULL;
	depth_info.flags = 0;
	depth_info.depthTestEnable = depth_format != VK_FORMAT_UNDEFINED;
	depth_info.depthWriteEnable = depth_format != VK_FORMAT_UNDEFINED;
	depth_info.depthCompareOp = VK_COMPARE_OP_LESS;
	depth_info.depthBoundsTestEnable = VK_FALSE;
	depth_info.stencilTestEnable = VK_FALSE;
	depth_info.front = (VkStencilOpState){ 0 };
	depth_info.back = (VkStencilOpState){ 0 };
	depth_info.minDepthBounds = 0.0f;
	depth_info.maxDepthBounds = 1.0f;

	VkDynamicState dynamic_states[] = {
		VK_DYNAMIC_STATE_VIEWPORT,
		VK_DYNAMIC_STATE_SCISSOR,
	};
	VkPipelineDynamicStateCreateInfo dynamic_info = { VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO };
	dynamic_info.pNext = NULL;
	dynamic_info.flags = 0;
	dynamic_info.dynamicStateCount = (u32)(sizeof(dynamic_states) / sizeof(dynamic_states[0]));
	dynamic_info.pDynamicStates = dynamic_states;

	VkPipelineRenderingCreateInfo rendering_info = { VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO };
	rendering_info.pNext = NULL;
	rendering_info.viewMask = 0;
	rendering_info.colorAttachmentCount = 1;
	rendering_info.pColorAttachmentFormats = &color_format;
	rendering_info.depthAttachmentFormat = depth_format;
	rendering_info.stencilAttachmentFormat = VK_FORMAT_UNDEFINED;

	VkGraphicsPipelineCreateInfo pipeline_info = { VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO };
	pipeline_info.pNext = &rendering_info;
	pipeline_info.flags = 0;
	pipeline_info.stageCount = 2;
	pipeline_info.pStages = stages;
	pipeline_info.pVertexInputState = &vertex_input_info;
	pipeline_info.pInputAssemblyState = &input_assembly_info;
	pipeline_info.pTessellationState = NULL;
	pipeline_info.pViewportState = &viewport_info;
	pipeline_info.pRasterizationState = &raster_info;
	pipeline_info.pMultisampleState = &multisample_info;
	pipeline_info.pDepthStencilState = &depth_info;
	pipeline_info.pColorBlendState = &color_blend_info;
	pipeline_info.pDynamicState = &dynamic_info;
	pipeline_info.layout = program->vk_pipeline_layout;
	pipeline_info.renderPass = VK_NULL_HANDLE;
	pipeline_info.subpass = 0;
	pipeline_info.basePipelineHandle = VK_NULL_HANDLE;
	pipeline_info.basePipelineIndex = -1;

	VkResult result = vkCreateGraphicsPipelines(ctx->vk_device, VK_NULL_HANDLE, 1, &pipeline_info, VK_ALLOCATOR, &program->vk_pipeline);
	if (result != VK_SUCCESS) {
		rtvk_throwf(rtvk_error_from_vk(result), NULL);
		return false;
	}

	program->vk_pipeline_format = color_format;
	program->vk_pipeline_depth_format = depth_format;
	return true;
}

bool rtvk_graphics_program_prepare(
	struct rtvk_context* ctx,
	struct rtvk_graphics_program* program,
	VkFormat color_format,
	VkFormat depth_format) {
	if (!program || !program->vk_pipeline_layout) {
		rtvk_throwf(RT_IMPROPER_USAGE, "graphics program must be linked before use");
		return false;
	}

	if (program->vk_pipeline && program->vk_pipeline_format == color_format &&
			program->vk_pipeline_depth_format == depth_format) {
		return true;
	}

	rtvk_graphics_program_destroy_pipeline(ctx, program);

	return rtvk_graphics_program_create_pipeline(ctx, program, color_format, depth_format);
}

void rtvk_graphics_program_vertex_layout(struct rtvk_context* ctx, struct rtvk_graphics_program* program, const rt_vertex_layout* layout) {
	if (!program) {
		rtvk_throwf(RT_IMPROPER_USAGE, "graphics program is NULL");
		return;
	}
	if (!layout || !layout->attributes || layout->attribute_count == 0) {
		program->vertex_layout = (rt_vertex_layout){ 0 };
		rtvk_graphics_program_destroy_pipeline_layout(ctx, program);
		rtvk_graphics_program_destroy_shader(ctx, &program->vk_vertex_shader);
		rtvk_graphics_program_destroy_shader(ctx, &program->vk_fragment_shader);
		rtvk_shader_reflection_clear(&program->vertex_reflection);
		rtvk_shader_reflection_clear(&program->fragment_reflection);
		return;
	}
	if (layout->attribute_count > RTVK_MAX_VERTEX_ATTRIBUTES) {
		rtvk_throwf(RT_IMPROPER_USAGE, "too many vertex attributes");
		return;
	}
	if (layout->stride == 0) {
		rtvk_throwf(RT_IMPROPER_USAGE, "vertex layout stride is zero");
		return;
	}

	memcpy(program->vertex_attributes, layout->attributes, sizeof(layout->attributes[0]) * layout->attribute_count);
	program->vertex_layout.stride = layout->stride;
	program->vertex_layout.attributes = program->vertex_attributes;
	program->vertex_layout.attribute_count = layout->attribute_count;
	rtvk_graphics_program_destroy_pipeline_layout(ctx, program);
	rtvk_graphics_program_destroy_shader(ctx, &program->vk_vertex_shader);
	rtvk_graphics_program_destroy_shader(ctx, &program->vk_fragment_shader);
	rtvk_shader_reflection_clear(&program->vertex_reflection);
	rtvk_shader_reflection_clear(&program->fragment_reflection);
}

static void rtvk_graphics_program_shader(
	struct rtvk_context* ctx,
	struct rtvk_graphics_program* program,
	VkShaderModule* shader,
	rtvk_shader_reflection* reflection,
	char** source,
	u64* source_size,
	u64 size,
	const void* data) {
	if (!program) {
		rtvk_throwf(RT_IMPROPER_USAGE, "graphics program is NULL");
		return;
	}
	if (!data || size == 0) {
		rtvk_graphics_program_destroy_pipeline_layout(ctx, program);
		rtvk_graphics_program_destroy_shader(ctx, shader);
		rtvk_graphics_program_destroy_shader_source(source, source_size);
		rtvk_shader_reflection_clear(reflection);
		return;
	}

	if (size > SIZE_MAX) {
		rtvk_throwf(RT_IMPROPER_USAGE, "shader source is too large");
		return;
	}

	char* new_source = malloc((size_t)size);
	if (!new_source) {
		rtvk_throwf(RT_OUT_OF_HOST_MEMORY, "failed to allocate shader source storage");
		return;
	}
	memcpy(new_source, data, (size_t)size);

	rtvk_graphics_program_destroy_pipeline_layout(ctx, program);
	rtvk_graphics_program_destroy_shader(ctx, shader);
	rtvk_graphics_program_destroy_shader_source(source, source_size);
	rtvk_shader_reflection_clear(reflection);
	*source = new_source;
	*source_size = size;
}

void rtvk_graphics_program_vertex_shader(struct rtvk_context* ctx, struct rtvk_graphics_program* program, u64 size, const void* data) {
	rtvk_graphics_program_shader(ctx, program, &program->vk_vertex_shader,
		&program->vertex_reflection, &program->vertex_shader_source, &program->vertex_shader_size, size, data);
}

void rtvk_graphics_program_fragment_shader(struct rtvk_context* ctx, struct rtvk_graphics_program* program, u64 size, const void* data) {
	rtvk_graphics_program_shader(ctx, program, &program->vk_fragment_shader,
		&program->fragment_reflection, &program->fragment_shader_source, &program->fragment_shader_size, size, data);
}

void rtvk_graphics_program_raster_state(
	struct rtvk_context* ctx,
	struct rtvk_graphics_program* program,
	enum rt_cull_mode cull_mode,
	enum rt_front_face front_face,
	enum rt_fill_mode fill_mode) {
	if (!program) {
		rtvk_throwf(RT_IMPROPER_USAGE, "graphics program is invalid");
		return;
	}

	program->cull_mode = cull_mode;
	program->front_face = front_face;
	program->fill_mode = fill_mode;
	rtvk_graphics_program_destroy_pipeline(ctx, program);
}

void rtvk_graphics_program_blend_state(
	struct rtvk_context* ctx,
	struct rtvk_graphics_program* program,
	bool enabled,
	enum rt_blend_factor src_color,
	enum rt_blend_factor dst_color,
	enum rt_blend_op color_op,
	enum rt_blend_factor src_alpha,
	enum rt_blend_factor dst_alpha,
	enum rt_blend_op alpha_op) {
	if (!program) {
		rtvk_throwf(RT_IMPROPER_USAGE, "graphics program is invalid");
		return;
	}

	program->blend_enabled = enabled;
	program->src_color_blend = src_color;
	program->dst_color_blend = dst_color;
	program->color_blend_op = color_op;
	program->src_alpha_blend = src_alpha;
	program->dst_alpha_blend = dst_alpha;
	program->alpha_blend_op = alpha_op;
	rtvk_graphics_program_destroy_pipeline(ctx, program);
}

static struct rtvk_uniform_location* rtvk_graphics_program_find_uniform_location(struct rtvk_graphics_program* program, const char* name) {
	for (u32 i = 0; i < program->uniform_location_count; i++) {
		if (strcmp(program->uniform_locations[i].name, name) == 0) {
			return &program->uniform_locations[i];
		}
	}
	return NULL;
}

static bool rtvk_graphics_program_reserve_uniform_locations(struct rtvk_graphics_program* program, u32 count) {
	if (program->uniform_location_capacity >= count) { return true; }
	u32 capacity = program->uniform_location_capacity ? program->uniform_location_capacity : 8;
	while (capacity < count) { capacity *= 2; }

	void* locations = realloc(program->uniform_locations, sizeof(program->uniform_locations[0]) * capacity);
	if (!locations) {
		rtvk_throwf(RT_OUT_OF_HOST_MEMORY, "failed to allocate uniform locations");
		return false;
	}

	program->uniform_locations = locations;
	program->uniform_location_capacity = capacity;
	return true;
}

static bool rtvk_graphics_program_add_uniform_block(
	struct rtvk_graphics_program* program,
	const rtvk_shader_uniform_block* block,
	VkShaderStageFlags stages) {
	struct rtvk_uniform_location* existing = rtvk_graphics_program_find_uniform_location(program, block->name);
	if (existing) {
		if (existing->kind != RTVK_UNIFORM_LOCATION_BUFFER || existing->binding != block->binding) {
			rtvk_throwf(RT_SHADER_LINK_FAILED, "uniform block %s uses different bindings across shader stages", block->name);
			return false;
		}
		existing->stages |= stages;
		return true;
	}

	for (u32 i = 0; i < program->uniform_location_count; i++) {
		if (program->uniform_locations[i].binding == block->binding) {
			rtvk_throwf(RT_SHADER_LINK_FAILED, "uniforms %s and %s use the same binding", program->uniform_locations[i].name, block->name);
			return false;
		}
	}
	if (!rtvk_graphics_program_reserve_uniform_locations(program, program->uniform_location_count + 1)) {
		return false;
	}

	struct rtvk_uniform_location* location = &program->uniform_locations[program->uniform_location_count];
	location->program = program;
	snprintf(location->name, sizeof(location->name), "%s", block->name);
	location->name[sizeof(location->name) - 1] = '\0';
	location->stages = stages;
	location->kind = RTVK_UNIFORM_LOCATION_BUFFER;
	location->binding = block->binding;
	location->index = program->uniform_location_count;
	program->uniform_location_count++;
	return true;
}

static bool rtvk_graphics_program_add_texture(
	struct rtvk_graphics_program* program,
	const rtvk_shader_texture* texture,
	VkShaderStageFlags stages) {
	struct rtvk_uniform_location* existing = rtvk_graphics_program_find_uniform_location(program, texture->name);
	if (existing) {
		if (existing->kind != RTVK_UNIFORM_LOCATION_TEXTURE || existing->binding != texture->binding) {
			rtvk_throwf(RT_SHADER_LINK_FAILED, "texture uniform %s uses different bindings across shader stages", texture->name);
			return false;
		}
		existing->stages |= stages;
		return true;
	}

	for (u32 i = 0; i < program->uniform_location_count; i++) {
		if (program->uniform_locations[i].binding == texture->binding) {
			rtvk_throwf(RT_SHADER_LINK_FAILED, "uniforms %s and %s use the same binding", program->uniform_locations[i].name, texture->name);
			return false;
		}
	}
	if (!rtvk_graphics_program_reserve_uniform_locations(program, program->uniform_location_count + 1)) {
		return false;
	}

	struct rtvk_uniform_location* location = &program->uniform_locations[program->uniform_location_count];
	location->program = program;
	snprintf(location->name, sizeof(location->name), "%s", texture->name);
	location->name[sizeof(location->name) - 1] = '\0';
	location->stages = stages;
	location->kind = RTVK_UNIFORM_LOCATION_TEXTURE;
	location->binding = texture->binding;
	location->index = program->uniform_location_count;
	program->uniform_location_count++;
	return true;
}

static bool rtvk_graphics_program_build_uniform_locations(struct rtvk_graphics_program* program) {
	program->uniform_location_count = 0;

	for (u32 i = 0; i < program->vertex_reflection.uniform_block_count; i++) {
		if (!rtvk_graphics_program_add_uniform_block(program, &program->vertex_reflection.uniform_blocks[i], VK_SHADER_STAGE_VERTEX_BIT)) {
			return false;
		}
	}
	for (u32 i = 0; i < program->fragment_reflection.uniform_block_count; i++) {
		if (!rtvk_graphics_program_add_uniform_block(program, &program->fragment_reflection.uniform_blocks[i], VK_SHADER_STAGE_FRAGMENT_BIT)) {
			return false;
		}
	}
	for (u32 i = 0; i < program->vertex_reflection.texture_count; i++) {
		if (!rtvk_graphics_program_add_texture(program, &program->vertex_reflection.textures[i], VK_SHADER_STAGE_VERTEX_BIT)) {
			return false;
		}
	}
	for (u32 i = 0; i < program->fragment_reflection.texture_count; i++) {
		if (!rtvk_graphics_program_add_texture(program, &program->fragment_reflection.textures[i], VK_SHADER_STAGE_FRAGMENT_BIT)) {
			return false;
		}
	}
	return true;
}

void rtvk_graphics_program_link(struct rtvk_context* ctx, struct rtvk_graphics_program* program) {
	if (!program) {
		rtvk_throwf(RT_IMPROPER_USAGE, "graphics program is NULL");
		return;
	}
	if (!program->vertex_shader_source || !program->fragment_shader_source) {
		rtvk_throwf(RT_SHADER_LINK_FAILED, "graphics program link requires vertex and fragment shaders");
		return;
	}

	rtvk_graphics_shader_compile_result shaders = rtvk_shader_compile_graphics(
			ctx,
			program->vertex_layout.attribute_count ? &program->vertex_layout : NULL,
			program->vertex_shader_size,
			program->vertex_shader_source,
			program->fragment_shader_size,
			program->fragment_shader_source);
	if (!shaders.vertex_shader || !shaders.fragment_shader) {
		return;
	}

	rtvk_graphics_program_destroy_pipeline_layout(ctx, program);
	rtvk_graphics_program_destroy_shader(ctx, &program->vk_vertex_shader);
	rtvk_graphics_program_destroy_shader(ctx, &program->vk_fragment_shader);
	rtvk_shader_reflection_clear(&program->vertex_reflection);
	rtvk_shader_reflection_clear(&program->fragment_reflection);
	program->vk_vertex_shader = shaders.vertex_shader;
	program->vk_fragment_shader = shaders.fragment_shader;
	program->vertex_reflection = shaders.vertex_reflection;
	program->fragment_reflection = shaders.fragment_reflection;

	if (!rtvk_graphics_program_build_uniform_locations(program)) {
		return;
	}
	if (!program->vk_pipeline_layout && !rtvk_graphics_program_create_pipeline_layout(ctx, program)) {
		return;
	}
}

static VkCullModeFlags rtvk_cull_mode(enum rt_cull_mode mode) {
	switch (mode) {
		case RT_CULL_NONE: return VK_CULL_MODE_NONE;
		case RT_CULL_FRONT: return VK_CULL_MODE_FRONT_BIT;
		case RT_CULL_BACK: return VK_CULL_MODE_BACK_BIT;
		default: return VK_CULL_MODE_NONE;
	}
}

static VkFrontFace rtvk_front_face(enum rt_front_face face) {
	switch (face) {
		case RT_FRONT_FACE_CCW: return VK_FRONT_FACE_COUNTER_CLOCKWISE;
		case RT_FRONT_FACE_CW: return VK_FRONT_FACE_CLOCKWISE;
		default: return VK_FRONT_FACE_COUNTER_CLOCKWISE;
	}
}

static VkPolygonMode rtvk_fill_mode(enum rt_fill_mode mode) {
	switch (mode) {
		case RT_FILL_SOLID: return VK_POLYGON_MODE_FILL;
		case RT_FILL_WIREFRAME: return VK_POLYGON_MODE_LINE;
		default: return VK_POLYGON_MODE_FILL;
	}
}

static VkBlendFactor rtvk_blend_factor(enum rt_blend_factor factor) {
	switch (factor) {
		case RT_BLEND_ZERO: return VK_BLEND_FACTOR_ZERO;
		case RT_BLEND_ONE: return VK_BLEND_FACTOR_ONE;
		case RT_BLEND_SRC_COLOR: return VK_BLEND_FACTOR_SRC_COLOR;
		case RT_BLEND_ONE_MINUS_SRC_COLOR: return VK_BLEND_FACTOR_ONE_MINUS_SRC_COLOR;
		case RT_BLEND_DST_COLOR: return VK_BLEND_FACTOR_DST_COLOR;
		case RT_BLEND_ONE_MINUS_DST_COLOR: return VK_BLEND_FACTOR_ONE_MINUS_DST_COLOR;
		case RT_BLEND_SRC_ALPHA: return VK_BLEND_FACTOR_SRC_ALPHA;
		case RT_BLEND_ONE_MINUS_SRC_ALPHA: return VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
		case RT_BLEND_DST_ALPHA: return VK_BLEND_FACTOR_DST_ALPHA;
		case RT_BLEND_ONE_MINUS_DST_ALPHA: return VK_BLEND_FACTOR_ONE_MINUS_DST_ALPHA;
		default: return VK_BLEND_FACTOR_ONE;
	}
}

static VkBlendOp rtvk_blend_op(enum rt_blend_op op) {
	switch (op) {
		case RT_BLEND_OP_ADD: return VK_BLEND_OP_ADD;
		case RT_BLEND_OP_SUBTRACT: return VK_BLEND_OP_SUBTRACT;
		case RT_BLEND_OP_REVERSE_SUBTRACT: return VK_BLEND_OP_REVERSE_SUBTRACT;
		case RT_BLEND_OP_MIN: return VK_BLEND_OP_MIN;
		case RT_BLEND_OP_MAX: return VK_BLEND_OP_MAX;
		default: return VK_BLEND_OP_ADD;
	}
}

struct rtvk_uniform_location* rtvk_graphics_program_uniform_location(struct rtvk_context* ctx, struct rtvk_graphics_program* program, const char* name) {
	if (!program) {
		rtvk_throwf(RT_IMPROPER_USAGE, "graphics program is NULL");
		return NULL;
	}
	if (!name) {
		rtvk_throwf(RT_IMPROPER_USAGE, "uniform location name is NULL");
		return NULL;
	}
	if (!program->vk_pipeline_layout) {
		rtvk_throwf(RT_IMPROPER_USAGE, "graphics program must be linked before querying uniforms");
		return NULL;
	}

	return rtvk_graphics_program_find_uniform_location(program, name);
}


