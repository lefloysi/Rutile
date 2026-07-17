#include "graphics_program.h"
#include "context.h"
#include "error.h"
#include "rtsl_spirv.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>

/*===============================================================================================*/
/*                                                                                               */
/*===============================================================================================*/

rt_graphics_program rtGraphicsProgramCreate(void) {
	return rtvk_graphics_program_to_handle(rtvk_graphics_program_create(rtvk_get_current_context()));
}

void rtGraphicsProgramDestroy(rt_graphics_program program) {
	rtvk_graphics_program_destroy(
		rtvk_get_current_context(), 
		rtvk_graphics_program_from_handle(program)
	);
}

void rtGraphicsProgramLayout(rt_graphics_program program, const rt_vertex_layout* layout) {
	rtvk_graphics_program_layout(
		rtvk_get_current_context(), 
		rtvk_graphics_program_from_handle(program), 
		layout);
}

void rtGraphicsProgramSource(rt_graphics_program program, u64 size, const void* data) {
	rtvk_graphics_program_source(
		rtvk_get_current_context(), 
		rtvk_graphics_program_from_handle(program), size, data
	);
}

void rtGraphicsProgramRasterState(rt_graphics_program program, enum rt_cull_mode cull_mode, enum rt_front_face front_face, enum rt_fill_mode fill_mode) {
	rtvk_graphics_program_raster_state(rtvk_get_current_context(), rtvk_graphics_program_from_handle(program), cull_mode, front_face, fill_mode);
}

void rtGraphicsProgramBlendState(rt_graphics_program program, bool enabled,enum rt_blend_factor src_color, enum rt_blend_factor dst_color, enum rt_blend_op color_op, enum rt_blend_factor src_alpha, enum rt_blend_factor dst_alpha, enum rt_blend_op alpha_op) {
	rtvk_graphics_program_blend_state(
		rtvk_get_current_context(), 
		rtvk_graphics_program_from_handle(program), enabled, src_color, dst_color, color_op, src_alpha, dst_alpha, alpha_op
	);
}

void rtGraphicsProgramFinalize(rt_graphics_program program) {
	rtvk_graphics_program_finalize(
		rtvk_get_current_context(), 
		rtvk_graphics_program_from_handle(program)
	);
}

void rtGraphicsProgramReset(rt_graphics_program program) {
	rtvk_graphics_program_reset(
		rtvk_get_current_context(),
		rtvk_graphics_program_from_handle(program)
	);
}

rt_uniform_location rtGraphicsProgramUniformLocation(rt_graphics_program program, const char* name) {
	return rtvk_uniform_location_to_handle(rtvk_graphics_program_uniform_location(rtvk_get_current_context(),rtvk_graphics_program_from_handle(program), name));
}

/*===============================================================================================*/
/*                                                                                               */
/*===============================================================================================*/

RTVK_DEFINE_RESOURCE_PRIVATE(graphics_program)

void rtvk_graphics_program_destroy_shader(struct rtvk_context* ctx, VkShaderModule* shader) {
	if (*shader) {
		vkDestroyShaderModule(ctx->vk_device, *shader, VK_ALLOCATOR);
		*shader = VK_NULL_HANDLE;
	}
}

void rtvk_graphics_program_destroy_shader_source(char** source, u64* size) {
	free(*source);
	*source = NULL;
	*size = 0;
}

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

void rtvk_graphics_program_clear_uniform_locations(struct rtvk_graphics_program* program) {
	free(program->uniform_locations);
	program->uniform_locations = NULL;
	program->uniform_location_count = 0;
	program->uniform_location_capacity = 0;
}

void rtvk_graphics_program_destroy_pipeline(struct rtvk_context* ctx, struct rtvk_graphics_program* program) {
	(void)ctx;
	if (program->vk_pipeline) {
		vkDestroyPipeline(ctx->vk_device, program->vk_pipeline, VK_ALLOCATOR);
		program->vk_pipeline = VK_NULL_HANDLE;
		program->vk_pipeline_format = VK_FORMAT_UNDEFINED;
		program->vk_pipeline_depth_format = VK_FORMAT_UNDEFINED;
	}
}

