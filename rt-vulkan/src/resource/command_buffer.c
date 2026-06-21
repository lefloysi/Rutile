#include <assert.h>
#include "command_buffer.h"
#include "context.h"
#include "error.h"
#include "resource/swapchain.h"
#include "resource/queue.h"
#include "resource/texture.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

/*===============================================================================================*/
/*                                                                                               */
/*===============================================================================================*/

rt_command_buffer rtCmdCreate(void) {
	struct rtvk_command_buffer* command_buffer = rtvk_command_buffer_create(rtvk_get_current_context());
	return rtvk_command_buffer_to_handle(command_buffer);
}
void rtCmdDestroy(rt_command_buffer command_buffer) {
	rtvk_command_buffer_destroy(rtvk_get_current_context(), rtvk_command_buffer_from_handle(command_buffer));
}

void rtCmdBegin(rt_command_buffer command_buffer, rt_queue queue) {
	rtvk_command_buffer_begin(
		rtvk_get_current_context(),
		rtvk_command_buffer_from_handle(command_buffer),
		rtvk_queue_from_handle(queue));
}
void rtCmdBeginRendering(rt_command_buffer command_buffer, rt_framebuffer framebuffer) {
	rtvk_command_buffer_begin_rendering(
		rtvk_get_current_context(),
		rtvk_command_buffer_from_handle(command_buffer),
		rtvk_framebuffer_from_handle(framebuffer));
}
void rtCmdClearColor(rt_command_buffer command_buffer, u32 color_index, f32 r, f32 g, f32 b, f32 a) {
	rtvk_command_buffer_clear_color(
		rtvk_get_current_context(),
		rtvk_command_buffer_from_handle(command_buffer),
		color_index,
		r,
		g,
		b,
		a);
}
void rtCmdClearDepth(rt_command_buffer command_buffer, f32 depth) {
	rtvk_command_buffer_clear_depth(
		rtvk_get_current_context(),
		rtvk_command_buffer_from_handle(command_buffer),
		depth);
}
void rtCmdClearStencil(rt_command_buffer command_buffer, u32 stencil) {
	rtvk_command_buffer_clear_stencil(
		rtvk_get_current_context(),
		rtvk_command_buffer_from_handle(command_buffer),
		stencil);
}
void rtCmdEndRendering(rt_command_buffer command_buffer) {
	rtvk_command_buffer_end_rendering(
		rtvk_get_current_context(),
		rtvk_command_buffer_from_handle(command_buffer));
}
void rtCmdEnd(rt_command_buffer command_buffer) {
	rtvk_command_buffer_end(
		rtvk_get_current_context(),
		rtvk_command_buffer_from_handle(command_buffer));
}

void rtCmdUseGraphicsProgram(rt_command_buffer command_buffer, rt_graphics_program program) {
	rtvk_command_buffer_use_graphics_program(
		rtvk_get_current_context(),
		rtvk_command_buffer_from_handle(command_buffer),
		rtvk_graphics_program_from_handle(program));
}

void rtCmdSetScissor(rt_command_buffer command_buffer, u32 x, u32 y, u32 width, u32 height) {
	rtvk_command_buffer_set_scissor(
		rtvk_get_current_context(),
		rtvk_command_buffer_from_handle(command_buffer),
		x,
		y,
		width,
		height);
}

void rtCmdUseComputeProgram(rt_command_buffer command_buffer, rt_compute_program program) {
	rtvk_command_buffer_use_compute_program(
		rtvk_get_current_context(),
		rtvk_command_buffer_from_handle(command_buffer),
		rtvk_compute_program_from_handle(program));
}

void rtCmdUniformBuffer(rt_command_buffer command_buffer, rt_uniform_location location, rt_buffer buffer, u64 offset, u64 size) {
	rtvk_command_buffer_uniform_buffer(
		rtvk_get_current_context(),
		rtvk_command_buffer_from_handle(command_buffer),
		rtvk_uniform_location_from_handle(location),
		rtvk_buffer_from_handle(buffer),
		offset,
		size);
}

void rtCmdUniformTexture(rt_command_buffer command_buffer, rt_uniform_location location, rt_texture_view texture_view) {
	rtvk_command_buffer_uniform_texture(
		rtvk_get_current_context(),
		rtvk_command_buffer_from_handle(command_buffer),
		rtvk_uniform_location_from_handle(location),
		rtvk_texture_view_from_handle(texture_view));
}

void rtCmdStorageBuffer(rt_command_buffer command_buffer, u32 binding, rt_buffer buffer, u64 offset, u64 size) {
	rtvk_command_buffer_storage_buffer(
		rtvk_get_current_context(),
		rtvk_command_buffer_from_handle(command_buffer),
		binding,
		rtvk_buffer_from_handle(buffer),
		offset,
		size);
}

void rtCmdStorageTexture(rt_command_buffer command_buffer, u32 binding, rt_texture_view texture_view) {
	rtvk_command_buffer_storage_texture(
		rtvk_get_current_context(),
		rtvk_command_buffer_from_handle(command_buffer),
		binding,
		rtvk_texture_view_from_handle(texture_view));
}

void rtCmdComputeBarrier(rt_command_buffer command_buffer) {
	rtvk_command_buffer_compute_barrier(
		rtvk_get_current_context(),
		rtvk_command_buffer_from_handle(command_buffer));
}

void rtCmdBindVertexBuffer(rt_command_buffer command_buffer, rt_buffer buffer, u64 offset) {
	rtvk_command_buffer_bind_vertex_buffer(
		rtvk_get_current_context(),
		rtvk_command_buffer_from_handle(command_buffer),
		rtvk_buffer_from_handle(buffer),
		offset);
}

void rtCmdDraw(rt_command_buffer command_buffer, u32 vertex_count, u32 first_vertex) {
	rtvk_command_buffer_draw(
		rtvk_get_current_context(),
		rtvk_command_buffer_from_handle(command_buffer),
		vertex_count,
		first_vertex);
}

void rtCmdDispatch(rt_command_buffer command_buffer, u32 group_count_x, u32 group_count_y, u32 group_count_z) {
	rtvk_command_buffer_dispatch(
		rtvk_get_current_context(),
		rtvk_command_buffer_from_handle(command_buffer),
		group_count_x,
		group_count_y,
		group_count_z);
}

/*===============================================================================================*/
/*                                                                                               */
/*===============================================================================================*/

RTVK_DEFINE_RESOURCE_PRIVATE(command_buffer)

void rtvk_command_buffer_init(struct rtvk_context* ctx, struct rtvk_command_buffer* command_buffer) {
	rtvk_init_resource_base(ctx, RTVK_RESOURCE_BASE(command_buffer), RT_RESOURCE_COMMAND_BUFFER);
	command_buffer->queue = NULL;
	command_buffer->family_index = (u32)-1;
}
void rtvk_command_buffer_finish(struct rtvk_context* ctx, struct rtvk_command_buffer* command_buffer) {
	struct rtvk_command_buffer* node = command_buffer->next;
	rtvk_command_buffer_wait_pending(ctx, command_buffer->active);
	rtvk_command_buffer_node_release(command_buffer->active);
	command_buffer->active = NULL;
	while (node) {
		struct rtvk_command_buffer* next = node->next;
		node->next = NULL;
		rtvk_command_buffer_wait_pending(ctx, node);
		rtvk_command_buffer_node_release(node);
		node = next;
	}
	command_buffer->next = NULL;
	rtvk_command_buffer_destroy_descriptor_pools(ctx, command_buffer);
	rtvk_finish_resource_base(ctx, RTVK_RESOURCE_BASE(command_buffer));
}

