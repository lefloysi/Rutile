#include "buffer.h"
#include <assert.h>
#include "context.h"
#include "error.h"
#include "resource/queue.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

/*===============================================================================================*/
/*                                                                                               */
/*===============================================================================================*/

rt_buffer rtBufferCreate(void) {
	return rtvk_buffer_to_handle(rtvk_buffer_create(rtvk_get_current_context()));
}

void rtBufferDestroy(rt_buffer buffer) {
	rtvk_buffer_destroy(rtvk_get_current_context(), rtvk_buffer_from_handle(buffer));
}

rt_timepoint rtBufferData(rt_buffer buffer, enum rt_buffer_mode mode, enum rt_buffer_usage usage, u64 size, const void* data) {
	struct rtvk_timepoint timepoint = rtvk_buffer_data(
		rtvk_get_current_context(),
		rtvk_buffer_from_handle(buffer),
		mode,
		usage,
		size,
		data);
	return rtvk_timepoint_to_public(timepoint);
}

rt_timepoint rtBufferSubdata(rt_buffer buffer, u64 offset, u64 size, const void* data) {
	struct rtvk_timepoint timepoint = rtvk_buffer_subdata(
		rtvk_get_current_context(),
		rtvk_buffer_from_handle(buffer),
		offset,
		size,
		data);
	return rtvk_timepoint_to_public(timepoint);
}

void rtBufferRead(rt_buffer buffer, u64 offset, u64 size, void* data) {
	rtvk_buffer_read(
		rtvk_get_current_context(),
		rtvk_buffer_from_handle(buffer),
		offset,
		size,
		data);
}

void* rtBufferMap(rt_buffer buffer, u64 offset, u64 size) {
	return rtvk_buffer_map(rtvk_get_current_context(), rtvk_buffer_from_handle(buffer), offset, size);
}

void rtBufferUnmap(rt_buffer buffer) {
	rtvk_buffer_unmap(rtvk_get_current_context(), rtvk_buffer_from_handle(buffer));
}

/*===============================================================================================*/
/*                                                                                               */
/*===============================================================================================*/

enum rt_buffer_usage rtvk_default_buffer_usage(void) {
	return (enum rt_buffer_usage)(
		RT_BUFFER_USAGE_VERTEX |
		RT_BUFFER_USAGE_INDEX |
		RT_BUFFER_USAGE_UNIFORM |
		RT_BUFFER_USAGE_STORAGE |
		RT_BUFFER_USAGE_TRANSFER_SRC |
		RT_BUFFER_USAGE_TRANSFER_DST);
}

bool rtvk_buffer_is_staging(enum rt_buffer_usage usage) {
	return (usage & RT_BUFFER_USAGE_STAGING) != 0;
}

bool rtvk_buffer_uses_host_storage(enum rt_buffer_mode mode, enum rt_buffer_usage usage) {
	return mode == RT_BUFFER_DYNAMIC || rtvk_buffer_is_staging(usage);
}

bool rtvk_buffer_needs_graphics_upload_queue(enum rt_buffer_usage usage) {
	return (usage & (RT_BUFFER_USAGE_VERTEX |
					 RT_BUFFER_USAGE_INDEX |
					 RT_BUFFER_USAGE_UNIFORM |
					 RT_BUFFER_USAGE_STORAGE)) != 0;
}

struct rtvk_queue* rtvk_buffer_upload_queue(struct rtvk_context* ctx, enum rt_buffer_usage usage) {
	if (rtvk_buffer_needs_graphics_upload_queue(usage)) {
		return rtvk_queue_query(ctx, RT_QUEUE_GRAPHICS);
	}

	struct rtvk_queue* queue = rtvk_queue_query(ctx, RT_QUEUE_TRANSFER);
	if (queue) {
		return queue;
	}
	return rtvk_queue_query(ctx, RT_QUEUE_GRAPHICS);
}