void rtvk_graphics_program_destroy_pipeline_layout(struct rtvk_context* ctx, struct rtvk_graphics_program* program) {
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
	case RT_R32_SFLOAT:
		return VK_FORMAT_R32_SFLOAT;
	case RT_RG32_SFLOAT:
		return VK_FORMAT_R32G32_SFLOAT;
	case RT_RGB32_SFLOAT:
		return VK_FORMAT_R32G32B32_SFLOAT;
	case RT_RGBA32_SFLOAT:
		return VK_FORMAT_R32G32B32A32_SFLOAT;
	case RT_R32_SINT:
		return VK_FORMAT_R32_SINT;
	case RT_RG32_SINT:
		return VK_FORMAT_R32G32_SINT;
	case RT_RGB32_SINT:
		return VK_FORMAT_R32G32B32_SINT;
	case RT_RGBA32_SINT:
		return VK_FORMAT_R32G32B32A32_SINT;
	case RT_R32_UINT:
		return VK_FORMAT_R32_UINT;
	case RT_RG32_UINT:
		return VK_FORMAT_R32G32_UINT;
	case RT_RGB32_UINT:
		return VK_FORMAT_R32G32B32_UINT;
	case RT_RGBA32_UINT:
		return VK_FORMAT_R32G32B32A32_UINT;
	default:
		return VK_FORMAT_UNDEFINED;
	}
}

void rtvk_graphics_program_finish(struct rtvk_graphics_program* program) {
	struct rtvk_context* ctx = program->base.ctx;
	rtvk_graphics_program_destroy_pipeline_layout(ctx, program);
	vkDestroyShaderModule(ctx->vk_device, program->vk_vertex_shader, VK_ALLOCATOR);
	vkDestroyShaderModule(ctx->vk_device, program->vk_fragment_shader, VK_ALLOCATOR);

	free(program->program_source);

	free(program->uniform_locations);
	program->uniform_locations = NULL;
	program->uniform_location_count = 0;
	program->uniform_location_capacity = 0;

	rtvk_finish_resource_base(RTVK_RESOURCE_BASE(program));
}

