#include "queue.h"
#include "context.h"
#include "error.h"
#include "resource/swapchain.h"
#include <assert.h>
#include <intrin.h>

#include <stdlib.h>
#include <string.h>

/*===============================================================================================*/
/*                                                                                               */
/*===============================================================================================*/

RTVK_DEFINE_HANDLE(queue, rtvk_virtual_queue)

rt_queue rtQueueCreate(enum rt_queue_capability capability) {
	rtvk_begin_errorable_operation();
	return rtvk_queue_to_handle(rtvk_virtual_queue_create(rtvk_get_current_context(), capability));
}

void rtQueueDestroy(rt_queue queue) {
	rtvk_virtual_queue_destroy(rtvk_queue_from_handle(queue));
}

rt_timepoint rtQueueSubmit(rt_queue queue, rt_command_buffer command_buffer) {
	rtvk_begin_errorable_operation();
	return rtvk_virtual_queue_submit(
		rtvk_get_current_context(),
		rtvk_queue_from_handle(queue),
		rtvk_command_buffer_from_handle(command_buffer)
	);
}

rt_timepoint rtQueueFlush(rt_queue queue) {
	rtvk_begin_errorable_operation();
	return rtvk_virtual_queue_flush(rtvk_get_current_context(), rtvk_queue_from_handle(queue));
}

void rtQueueWait(rt_queue queue, rt_timepoint timepoint) {
	rtvk_begin_errorable_operation();
	rtvk_virtual_queue_wait(rtvk_get_current_context(), rtvk_queue_from_handle(queue), timepoint);
}

void rtTimepointWait(rt_timepoint timepoint) {
	rtvk_begin_errorable_operation();
	rtvk_timepoint_wait_public(rtvk_get_current_context(), timepoint);
}

bool rtTimepointReached(rt_timepoint timepoint) {
	return rtvk_timepoint_reached_public(rtvk_get_current_context(), timepoint);
}

/*===============================================================================================*/
/*                                                                                                */
/*===============================================================================================*/

rt_timepoint rtvk_virtual_queue_submit(struct rtvk_context* ctx, struct rtvk_virtual_queue* virtual_queue, struct rtvk_command_buffer* command_buffer) {
	rtvk_mutex_lock(&virtual_queue->lock);
	rt_timepoint result = rtvk_queue_submit(
		ctx,
		virtual_queue->native_queue,
		command_buffer,
		&virtual_queue->wait_timepoints,
		&virtual_queue->wait_count,
		&virtual_queue->wait_capacity
	);
	rtvk_mutex_unlock(&virtual_queue->lock);
	return result;
}

rt_timepoint rtvk_virtual_queue_flush(struct rtvk_context* ctx, struct rtvk_virtual_queue* virtual_queue) {
	if (!virtual_queue) {
		return (rt_timepoint){ 0 };
	}
	rtvk_mutex_lock(&virtual_queue->lock);
	rtvk_mutex_lock(&virtual_queue->native_queue->lock);
	rt_timepoint result = rtvk_queue_flush_locked(ctx, virtual_queue->native_queue);
	rtvk_mutex_unlock(&virtual_queue->native_queue->lock);
	rtvk_mutex_unlock(&virtual_queue->lock);
	return result;
}

void rtvk_virtual_queue_wait(struct rtvk_context* ctx, struct rtvk_virtual_queue* virtual_queue, rt_timepoint timepoint) {
	(void)ctx;
	if (!virtual_queue) {
		return;
	}
	rtvk_mutex_lock(&virtual_queue->lock);
	if (virtual_queue->wait_count == virtual_queue->wait_capacity) {
		usize required_count = virtual_queue->wait_count + 1;
		usize capacity = required_count;
		if (capacity < 8) {
			capacity = 8;
		} else {
			unsigned long most_significant_bit = 0;
			_BitScanReverse64(&most_significant_bit, capacity - 1);
			capacity = (usize)1 << (most_significant_bit + 1);
		}
		rt_timepoint* wait_timepoints = realloc(virtual_queue->wait_timepoints, capacity * sizeof(*wait_timepoints));
		if (!wait_timepoints) {
			rtvk_throwf(RT_OUT_OF_HOST_MEMORY, "failed to allocate %zu bytes for virtual queue wait timepoints", capacity * sizeof(*wait_timepoints));
			rtvk_mutex_unlock(&virtual_queue->lock);
			return;
		}
		virtual_queue->wait_timepoints = wait_timepoints;
		virtual_queue->wait_capacity = capacity;
	}
	virtual_queue->wait_timepoints[virtual_queue->wait_count++] = timepoint;
	rtvk_mutex_unlock(&virtual_queue->lock);
}

void rtvk_timepoint_wait_public(struct rtvk_context* ctx, rt_timepoint timepoint) {
	rtvk_timepoint_wait(ctx, timepoint);
}

bool rtvk_timepoint_reached_public(struct rtvk_context* ctx, rt_timepoint timepoint) {
	return rtvk_timepoint_reached(ctx, timepoint);
}

/*===============================================================================================*/
/*                                                                                               */
/*===============================================================================================*/

RTVK_DEFINE_RESOURCE_FINALIZER(queue)

bool rtvk_timepoint_complete(rt_timepoint timepoint);