VkBufferUsageFlags rtvk_buffer_vk_usage(enum rt_buffer_mode mode, enum rt_buffer_usage usage) {
	VkBufferUsageFlags flags = 0;

	if (rtvk_buffer_is_staging(usage)) {
		flags |= VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
	}
	if (usage & RT_BUFFER_USAGE_VERTEX) {
		flags |= VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
	}
	if (usage & RT_BUFFER_USAGE_INDEX) {
		flags |= VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
	}
	if (usage & RT_BUFFER_USAGE_UNIFORM) {
		flags |= VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
	}
	if (usage & RT_BUFFER_USAGE_STORAGE) {
		flags |= VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
	}
	if (usage & RT_BUFFER_USAGE_TRANSFER_SRC) {
		flags |= VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
	}
	if (usage & RT_BUFFER_USAGE_TRANSFER_DST) {
		flags |= VK_BUFFER_USAGE_TRANSFER_DST_BIT;
	}
	if (mode == RT_BUFFER_STATIC && !rtvk_buffer_is_staging(usage)) {
		flags |= VK_BUFFER_USAGE_TRANSFER_DST_BIT;
	}
	if (flags == 0) {
		flags = VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
	}
	return flags;
}
VkAccessFlags rtvk_buffer_vk_access(enum rt_buffer_usage usage) {
	VkAccessFlags flags = 0;
	if (usage & RT_BUFFER_USAGE_VERTEX) {
		flags |= VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT;
	}
	if (usage & RT_BUFFER_USAGE_INDEX) {
		flags |= VK_ACCESS_INDEX_READ_BIT;
	}
	if (usage & RT_BUFFER_USAGE_UNIFORM) {
		flags |= VK_ACCESS_UNIFORM_READ_BIT;
	}
	if (usage & RT_BUFFER_USAGE_STORAGE) {
		flags |= VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
	}
	if (usage & RT_BUFFER_USAGE_TRANSFER_SRC) {
		flags |= VK_ACCESS_TRANSFER_READ_BIT;
	}
	if (usage & RT_BUFFER_USAGE_TRANSFER_DST) {
		flags |= VK_ACCESS_TRANSFER_WRITE_BIT;
	}
	return flags;
}
VkPipelineStageFlags rtvk_buffer_vk_stage(enum rt_buffer_usage usage) {
	VkPipelineStageFlags flags = 0;
	if (usage & (RT_BUFFER_USAGE_VERTEX | RT_BUFFER_USAGE_INDEX)) {
		flags |= VK_PIPELINE_STAGE_VERTEX_INPUT_BIT;
	}
	if (usage & (RT_BUFFER_USAGE_UNIFORM | RT_BUFFER_USAGE_STORAGE)) {
		flags |= VK_PIPELINE_STAGE_VERTEX_SHADER_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
	}
	if (usage & RT_BUFFER_USAGE_STORAGE) {
		flags |= VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
	}
	if (usage & (RT_BUFFER_USAGE_TRANSFER_SRC | RT_BUFFER_USAGE_TRANSFER_DST)) {
		flags |= VK_PIPELINE_STAGE_TRANSFER_BIT;
	}
	return flags ? flags : VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
}
VkPipelineStageFlags rtvk_buffer_vk_stage_for_queue(enum rt_buffer_usage usage, struct rtvk_queue* queue) {
	VkPipelineStageFlags flags = rtvk_buffer_vk_stage(usage);
	if (!queue || queue->capability == RT_QUEUE_GRAPHICS) {
		return flags;
	}

	if (queue->capability == RT_QUEUE_COMPUTE) {
		VkPipelineStageFlags compute_flags = flags & (VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_TRANSFER_BIT);
		if (usage & (RT_BUFFER_USAGE_UNIFORM | RT_BUFFER_USAGE_STORAGE)) {
			compute_flags |= VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
		}
		return compute_flags ? compute_flags : VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
	}

	VkPipelineStageFlags transfer_flags = flags & VK_PIPELINE_STAGE_TRANSFER_BIT;
	return transfer_flags ? transfer_flags : VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
}

/*===============================================================================================*/
/*                                                                                               */
/*===============================================================================================*/