static void rtvk_graphics_program_create_descriptor_set_layout(struct rtvk_context* ctx, struct rtvk_graphics_program* program) {
	if (program->uniform_location_count == 0) {
		return;
	}

	VkDescriptorSetLayoutBinding* bindings = calloc(program->uniform_location_count, sizeof(*bindings));
	if (!bindings) {
		rtvk_throwf(RT_OUT_OF_HOST_MEMORY, "failed to allocate descriptor set layout bindings");
		return;
	}

	for (u32 i = 0; i < program->uniform_location_count; i++) {
		bindings[i].binding = program->uniform_locations[i].binding;
		bindings[i].descriptorType = program->uniform_locations[i].kind == RTVK_UNIFORM_LOCATION_TEXTURE ? VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER : (program->uniform_locations[i].kind == RTVK_UNIFORM_LOCATION_STORAGE_BUFFER ? VK_DESCRIPTOR_TYPE_STORAGE_BUFFER : VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
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
		rtvk_throwf(rtvk_error_from_vk(result), "Vulkan call returned %s", rtvk_vk_result_name(result));
		return;
	}
}

static void rtvk_graphics_program_create_pipeline_layout(struct rtvk_context* ctx, struct rtvk_graphics_program* program) {
	VkPipelineLayoutCreateInfo layout_info = { VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO };

	if (!program->vk_descriptor_set_layout) {
		rtvk_graphics_program_create_descriptor_set_layout(ctx, program);
		if (program->uniform_location_count && !program->vk_descriptor_set_layout) {
			return;
		}
	}

	layout_info.pNext = NULL;
	layout_info.flags = 0;
	layout_info.setLayoutCount = program->vk_descriptor_set_layout ? 1 : 0;
	layout_info.pSetLayouts = program->vk_descriptor_set_layout ? &program->vk_descriptor_set_layout : NULL;
	layout_info.pushConstantRangeCount = 0;
	layout_info.pPushConstantRanges = NULL;

	VkResult result = vkCreatePipelineLayout(ctx->vk_device, &layout_info, VK_ALLOCATOR, &program->vk_pipeline_layout);
	if (result != VK_SUCCESS) {
		rtvk_throwf(rtvk_error_from_vk(result), "Vulkan call returned %s", rtvk_vk_result_name(result));
		return;
	}
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

static void rtvk_graphics_program_color_blend_state(struct rtvk_graphics_program* program, VkPipelineColorBlendAttachmentState* attachment, VkPipelineColorBlendStateCreateInfo* color_blend_info) {
	attachment->blendEnable = program->blend_enabled ? VK_TRUE : VK_FALSE;
	attachment->srcColorBlendFactor = rtvk_blend_factor(program->src_color_blend);
	attachment->dstColorBlendFactor = rtvk_blend_factor(program->dst_color_blend);
	attachment->colorBlendOp = rtvk_blend_op(program->color_blend_op);
	attachment->srcAlphaBlendFactor = rtvk_blend_factor(program->src_alpha_blend);
	attachment->dstAlphaBlendFactor = rtvk_blend_factor(program->dst_alpha_blend);
	attachment->alphaBlendOp = rtvk_blend_op(program->alpha_blend_op);
	attachment->colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

	*color_blend_info = (VkPipelineColorBlendStateCreateInfo){ VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO };
	color_blend_info->logicOpEnable = VK_FALSE;
	color_blend_info->logicOp = VK_LOGIC_OP_COPY;
	color_blend_info->attachmentCount = 1;
	color_blend_info->pAttachments = attachment;
}

static void rtvk_graphics_program_create_pipeline(struct rtvk_context* ctx, struct rtvk_graphics_program* program, VkFormat color_format, VkFormat depth_format) {
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
			attributes[i].location = i;
			attributes[i].binding = 0;
			attributes[i].format = rtvk_vertex_format(program->vertex_attributes[i].format);
			attributes[i].offset = program->vertex_attributes[i].offset;
			if (attributes[i].format == VK_FORMAT_UNDEFINED) {
				rtvk_throwf(RT_UNSUPPORTED_FEATURE, "unsupported vertex attribute format");
				return;
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
		rtvk_throwf(rtvk_error_from_vk(result), "Vulkan call returned %s", rtvk_vk_result_name(result));
		return;
	}

	program->vk_pipeline_format = color_format;
	program->vk_pipeline_depth_format = depth_format;
}

void rtvk_graphics_program_prepare(struct rtvk_context* ctx, struct rtvk_graphics_program* program, VkFormat color_format, VkFormat depth_format) {
	if (!program || !program->vk_pipeline_layout) {
		rtvk_throwf(RT_IMPROPER_USAGE, "graphics program must be finalized before use");
		return;
	}

	if (program->vk_pipeline && program->vk_pipeline_format == color_format &&
		program->vk_pipeline_depth_format == depth_format) {
		return;
	}

	rtvk_graphics_program_destroy_pipeline(ctx, program);
	rtvk_graphics_program_create_pipeline(ctx, program, color_format, depth_format);
}

void rtvk_graphics_program_layout(struct rtvk_context* ctx, struct rtvk_graphics_program* program, const rt_vertex_layout* layout) {
	assert(ctx);
	assert(program);
	assert(layout);
	if (!layout || !layout->attributes || layout->attribute_count == 0) {
		program->vertex_layout = (rt_vertex_layout){ 0 };
		rtvk_graphics_program_destroy_pipeline_layout(ctx, program);
		rtvk_graphics_program_destroy_shader(ctx, &program->vk_vertex_shader);
		rtvk_graphics_program_destroy_shader(ctx, &program->vk_fragment_shader);
		rtvk_graphics_program_clear_uniform_locations(program);
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
}

void rtvk_graphics_program_source(struct rtvk_context* ctx, struct rtvk_graphics_program* program, u64 size, const void* data) {
	assert(program);
	if (!data || size == 0) {
		rtvk_graphics_program_destroy_pipeline_layout(ctx, program);
		rtvk_graphics_program_destroy_shader(ctx, &program->vk_vertex_shader);
		rtvk_graphics_program_destroy_shader(ctx, &program->vk_fragment_shader);
		rtvk_graphics_program_destroy_shader_source(&program->program_source, &program->program_source_size);
		rtvk_graphics_program_clear_uniform_locations(program);
		return;
	}

	char* new_source = malloc((size_t)size);
	if (!new_source) {
		rtvk_throwf(RT_OUT_OF_HOST_MEMORY, "failed to allocate shader source storage");
		return;
	}
	memcpy(new_source, data, (size_t)size);

	rtvk_graphics_program_destroy_pipeline_layout(ctx, program);
	rtvk_graphics_program_destroy_shader(ctx, &program->vk_vertex_shader);
	rtvk_graphics_program_destroy_shader(ctx, &program->vk_fragment_shader);
	rtvk_graphics_program_destroy_shader_source(&program->program_source, &program->program_source_size);
	rtvk_graphics_program_clear_uniform_locations(program);
	program->program_source = new_source;
	program->program_source_size = size;
}

void rtvk_graphics_program_raster_state(struct rtvk_context* ctx, struct rtvk_graphics_program* program, enum rt_cull_mode cull_mode, enum rt_front_face front_face, enum rt_fill_mode fill_mode) {
	assert(ctx);
	assert(program);

	program->cull_mode = cull_mode;
	program->front_face = front_face;
	program->fill_mode = fill_mode;
	rtvk_graphics_program_destroy_pipeline(ctx, program);
}

void rtvk_graphics_program_blend_state(struct rtvk_context* ctx, struct rtvk_graphics_program* program, bool enabled, enum rt_blend_factor src_color, enum rt_blend_factor dst_color, enum rt_blend_op color_op, enum rt_blend_factor src_alpha, enum rt_blend_factor dst_alpha, enum rt_blend_op alpha_op) {
	assert(ctx);
	assert(program);

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
	assert(program);
	assert(name);
	for (u32 i = 0; i < program->uniform_location_count; i++) {
		if (strcmp(program->uniform_locations[i].name, name) == 0) {
			return &program->uniform_locations[i];
		}
	}
	return NULL;
}

static bool rtvk_graphics_program_reserve_uniform_locations(struct rtvk_graphics_program* program, u32 count) {
	assert(program);

	if (program->uniform_location_capacity >= count) {
		return true;
	}
	u32 capacity = program->uniform_location_capacity ? program->uniform_location_capacity : 8;
	while (capacity < count) {
		capacity *= 2;
	}

	void* locations = realloc(program->uniform_locations, sizeof(program->uniform_locations[0]) * capacity);
	if (!locations) {
		rtvk_throwf(RT_OUT_OF_HOST_MEMORY, "failed to allocate uniform locations");
		return false;
	}

	program->uniform_locations = locations;
	program->uniform_location_capacity = capacity;
	return true;
}

static bool rtvk_graphics_program_build_uniform_locations(
	struct rtvk_graphics_program* program,
	const rtsl_spirv_translation* translation
) {
	assert(program);
	assert(translation);

	rtvk_graphics_program_clear_uniform_locations(program);
	const u32 resource_count = rtsl_spirv_resource_count(translation);
	if (!rtvk_graphics_program_reserve_uniform_locations(program, resource_count)) {
		return false;
	}

	for (u32 i = 0; i < resource_count; i++) {
		rtsl_spirv_resource_info resource;
		if (!rtsl_spirv_resource(translation, i, &resource)) {
			rtvk_throwf(RT_SHADER_LINK_FAILED, "RTSL resource reflection failed at index %u", i);
			return false;
		}
		if (resource.descriptor_set != 0) {
			rtvk_throwf(RT_UNSUPPORTED_FEATURE, "RTSL resource %s uses unsupported descriptor set %u", resource.name, resource.descriptor_set);
			return false;
		}
		if (!resource.name || strlen(resource.name) >= RTVK_MAX_SHADER_UNIFORM_NAME) {
			rtvk_throwf(RT_SHADER_LINK_FAILED, "RTSL resource name is missing or exceeds the Vulkan backend limit");
			return false;
		}

		VkShaderStageFlags stages = 0;
		if (resource.stages & RTSL_SPIRV_STAGE_VERTEX) {
			stages |= VK_SHADER_STAGE_VERTEX_BIT;
		}
		if (resource.stages & RTSL_SPIRV_STAGE_FRAGMENT) {
			stages |= VK_SHADER_STAGE_FRAGMENT_BIT;
		}
		if (!stages) {
			continue;
		}

		rtvk_uniform_location_kind kind;
		switch (resource.kind) {
		case RTSL_SPIRV_UNIFORM_BUFFER:
			kind = RTVK_UNIFORM_LOCATION_BUFFER;
			break;
		case RTSL_SPIRV_STORAGE_BUFFER:
			kind = RTVK_UNIFORM_LOCATION_STORAGE_BUFFER;
			break;
		case RTSL_SPIRV_SAMPLED_TEXTURE:
			kind = RTVK_UNIFORM_LOCATION_TEXTURE;
			break;
		case RTSL_SPIRV_SAMPLER:
		case RTSL_SPIRV_STORAGE_IMAGE:
		default:
			rtvk_throwf(RT_UNSUPPORTED_FEATURE, "RTSL resource %s has an unsupported graphics resource kind", resource.name);
			return false;
		}

		struct rtvk_uniform_location* location = &program->uniform_locations[program->uniform_location_count];
		location->program = program;
		memcpy(location->name, resource.name, strlen(resource.name) + 1);
		location->stages = stages;
		location->kind = kind;
		location->binding = resource.binding;
		location->index = program->uniform_location_count;
		program->uniform_location_count++;
	}
	return true;
}

void rtvk_graphics_program_reset(struct rtvk_context* ctx, struct rtvk_graphics_program* program) {
	assert(ctx);
	assert(program);
	rtvk_graphics_program_destroy_pipeline_layout(ctx, program);
	rtvk_graphics_program_destroy_shader(ctx, &program->vk_vertex_shader);
	rtvk_graphics_program_destroy_shader(ctx, &program->vk_fragment_shader);
	rtvk_graphics_program_clear_uniform_locations(program);
}

static bool rtvk_graphics_program_create_shader(
	struct rtvk_context* ctx,
	const u32* words,
	u64 word_count,
	VkShaderModule* shader
) {
	if (!words || word_count == 0) {
		rtvk_throwf(RT_SHADER_COMPILATION_FAILED, "RTSL transpiler returned an empty SPIR-V module");
		return false;
	}

	VkShaderModuleCreateInfo shader_info = { VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO };
	shader_info.codeSize = (size_t)word_count * sizeof(*words);
	shader_info.pCode = words;
	const VkResult result = vkCreateShaderModule(ctx->vk_device, &shader_info, VK_ALLOCATOR, shader);
	if (result != VK_SUCCESS) {
		rtvk_throwf(rtvk_error_from_vk(result), "vkCreateShaderModule failed: %s", rtvk_vk_result_name(result));
		return false;
	}
	return true;
}

void rtvk_graphics_program_finalize(struct rtvk_context* ctx, struct rtvk_graphics_program* program) {
	assert(ctx);
	assert(program);

	if (!program->program_source || program->program_source_size == 0) {
		rtvk_throwf(RT_SHADER_LINK_FAILED, "graphics program finalize requires an RTSLP source set via rtGraphicsProgramSource");
		return;
	}
	if (program->vk_pipeline_layout || program->vk_vertex_shader || program->vk_fragment_shader) {
		rtvk_throwf(RT_IMPROPER_USAGE, "graphics program is already finalized; reset it before finalizing again");
		return;
	}

	char translation_message[1024] = { 0 };
	rtsl_spirv_translation* translation = NULL;
	const rtsl_spirv_status status = rtsl_spirv_translate(
		program->program_source_size,
		program->program_source,
		&translation,
		translation_message,
		sizeof(translation_message)
	);
	if (status != RTSL_SPIRV_SUCCESS) {
		const enum rt_error error = status == RTSL_SPIRV_OUT_OF_MEMORY
			? RT_OUT_OF_HOST_MEMORY
			: status == RTSL_SPIRV_INVALID_PROGRAM
				? RT_SHADER_LINK_FAILED
				: RT_SHADER_COMPILATION_FAILED;
		rtvk_throwf(error, "RTSL translation failed: %s", translation_message);
		return;
	}

	bool complete = false;
	u64 vertex_word_count = 0;
	const u32* vertex_words = rtsl_spirv_stage_words(translation, RTSL_SPIRV_VERTEX, &vertex_word_count);
	if (!rtvk_graphics_program_create_shader(ctx, vertex_words, vertex_word_count, &program->vk_vertex_shader)) {
		goto cleanup;
	}

	u64 fragment_word_count = 0;
	const u32* fragment_words = rtsl_spirv_stage_words(translation, RTSL_SPIRV_FRAGMENT, &fragment_word_count);
	if (!rtvk_graphics_program_create_shader(ctx, fragment_words, fragment_word_count, &program->vk_fragment_shader)) {
		goto cleanup;
	}

	if (!rtvk_graphics_program_build_uniform_locations(program, translation)) {
		goto cleanup;
	}
	rtvk_graphics_program_create_pipeline_layout(ctx, program);
	if (!program->vk_pipeline_layout) {
		goto cleanup;
	}
	complete = true;

cleanup:
	rtsl_spirv_translation_destroy(translation);
	if (!complete) {
		rtvk_graphics_program_destroy_pipeline_layout(ctx, program);
		rtvk_graphics_program_destroy_shader(ctx, &program->vk_vertex_shader);
		rtvk_graphics_program_destroy_shader(ctx, &program->vk_fragment_shader);
		rtvk_graphics_program_clear_uniform_locations(program);
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
	case RT_FRONT_FACE_CCW:
		return VK_FRONT_FACE_COUNTER_CLOCKWISE;
	case RT_FRONT_FACE_CW:
		return VK_FRONT_FACE_CLOCKWISE;
	default:
		return VK_FRONT_FACE_COUNTER_CLOCKWISE;
	}
}

static VkPolygonMode rtvk_fill_mode(enum rt_fill_mode mode) {
	switch (mode) {
	case RT_FILL_SOLID:
		return VK_POLYGON_MODE_FILL;
	case RT_FILL_WIREFRAME:
		return VK_POLYGON_MODE_LINE;
	default:
		return VK_POLYGON_MODE_FILL;
	}
}

static VkBlendFactor rtvk_blend_factor(enum rt_blend_factor factor) {
	switch (factor) {
	case RT_BLEND_ZERO:
		return VK_BLEND_FACTOR_ZERO;
	case RT_BLEND_ONE:
		return VK_BLEND_FACTOR_ONE;
	case RT_BLEND_SRC_COLOR:
		return VK_BLEND_FACTOR_SRC_COLOR;
	case RT_BLEND_ONE_MINUS_SRC_COLOR:
		return VK_BLEND_FACTOR_ONE_MINUS_SRC_COLOR;
	case RT_BLEND_DST_COLOR:
		return VK_BLEND_FACTOR_DST_COLOR;
	case RT_BLEND_ONE_MINUS_DST_COLOR:
		return VK_BLEND_FACTOR_ONE_MINUS_DST_COLOR;
	case RT_BLEND_SRC_ALPHA:
		return VK_BLEND_FACTOR_SRC_ALPHA;
	case RT_BLEND_ONE_MINUS_SRC_ALPHA:
		return VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
	case RT_BLEND_DST_ALPHA:
		return VK_BLEND_FACTOR_DST_ALPHA;
	case RT_BLEND_ONE_MINUS_DST_ALPHA:
		return VK_BLEND_FACTOR_ONE_MINUS_DST_ALPHA;
	default:
		return VK_BLEND_FACTOR_ONE;
	}
}

static VkBlendOp rtvk_blend_op(enum rt_blend_op op) {
	switch (op) {
	case RT_BLEND_OP_ADD:
		return VK_BLEND_OP_ADD;
	case RT_BLEND_OP_SUBTRACT:
		return VK_BLEND_OP_SUBTRACT;
	case RT_BLEND_OP_REVERSE_SUBTRACT:
		return VK_BLEND_OP_REVERSE_SUBTRACT;
	case RT_BLEND_OP_MIN:
		return VK_BLEND_OP_MIN;
	case RT_BLEND_OP_MAX:
		return VK_BLEND_OP_MAX;
	default:
		return VK_BLEND_OP_ADD;
	}
}

struct rtvk_uniform_location* rtvk_graphics_program_uniform_location(struct rtvk_context* ctx, struct rtvk_graphics_program* program, const char* name) {
	(void)ctx;
	if (!program || !name) {
		rtvk_throwf(RT_IMPROPER_USAGE, "graphics program and uniform name must be valid");
		return NULL;
	}
	if (!program->vk_pipeline_layout) {
		rtvk_throwf(RT_IMPROPER_USAGE, "graphics program must be finalized before querying uniforms");
		return NULL;
	}
	return rtvk_graphics_program_find_uniform_location(program, name);
}