struct rtvk_queue* rtvk_queue_create(struct rtvk_context* ctx, VkQueue vk_queue, VkQueueFlags capabilities, enum rt_queue_capability capability, u32 family_index, u32 queue_index) {
	struct rtvk_queue* queue = RTVK_ALLOC_RESOURCE(struct rtvk_queue);
	if (!queue) {
		rtvk_throwf(RT_OUT_OF_HOST_MEMORY, "failed to allocate %zu bytes for queue metadata", sizeof(*queue));
		return NULL;
	}

	rtvk_queue_init(ctx, queue, vk_queue, capabilities, capability, family_index, queue_index);
	if (rtvk_error() != RT_SUCCESS) {
		rtvk_queue_finish(queue);
		free(queue);
		return NULL;
	}

	return queue;
}
void rtvk_queue_destroy(struct rtvk_context* ctx, struct rtvk_queue* queue) {
	(void)ctx;
	assert(queue);
	rtvk_resource_retire(RTVK_RESOURCE_BASE(queue));
}

void rtvk_queue_init(struct rtvk_context* ctx, struct rtvk_queue* queue, VkQueue vk_queue, VkQueueFlags capabilities, enum rt_queue_capability capability, u32 family_index, u32 queue_index) {
	memset(queue, 0, sizeof(*queue));
	rtvk_init_resource_base(ctx, RTVK_RESOURCE_BASE(queue), queue, rtvk_queue_finalize_resource);
	queue->vk_queue = vk_queue;
	queue->capabilities = capabilities;
	queue->capability = capability;
	queue->family_index = family_index;
	queue->queue_index = queue_index;
	queue->timepoint_id = (u08)(ctx->queue_count + 1);
	if (!rtvk_mutex_init(&queue->lock)) {
		rtvk_throwf(RT_PLATFORM_FAILURE, "failed to initialize Vulkan queue synchronization");
		return;
	}

	VkSemaphoreTypeCreateInfo timeline_info = { VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO };
	timeline_info.pNext = NULL;
	timeline_info.semaphoreType = VK_SEMAPHORE_TYPE_TIMELINE;
	timeline_info.initialValue = 0;

	VkSemaphoreCreateInfo semaphore_info = { VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO };
	semaphore_info.pNext = &timeline_info;
	semaphore_info.flags = 0;

	VkResult result = vkCreateSemaphore(ctx->vk_device, &semaphore_info, VK_ALLOCATOR, &queue->vk_timeline);
	if (result != VK_SUCCESS) {
		rtvk_throwf(rtvk_error_from_vk(result), "Vulkan call returned %s", rtvk_vk_result_name(result));
		return;
	}
}

struct rtvk_virtual_queue* rtvk_virtual_queue_create(struct rtvk_context* ctx, enum rt_queue_capability capability) {
	struct rtvk_queue* native_queue = rtvk_queue_query(ctx, capability);
	if (!native_queue) {
		rtvk_throwf(RT_UNSUPPORTED_FEATURE, "no Vulkan queue supports the requested capability");
		return NULL;
	}
	struct rtvk_virtual_queue* queue = calloc(1, sizeof(*queue));
	if (!queue) {
		rtvk_throwf(RT_OUT_OF_HOST_MEMORY, "failed to allocate %zu bytes for virtual queue", sizeof(*queue));
		return NULL;
	}
	if (!rtvk_mutex_init(&queue->lock)) {
		free(queue);
		rtvk_throwf(RT_PLATFORM_FAILURE, "failed to initialize Vulkan virtual queue synchronization");
		return NULL;
	}
	queue->native_queue = native_queue;
	return queue;
}

void rtvk_virtual_queue_destroy(struct rtvk_virtual_queue* queue) {
	if (!queue) {
		return;
	}
	rtvk_mutex_finish(&queue->lock);
	free(queue->wait_timepoints);
	free(queue);
}
void rtvk_queue_finish(struct rtvk_queue* queue) {
	assert(queue);
	struct rtvk_context* ctx = queue->base.ctx;
	rtvk_queue_flush(ctx, queue);
	if (queue->timeline_value) {
		rt_timepoint last = rtvk_timepoint_make(queue, queue->timeline_value);
		rtvk_timepoint_wait(ctx, last);
	}
	rtvk_queue_retire_upload_resources(ctx, queue, true, true);
	rtvk_queue_collect_to_value(ctx, queue, queue->timeline_value);

	vkDestroySemaphore(ctx->vk_device, queue->vk_timeline, VK_ALLOCATOR);
	queue->vk_timeline = VK_NULL_HANDLE;
	rtvk_mutex_finish(&queue->lock);

	vkDestroyCommandPool(ctx->vk_device, queue->copy_command_pool, VK_ALLOCATOR);
	queue->copy_command_pool = VK_NULL_HANDLE;
	queue->copy_command_buffer = VK_NULL_HANDLE;

	vkDestroyCommandPool(ctx->vk_device, queue->upload_command_pool, VK_ALLOCATOR);
	queue->upload_command_pool = VK_NULL_HANDLE;
	queue->upload_command_buffer = VK_NULL_HANDLE;

	vmaDestroyBuffer(ctx->vma_allocator, queue->upload_staging_buffer, queue->upload_staging_allocation);
	queue->upload_staging_buffer = VK_NULL_HANDLE;
	queue->upload_staging_allocation = NULL;
	queue->upload_staging_size = 0;
}