RTVK_DEFINE_RESOURCE_PRIVATE(buffer)



void rtvk_buffer_init(struct rtvk_context* ctx, struct rtvk_buffer* buffer) {
	rtvk_init_resource_base(ctx, RTVK_RESOURCE_BASE(buffer), RT_RESOURCE_BUFFER);
	buffer->mode = RT_BUFFER_DYNAMIC;
	buffer->usage = rtvk_default_buffer_usage();
}

void rtvk_buffer_finish(struct rtvk_context* ctx, struct rtvk_buffer* buffer) {
	rtvk_buffer_node_release(buffer->active);
	buffer->active = NULL;

	struct rtvk_buffer* node = buffer->next;
	while (node) {
		struct rtvk_buffer* next = node->next;
		node->next = NULL;
		rtvk_buffer_node_release(node);
		node = next;
	}
	buffer->next = NULL;
	rtvk_finish_resource_base(ctx, RTVK_RESOURCE_BASE(buffer));
}

struct rtvk_buffer* rtvk_buffer_node_create(struct rtvk_context* ctx, u64 size, enum rt_buffer_mode mode, enum rt_buffer_usage usage) {
	struct rtvk_buffer* node = calloc(1, sizeof(*node));
	if (!node) {
		rtvk_throwf(RT_OUT_OF_HOST_MEMORY, "failed to allocate buffer metadata");
		return NULL;
	}

	VkBufferCreateInfo buffer_info = { VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
	buffer_info.pNext = NULL;
	buffer_info.flags = 0;
	buffer_info.size = size;
	buffer_info.usage = rtvk_buffer_vk_usage(mode, usage);
	buffer_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	buffer_info.queueFamilyIndexCount = 0;
	buffer_info.pQueueFamilyIndices = NULL;

	VmaAllocationCreateInfo allocation_info = { 0 };
	if (rtvk_buffer_uses_host_storage(mode, usage)) {
		allocation_info.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT |
								VMA_ALLOCATION_CREATE_MAPPED_BIT;
		allocation_info.usage = VMA_MEMORY_USAGE_AUTO_PREFER_HOST;
	} else {
		allocation_info.flags = 0;
		allocation_info.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;
	}

	VkResult result = vmaCreateBuffer(ctx->vma_allocator, &buffer_info, &allocation_info, &node->vk_buffer, &node->vma_allocation, NULL);
	if (result != VK_SUCCESS) {
		free(node);
		rtvk_throwf(rtvk_error_from_vk(result), NULL);
		return NULL;
	}

	rtvk_init_resource_base(ctx, RTVK_RESOURCE_BASE(node), RT_RESOURCE_BUFFER);
	node->size = size;
	node->mode = mode;
	node->usage = usage;

	return node;
}

void rtvk_buffer_node_retain(struct rtvk_buffer* buffer) {
	assert(buffer);
	buffer->base.ref_count++;
}

void rtvk_buffer_node_release(struct rtvk_buffer* buffer) {
	assert(buffer);
	assert(buffer->base.ref_count > 0);
	if (--buffer->base.ref_count != 0) {
		return;
	}

	if (buffer->vk_buffer) {
		vmaDestroyBuffer(buffer->base.ctx->vma_allocator, buffer->vk_buffer, buffer->vma_allocation);
	}
	free(buffer);
}

void rtvk_buffer_write_host(struct rtvk_context* ctx, struct rtvk_buffer* buffer, u64 offset, u64 size, const void* data) {
	if (!data || !size) {
		return;
	}
	assert(buffer);

	VmaAllocationInfo allocation_info;
	vmaGetAllocationInfo(ctx->vma_allocator, buffer->vma_allocation, &allocation_info);
	if (!allocation_info.pMappedData) {
		rtvk_throwf(RT_PLATFORM_FAILURE, "buffer allocation is not mapped");
		return;
	}
	void* dst = (char*)allocation_info.pMappedData + offset;
	if (dst != data) {
		memcpy(dst, data, (usize)size);
	}
	vmaFlushAllocation(ctx->vma_allocator, buffer->vma_allocation, offset, size);
}

void rtvk_buffer_upload_command(struct rtvk_context* ctx, struct rtvk_queue* queue) {
	assert(queue);
	if (queue->upload_command_pool && queue->upload_command_buffer) {
		rtvk_queue_collect_to_value(ctx, queue, rtvk_queue_completed_value(ctx, queue));
		if (queue->upload_command_timepoint.queue && queue->upload_command_timepoint.value > queue->completed_value) {
			rtvk_queue_retire_upload_resources(ctx, queue, true, false);
		}
	}
	if (queue->upload_command_pool && queue->upload_command_buffer) {
		queue->upload_command_timepoint = (struct rtvk_timepoint){ NULL, 0 };
		vkResetCommandPool(ctx->vk_device, queue->upload_command_pool, 0);
		return;
	}

	VkCommandPoolCreateInfo pool_info = { VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO };
	pool_info.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT | VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
	pool_info.queueFamilyIndex = queue->family_index;
	VkResult result = vkCreateCommandPool(ctx->vk_device, &pool_info, VK_ALLOCATOR, &queue->upload_command_pool);
	if (result != VK_SUCCESS) {
		rtvk_throwf(rtvk_error_from_vk(result), NULL);
		return;
	}

	VkCommandBufferAllocateInfo alloc_info = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO };
	alloc_info.commandPool = queue->upload_command_pool;
	alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	alloc_info.commandBufferCount = 1;
	result = vkAllocateCommandBuffers(ctx->vk_device, &alloc_info, &queue->upload_command_buffer);
	if (result != VK_SUCCESS) {
		vkDestroyCommandPool(ctx->vk_device, queue->upload_command_pool, VK_ALLOCATOR);
		queue->upload_command_pool = VK_NULL_HANDLE;
		rtvk_throwf(rtvk_error_from_vk(result), NULL);
		return;
	}
}