void rtvk_command_buffer_release_recorded_resources(struct rtvk_command_buffer* command_buffer) {
	assert(command_buffer);
	if (command_buffer->vertex_buffer_node) {
		rtvk_buffer_node_release(command_buffer->vertex_buffer_node);
	}
	command_buffer->vertex_buffer_node = NULL;
	if (command_buffer->graphics_program) {
		rtvk_release_resource(command_buffer->graphics_program);
	}
	command_buffer->graphics_program = NULL;
	if (command_buffer->compute_program) {
		rtvk_release_resource(command_buffer->compute_program);
	}
	command_buffer->compute_program = NULL;
	for (u32 i = 0; i < command_buffer->uniform_slot_count; i++) {
		rtvk_command_buffer_clear_uniform_slot(&command_buffer->uniform_slots[i]);
	}
	free(command_buffer->uniform_slots);
	command_buffer->uniform_slots = NULL;
	command_buffer->uniform_slot_count = 0;
	rtvk_command_buffer_reset_descriptor_pools(command_buffer->base.ctx, command_buffer);
	command_buffer->bound_descriptor_set = VK_NULL_HANDLE;
	command_buffer->uniforms_dirty = true;
}

void rtvk_command_buffer_clear_uniform_slot(rtvk_uniform_slot* slot) {
	assert(slot);
	if (slot->kind == RTVK_UNIFORM_SLOT_BUFFER || slot->kind == RTVK_UNIFORM_SLOT_STORAGE_BUFFER) {
		rtvk_buffer_node_release(slot->buffer.node);
	}
	if (slot->kind == RTVK_UNIFORM_SLOT_TEXTURE || slot->kind == RTVK_UNIFORM_SLOT_STORAGE_TEXTURE) {
		if (slot->texture.view) {
			rtvk_release_resource(slot->texture.view);
		}
	}
	*slot = (rtvk_uniform_slot){ 0 };
}

rtvk_uniform_slot* rtvk_command_buffer_uniform_slot(struct rtvk_command_buffer* command_buffer, u32 index) {
	if (index < command_buffer->uniform_slot_count) {
		return &command_buffer->uniform_slots[index];
	}

	u32 count = command_buffer->uniform_slot_count ? command_buffer->uniform_slot_count : 8;
	while (count <= index) {
		count *= 2;
	}
	void* slots = realloc(command_buffer->uniform_slots, sizeof(command_buffer->uniform_slots[0]) * count);
	if (!slots) {
		rtvk_throwf(RT_OUT_OF_HOST_MEMORY, "failed to allocate command buffer uniform slots");
		return NULL;
	}

	command_buffer->uniform_slots = slots;
	memset(&command_buffer->uniform_slots[command_buffer->uniform_slot_count], 0, sizeof(command_buffer->uniform_slots[0]) * (count - command_buffer->uniform_slot_count));
	command_buffer->uniform_slot_count = count;
	return &command_buffer->uniform_slots[index];
}

void rtvk_command_buffer_reset_descriptor_pools(struct rtvk_context* ctx, struct rtvk_command_buffer* command_buffer) {
	assert(command_buffer);
	command_buffer->current_descriptor_pool = command_buffer->descriptor_pools;
	for (rtvk_descriptor_pool_node* pool = command_buffer->descriptor_pools; pool; pool = pool->next) {
		if (!pool->vk_pool) {
			continue;
		}
		VkResult result = vkResetDescriptorPool(ctx->vk_device, pool->vk_pool, 0);
		if (result != VK_SUCCESS) {
			rtvk_throwf(rtvk_error_from_vk(result), NULL);
			continue;
		}
		pool->allocated_sets = 0;
	}
}

void rtvk_command_buffer_destroy_descriptor_pools(struct rtvk_context* ctx, struct rtvk_command_buffer* command_buffer) {
	assert(command_buffer);
	rtvk_descriptor_pool_node* pool = command_buffer->descriptor_pools;
	while (pool) {
		rtvk_descriptor_pool_node* next = pool->next;
		if (pool->vk_pool) {
			vkDestroyDescriptorPool(ctx->vk_device, pool->vk_pool, VK_ALLOCATOR);
		}
		free(pool);
		pool = next;
	}
	command_buffer->descriptor_pools = NULL;
	command_buffer->current_descriptor_pool = NULL;
}

void rtvk_command_buffer_destroy_vk_handles(struct rtvk_context* ctx, struct rtvk_command_buffer* command_buffer) {
	assert(command_buffer);
	if (command_buffer->vk_command_buffer) {
		vkFreeCommandBuffers(ctx->vk_device, command_buffer->vk_command_pool, 1, &command_buffer->vk_command_buffer);
		command_buffer->vk_command_buffer = VK_NULL_HANDLE;
	}
	if (command_buffer->vk_command_pool) {
		vkDestroyCommandPool(ctx->vk_device, command_buffer->vk_command_pool, VK_ALLOCATOR);
		command_buffer->vk_command_pool = VK_NULL_HANDLE;
	}
	command_buffer->bound_descriptor_set = VK_NULL_HANDLE;
	command_buffer->uniforms_dirty = true;
	command_buffer->family_index = (u32)-1;
}
void rtvk_command_buffer_wait_pending(struct rtvk_context* ctx, struct rtvk_command_buffer* command_buffer) {
	if (!command_buffer->pending_timepoint.queue || command_buffer->pending_timepoint.value == 0) {
		return;
	}

	struct rtvk_timepoint timepoint = command_buffer->pending_timepoint;
	rtvk_timepoint_wait(ctx, timepoint);
	command_buffer->pending_timepoint.queue = NULL;
	command_buffer->pending_timepoint.value = 0;
}
struct rtvk_command_buffer* rtvk_command_buffer_node_create(struct rtvk_context* ctx, u32 family_index) {
	struct rtvk_command_buffer* node = calloc(1, sizeof(*node));
	if (!node) {
		rtvk_throwf(RT_OUT_OF_HOST_MEMORY, "failed to allocate command buffer node");
		return NULL;
	}

	VkCommandPoolCreateInfo pool_info = { VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO };
	pool_info.pNext = NULL;
	pool_info.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
	pool_info.queueFamilyIndex = family_index;

	VkResult result = vkCreateCommandPool(ctx->vk_device, &pool_info, VK_ALLOCATOR, &node->vk_command_pool);
	if (result != VK_SUCCESS) {
		free(node);
		rtvk_throwf(rtvk_error_from_vk(result), NULL);
		return NULL;
	}

	VkCommandBufferAllocateInfo allocate_info = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO };
	allocate_info.pNext = NULL;
	allocate_info.commandPool = node->vk_command_pool;
	allocate_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	allocate_info.commandBufferCount = 1;

	result = vkAllocateCommandBuffers(ctx->vk_device, &allocate_info, &node->vk_command_buffer);
	if (result != VK_SUCCESS) {
		vkDestroyCommandPool(ctx->vk_device, node->vk_command_pool, VK_ALLOCATOR);
		free(node);
		rtvk_throwf(rtvk_error_from_vk(result), NULL);
		return NULL;
	}

	rtvk_init_resource_base(ctx, RTVK_RESOURCE_BASE(node), RT_RESOURCE_COMMAND_BUFFER);
	node->family_index = family_index;
	return node;
}

static void rtvk_command_buffer_recycle_node(struct rtvk_command_buffer* command_buffer, struct rtvk_command_buffer* node) {
	if (!node) {
		return;
	}
	node->next = command_buffer->next;
	command_buffer->next = node;
}