struct rtvk_queue* rtvk_queue_query(struct rtvk_context* ctx, enum rt_queue_capability capability) {
	switch (capability) {
	case RT_QUEUE_TRANSFER:
		for (u32 i = 0; i < ctx->queue_count; i++) {
			VkQueueFlags capabilities = ctx->queues[i]->capabilities;
			if ((capabilities & VK_QUEUE_TRANSFER_BIT) && !(capabilities & (VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT))) {
				return ctx->queues[i];
			}
		}
		for (u32 i = 0; i < ctx->queue_count; i++) {
			VkQueueFlags capabilities = ctx->queues[i]->capabilities;
			if ((capabilities & VK_QUEUE_COMPUTE_BIT) && !(capabilities & VK_QUEUE_GRAPHICS_BIT)) {
				return ctx->queues[i];
			}
		}
		for (u32 i = 0; i < ctx->queue_count; i++) {
			if (ctx->queues[i]->capabilities & VK_QUEUE_GRAPHICS_BIT) {
				return ctx->queues[i];
			}
		}
		break;
	case RT_QUEUE_COMPUTE:
		for (u32 i = 0; i < ctx->queue_count; i++) {
			VkQueueFlags capabilities = ctx->queues[i]->capabilities;
			if ((capabilities & VK_QUEUE_COMPUTE_BIT) && !(capabilities & VK_QUEUE_GRAPHICS_BIT)) {
				return ctx->queues[i];
			}
		}
		for (u32 i = 0; i < ctx->queue_count; i++) {
			if (ctx->queues[i]->capabilities & VK_QUEUE_GRAPHICS_BIT) {
				return ctx->queues[i];
			}
		}
		break;
	case RT_QUEUE_GRAPHICS:
		for (u32 i = 0; i < ctx->queue_count; i++) {
			if (ctx->queues[i]->capabilities & VK_QUEUE_GRAPHICS_BIT) {
				return ctx->queues[i];
			}
		}
		break;
	default:
		return NULL;
	}
	return NULL;
}

VkPipelineStageFlags rtvk_queue_wait_stage(struct rtvk_queue* queue) {
	assert(queue);
	return VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
}

struct rtvk_lowered_command_buffer* rtvk_queue_create_lowered_command_buffer(struct rtvk_context* ctx, struct rtvk_queue* queue) {
	struct rtvk_lowered_command_buffer* lowered = calloc(1, sizeof(*lowered));
	if (!lowered) {
		rtvk_throwf(RT_OUT_OF_HOST_MEMORY, "failed to allocate %zu bytes for lowered command buffer", sizeof(*lowered));
		return NULL;
	}

	VkCommandPoolCreateInfo pool_info = { VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO };
	pool_info.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;
	pool_info.queueFamilyIndex = queue->family_index;
	VkResult result = vkCreateCommandPool(ctx->vk_device, &pool_info, VK_ALLOCATOR, &lowered->vk_command_pool);
	if (result != VK_SUCCESS) {
		free(lowered);
		rtvk_throwf(rtvk_error_from_vk(result), "Vulkan call returned %s", rtvk_vk_result_name(result));
		return NULL;
	}

	VkCommandBufferAllocateInfo allocate_info = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO };
	allocate_info.commandPool = lowered->vk_command_pool;
	allocate_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	allocate_info.commandBufferCount = 1;
	result = vkAllocateCommandBuffers(ctx->vk_device, &allocate_info, &lowered->vk_command_buffer);
	if (result != VK_SUCCESS) {
		rtvk_throwf(rtvk_error_from_vk(result), "Vulkan call returned %s", rtvk_vk_result_name(result));
		rtvk_lowered_command_buffer_destroy(ctx, lowered);
		return NULL;
	}

	VkCommandBufferBeginInfo begin_info = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
	begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
	result = vkBeginCommandBuffer(lowered->vk_command_buffer, &begin_info);
	if (result != VK_SUCCESS) {
		rtvk_throwf(rtvk_error_from_vk(result), "Vulkan call returned %s", rtvk_vk_result_name(result));
		rtvk_lowered_command_buffer_destroy(ctx, lowered);
		return NULL;
	}

	return lowered;
}

u64 rtvk_queue_completed_value(struct rtvk_context* ctx, struct rtvk_queue* queue) {
	assert(queue);
	if (!queue->vk_timeline) {
		return 0;
	}

	u64 value = 0;
	VkResult result = vkGetSemaphoreCounterValue(ctx->vk_device, queue->vk_timeline, &value);
	if (result != VK_SUCCESS) {
		rtvk_throwf(rtvk_error_from_vk(result), "Vulkan call returned %s", rtvk_vk_result_name(result));
		return 0;
	}
	if (value > queue->completed_value) {
		queue->completed_value = value;
	}
	return value;
}
struct rtvk_submitted_batch* rtvk_queue_create_batch(struct rtvk_lowered_command_buffer* lowered, rt_timepoint* wait_timepoints, usize wait_count, u64 first_value, u64 value) {
	struct rtvk_submitted_batch* batch = calloc(1, sizeof(*batch));
	if (!batch) {
		rtvk_throwf(RT_OUT_OF_HOST_MEMORY, "failed to allocate %zu bytes for submitted batch metadata", sizeof(*batch));
		return NULL;
	}