void rtvk_buffer_upload_staging(struct rtvk_context* ctx, struct rtvk_queue* queue, u64 size) {
	assert(queue);
	if (queue->upload_staging_buffer && queue->upload_staging_size >= size) {
		rtvk_queue_collect_to_value(ctx, queue, rtvk_queue_completed_value(ctx, queue));
		if (queue->upload_command_timepoint.queue && queue->upload_command_timepoint.value > queue->completed_value) {
			rtvk_queue_retire_upload_resources(ctx, queue, false, true);
		}
	}
	if (queue->upload_staging_buffer && queue->upload_staging_size >= size) {
		return;
	}

	rtvk_queue_collect_to_value(ctx, queue, rtvk_queue_completed_value(ctx, queue));
	if (queue->upload_staging_buffer) {
		if (queue->upload_command_timepoint.queue && queue->upload_command_timepoint.value > queue->completed_value) {
			rtvk_queue_retire_upload_resources(ctx, queue, false, true);
		} else {
			vmaDestroyBuffer(ctx->vma_allocator, queue->upload_staging_buffer, queue->upload_staging_allocation);
			queue->upload_staging_buffer = VK_NULL_HANDLE;
			queue->upload_staging_allocation = NULL;
			queue->upload_staging_size = 0;
		}
	}

	VkBufferCreateInfo buffer_info = { VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
	buffer_info.size = size;
	buffer_info.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
	buffer_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

	VmaAllocationCreateInfo alloc_info = { 0 };
	alloc_info.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT;
	alloc_info.usage = VMA_MEMORY_USAGE_AUTO_PREFER_HOST;

	VkResult result = vmaCreateBuffer(
		ctx->vma_allocator,
		&buffer_info,
		&alloc_info,
		&queue->upload_staging_buffer,
		&queue->upload_staging_allocation,
		NULL);
	if (result != VK_SUCCESS) {
		rtvk_throwf(rtvk_error_from_vk(result), NULL);
		return;
	}
	queue->upload_staging_size = size;
}

struct rtvk_timepoint rtvk_buffer_upload_static(struct rtvk_context* ctx, struct rtvk_queue* queue, struct rtvk_buffer* buffer, u64 offset, u64 size, const void* data) {
	struct rtvk_timepoint timepoint = { queue, 0 };
	if (!size) {
		return timepoint;
	}
	if (!queue) {
		rtvk_throwf(RT_IMPROPER_USAGE, "static buffer uploads require a valid queue");
		return timepoint;
	}

	rtvk_queue_collect_to_value(ctx, queue, rtvk_queue_completed_value(ctx, queue));

	rtvk_buffer_upload_staging(ctx, queue, size);
	if (rtvk_error() != RT_SUCCESS) {
		return timepoint;
	}

	VmaAllocationInfo staging_alloc_info;
	vmaGetAllocationInfo(ctx->vma_allocator, queue->upload_staging_allocation, &staging_alloc_info);
	memcpy(staging_alloc_info.pMappedData, data, (usize)size);
	vmaFlushAllocation(ctx->vma_allocator, queue->upload_staging_allocation, 0, size);

	rtvk_buffer_upload_command(ctx, queue);
	if (rtvk_error() != RT_SUCCESS) {
		return timepoint;
	}
	VkCommandBuffer command_buffer = queue->upload_command_buffer;

	VkResult result;
	VkCommandBufferBeginInfo begin_info = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
	begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
	result = vkBeginCommandBuffer(command_buffer, &begin_info);
	if (result != VK_SUCCESS) {
		rtvk_throwf(rtvk_error_from_vk(result), NULL);
		return timepoint;
	}

	VkBufferCopy copy_region = { 0 };
	copy_region.srcOffset = 0;
	copy_region.dstOffset = offset;
	copy_region.size = size;
	vkCmdCopyBuffer(command_buffer, queue->upload_staging_buffer, buffer->vk_buffer, 1, &copy_region);

	VkAccessFlags dst_access = rtvk_buffer_vk_access(buffer->usage);
	if (dst_access) {
		VkPipelineStageFlags src_stage = VK_PIPELINE_STAGE_TRANSFER_BIT;
		VkPipelineStageFlags dst_stage = rtvk_buffer_vk_stage_for_queue(buffer->usage, queue);
		VkBufferMemoryBarrier barrier = { VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER };
		barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		barrier.dstAccessMask = dst_access;
		barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		barrier.buffer = buffer->vk_buffer;
		barrier.offset = 0;
		barrier.size = buffer->size;
		vkCmdPipelineBarrier(command_buffer, src_stage, dst_stage, 0, 0, NULL, 1, &barrier, 0, NULL);
	}

	result = vkEndCommandBuffer(command_buffer);
	if (result != VK_SUCCESS) {
		rtvk_throwf(rtvk_error_from_vk(result), NULL);
		return timepoint;
	}

	u64 signal_value = queue->timeline_value + 1;
	VkTimelineSemaphoreSubmitInfo timeline_info = { VK_STRUCTURE_TYPE_TIMELINE_SEMAPHORE_SUBMIT_INFO };
	timeline_info.signalSemaphoreValueCount = 1;
	timeline_info.pSignalSemaphoreValues = &signal_value;

	VkSubmitInfo submit_info = { VK_STRUCTURE_TYPE_SUBMIT_INFO };
	submit_info.pNext = &timeline_info;
	submit_info.commandBufferCount = 1;
	submit_info.pCommandBuffers = &command_buffer;
	submit_info.signalSemaphoreCount = 1;
	submit_info.pSignalSemaphores = &queue->vk_timeline;

	result = vkQueueSubmit(queue->vk_queue, 1, &submit_info, VK_NULL_HANDLE);
	if (result != VK_SUCCESS) {
		rtvk_throwf(rtvk_error_from_vk(result), NULL);
		return timepoint;
	}

	queue->timeline_value = signal_value;
	queue->submitted_value = signal_value;
	timepoint.value = signal_value;
	queue->upload_command_timepoint = timepoint;
	return timepoint;
}

void rtvk_buffer_recycle_node(struct rtvk_buffer* buffer, struct rtvk_buffer* node) {
	if (!node) {
		return;
	}
	node->next = buffer->next;
	buffer->next = node;
}

struct rtvk_buffer* rtvk_buffer_take_reusable_node(struct rtvk_buffer* buffer, u64 size, enum rt_buffer_mode mode, enum rt_buffer_usage usage) {
	struct rtvk_buffer** link = &buffer->next;

	while (*link) {
		struct rtvk_buffer* node = *link;
		if (node->size == size &&
			node->mode == mode &&
			node->usage == usage &&
			node->base.ref_count == 1) {
			*link = node->next;
			node->next = NULL;
			return node;
		}
		link = &node->next;
	}
	return NULL;
}

struct rtvk_timepoint rtvk_buffer_data(struct rtvk_context* ctx, struct rtvk_buffer* buffer, enum rt_buffer_mode mode, enum rt_buffer_usage usage, u64 size, const void* data) {
	struct rtvk_timepoint timepoint = { NULL, 0 };
	if (!buffer) {
		rtvk_throwf(RT_IMPROPER_USAGE, "buffer is NULL");
		return timepoint;
	}
	if (mode != RT_BUFFER_STATIC && mode != RT_BUFFER_DYNAMIC) {
		rtvk_throwf(RT_IMPROPER_USAGE, "unsupported buffer mode");
		return timepoint;
	}
	if (usage == RT_BUFFER_USAGE_NONE) {
		rtvk_throwf(RT_IMPROPER_USAGE, "buffer usage must not be empty");
		return timepoint;
	}

	struct rtvk_queue* queue = NULL;
	if (!rtvk_buffer_uses_host_storage(mode, usage) && size) {
		queue = rtvk_buffer_upload_queue(ctx, usage);
		if (!queue) {
			rtvk_throwf(RT_IMPROPER_USAGE, "no queue is available for static buffer uploads");
			return timepoint;
		}
	}

	if (queue) {
		rtvk_queue_collect_to_value(ctx, queue, rtvk_queue_completed_value(ctx, queue));
	}
	buffer->mode = mode;
	buffer->usage = usage;

	if (buffer->active) {
		rtvk_buffer_recycle_node(buffer, buffer->active);
	}
	buffer->active = NULL;

	struct rtvk_buffer* node = rtvk_buffer_take_reusable_node(buffer, size, buffer->mode, buffer->usage);
	if (!node) {
		node = rtvk_buffer_node_create(ctx, size, buffer->mode, buffer->usage);
	}
	if (!node) {
		return timepoint;
	}

	buffer->active = node;

	if (rtvk_buffer_uses_host_storage(node->mode, node->usage)) {
		if (data && size) {
			rtvk_buffer_write_host(ctx, node, 0, size, data);
			if (rtvk_error() != RT_SUCCESS) {
				return timepoint;
			}
		}
		return timepoint;
	}

	if (size && data) {
		timepoint = rtvk_buffer_upload_static(ctx, queue, node, 0, size, data);
	}
	return timepoint;
}

struct rtvk_timepoint rtvk_buffer_subdata(struct rtvk_context* ctx, struct rtvk_buffer* buffer, u64 offset, u64 size, const void* data) {
	struct rtvk_timepoint timepoint = { NULL, 0 };
	if (!buffer || !buffer->active) {
		rtvk_throwf(RT_IMPROPER_USAGE, "buffer has no storage");
		return timepoint;
	}
	if (offset > buffer->active->size || size > buffer->active->size - offset) {
		rtvk_throwf(RT_IMPROPER_USAGE, "buffer subdata range is out of bounds");
		return timepoint;
	}
	struct rtvk_queue* queue = NULL;
	if (!rtvk_buffer_uses_host_storage(buffer->active->mode, buffer->active->usage) && size && data) {
		queue = rtvk_buffer_upload_queue(ctx, buffer->active->usage);
		if (!queue) {
			rtvk_throwf(RT_IMPROPER_USAGE, "no queue is available for static buffer uploads");
			return timepoint;
		}
	}
	if (queue) {
		rtvk_queue_collect_to_value(ctx, queue, rtvk_queue_completed_value(ctx, queue));
	}
	if (!size || !data) {
		return timepoint;
	}

	bool replaced_storage = false;
	if (buffer->active->base.ref_count > 1) {
		struct rtvk_buffer* old_node = buffer->active;

		rtvk_buffer_recycle_node(buffer, old_node);
		buffer->active = NULL;

		struct rtvk_buffer* new_node = rtvk_buffer_take_reusable_node(buffer, old_node->size, old_node->mode, old_node->usage);
		if (!new_node) {
			new_node = rtvk_buffer_node_create(ctx, old_node->size, old_node->mode, old_node->usage);
		}
		if (!new_node) {
			return timepoint;
		}
		buffer->active = new_node;
		replaced_storage = true;
	}

	if (rtvk_buffer_uses_host_storage(buffer->active->mode, buffer->active->usage)) {
		rtvk_buffer_write_host(ctx, buffer->active, offset, size, data);
		if (rtvk_error() != RT_SUCCESS) {
			return timepoint;
		}
		return timepoint;
	}

	if (replaced_storage) {
		return rtvk_buffer_upload_static(ctx, queue, buffer->active, 0, buffer->active->size, data);
	}
	return rtvk_buffer_upload_static(ctx, queue, buffer->active, offset, size, data);
}

void rtvk_buffer_read(struct rtvk_context* ctx, struct rtvk_buffer* buffer, u64 offset, u64 size, void* data) {
	if (!buffer || !buffer->active) {
		rtvk_throwf(RT_IMPROPER_USAGE, "buffer has no storage");
		return;
	}
	if (!data && size) {
		rtvk_throwf(RT_IMPROPER_USAGE, "buffer read destination is NULL");
		return;
	}
	if (offset > buffer->active->size || size > buffer->active->size - offset) {
		rtvk_throwf(RT_IMPROPER_USAGE, "buffer read range is out of bounds");
		return;
	}

	if (!rtvk_buffer_uses_host_storage(buffer->active->mode, buffer->active->usage)) {
		rtvk_throwf(RT_IMPROPER_USAGE, "buffer has no CPU-readable storage");
		return;
	}

	VmaAllocationInfo allocation_info;
	vmaGetAllocationInfo(ctx->vma_allocator, buffer->active->vma_allocation, &allocation_info);
	vmaInvalidateAllocation(ctx->vma_allocator, buffer->active->vma_allocation, offset, size);
	void* src = (char*)allocation_info.pMappedData + offset;
	if (src != data) {
		memcpy(data, src, (usize)size);
	}
}

void* rtvk_buffer_map(struct rtvk_context* ctx, struct rtvk_buffer* buffer, u64 offset, u64 size) {
	if (!buffer || !buffer->active) {
		rtvk_throwf(RT_IMPROPER_USAGE, "buffer has no storage");
		return NULL;
	}
	if (offset > buffer->active->size || size > buffer->active->size - offset) {
		rtvk_throwf(RT_IMPROPER_USAGE, "buffer map range is out of bounds");
		return NULL;
	}
	if (!rtvk_buffer_uses_host_storage(buffer->active->mode, buffer->active->usage)) {
		rtvk_throwf(RT_IMPROPER_USAGE, "buffer has no CPU-mappable storage");
		return NULL;
	}

	VmaAllocationInfo allocation_info;
	vmaGetAllocationInfo(ctx->vma_allocator, buffer->active->vma_allocation, &allocation_info);
	vmaInvalidateAllocation(ctx->vma_allocator, buffer->active->vma_allocation, offset, size);
	return (char*)allocation_info.pMappedData + offset;
}

void rtvk_buffer_unmap(struct rtvk_context* ctx, struct rtvk_buffer* buffer) {
}