static struct rtvk_command_buffer* rtvk_command_buffer_take_reusable_node(struct rtvk_command_buffer* command_buffer, u32 family_index) {
	struct rtvk_command_buffer** link = &command_buffer->next;
	while (*link) {
		struct rtvk_command_buffer* node = *link;
		if (node->family_index == family_index && node->base.ref_count == 1) {
			*link = node->next;
			node->next = NULL;
			return node;
		}
		link = &node->next;
	}
	return NULL;
}
void rtvk_command_buffer_prepare(struct rtvk_context* ctx, struct rtvk_command_buffer* command_buffer, struct rtvk_queue* queue) {
	rtvk_command_buffer_recycle_node(command_buffer, command_buffer->active);
	command_buffer->active = NULL;

	struct rtvk_command_buffer* node = rtvk_command_buffer_take_reusable_node(command_buffer, queue->family_index);
	if (!node) {
		rtvk_queue_collect_to_value(ctx, queue, rtvk_queue_completed_value(ctx, queue));
		node = rtvk_command_buffer_take_reusable_node(command_buffer, queue->family_index);
	}
	if (!node) {
		node = rtvk_command_buffer_node_create(ctx, queue->family_index);
	}
	if (!node) {
		return;
	}

	rtvk_command_buffer_release_recorded_resources(node);
	command_buffer->active = node;
}

void rtvk_command_buffer_begin(struct rtvk_context* ctx, struct rtvk_command_buffer* command_buffer, struct rtvk_queue* queue) {
	command_buffer->queue = queue;
	rtvk_command_buffer_prepare(ctx, command_buffer, queue);
	if (rtvk_error() != RT_SUCCESS) { return; }

	struct rtvk_command_buffer* node = command_buffer->active;
	vkResetCommandBuffer(node->vk_command_buffer, 0);

	VkCommandBufferBeginInfo begin_info = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
	begin_info.pNext = NULL;
	begin_info.flags = 0;
	begin_info.pInheritanceInfo = NULL;

	VkResult result = vkBeginCommandBuffer(node->vk_command_buffer, &begin_info);
	if (result != VK_SUCCESS) {
		rtvk_throwf(rtvk_error_from_vk(result), NULL);
		return;
	}

	command_buffer->recording = true;
	command_buffer->framebuffer = NULL;
	command_buffer->graphics_program = NULL;
	command_buffer->compute_program = NULL;
	command_buffer->vertex_buffer = NULL;
	node->bound_descriptor_set = VK_NULL_HANDLE;
	node->uniforms_dirty = true;
}
void rtvk_command_buffer_begin_rendering(struct rtvk_context* ctx, struct rtvk_command_buffer* command_buffer, struct rtvk_framebuffer* framebuffer) {
	struct rtvk_command_buffer* node = command_buffer ? command_buffer->active : NULL;
	if (!node || !command_buffer->recording) {
		rtvk_throwf(RT_IMPROPER_USAGE, "begin rendering requires a recording command buffer");
		return;
	}
	if (!framebuffer) {
		rtvk_throwf(RT_IMPROPER_USAGE, "begin rendering requires a framebuffer");
		return;
	}
	struct rtvk_texture_view* color_view = framebuffer->color_views[0];
	struct rtvk_texture_view* depth_view = framebuffer->depth_view;
	VkRenderingAttachmentInfo color_attachment = { VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO };
	color_attachment.pNext = NULL;
	color_attachment.imageView = color_view ? color_view->vk_image_view : VK_NULL_HANDLE;
	color_attachment.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
	color_attachment.resolveMode = VK_RESOLVE_MODE_NONE;
	color_attachment.resolveImageView = VK_NULL_HANDLE;
	color_attachment.resolveImageLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	color_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
	color_attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;

	rtvk_command_buffer_transition_texture(
		node,
		color_view,
		VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
		VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
		VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
		VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT
	);

	VkRenderingAttachmentInfo depth_attachment = { VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO };
	depth_attachment.pNext = NULL;
	depth_attachment.imageView = depth_view ? depth_view->vk_image_view : VK_NULL_HANDLE;
	depth_attachment.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
	depth_attachment.resolveMode = VK_RESOLVE_MODE_NONE;
	depth_attachment.resolveImageView = VK_NULL_HANDLE;
	depth_attachment.resolveImageLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	depth_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
	depth_attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	if (depth_view) {
		rtvk_command_buffer_transition_texture(
			node,
			depth_view,
			VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
			VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
			VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
			VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT);
	}

	VkRenderingInfo rendering_info = { VK_STRUCTURE_TYPE_RENDERING_INFO };
	rendering_info.pNext = NULL;
	rendering_info.flags = 0;
	rendering_info.renderArea.offset.x = 0;
	rendering_info.renderArea.offset.y = 0;
	rendering_info.renderArea.extent.width = color_view ? color_view->width : 0;
	rendering_info.renderArea.extent.height = color_view ? color_view->height : 0;
	rendering_info.layerCount = 1;
	rendering_info.viewMask = 0;
	rendering_info.colorAttachmentCount = 1;
	rendering_info.pColorAttachments = &color_attachment;
	rendering_info.pDepthAttachment = depth_view ? &depth_attachment : NULL;
	rendering_info.pStencilAttachment = NULL;

	vkCmdBeginRendering(node->vk_command_buffer, &rendering_info);
	command_buffer->framebuffer = framebuffer;
}
void rtvk_command_buffer_clear_color(struct rtvk_context* ctx, struct rtvk_command_buffer* command_buffer, u32 color_index, f32 r, f32 g, f32 b, f32 a) {
	struct rtvk_command_buffer* node = command_buffer ? command_buffer->active : NULL;
	if (!node || !command_buffer->recording || !command_buffer->framebuffer) {
		rtvk_throwf(RT_IMPROPER_USAGE, "clear color requires active rendering");
		return;
	}

	struct rtvk_framebuffer* framebuffer = command_buffer->framebuffer;
	if (!framebuffer || color_index >= framebuffer->color_texture_count) {
		rtvk_throwf(RT_IMPROPER_USAGE, "color attachment index is out of range");
		return;
	}

	struct rtvk_texture_view* color_view = framebuffer->color_views[color_index];
	if (!color_view || !color_view->vk_image_view) {
		rtvk_throwf(RT_IMPROPER_USAGE, "color attachment view is NULL");
		return;
	}

	VkClearAttachment attachment = { 0 };
	attachment.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	attachment.colorAttachment = color_index;
	attachment.clearValue.color.float32[0] = r;
	attachment.clearValue.color.float32[1] = g;
	attachment.clearValue.color.float32[2] = b;
	attachment.clearValue.color.float32[3] = a;

	VkClearRect rect = { 0 };
	rect.rect.offset.x = 0;
	rect.rect.offset.y = 0;
	rect.rect.extent.width = color_view->width;
	rect.rect.extent.height = color_view->height;
	rect.baseArrayLayer = 0;
	rect.layerCount = 1;

	vkCmdClearAttachments(node->vk_command_buffer, 1, &attachment, 1, &rect);
}
void rtvk_command_buffer_clear_depth(struct rtvk_context* ctx, struct rtvk_command_buffer* command_buffer, f32 depth) {
	struct rtvk_command_buffer* node = command_buffer ? command_buffer->active : NULL;
	if (!node || !command_buffer->recording || !command_buffer->framebuffer) {
		rtvk_throwf(RT_IMPROPER_USAGE, "clear depth requires active rendering");
		return;
	}

	struct rtvk_texture_view* depth_view = command_buffer->framebuffer->depth_view;
	if (!depth_view || !depth_view->vk_image_view) {
		rtvk_throwf(RT_IMPROPER_USAGE, "depth attachment view is NULL");
		return;
	}

	VkClearAttachment attachment = { 0 };
	attachment.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
	attachment.clearValue.depthStencil.depth = depth;

	VkClearRect rect = { 0 };
	rect.rect.offset.x = 0;
	rect.rect.offset.y = 0;
	rect.rect.extent.width = depth_view->width;
	rect.rect.extent.height = depth_view->height;
	rect.baseArrayLayer = 0;
	rect.layerCount = 1;

	vkCmdClearAttachments(node->vk_command_buffer, 1, &attachment, 1, &rect);
}
void rtvk_command_buffer_clear_stencil(struct rtvk_context* ctx, struct rtvk_command_buffer* command_buffer, u32 stencil) {
	struct rtvk_command_buffer* node = command_buffer ? command_buffer->active : NULL;
	if (!node || !command_buffer->recording || !command_buffer->framebuffer) {
		rtvk_throwf(RT_IMPROPER_USAGE, "clear stencil requires active rendering");
		return;
	}

	struct rtvk_texture_view* depth_view = command_buffer->framebuffer->depth_view;
	if (!depth_view || !depth_view->vk_image_view) {
		rtvk_throwf(RT_IMPROPER_USAGE, "depth attachment view is NULL");
		return;
	}

	VkClearAttachment attachment = { 0 };
	attachment.aspectMask = VK_IMAGE_ASPECT_STENCIL_BIT;
	attachment.clearValue.depthStencil.stencil = stencil;

	VkClearRect rect = { 0 };
	rect.rect.offset.x = 0;
	rect.rect.offset.y = 0;
	rect.rect.extent.width = depth_view->width;
	rect.rect.extent.height = depth_view->height;
	rect.baseArrayLayer = 0;
	rect.layerCount = 1;

	vkCmdClearAttachments(node->vk_command_buffer, 1, &attachment, 1, &rect);
}
void rtvk_command_buffer_end_rendering(struct rtvk_context* ctx, struct rtvk_command_buffer* command_buffer) {
	struct rtvk_command_buffer* node = command_buffer ? command_buffer->active : NULL;
	if (!node || !command_buffer->recording || !command_buffer->framebuffer) {
		rtvk_throwf(RT_IMPROPER_USAGE, "end rendering requires an active rendering pass");
		return;
	}
	vkCmdEndRendering(node->vk_command_buffer);
	command_buffer->framebuffer = NULL;
}
void rtvk_command_buffer_end(struct rtvk_context* ctx, struct rtvk_command_buffer* command_buffer) {
	struct rtvk_command_buffer* node = command_buffer ? command_buffer->active : NULL;
	if (!node || !command_buffer->recording) {
		rtvk_throwf(RT_IMPROPER_USAGE, "end command buffer requires a recording command buffer");
		return;
	}
	VkResult result = vkEndCommandBuffer(node->vk_command_buffer);
	if (result != VK_SUCCESS) {
		rtvk_throwf(rtvk_error_from_vk(result), NULL);
		return;
	}
	command_buffer->recording = false;
}