	batch->lowered_command_buffer = lowered;
	batch->wait_timepoints = wait_timepoints;
	batch->wait_count = wait_count;
	batch->first_value = first_value;
	batch->value = value;
	return batch;
}
void rtvk_queue_push_batch(struct rtvk_queue* queue, struct rtvk_submitted_batch* batch) {
	assert(queue);
	if (!batch) {
		return;
	}
	if (queue->submitted_tail) {
		queue->submitted_tail->next = batch;
	} else {
		queue->submitted_head = batch;
	}
	queue->submitted_tail = batch;
}

void rtvk_queue_push_pending_batch(struct rtvk_queue* queue, struct rtvk_submitted_batch* batch) {
	assert(queue);
	if (!batch) {
		return;
	}
	if (queue->pending_tail) {
		queue->pending_tail->next = batch;
	} else {
		queue->pending_head = batch;
	}
	queue->pending_tail = batch;
}

void rtvk_queue_push_submitted_list(struct rtvk_queue* queue, struct rtvk_submitted_batch* head, struct rtvk_submitted_batch* tail) {
	assert(queue);
	if (!head) {
		return;
	}
	if (queue->submitted_tail) {
		queue->submitted_tail->next = head;
	} else {
		queue->submitted_head = head;
	}
	queue->submitted_tail = tail;
}

void rtvk_queue_destroy_retired_upload(struct rtvk_context* ctx, struct rtvk_retired_upload_resource* retired) {
	if (!retired) {
		return;
	}
	vkDestroyCommandPool(ctx->vk_device, retired->command_pool, VK_ALLOCATOR);
	vmaDestroyBuffer(ctx->vma_allocator, retired->staging_buffer, retired->staging_allocation);
	free(retired);
}

void rtvk_queue_collect_to_value(struct rtvk_context* ctx, struct rtvk_queue* queue, u64 completed_value) {
	assert(queue);

	while (queue->submitted_head && queue->submitted_head->value <= completed_value) {
		struct rtvk_submitted_batch* batch = queue->submitted_head;
		queue->submitted_head = batch->next;
		if (!queue->submitted_head) {
			queue->submitted_tail = NULL;
		}

		rtvk_lowered_command_buffer_destroy(ctx, batch->lowered_command_buffer);
		free(batch->binary_signals);
		free(batch->binary_waits);
		free(batch->wait_timepoints);
		free(batch);
	}

	struct rtvk_retired_upload_resource** retired_link = &queue->retired_uploads;
	while (*retired_link) {
		struct rtvk_retired_upload_resource* retired = *retired_link;
		if (rtvk_timepoint_queue(ctx, retired->timepoint) == queue && rtvk_timepoint_value(retired->timepoint) > completed_value) {
			retired_link = &retired->next;
			continue;
		}
		*retired_link = retired->next;
		rtvk_queue_destroy_retired_upload(ctx, retired);
	}
}

void rtvk_queue_retire_upload_resources(struct rtvk_context* ctx, struct rtvk_queue* queue, bool command, bool staging) {
	assert(queue);
	if ((!command || !queue->upload_command_pool) && (!staging || !queue->upload_staging_buffer)) {
		return;
	}

	rt_timepoint timepoint = queue->upload_command_timepoint;
	if (rtvk_timepoint_complete(timepoint) || rtvk_timepoint_value(timepoint) <= queue->completed_value) {
		if (command && queue->upload_command_pool) {
			vkDestroyCommandPool(ctx->vk_device, queue->upload_command_pool, VK_ALLOCATOR);
			queue->upload_command_pool = VK_NULL_HANDLE;
			queue->upload_command_buffer = VK_NULL_HANDLE;
		}
		if (staging && queue->upload_staging_buffer) {
			vmaDestroyBuffer(ctx->vma_allocator, queue->upload_staging_buffer, queue->upload_staging_allocation);
			queue->upload_staging_buffer = VK_NULL_HANDLE;
			queue->upload_staging_allocation = NULL;
			queue->upload_staging_size = 0;
		}
		return;
	}

	struct rtvk_retired_upload_resource* retired = calloc(1, sizeof(*retired));
	if (!retired) {
		rtvk_throwf(RT_OUT_OF_HOST_MEMORY, "failed to allocate %zu bytes for retired upload resource", sizeof(*retired));
		return;
	}
	retired->timepoint = timepoint;
	if (command) {
		retired->command_pool = queue->upload_command_pool;
		queue->upload_command_pool = VK_NULL_HANDLE;
		queue->upload_command_buffer = VK_NULL_HANDLE;
	}
	if (staging) {
		retired->staging_buffer = queue->upload_staging_buffer;
		retired->staging_allocation = queue->upload_staging_allocation;
		queue->upload_staging_buffer = VK_NULL_HANDLE;
		queue->upload_staging_allocation = NULL;
		queue->upload_staging_size = 0;
	}
	retired->next = queue->retired_uploads;
	queue->retired_uploads = retired;
}

rt_timepoint rtvk_queue_submit(struct rtvk_context* ctx, struct rtvk_queue* queue, struct rtvk_command_buffer* command_buffer, rt_timepoint** wait_timepoints, usize* wait_count, usize* wait_capacity) {
	assert(queue);
	rt_timepoint* waits = wait_timepoints ? *wait_timepoints : NULL;
	usize count = wait_count ? *wait_count : 0;
	for (usize index = 0; index < count; index++) {
		struct rtvk_queue* source_queue = rtvk_timepoint_queue(ctx, waits[index]);
		u64 source_value = rtvk_timepoint_value(waits[index]);
		if (!source_queue || source_queue == queue || source_value == 0) {
			continue;
		}
		rtvk_queue_lock_pair(queue, source_queue);
		if (source_value > source_queue->submitted_value) {
			rtvk_queue_flush_locked(ctx, source_queue);
		}
		rtvk_queue_unlock_pair(queue, source_queue);
		if (rtvk_error() != RT_SUCCESS) {
			return rtvk_timepoint_make(queue, queue->timeline_value);
		}
	}
	if (command_buffer) {
		struct rtvk_lowered_command_buffer* lowered = rtvk_queue_create_lowered_command_buffer(ctx, queue);
		if (!lowered) {
			return rtvk_timepoint_make(queue, queue->timeline_value);
		}
		rtvk_command_buffer_lower(ctx, command_buffer, lowered);
		if (rtvk_error() != RT_SUCCESS) {
			rtvk_lowered_command_buffer_destroy(ctx, lowered);
			return rtvk_timepoint_make(queue, queue->timeline_value);
		}
		rtvk_mutex_lock(&queue->lock);
		rt_timepoint result = rtvk_queue_submit_lowered(ctx, queue, lowered, waits, count);
		if (rtvk_error() == RT_SUCCESS && wait_timepoints) {
			*wait_timepoints = NULL;
			*wait_count = 0;
			*wait_capacity = 0;
		}
		rtvk_mutex_unlock(&queue->lock);
		return result;
	}
	rtvk_mutex_lock(&queue->lock);
	rt_timepoint result = rtvk_queue_submit_lowered(ctx, queue, NULL, waits, count);
	if (rtvk_error() == RT_SUCCESS && wait_timepoints) {
		*wait_timepoints = NULL;
		*wait_count = 0;
		*wait_capacity = 0;
	}
	rtvk_mutex_unlock(&queue->lock);
	return result;
}

void rtvk_queue_lock_pair(struct rtvk_queue* first, struct rtvk_queue* second) {
	if (first == second) {
		rtvk_mutex_lock(&first->lock);
		return;
	}
	if (first->timepoint_id < second->timepoint_id) {
		rtvk_mutex_lock(&first->lock);
		rtvk_mutex_lock(&second->lock);
		return;
	}
	rtvk_mutex_lock(&second->lock);
	rtvk_mutex_lock(&first->lock);
}

void rtvk_queue_unlock_pair(struct rtvk_queue* first, struct rtvk_queue* second) {
	if (first == second) {
		rtvk_mutex_unlock(&first->lock);
		return;
	}
	if (first->timepoint_id < second->timepoint_id) {
		rtvk_mutex_unlock(&second->lock);
		rtvk_mutex_unlock(&first->lock);
		return;
	}
	rtvk_mutex_unlock(&first->lock);
	rtvk_mutex_unlock(&second->lock);
}

rt_timepoint rtvk_queue_submit_lowered(struct rtvk_context* ctx, struct rtvk_queue* queue, struct rtvk_lowered_command_buffer* lowered, rt_timepoint* wait_timepoints, usize wait_count) {
	(void)ctx;
	u64 value = queue->timeline_value + 1;
	struct rtvk_submitted_batch* batch = rtvk_queue_create_batch(lowered, wait_timepoints, wait_count, value, value);
	if (!batch) {
		rtvk_lowered_command_buffer_destroy(queue->base.ctx, lowered);
		return rtvk_timepoint_make(queue, queue->timeline_value);
	}
	queue->timeline_value = value;
	rtvk_queue_push_pending_batch(queue, batch);
	return rtvk_timepoint_make(queue, value);
}