bool rtvk_command_buffer_has_queue(struct rtvk_command_buffer* command_buffer) {
	return command_buffer && command_buffer->queue;
}

static VkAccessFlags rtvk_command_buffer_layout_access(VkImageLayout layout) {
	switch (layout) {
	case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL: /**********/ return VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
	case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL: /**/ return VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
	case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL: /**************/ return VK_ACCESS_TRANSFER_READ_BIT;
	case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL: /**************/ return VK_ACCESS_TRANSFER_WRITE_BIT;
	case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL: /**********/ return VK_ACCESS_SHADER_READ_BIT;
	case VK_IMAGE_LAYOUT_GENERAL: /***************************/ return VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
	default: return 0;
	}
}

static VkPipelineStageFlags rtvk_command_buffer_layout_stage(VkImageLayout layout) {
	switch (layout) {
	case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL: /**********/ return VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL: /**/ return VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
	case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL: /**************/ return VK_PIPELINE_STAGE_TRANSFER_BIT;
	case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL: /**************/ return VK_PIPELINE_STAGE_TRANSFER_BIT;
	case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:/***********/ return VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
	case VK_IMAGE_LAYOUT_GENERAL: /***************************/ return VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
	default: /************************************************/ return VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
	}
}

void rtvk_command_buffer_transition_texture(struct rtvk_command_buffer* command_buffer, struct rtvk_texture_view* view, VkImageLayout layout, VkAccessFlags dst_access, VkPipelineStageFlags src_stage, VkPipelineStageFlags dst_stage) {
	assert(command_buffer);
	assert(view);
	assert(view->vk_image);
	struct rtvk_texture* texture = view->texture;

	if (view->vk_layout == layout) {
		return;
	}

	VkImageMemoryBarrier barrier = { VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER };
	barrier.pNext = NULL;
	VkImageLayout old_layout = view->vk_layout;
	barrier.srcAccessMask = rtvk_command_buffer_layout_access(old_layout);
	VkPipelineStageFlags actual_src_stage = rtvk_command_buffer_layout_stage(old_layout);
	if (actual_src_stage == VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT) {
		barrier.srcAccessMask = 0;
		actual_src_stage = src_stage;
	}
	barrier.dstAccessMask = dst_access;
	barrier.oldLayout = old_layout;
	barrier.newLayout = layout;
	barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.image = view->vk_image;
	barrier.subresourceRange.aspectMask = rtvk_texture_format_aspect(view->vk_format);
	barrier.subresourceRange.baseMipLevel = 0;
	barrier.subresourceRange.levelCount = texture && texture->active ? texture->active->mip_levels : 1;
	barrier.subresourceRange.baseArrayLayer = 0;
	barrier.subresourceRange.layerCount = 1;

	vkCmdPipelineBarrier(command_buffer->vk_command_buffer, actual_src_stage, dst_stage, 0, 0, NULL, 0, NULL, 1, &barrier);
	view->vk_layout = layout;
	if (texture) {
		texture->vk_layout = layout;
	}
}

void rtvk_command_buffer_suspend_rendering(struct rtvk_context* ctx, struct rtvk_command_buffer* command_buffer) {
	(void)ctx;
	if (!command_buffer || !command_buffer->framebuffer) {
		return;
	}
	struct rtvk_command_buffer* node = command_buffer->active;
	vkCmdEndRendering(node->vk_command_buffer);
	command_buffer->framebuffer = NULL;
}

void rtvk_command_buffer_resume_rendering(struct rtvk_context* ctx, struct rtvk_command_buffer* command_buffer, struct rtvk_framebuffer* framebuffer) {
	if (!framebuffer) {
		return;
	}
	rtvk_command_buffer_begin_rendering(ctx, command_buffer, framebuffer);
}

void rtvk_command_buffer_use_graphics_program(struct rtvk_context* ctx, struct rtvk_command_buffer* command_buffer, struct rtvk_graphics_program* program) {
	struct rtvk_command_buffer* node = command_buffer->active;
	struct rtvk_framebuffer* framebuffer = command_buffer->framebuffer;
	struct rtvk_texture_view* color_view = framebuffer ? framebuffer->color_views[0] : NULL;
	struct rtvk_texture_view* depth_view = framebuffer ? framebuffer->depth_view : NULL;

	if (!color_view) {
		rtvk_throwf(RT_IMPROPER_USAGE, "command buffer has no color attachment");
		return;
	}

	VkFormat depth_format = depth_view ? depth_view->vk_format : VK_FORMAT_UNDEFINED;
	rtvk_graphics_program_prepare(ctx, program, color_view->vk_format, depth_format);
	if (rtvk_error() != RT_SUCCESS) { return; }

	vkCmdBindPipeline(node->vk_command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, program->vk_pipeline);

	VkViewport viewport = { 0 };
	viewport.x = 0.0f;
	viewport.y = (f32)color_view->height;
	viewport.width = (f32)color_view->width;
	viewport.height = -(f32)color_view->height;
	viewport.minDepth = 0.0f;
	viewport.maxDepth = 1.0f;
	vkCmdSetViewport(node->vk_command_buffer, 0, 1, &viewport);

	VkRect2D scissor = { 0 };
	scissor.offset.x = 0;
	scissor.offset.y = 0;
	scissor.extent.width = color_view->width;
	scissor.extent.height = color_view->height;
	vkCmdSetScissor(node->vk_command_buffer, 0, 1, &scissor);

	if (command_buffer->graphics_program != program) {
		node->bound_descriptor_set = VK_NULL_HANDLE;
		node->uniforms_dirty = true;
	}
	if (node->graphics_program != program) {
		rtvk_retain_resource(program);
		rtvk_release_resource(node->graphics_program);
		node->graphics_program = program;
	}
	rtvk_release_resource(node->compute_program);
	node->compute_program = NULL;
	command_buffer->graphics_program = program;
	command_buffer->compute_program = NULL;
}

void rtvk_command_buffer_set_scissor(struct rtvk_context* ctx, struct rtvk_command_buffer* command_buffer, u32 x, u32 y, u32 width, u32 height) {
	(void)ctx;
	struct rtvk_command_buffer* node = command_buffer ? command_buffer->active : NULL;
	if (!node || !command_buffer->recording || !command_buffer->framebuffer) {
		rtvk_throwf(RT_IMPROPER_USAGE, "setting a scissor requires active rendering");
		return;
	}

	VkRect2D scissor = { 0 };
	scissor.offset.x = (int32_t)x;
	scissor.offset.y = (int32_t)y;
	scissor.extent.width = width;
	scissor.extent.height = height;
	vkCmdSetScissor(node->vk_command_buffer, 0, 1, &scissor);
}

void rtvk_command_buffer_uniform_buffer(
	struct rtvk_context* ctx,
	struct rtvk_command_buffer* command_buffer,
	struct rtvk_uniform_location* location,
	struct rtvk_buffer* buffer,
	u64 offset,
	u64 size) {
	struct rtvk_command_buffer* node = command_buffer ? command_buffer->active : NULL;
	struct rtvk_buffer* buffer_node = buffer ? buffer->active : NULL;

	if (!node || !command_buffer->recording || !command_buffer->framebuffer) {
		rtvk_throwf(RT_IMPROPER_USAGE, "setting a uniform buffer requires active rendering");
		return;
	}
	if (!location) {
		rtvk_throwf(RT_IMPROPER_USAGE, "uniform location is NULL");
		return;
	}
	if (location->kind != RTVK_UNIFORM_LOCATION_BUFFER) {
		rtvk_throwf(RT_IMPROPER_USAGE, "uniform location does not accept a buffer");
		return;
	}
	if (!command_buffer->graphics_program || command_buffer->graphics_program != location->program) {
		rtvk_throwf(RT_IMPROPER_USAGE, "uniform location does not belong to the active graphics program");
		return;
	}
	if (!buffer_node || !buffer_node->vk_buffer) {
		rtvk_throwf(RT_IMPROPER_USAGE, "uniform buffer has no storage");
		return;
	}
	if (!(buffer_node->usage & RT_BUFFER_USAGE_UNIFORM) || (buffer_node->usage & RT_BUFFER_USAGE_STAGING)) {
		rtvk_throwf(RT_IMPROPER_USAGE, "buffer usage is not compatible with uniform binding");
		return;
	}
	if (size == 0) {
		rtvk_throwf(RT_IMPROPER_USAGE, "uniform buffer binding size is zero");
		return;
	}
	if (offset > buffer_node->size || size > buffer_node->size - offset) {
		rtvk_throwf(RT_IMPROPER_USAGE, "uniform buffer range is out of bounds");
		return;
	}

	rtvk_uniform_slot* slot = rtvk_command_buffer_uniform_slot(node, location->index);
	if (!slot) {
		return;
	}
	if (slot->kind == RTVK_UNIFORM_SLOT_BUFFER &&
		slot->buffer.node == buffer_node &&
		slot->buffer.offset == offset &&
		slot->buffer.size == size) {
		return;
	}
	rtvk_command_buffer_clear_uniform_slot(slot);
	rtvk_buffer_node_retain(buffer_node);
	slot->kind = RTVK_UNIFORM_SLOT_BUFFER;
	slot->buffer.node = buffer_node;
	slot->buffer.offset = offset;
	slot->buffer.size = size;
	node->bound_descriptor_set = VK_NULL_HANDLE;
	node->uniforms_dirty = true;
}