void rtvk_queue_submit_command_buffer(struct rtvk_context* ctx, struct rtvk_queue* queue, struct rtvk_submitted_batch* batch, VkCommandBuffer command_buffer, u64 value) {
	usize wait_capacity = batch->wait_count + batch->binary_wait_count;
	VkSemaphore* wait_semaphores = NULL;
	u64* wait_values = NULL;
	VkPipelineStageFlags* wait_stages = NULL;
	if (wait_capacity) {
		wait_semaphores = malloc(wait_capacity * sizeof(*wait_semaphores));
		if (!wait_semaphores) {
			rtvk_throwf(RT_OUT_OF_HOST_MEMORY, "failed to allocate %zu bytes for queue wait semaphores", wait_capacity * sizeof(*wait_semaphores));
			return;
		}
		wait_values = malloc(wait_capacity * sizeof(*wait_values));
		if (!wait_values) {
			free(wait_semaphores);
			rtvk_throwf(RT_OUT_OF_HOST_MEMORY, "failed to allocate %zu bytes for queue wait values", wait_capacity * sizeof(*wait_values));
			return;
		}
		wait_stages = malloc(wait_capacity * sizeof(*wait_stages));
		if (!wait_stages) {
			free(wait_values);
			free(wait_semaphores);
			rtvk_throwf(RT_OUT_OF_HOST_MEMORY, "failed to allocate %zu bytes for queue wait stages", wait_capacity * sizeof(*wait_stages));
			return;
		}
	}
	usize wait_count = 0;
	for (usize index = 0; index < batch->wait_count; index++) {
		rt_timepoint wait = batch->wait_timepoints[index];
		struct rtvk_queue* wait_queue = rtvk_timepoint_queue(ctx, wait);
		u64 wait_value = rtvk_timepoint_value(wait);
		if (wait_queue && wait_value) {
			wait_semaphores[wait_count] = wait_queue->vk_timeline;
			wait_values[wait_count] = wait_value;
			wait_stages[wait_count] = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
			wait_count++;
		}
	}
	for (usize index = 0; index < batch->binary_wait_count; index++) {
		wait_semaphores[wait_count] = batch->binary_waits[index];
		wait_values[wait_count] = 0;
		wait_stages[wait_count] = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
		wait_count++;
	}

	usize signal_capacity = 1 + batch->binary_signal_count;
	VkSemaphore* signal_semaphores = malloc(signal_capacity * sizeof(*signal_semaphores));
	if (!signal_semaphores) {
		free(wait_stages);
		free(wait_values);
		free(wait_semaphores);
		rtvk_throwf(RT_OUT_OF_HOST_MEMORY, "failed to allocate %zu bytes for queue signal semaphores", signal_capacity * sizeof(*signal_semaphores));
		return;
	}
	u64* signal_values = malloc(signal_capacity * sizeof(*signal_values));
	if (!signal_values) {
		free(signal_semaphores);
		free(wait_stages);
		free(wait_values);
		free(wait_semaphores);
		rtvk_throwf(RT_OUT_OF_HOST_MEMORY, "failed to allocate %zu bytes for queue signal values", signal_capacity * sizeof(*signal_values));
		return;
	}
	usize signal_count = 1;
	signal_semaphores[0] = queue->vk_timeline;
	signal_values[0] = value;
	for (usize index = 0; index < batch->binary_signal_count; index++) {
		signal_semaphores[signal_count] = batch->binary_signals[index];
		signal_values[signal_count] = 0;
		signal_count++;
	}

	VkTimelineSemaphoreSubmitInfo timeline_info = { VK_STRUCTURE_TYPE_TIMELINE_SEMAPHORE_SUBMIT_INFO };
	timeline_info.waitSemaphoreValueCount = (u32)wait_count;
	timeline_info.pWaitSemaphoreValues = wait_count ? wait_values : NULL;
	timeline_info.signalSemaphoreValueCount = (u32)signal_count;
	timeline_info.pSignalSemaphoreValues = signal_values;

	VkSubmitInfo submit_info = { VK_STRUCTURE_TYPE_SUBMIT_INFO };
	submit_info.pNext = &timeline_info;
	submit_info.waitSemaphoreCount = (u32)wait_count;
	submit_info.pWaitSemaphores = wait_count ? wait_semaphores : NULL;
	submit_info.pWaitDstStageMask = wait_count ? wait_stages : NULL;
	submit_info.commandBufferCount = command_buffer ? 1 : 0;
	submit_info.pCommandBuffers = command_buffer ? &command_buffer : NULL;
	submit_info.signalSemaphoreCount = (u32)signal_count;
	submit_info.pSignalSemaphores = signal_semaphores;

	VkResult result = vkQueueSubmit(queue->vk_queue, 1, &submit_info, VK_NULL_HANDLE);
	free(signal_values);
	free(signal_semaphores);
	free(wait_stages);
	free(wait_values);
	free(wait_semaphores);
	if (result != VK_SUCCESS) {
		rtvk_throwf(rtvk_error_from_vk(result), "Vulkan call returned %s", rtvk_vk_result_name(result));
		return;
	}
}

rt_timepoint rtvk_queue_flush_locked(struct rtvk_context* ctx, struct rtvk_queue* queue) {
	assert(queue);
	if (!queue->pending_head) {
		return rtvk_timepoint_make(queue, queue->submitted_value);
	}

	for (struct rtvk_submitted_batch* batch = queue->pending_head; batch; batch = batch->next) {
		if (!batch->lowered_command_buffer) {
			rtvk_queue_submit_command_buffer(ctx, queue, batch, VK_NULL_HANDLE, batch->value);
			if (rtvk_error() != RT_SUCCESS) {
				return rtvk_timepoint_make(queue, queue->submitted_value);
			}
			continue;
		}

		struct rtvk_lowered_command_buffer* lowered = batch->lowered_command_buffer;
		rtvk_queue_submit_command_buffer(ctx, queue, batch, lowered->vk_command_buffer, batch->value);
		if (rtvk_error() != RT_SUCCESS) {
			return rtvk_timepoint_make(queue, queue->submitted_value);
		}
	}

	struct rtvk_submitted_batch* head = queue->pending_head;
	struct rtvk_submitted_batch* tail = queue->pending_tail;
	queue->pending_head = NULL;
	queue->pending_tail = NULL;
	rtvk_queue_push_submitted_list(queue, head, tail);
	queue->submitted_value = tail->value;
	return rtvk_timepoint_make(queue, tail->value);
}

rt_timepoint rtvk_queue_flush(struct rtvk_context* ctx, struct rtvk_queue* queue) {
	if (!queue) {
		return (rt_timepoint){ 0 };
	}
	rtvk_mutex_lock(&queue->lock);
	rt_timepoint result = rtvk_queue_flush_locked(ctx, queue);
	rtvk_mutex_unlock(&queue->lock);
	return result;
}