void rtvk_command_buffer_uniform_texture(
	struct rtvk_context* ctx,
	struct rtvk_command_buffer* command_buffer,
	struct rtvk_uniform_location* location,
	struct rtvk_texture_view* texture_view) {
	struct rtvk_command_buffer* node = command_buffer ? command_buffer->active : NULL;
	if (!node || !command_buffer->recording || !command_buffer->framebuffer) {
		rtvk_throwf(RT_IMPROPER_USAGE, "setting a uniform texture requires active rendering");
		return;
	}
	if (!location) {
		rtvk_throwf(RT_IMPROPER_USAGE, "uniform location is NULL");
		return;
	}
	if (location->kind != RTVK_UNIFORM_LOCATION_TEXTURE) {
		rtvk_throwf(RT_IMPROPER_USAGE, "uniform location does not accept a texture");
		return;
	}
	if (!command_buffer->graphics_program || command_buffer->graphics_program != location->program) {
		rtvk_throwf(RT_IMPROPER_USAGE, "uniform location does not belong to the active graphics program");
		return;
	}
	if (!texture_view || !texture_view->vk_image || !texture_view->vk_image_view) {
		rtvk_throwf(RT_IMPROPER_USAGE, "uniform texture view is NULL");
		return;
	}
	struct rtvk_framebuffer* active_framebuffer = command_buffer->framebuffer;
	bool resume_rendering = texture_view->texture && texture_view->texture->vk_layout != VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	if (resume_rendering) {
		rtvk_command_buffer_suspend_rendering(ctx, command_buffer);
	}
	rtvk_command_buffer_transition_texture(node, texture_view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
										   VK_ACCESS_SHADER_READ_BIT,
										   VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
										   VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
	if (resume_rendering) {
		rtvk_command_buffer_resume_rendering(ctx, command_buffer, active_framebuffer);
	}
	rtvk_uniform_slot* slot = rtvk_command_buffer_uniform_slot(node, location->index);
	if (!slot) {
		return;
	}
	if (slot->kind == RTVK_UNIFORM_SLOT_TEXTURE && slot->texture.view == texture_view) {
		return;
	}
	rtvk_command_buffer_clear_uniform_slot(slot);
	rtvk_retain_resource(texture_view);
	slot->kind = RTVK_UNIFORM_SLOT_TEXTURE;
	slot->texture.view = texture_view;
	node->bound_descriptor_set = VK_NULL_HANDLE;
	node->uniforms_dirty = true;
}

void rtvk_command_buffer_use_compute_program(struct rtvk_context* ctx, struct rtvk_command_buffer* command_buffer, struct rtvk_compute_program* program) {
	struct rtvk_command_buffer* node = command_buffer ? command_buffer->active : NULL;
	if (!node || !command_buffer->recording || command_buffer->framebuffer) {
		rtvk_throwf(RT_IMPROPER_USAGE, "binding a compute program requires command recording outside active rendering");
		return;
	}
	if (!program || !program->vk_pipeline || !program->vk_pipeline_layout) {
		rtvk_throwf(RT_IMPROPER_USAGE, "compute program must be linked before use");
		return;
	}
	vkCmdBindPipeline(node->vk_command_buffer, VK_PIPELINE_BIND_POINT_COMPUTE, program->vk_pipeline);
	if (command_buffer->compute_program != program) {
		node->bound_descriptor_set = VK_NULL_HANDLE;
		node->uniforms_dirty = true;
	}
	if (node->compute_program != program) {
		rtvk_retain_resource(program);
		rtvk_release_resource(node->compute_program);
		node->compute_program = program;
	}
	rtvk_release_resource(node->graphics_program);
	node->graphics_program = NULL;
	command_buffer->compute_program = program;
	command_buffer->graphics_program = NULL;
}

void rtvk_command_buffer_storage_buffer(
	struct rtvk_context* ctx,
	struct rtvk_command_buffer* command_buffer,
	u32 binding,
	struct rtvk_buffer* buffer,
	u64 offset,
	u64 size) {
	struct rtvk_command_buffer* node = command_buffer ? command_buffer->active : NULL;
	struct rtvk_buffer* buffer_node = buffer ? buffer->active : NULL;
	if (!node || !command_buffer->recording) {
		rtvk_throwf(RT_IMPROPER_USAGE, "setting a storage buffer requires command recording");
		return;
	}
	u32 slot_index = binding;
	if (command_buffer->framebuffer) {
		if (!command_buffer->graphics_program) {
			rtvk_throwf(RT_IMPROPER_USAGE, "setting a graphics storage buffer requires an active graphics program");
			return;
		}
		struct rtvk_uniform_location* location = NULL;
		for (u32 i = 0; i < command_buffer->graphics_program->uniform_location_count; ++i) {
			struct rtvk_uniform_location* candidate = &command_buffer->graphics_program->uniform_locations[i];
			if (candidate->kind == RTVK_UNIFORM_LOCATION_STORAGE_BUFFER && candidate->binding == binding) {
				location = candidate;
				break;
			}
		}
		if (!location) {
			rtvk_throwf(RT_IMPROPER_USAGE, "storage buffer binding is not declared by the active graphics program");
			return;
		}
		slot_index = location->index;
	} else {
		if (!command_buffer->compute_program || binding >= command_buffer->compute_program->binding_count ||
			command_buffer->compute_program->bindings[binding] != RTVK_COMPUTE_BINDING_STORAGE_BUFFER) {
			rtvk_throwf(RT_IMPROPER_USAGE, "storage buffer binding is not declared by the active compute program");
			return;
		}
	}
	if (!buffer_node || !buffer_node->vk_buffer) {
		rtvk_throwf(RT_IMPROPER_USAGE, "storage buffer has no storage");
		return;
	}
	if (!(buffer_node->usage & RT_BUFFER_USAGE_STORAGE) || (buffer_node->usage & RT_BUFFER_USAGE_STAGING)) {
		rtvk_throwf(RT_IMPROPER_USAGE, "buffer usage is not compatible with storage binding");
		return;
	}
	if (size == 0 || offset > buffer_node->size || size > buffer_node->size - offset) {
		rtvk_throwf(RT_IMPROPER_USAGE, "storage buffer range is out of bounds");
		return;
	}
	rtvk_uniform_slot* slot = rtvk_command_buffer_uniform_slot(node, slot_index);
	if (!slot) {
		return;
	}
	rtvk_command_buffer_clear_uniform_slot(slot);
	rtvk_buffer_node_retain(buffer_node);
	slot->kind = RTVK_UNIFORM_SLOT_STORAGE_BUFFER;
	slot->buffer.node = buffer_node;
	slot->buffer.offset = offset;
	slot->buffer.size = size;
	node->bound_descriptor_set = VK_NULL_HANDLE;
	node->uniforms_dirty = true;
}

void rtvk_command_buffer_storage_texture(
	struct rtvk_context* ctx,
	struct rtvk_command_buffer* command_buffer,
	u32 binding,
	struct rtvk_texture_view* texture_view) {
	struct rtvk_command_buffer* node = command_buffer ? command_buffer->active : NULL;
	if (!node || !command_buffer->recording || command_buffer->framebuffer) {
		rtvk_throwf(RT_IMPROPER_USAGE, "setting a storage texture requires command recording outside active rendering");
		return;
	}
	if (!command_buffer->compute_program || binding >= command_buffer->compute_program->binding_count ||
		command_buffer->compute_program->bindings[binding] != RTVK_COMPUTE_BINDING_STORAGE_TEXTURE) {
		rtvk_throwf(RT_IMPROPER_USAGE, "storage texture binding is not declared by the active compute program");
		return;
	}
	if (!texture_view || !texture_view->vk_image || !texture_view->vk_image_view) {
		rtvk_throwf(RT_IMPROPER_USAGE, "storage texture view is NULL");
		return;
	}
	rtvk_command_buffer_transition_texture(node, texture_view, VK_IMAGE_LAYOUT_GENERAL,
										   VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT,
										   VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
										   VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
	rtvk_uniform_slot* slot = rtvk_command_buffer_uniform_slot(node, binding);
	if (!slot) {
		return;
	}
	rtvk_command_buffer_clear_uniform_slot(slot);
	rtvk_retain_resource(texture_view);
	slot->kind = RTVK_UNIFORM_SLOT_STORAGE_TEXTURE;
	slot->texture.view = texture_view;
	node->bound_descriptor_set = VK_NULL_HANDLE;
	node->uniforms_dirty = true;
}

void rtvk_command_buffer_compute_barrier(struct rtvk_context* ctx, struct rtvk_command_buffer* command_buffer) {
	(void)ctx;
	struct rtvk_command_buffer* node = command_buffer ? command_buffer->active : NULL;
	if (!node || !command_buffer->recording || command_buffer->framebuffer) {
		rtvk_throwf(RT_IMPROPER_USAGE, "compute barrier requires command recording outside active rendering");
		return;
	}

	VkMemoryBarrier barrier = { VK_STRUCTURE_TYPE_MEMORY_BARRIER };
	barrier.srcAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
	barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
	VkPipelineStageFlags dst_stage = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
	if (node->queue && node->queue->capability == RT_QUEUE_GRAPHICS) {
		dst_stage |= VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
	}
	vkCmdPipelineBarrier(
		node->vk_command_buffer,
		VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
		dst_stage,
		0,
		1,
		&barrier,
		0,
		NULL,
		0,
		NULL);
}

void rtvk_command_buffer_bind_vertex_buffer(struct rtvk_context* ctx, struct rtvk_command_buffer* command_buffer, struct rtvk_buffer* buffer, u64 offset) {
	struct rtvk_command_buffer* command_buffer_node = command_buffer->active;
	struct rtvk_buffer* node = buffer ? buffer->active : NULL;

	if (!node || !node->vk_buffer) {
		rtvk_throwf(RT_IMPROPER_USAGE, "vertex buffer has no storage");
		return;
	}
	if (!(node->usage & RT_BUFFER_USAGE_VERTEX) || (node->usage & RT_BUFFER_USAGE_STAGING)) {
		rtvk_throwf(RT_IMPROPER_USAGE, "buffer usage is not compatible with vertex binding");
		return;
	}
	if (offset > node->size) {
		rtvk_throwf(RT_IMPROPER_USAGE, "vertex buffer offset is out of range");
		return;
	}

	VkDeviceSize vk_offset = (VkDeviceSize)offset;
	vkCmdBindVertexBuffers(command_buffer_node->vk_command_buffer, 0, 1, &node->vk_buffer, &vk_offset);
	rtvk_buffer_node_retain(node);
	if (command_buffer_node->vertex_buffer_node) {
		rtvk_buffer_node_release(command_buffer_node->vertex_buffer_node);
	}
	command_buffer_node->vertex_buffer_node = node;
	command_buffer->vertex_buffer = buffer;
}

static rtvk_descriptor_pool_node* rtvk_command_buffer_create_descriptor_pool(
	struct rtvk_context* ctx,
	struct rtvk_command_buffer* command_buffer,
	u32 min_sets,
	u32 descriptors_per_type) {
	u32 max_sets = RTVK_INITIAL_DESCRIPTOR_SETS_PER_POOL;
	if (command_buffer->current_descriptor_pool && command_buffer->current_descriptor_pool->max_sets >= max_sets) {
		max_sets = command_buffer->current_descriptor_pool->max_sets * 2;
	}
	if (max_sets < min_sets) {
		max_sets = min_sets;
	}
	if (descriptors_per_type == 0) {
		descriptors_per_type = 1;
	}

	VkDescriptorPoolSize pool_sizes[4] = {
		{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, max_sets * descriptors_per_type },
		{ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, max_sets * descriptors_per_type },
		{ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, max_sets * descriptors_per_type },
		{ VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, max_sets * descriptors_per_type },
	};

	VkDescriptorPoolCreateInfo pool_info = { VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO };
	pool_info.pNext = NULL;
	pool_info.flags = 0;
	pool_info.maxSets = max_sets;
	pool_info.poolSizeCount = 4;
	pool_info.pPoolSizes = pool_sizes;

	rtvk_descriptor_pool_node* pool = calloc(1, sizeof(*pool));
	if (!pool) {
		rtvk_throwf(RT_OUT_OF_HOST_MEMORY, "failed to allocate descriptor pool metadata");
		return NULL;
	}

	VkResult result = vkCreateDescriptorPool(ctx->vk_device, &pool_info, VK_ALLOCATOR, &pool->vk_pool);
	if (result != VK_SUCCESS) {
		free(pool);
		rtvk_throwf(rtvk_error_from_vk(result), NULL);
		return NULL;
	}

	pool->max_sets = max_sets;
	pool->descriptors_per_type = descriptors_per_type;
	pool->next = command_buffer->descriptor_pools;
	command_buffer->descriptor_pools = pool;
	command_buffer->current_descriptor_pool = pool;
	return pool;
}

static void rtvk_command_buffer_allocate_descriptor_set(
	struct rtvk_context* ctx,
	struct rtvk_command_buffer* command_buffer,
	struct rtvk_graphics_program* program,
	VkDescriptorSet* descriptor_set) {
	u32 descriptors_per_type = program->uniform_location_count ? program->uniform_location_count : 1;
	rtvk_descriptor_pool_node* pool = command_buffer->current_descriptor_pool;
	if (!pool || pool->allocated_sets >= pool->max_sets ||
		pool->descriptors_per_type < descriptors_per_type) {
		pool = rtvk_command_buffer_create_descriptor_pool(ctx, command_buffer, 1, descriptors_per_type);
		if (!pool) {
			return;
		}
	}

	VkDescriptorSetAllocateInfo allocate_info = { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO };
	allocate_info.pNext = NULL;
	allocate_info.descriptorPool = pool->vk_pool;
	allocate_info.descriptorSetCount = 1;
	allocate_info.pSetLayouts = &program->vk_descriptor_set_layout;

	VkResult result = vkAllocateDescriptorSets(ctx->vk_device, &allocate_info, descriptor_set);
	if (result == VK_ERROR_OUT_OF_POOL_MEMORY || result == VK_ERROR_FRAGMENTED_POOL) {
		pool = rtvk_command_buffer_create_descriptor_pool(ctx, command_buffer, 1, descriptors_per_type);
		if (!pool) {
			return;
		}
		allocate_info.descriptorPool = pool->vk_pool;
		result = vkAllocateDescriptorSets(ctx->vk_device, &allocate_info, descriptor_set);
	}
	if (result != VK_SUCCESS) {
		rtvk_throwf(rtvk_error_from_vk(result), NULL);
		return;
	}

	pool->allocated_sets++;
}

static void rtvk_command_buffer_allocate_compute_descriptor_set(
	struct rtvk_context* ctx,
	struct rtvk_command_buffer* command_buffer,
	struct rtvk_compute_program* program,
	VkDescriptorSet* descriptor_set) {
	u32 descriptors_per_type = program->binding_count ? program->binding_count : 1;
	rtvk_descriptor_pool_node* pool = command_buffer->current_descriptor_pool;
	if (!pool || pool->allocated_sets >= pool->max_sets || pool->descriptors_per_type < descriptors_per_type) {
		pool = rtvk_command_buffer_create_descriptor_pool(ctx, command_buffer, 1, descriptors_per_type);
		if (!pool) {
			return;
		}
	}

	VkDescriptorSetAllocateInfo allocate_info = { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO };
	allocate_info.descriptorPool = pool->vk_pool;
	allocate_info.descriptorSetCount = 1;
	allocate_info.pSetLayouts = &program->vk_descriptor_set_layout;

	VkResult result = vkAllocateDescriptorSets(ctx->vk_device, &allocate_info, descriptor_set);
	if (result == VK_ERROR_OUT_OF_POOL_MEMORY || result == VK_ERROR_FRAGMENTED_POOL) {
		pool = rtvk_command_buffer_create_descriptor_pool(ctx, command_buffer, 1, descriptors_per_type);
		if (!pool) {
			return;
		}
		allocate_info.descriptorPool = pool->vk_pool;
		result = vkAllocateDescriptorSets(ctx->vk_device, &allocate_info, descriptor_set);
	}
	if (result != VK_SUCCESS) {
		rtvk_throwf(rtvk_error_from_vk(result), NULL);
		return;
	}
	pool->allocated_sets++;
}

static void rtvk_command_buffer_bind_uniform_buffers(struct rtvk_context* ctx, struct rtvk_command_buffer* command_buffer) {
	struct rtvk_command_buffer* node = command_buffer->active;
	struct rtvk_graphics_program* program = command_buffer->graphics_program;
	if (!program || program->uniform_location_count == 0) {
		return;
	}
	if (!program->vk_descriptor_set_layout || !program->vk_pipeline_layout) {
		rtvk_throwf(RT_IMPROPER_USAGE, "graphics program uniform layout is not ready");
		return;
	}
	if (!node->uniforms_dirty && node->bound_descriptor_set) {
		return;
	}
	VkDescriptorSet descriptor_set = VK_NULL_HANDLE;
	rtvk_command_buffer_allocate_descriptor_set(ctx, node, program, &descriptor_set);
	if (rtvk_error() != RT_SUCCESS) { return; }

	u32 location_count = program->uniform_location_count;
	VkDescriptorBufferInfo stack_buffer_infos[16];
	VkDescriptorImageInfo stack_image_infos[16];
	VkWriteDescriptorSet stack_writes[16];
	VkDescriptorBufferInfo* buffer_infos = location_count <= 16 ? stack_buffer_infos : calloc(location_count, sizeof(*buffer_infos));
	VkDescriptorImageInfo* image_infos = location_count <= 16 ? stack_image_infos : calloc(location_count, sizeof(*image_infos));
	VkWriteDescriptorSet* writes = location_count <= 16 ? stack_writes : calloc(location_count, sizeof(*writes));
	if (location_count <= 16) {
		memset(buffer_infos, 0, sizeof(stack_buffer_infos));
		memset(image_infos, 0, sizeof(stack_image_infos));
		memset(writes, 0, sizeof(stack_writes));
	}
	if (!buffer_infos || !image_infos || !writes) {
		if (buffer_infos != stack_buffer_infos) {
			free(buffer_infos);
		}
		if (image_infos != stack_image_infos) {
			free(image_infos);
		}
		if (writes != stack_writes) {
			free(writes);
		}
		rtvk_throwf(RT_OUT_OF_HOST_MEMORY, "failed to allocate descriptor writes");
		return;
	}

	for (u32 i = 0; i < program->uniform_location_count; i++) {
		struct rtvk_uniform_location* location = &program->uniform_locations[i];
		writes[i] = (VkWriteDescriptorSet){ VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
		writes[i].pNext = NULL;
		writes[i].dstSet = descriptor_set;
		writes[i].dstBinding = location->binding;
		writes[i].dstArrayElement = 0;
		writes[i].descriptorCount = 1;
		writes[i].pTexelBufferView = NULL;

		if (location->kind == RTVK_UNIFORM_LOCATION_BUFFER || location->kind == RTVK_UNIFORM_LOCATION_STORAGE_BUFFER) {
			rtvk_uniform_slot* slot = location->index < node->uniform_slot_count ? &node->uniform_slots[location->index] : NULL;
			const bool wants_storage = location->kind == RTVK_UNIFORM_LOCATION_STORAGE_BUFFER;
			if (!slot || slot->kind != (wants_storage ? RTVK_UNIFORM_SLOT_STORAGE_BUFFER : RTVK_UNIFORM_SLOT_BUFFER) || !slot->buffer.node) {
				rtvk_throwf(RT_IMPROPER_USAGE, "%s %s is not bound", wants_storage ? "storage buffer" : "uniform buffer", location->name);
				if (buffer_infos != stack_buffer_infos) {
					free(buffer_infos);
				}
				if (image_infos != stack_image_infos) {
					free(image_infos);
				}
				if (writes != stack_writes) {
					free(writes);
				}
				return;
			}

			buffer_infos[i].buffer = slot->buffer.node->vk_buffer;
			buffer_infos[i].offset = slot->buffer.offset;
			buffer_infos[i].range = slot->buffer.size;
			writes[i].descriptorType = wants_storage ? VK_DESCRIPTOR_TYPE_STORAGE_BUFFER : VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
			writes[i].pImageInfo = NULL;
			writes[i].pBufferInfo = &buffer_infos[i];
		} else {
			rtvk_uniform_slot* slot = location->index < node->uniform_slot_count ? &node->uniform_slots[location->index] : NULL;
			if (!slot || slot->kind != RTVK_UNIFORM_SLOT_TEXTURE || !slot->texture.view) {
				rtvk_throwf(RT_IMPROPER_USAGE, "uniform texture %s is not bound", location->name);
				if (buffer_infos != stack_buffer_infos) {
					free(buffer_infos);
				}
				if (image_infos != stack_image_infos) {
					free(image_infos);
				}
				if (writes != stack_writes) {
					free(writes);
				}
				return;
			}

			image_infos[i].sampler = slot->texture.view->vk_sampler;
			image_infos[i].imageView = slot->texture.view->vk_image_view;
			image_infos[i].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
			writes[i].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
			writes[i].pImageInfo = &image_infos[i];
			writes[i].pBufferInfo = NULL;
		}
	}

	vkUpdateDescriptorSets(ctx->vk_device, program->uniform_location_count, writes, 0, NULL);
	vkCmdBindDescriptorSets(node->vk_command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
							program->vk_pipeline_layout, 0, 1, &descriptor_set, 0, NULL);
	node->bound_descriptor_set = descriptor_set;
	node->uniforms_dirty = false;
	if (buffer_infos != stack_buffer_infos) {
		free(buffer_infos);
	}
	if (image_infos != stack_image_infos) {
		free(image_infos);
	}
	if (writes != stack_writes) {
		free(writes);
	}
}

void rtvk_command_buffer_draw(struct rtvk_context* ctx, struct rtvk_command_buffer* command_buffer, u32 vertex_count, u32 first_vertex) {
	rtvk_command_buffer_bind_uniform_buffers(ctx, command_buffer);
	if (rtvk_error() != RT_SUCCESS) { return; }
	vkCmdDraw(command_buffer->active->vk_command_buffer, vertex_count, 1, first_vertex, 0);
}

static void rtvk_command_buffer_bind_compute_storage(struct rtvk_context* ctx, struct rtvk_command_buffer* command_buffer) {
	struct rtvk_command_buffer* node = command_buffer->active;
	struct rtvk_compute_program* program = command_buffer->compute_program;
	if (!program) {
		rtvk_throwf(RT_IMPROPER_USAGE, "no compute program is bound");
		return;
	}
	if (!program->vk_descriptor_set_layout || !program->vk_pipeline_layout) {
		rtvk_throwf(RT_IMPROPER_USAGE, "compute program descriptor layout is not ready");
		return;
	}
	if (!node->uniforms_dirty && node->bound_descriptor_set) {
		return;
	}

	VkDescriptorSet descriptor_set = VK_NULL_HANDLE;
	rtvk_command_buffer_allocate_compute_descriptor_set(ctx, node, program, &descriptor_set);
	if (rtvk_error() != RT_SUCCESS) { return; }

	VkDescriptorBufferInfo buffer_infos[RTVK_MAX_COMPUTE_BINDINGS] = { 0 };
	VkDescriptorImageInfo image_infos[RTVK_MAX_COMPUTE_BINDINGS] = { 0 };
	VkWriteDescriptorSet writes[RTVK_MAX_COMPUTE_BINDINGS] = { 0 };
	u32 write_count = 0;
	for (u32 binding = 0; binding < program->binding_count; binding++) {
		rtvk_compute_binding_kind kind = program->bindings[binding];
		if (!kind) {
			continue;
		}
		rtvk_uniform_slot* slot = binding < node->uniform_slot_count ? &node->uniform_slots[binding] : NULL;
		if (!slot) {
			rtvk_throwf(RT_IMPROPER_USAGE, "compute storage binding %u is not bound", binding);
			return;
		}
		VkWriteDescriptorSet* write = &writes[write_count];
		*write = (VkWriteDescriptorSet){ VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
		write->dstSet = descriptor_set;
		write->dstBinding = binding;
		write->descriptorCount = 1;

		if (kind == RTVK_COMPUTE_BINDING_STORAGE_BUFFER) {
			if (slot->kind != RTVK_UNIFORM_SLOT_STORAGE_BUFFER || !slot->buffer.node) {
				rtvk_throwf(RT_IMPROPER_USAGE, "compute storage buffer binding %u is not bound", binding);
				return;
			}
			buffer_infos[write_count].buffer = slot->buffer.node->vk_buffer;
			buffer_infos[write_count].offset = slot->buffer.offset;
			buffer_infos[write_count].range = slot->buffer.size;
			write->descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
			write->pBufferInfo = &buffer_infos[write_count];
		} else {
			if (slot->kind != RTVK_UNIFORM_SLOT_STORAGE_TEXTURE || !slot->texture.view) {
				rtvk_throwf(RT_IMPROPER_USAGE, "compute storage texture binding %u is not bound", binding);
				return;
			}
			image_infos[write_count].imageView = slot->texture.view->vk_image_view;
			image_infos[write_count].imageLayout = VK_IMAGE_LAYOUT_GENERAL;
			write->descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
			write->pImageInfo = &image_infos[write_count];
		}
		write_count++;
	}

	vkUpdateDescriptorSets(ctx->vk_device, write_count, writes, 0, NULL);
	vkCmdBindDescriptorSets(node->vk_command_buffer, VK_PIPELINE_BIND_POINT_COMPUTE,
							program->vk_pipeline_layout, 0, 1, &descriptor_set, 0, NULL);
	node->bound_descriptor_set = descriptor_set;
	node->uniforms_dirty = false;
}

void rtvk_command_buffer_dispatch(struct rtvk_context* ctx, struct rtvk_command_buffer* command_buffer, u32 group_count_x, u32 group_count_y, u32 group_count_z) {
	if (!command_buffer || !command_buffer->active || !command_buffer->recording || command_buffer->framebuffer) {
		rtvk_throwf(RT_IMPROPER_USAGE, "dispatch requires command recording outside active rendering");
		return;
	}
	if (!group_count_x || !group_count_y || !group_count_z) {
		rtvk_throwf(RT_IMPROPER_USAGE, "dispatch group counts must be non-zero");
		return;
	}
	rtvk_command_buffer_bind_compute_storage(ctx, command_buffer);
	if (rtvk_error() != RT_SUCCESS) { return; }
	vkCmdDispatch(command_buffer->active->vk_command_buffer, group_count_x, group_count_y, group_count_z);
}

void rtvk_command_buffer_node_retain(struct rtvk_command_buffer* command_buffer) {
	assert(command_buffer);
	command_buffer->base.ref_count++;
}

void rtvk_command_buffer_node_release(struct rtvk_command_buffer* command_buffer) {
	assert(command_buffer);
	assert(command_buffer->base.ref_count > 0);
	if (--command_buffer->base.ref_count != 0) {
		return;
	}

	rtvk_command_buffer_release_recorded_resources(command_buffer);
	rtvk_command_buffer_destroy_descriptor_pools(command_buffer->base.ctx, command_buffer);
	rtvk_command_buffer_destroy_vk_handles(command_buffer->base.ctx, command_buffer);
	rtvk_finish_resource_base(command_buffer->base.ctx, RTVK_RESOURCE_BASE(command_buffer));
	free(command_buffer);
}