rt_timepoint rtvk_queue_submit_immediate(struct rtvk_context* ctx, struct rtvk_queue* queue, VkCommandBuffer command_buffer) {
	if (!queue || !command_buffer) {
		rtvk_throwf(RT_IMPROPER_USAGE, "immediate queue submission requires a queue and command buffer");
		return (rt_timepoint){ 0 };
	}

	rtvk_mutex_lock(&queue->lock);
	rt_timepoint timepoint = rtvk_queue_submit_immediate_locked(ctx, queue, command_buffer);
	rtvk_mutex_unlock(&queue->lock);
	return timepoint;
}

rt_timepoint rtvk_queue_submit_immediate_locked(struct rtvk_context* ctx, struct rtvk_queue* queue, VkCommandBuffer command_buffer) {
	assert(queue);
	assert(command_buffer);
	if (queue->pending_head) {
		rtvk_queue_flush_locked(ctx, queue);
		if (rtvk_error() != RT_SUCCESS) {
			return (rt_timepoint){ 0 };
		}
	}

	u64 value = queue->timeline_value + 1;
	VkTimelineSemaphoreSubmitInfo timeline_info = { VK_STRUCTURE_TYPE_TIMELINE_SEMAPHORE_SUBMIT_INFO };
	timeline_info.signalSemaphoreValueCount = 1;
	timeline_info.pSignalSemaphoreValues = &value;
	VkSubmitInfo submit_info = { VK_STRUCTURE_TYPE_SUBMIT_INFO };
	submit_info.pNext = &timeline_info;
	submit_info.commandBufferCount = 1;
	submit_info.pCommandBuffers = &command_buffer;
	submit_info.signalSemaphoreCount = 1;
	submit_info.pSignalSemaphores = &queue->vk_timeline;
	VkResult result = vkQueueSubmit(queue->vk_queue, 1, &submit_info, VK_NULL_HANDLE);
	if (result != VK_SUCCESS) {
		rtvk_throwf(rtvk_error_from_vk(result), "Vulkan call returned %s", rtvk_vk_result_name(result));
		return (rt_timepoint){ 0 };
	}

	queue->timeline_value = value;
	queue->submitted_value = value;
	return rtvk_timepoint_make(queue, value);
}

rt_timepoint rtvk_queue_signal(struct rtvk_context* ctx, struct rtvk_queue* queue) {
	return rtvk_queue_submit(ctx, queue, NULL, NULL, NULL, NULL);
}

bool rtvk_queue_signal_binary_on_next_flush(struct rtvk_queue* queue, VkSemaphore semaphore) {
	assert(queue);
	if (!semaphore) {
		return false;
	}
	rtvk_mutex_lock(&queue->lock);
	u64 value = queue->timeline_value + 1;
	struct rtvk_submitted_batch* batch = rtvk_queue_create_batch(NULL, NULL, 0, value, value);
	if (!batch) {
		rtvk_mutex_unlock(&queue->lock);
		return false;
	}
	if (batch->binary_signal_count == batch->binary_signal_capacity) {
		usize required_count = batch->binary_signal_count + 1;
		usize capacity = required_count;
		if (capacity < 8) {
			capacity = 8;
		} else {
			unsigned long most_significant_bit = 0;
			_BitScanReverse64(&most_significant_bit, capacity - 1);
			capacity = (usize)1 << (most_significant_bit + 1);
		}
		VkSemaphore* binary_signals = realloc(batch->binary_signals, capacity * sizeof(*binary_signals));
		if (!binary_signals) {
			free(batch);
			rtvk_throwf(RT_OUT_OF_HOST_MEMORY, "failed to allocate %zu bytes for batch binary signal semaphores", capacity * sizeof(*binary_signals));
			rtvk_mutex_unlock(&queue->lock);
			return false;
		}
		batch->binary_signals = binary_signals;
		batch->binary_signal_capacity = capacity;
	}
	batch->binary_signals[batch->binary_signal_count++] = semaphore;
	queue->timeline_value = value;
	rtvk_queue_push_pending_batch(queue, batch);
	rtvk_mutex_unlock(&queue->lock);
	return true;
}

rt_timepoint rtvk_queue_wait_binary(struct rtvk_context* ctx, struct rtvk_queue* queue, VkSemaphore semaphore) {
	(void)ctx;
	assert(queue);
	if (!semaphore) {
		return (rt_timepoint){ 0 };
	}
	rtvk_mutex_lock(&queue->lock);
	u64 value = queue->timeline_value + 1;
	struct rtvk_submitted_batch* batch = rtvk_queue_create_batch(NULL, NULL, 0, value, value);
	if (!batch) {
		rtvk_mutex_unlock(&queue->lock);
		return rtvk_timepoint_make(queue, queue->submitted_value);
	}
	batch->binary_waits = malloc(sizeof(*batch->binary_waits));
	if (!batch->binary_waits) {
		free(batch);
		rtvk_throwf(RT_OUT_OF_HOST_MEMORY, "failed to allocate %zu bytes for batch binary wait semaphores", sizeof(*batch->binary_waits));
		rtvk_mutex_unlock(&queue->lock);
		return rtvk_timepoint_make(queue, queue->submitted_value);
	}
	batch->binary_waits[0] = semaphore;
	batch->binary_wait_count = 1;
	batch->binary_wait_capacity = 1;
	queue->timeline_value = value;
	rtvk_queue_push_pending_batch(queue, batch);
	rtvk_mutex_unlock(&queue->lock);
	return rtvk_timepoint_make(queue, value);
}

rt_timepoint rtvk_queue_signal_binary_after_timepoint(struct rtvk_queue* queue, u64 wait_value, VkSemaphore semaphore) {
	assert(queue);
	if (!semaphore || wait_value == 0) {
		return (rt_timepoint){ 0 };
	}
	rtvk_mutex_lock(&queue->lock);
	u64 signal_value = queue->timeline_value + 1;
	rt_timepoint* wait_timepoints = malloc(sizeof(*wait_timepoints));
	if (!wait_timepoints) {
		rtvk_throwf(RT_OUT_OF_HOST_MEMORY, "failed to allocate %zu bytes for batch wait timepoints", sizeof(*wait_timepoints));
		rtvk_mutex_unlock(&queue->lock);
		return rtvk_timepoint_make(queue, queue->timeline_value);
	}
	wait_timepoints[0] = rtvk_timepoint_make(queue, wait_value);
	struct rtvk_submitted_batch* batch = rtvk_queue_create_batch(NULL, wait_timepoints, 1, signal_value, signal_value);
	if (!batch) {
		free(wait_timepoints);
		rtvk_mutex_unlock(&queue->lock);
		return rtvk_timepoint_make(queue, queue->timeline_value);
	}
	batch->binary_signals = malloc(sizeof(*batch->binary_signals));
	if (!batch->binary_signals) {
		free(batch->wait_timepoints);
		free(batch);
		rtvk_throwf(RT_OUT_OF_HOST_MEMORY, "failed to allocate %zu bytes for batch binary signal semaphores", sizeof(*batch->binary_signals));
		rtvk_mutex_unlock(&queue->lock);
		return rtvk_timepoint_make(queue, queue->timeline_value);
	}
	batch->binary_signals[0] = semaphore;
	batch->binary_signal_count = 1;
	batch->binary_signal_capacity = 1;
	queue->timeline_value = signal_value;
	rtvk_queue_push_pending_batch(queue, batch);
	rtvk_mutex_unlock(&queue->lock);
	return rtvk_timepoint_make(queue, signal_value);
}

/*===============================================================================================*/
/*                                                                                               */
/*===============================================================================================*/

struct rtvk_queue* rtvk_queue_query_present(struct rtvk_context* ctx, VkSurfaceKHR surface) {
	for (u32 i = 0; i < ctx->queue_count; i++) {
		VkBool32 supported = VK_FALSE;
		VkResult result = vkGetPhysicalDeviceSurfaceSupportKHR(ctx->vk_physical_device, ctx->queues[i]->family_index, surface, &supported);
		if (result != VK_SUCCESS) {
			rtvk_throwf(rtvk_error_from_vk(result), "Vulkan call returned %s", rtvk_vk_result_name(result));
			return NULL;
		}
		if (supported) {
			return ctx->queues[i];
		}
	}
	return NULL;
}

void rtvk_timepoint_wait(struct rtvk_context* ctx, rt_timepoint timepoint) {
	if (rtvk_timepoint_complete(timepoint)) {
		return;
	}
	struct rtvk_queue* queue = rtvk_timepoint_queue(ctx, timepoint);
	u64 value = rtvk_timepoint_value(timepoint);
	rtvk_mutex_lock(&queue->lock);
	if (value <= queue->completed_value) {
		rtvk_mutex_unlock(&queue->lock);
		return;
	}
	if (value > queue->submitted_value) {
		rtvk_queue_flush_locked(ctx, queue);
	}

	VkSemaphoreWaitInfo wait_info = { VK_STRUCTURE_TYPE_SEMAPHORE_WAIT_INFO };
	wait_info.pNext = NULL;
	wait_info.flags = 0;
	wait_info.semaphoreCount = 1;
	wait_info.pSemaphores = &queue->vk_timeline;
	wait_info.pValues = &value;

	VkResult result = vkWaitSemaphores(ctx->vk_device, &wait_info, UINT64_MAX);
	if (result != VK_SUCCESS) {
		rtvk_throwf(rtvk_error_from_vk(result), "Vulkan call returned %s", rtvk_vk_result_name(result));
	}
	if (value > queue->completed_value) {
		queue->completed_value = value;
	}
	rtvk_queue_collect_to_value(ctx, queue, queue->completed_value);
	rtvk_mutex_unlock(&queue->lock);
}
bool rtvk_timepoint_reached(struct rtvk_context* ctx, rt_timepoint timepoint) {
	if (rtvk_timepoint_complete(timepoint)) {
		return true;
	}
	struct rtvk_queue* queue = rtvk_timepoint_queue(ctx, timepoint);
	u64 requested_value = rtvk_timepoint_value(timepoint);
	rtvk_mutex_lock(&queue->lock);
	if (requested_value <= queue->completed_value) {
		rtvk_mutex_unlock(&queue->lock);
		return true;
	}

	u64 value = 0;
	VkResult result = vkGetSemaphoreCounterValue(ctx->vk_device, queue->vk_timeline, &value);
	if (result != VK_SUCCESS) {
		rtvk_throwf(rtvk_error_from_vk(result), "Vulkan call returned %s", rtvk_vk_result_name(result));
		rtvk_mutex_unlock(&queue->lock);
		return false;
	}
	if (value > queue->completed_value) {
		queue->completed_value = value;
	}
	bool reached = value >= requested_value;
	rtvk_mutex_unlock(&queue->lock);
	return reached;
}
bool rtvk_timepoint_complete(rt_timepoint timepoint) {
	return timepoint.value == 0;
}
