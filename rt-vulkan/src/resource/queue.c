#include "queue.h"
#include "context.h"
#include "error.h"
#include "extension/swapchain/swapchain.h"

#include <stdlib.h>

/*===============================================================================================*/
/*                                                                                               */
/*===============================================================================================*/


rt_queue rtQueueQuery(enum rt_queue_capability capability) {
	struct rtvk_queue* queue = rtvk_queue_query(rtvk_get_current_context(), capability);
	return rtvk_queue_to_handle(queue);
}

rt_timepoint rtQueueSubmit(rt_queue queue, rt_command_buffer command_buffer) {
	struct rtvk_timepoint timepoint = rtvk_queue_submit(
		rtvk_get_current_context(),
		rtvk_queue_from_handle(queue),
		rtvk_command_buffer_from_handle(command_buffer));
	return rtvk_timepoint_to_public(timepoint);
}

rt_timepoint rtQueueFlush(rt_queue queue) {
	struct rtvk_timepoint timepoint = rtvk_queue_flush(rtvk_get_current_context(), rtvk_queue_from_handle(queue));
	return rtvk_timepoint_to_public(timepoint);
}

void rtQueueWait(rt_queue queue, rt_timepoint timepoint) {
	struct rtvk_timepoint internal = { rtvk_queue_from_handle(timepoint.queue), timepoint.value };
	rtvk_queue_wait(rtvk_get_current_context(), rtvk_queue_from_handle(queue), internal);
}

void rtTimepointWait(rt_timepoint timepoint) {
	struct rtvk_timepoint internal = { rtvk_queue_from_handle(timepoint.queue), timepoint.value };
	rtvk_timepoint_wait(rtvk_get_current_context(), internal);
}

bool rtTimepointReached(rt_timepoint timepoint) {
	struct rtvk_timepoint internal = { rtvk_queue_from_handle(timepoint.queue), timepoint.value };
	return rtvk_timepoint_reached(rtvk_get_current_context(), internal);
}


/*===============================================================================================*/
/*                                                                                               */
/*===============================================================================================*/

bool rtvk_timepoint_complete(struct rtvk_timepoint timepoint);

struct rtvk_queue* rtvk_queue_create(struct rtvk_context* ctx, VkQueue vk_queue, enum rt_queue_capability capability, u32 family_index, u32 queue_index) {
	struct rtvk_queue* queue = RTVK_ALLOC_RESOURCE(struct rtvk_queue);
	if (!queue) { return NULL; }

	if (!rtvk_queue_init(ctx, queue, vk_queue, capability, family_index, queue_index)) {
		rtvk_queue_finish(ctx, queue);
		RTVK_FREE_RESOURCE(queue);
		return NULL;
	}

	return queue;
}
void rtvk_queue_destroy(struct rtvk_context* ctx, struct rtvk_queue* queue) {
	if (!queue) { return; }
	rtvk_queue_finish(ctx, queue);
	rtvk_resource_retire(RTVK_RESOURCE_BASE(queue));
}

bool rtvk_queue_init(struct rtvk_context* ctx, struct rtvk_queue* queue, VkQueue vk_queue, enum rt_queue_capability capability, u32 family_index, u32 queue_index) {
	rtvk_init_resource_base(ctx, RTVK_RESOURCE_BASE(queue), RT_RESOURCE_QUEUE);
	queue->vk_queue = vk_queue;
	queue->capability = capability;
	queue->family_index = family_index;
	queue->queue_index = queue_index;

	VkSemaphoreTypeCreateInfo timeline_info = { VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO };
	timeline_info.pNext = NULL;
	timeline_info.semaphoreType = VK_SEMAPHORE_TYPE_TIMELINE;
	timeline_info.initialValue = 0;

	VkSemaphoreCreateInfo semaphore_info = { VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO };
	semaphore_info.pNext = &timeline_info;
	semaphore_info.flags = 0;

	VkResult result = vkCreateSemaphore(ctx->vk_device, &semaphore_info, VK_ALLOCATOR, &queue->vk_timeline);
	if (result != VK_SUCCESS) {
		rtvk_throwf(rtvk_error_from_vk(result), NULL);
		return false;
	}

	return true;
}
void rtvk_queue_finish(struct rtvk_context* ctx, struct rtvk_queue* queue) {
	if (!queue) { return; }
	rtvk_queue_flush(ctx, queue);
	if (queue->timeline_value) {
		struct rtvk_timepoint last = { queue, queue->timeline_value };
		rtvk_timepoint_wait(ctx, last);
	}
	rtvk_queue_collect(ctx, queue);
	if (queue->vk_timeline) {
		vkDestroySemaphore(ctx->vk_device, queue->vk_timeline, VK_ALLOCATOR);
		queue->vk_timeline = VK_NULL_HANDLE;
	}
	if (queue->copy_command_pool) {
		vkDestroyCommandPool(ctx->vk_device, queue->copy_command_pool, VK_ALLOCATOR);
		queue->copy_command_pool = VK_NULL_HANDLE;
		queue->copy_command_buffer = VK_NULL_HANDLE;
	}
	if (queue->upload_command_pool) {
		vkDestroyCommandPool(ctx->vk_device, queue->upload_command_pool, VK_ALLOCATOR);
		queue->upload_command_pool = VK_NULL_HANDLE;
		queue->upload_command_buffer = VK_NULL_HANDLE;
	}
	if (queue->upload_staging_buffer) {
		vmaDestroyBuffer(ctx->vma_allocator, queue->upload_staging_buffer, queue->upload_staging_allocation);
		queue->upload_staging_buffer = VK_NULL_HANDLE;
		queue->upload_staging_allocation = NULL;
		queue->upload_staging_size = 0;
	}
	rtvk_finish_resource_base(ctx, RTVK_RESOURCE_BASE(queue));
}

struct rtvk_queue* rtvk_queue_query(struct rtvk_context* ctx, enum rt_queue_capability capability) {
	for (u32 i = 0; i < ctx->queue_count; i++) {
		if (ctx->queues[i]->capability == capability) { return ctx->queues[i]; }
	}
	return NULL;
}

VkPipelineStageFlags rtvk_queue_wait_stage(struct rtvk_queue* queue) {
	if (!queue) { return VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT; }
	if (queue->capability == RT_QUEUE_GRAPHICS) { return VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT; }
	if (queue->capability == RT_QUEUE_COMPUTE) { return VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT; }
	return VK_PIPELINE_STAGE_TRANSFER_BIT;
}

u64 rtvk_queue_completed_value(struct rtvk_context* ctx, struct rtvk_queue* queue) {
	if (!queue || !queue->vk_timeline) { return 0; }

	u64 value = 0;
	VkResult result = vkGetSemaphoreCounterValue(ctx->vk_device, queue->vk_timeline, &value);
	if (result != VK_SUCCESS) {
		rtvk_throwf(rtvk_error_from_vk(result), NULL);
		return 0;
	}
	return value;
}
struct rtvk_submitted_batch* rtvk_queue_create_batch(struct rtvk_context* ctx, struct rtvk_command_buffer* command_buffer, u64 value) {
	struct rtvk_submitted_batch* batch = calloc(1, sizeof(*batch));
	if (!batch) {
		rtvk_throwf(RT_OUT_OF_HOST_MEMORY, "failed to allocate submitted batch metadata");
		return NULL;
	}

	batch->value = value;
	batch->command_buffer_node = command_buffer ? command_buffer->active : NULL;
	if (batch->command_buffer_node) { rtvk_command_buffer_node_retain(batch->command_buffer_node); }
	return batch;
}
void rtvk_queue_push_batch(struct rtvk_queue* queue, struct rtvk_submitted_batch* batch) {
	if (!batch) { return; }
	if (queue->submitted_tail) {
		queue->submitted_tail->next = batch;
	} else {
		queue->submitted_head = batch;
	}
	queue->submitted_tail = batch;
}

static void rtvk_queue_push_pending_batch(struct rtvk_queue* queue, struct rtvk_submitted_batch* batch) {
	if (!batch) { return; }
	if (queue->pending_tail) {
		queue->pending_tail->next = batch;
	} else {
		queue->pending_head = batch;
	}
	queue->pending_tail = batch;
}

static void rtvk_queue_push_submitted_list(struct rtvk_queue* queue, struct rtvk_submitted_batch* head, struct rtvk_submitted_batch* tail) {
	if (!head) { return; }
	if (queue->submitted_tail) {
		queue->submitted_tail->next = head;
	} else {
		queue->submitted_head = head;
	}
	queue->submitted_tail = tail;
}
void rtvk_queue_collect(struct rtvk_context* ctx, struct rtvk_queue* queue) {
	if (!queue) { return; }

	u64 completed_value = rtvk_queue_completed_value(ctx, queue);
	while (queue->submitted_head && queue->submitted_head->value <= completed_value) {
		struct rtvk_submitted_batch* batch = queue->submitted_head;
		queue->submitted_head = batch->next;
		if (!queue->submitted_head) {
			queue->submitted_tail = NULL;
		}

		if (batch->command_buffer_node &&
			batch->command_buffer_node->pending_timepoint.queue == queue &&
			batch->command_buffer_node->pending_timepoint.value <= completed_value) {
			batch->command_buffer_node->pending_timepoint.queue = NULL;
			batch->command_buffer_node->pending_timepoint.value = 0;
		}
		rtvk_command_buffer_node_release(batch->command_buffer_node);
		free(batch);
	}
}
struct rtvk_timepoint rtvk_queue_submit(struct rtvk_context* ctx, struct rtvk_queue* queue, struct rtvk_command_buffer* command_buffer) {
	if (!queue) { return (struct rtvk_timepoint){ NULL, 0 }; }

	if (command_buffer && !command_buffer->active) {
		rtvk_throwf(RT_IMPROPER_USAGE, "command buffer has not been recorded");
		return (struct rtvk_timepoint){ queue, queue->timeline_value };
	}

	rtvk_queue_collect(ctx, queue);

	u64 value = queue->timeline_value + 1;
	struct rtvk_submitted_batch* batch = rtvk_queue_create_batch(ctx, command_buffer, value);
	if (!batch) { return (struct rtvk_timepoint){ queue, queue->timeline_value }; }

	queue->timeline_value = value;
	rtvk_queue_push_pending_batch(queue, batch);
	if (command_buffer) {
		struct rtvk_command_buffer* node = command_buffer->active;
		node->pending_timepoint = (struct rtvk_timepoint){ queue, value };
	}
	return (struct rtvk_timepoint){ queue, value };
}

struct rtvk_timepoint rtvk_queue_flush(struct rtvk_context* ctx, struct rtvk_queue* queue) {
	if (!queue) { return (struct rtvk_timepoint){ NULL, 0 }; }
	if (!queue->pending_head) { return (struct rtvk_timepoint){ queue, queue->submitted_value }; }

	u32 command_buffer_count = 0;
	for (struct rtvk_submitted_batch* batch = queue->pending_head; batch; batch = batch->next) {
		if (batch->command_buffer_node) {
			command_buffer_count++;
		}
	}

	VkCommandBuffer stack_command_buffers[64];
	VkCommandBuffer* command_buffers = stack_command_buffers;
	if (command_buffer_count > (u32)(sizeof(stack_command_buffers) / sizeof(stack_command_buffers[0]))) {
		command_buffers = calloc(command_buffer_count, sizeof(*command_buffers));
		if (!command_buffers) {
			rtvk_throwf(RT_OUT_OF_HOST_MEMORY, "failed to allocate queue flush command buffer list");
			return (struct rtvk_timepoint){ queue, queue->submitted_value };
		}
	}

	u32 command_buffer_index = 0;
	for (struct rtvk_submitted_batch* batch = queue->pending_head; batch; batch = batch->next) {
		if (batch->command_buffer_node) {
			command_buffers[command_buffer_index++] = batch->command_buffer_node->vk_command_buffer;
		}
	}

	u64 value = queue->pending_tail->value;
	u32 timeline_wait_count = queue->wait_count;
	u32 binary_wait_count = queue->binary_wait_count;
	u32 binary_signal_count = queue->binary_signal_count;
	u32 wait_count = timeline_wait_count + binary_wait_count;
	VkSemaphore wait_semaphores[16];
	u64 wait_values[16];
	VkPipelineStageFlags wait_stages[16];
	for (u32 i = 0; i < timeline_wait_count; i++) {
		wait_semaphores[i] = queue->wait_timepoints[i].queue->vk_timeline;
		wait_values[i] = queue->wait_timepoints[i].value;
		wait_stages[i] = rtvk_queue_wait_stage(queue);
	}
	for (u32 i = 0; i < binary_wait_count; i++) {
		u32 wait_index = timeline_wait_count + i;
		wait_semaphores[wait_index] = queue->binary_waits[i];
		wait_values[wait_index] = 0;
		wait_stages[wait_index] = rtvk_queue_wait_stage(queue);
	}

	VkSemaphore signal_semaphores[9];
	u64 signal_values[9];
	signal_semaphores[0] = queue->vk_timeline;
	signal_values[0] = value;
	for (u32 i = 0; i < binary_signal_count; i++) {
		signal_semaphores[i + 1] = queue->binary_signals[i];
		signal_values[i + 1] = 0;
	}

	VkTimelineSemaphoreSubmitInfo timeline_info = { VK_STRUCTURE_TYPE_TIMELINE_SEMAPHORE_SUBMIT_INFO };
	timeline_info.pNext = NULL;
	timeline_info.waitSemaphoreValueCount = wait_count;
	timeline_info.pWaitSemaphoreValues = wait_count ? wait_values : NULL;
	timeline_info.signalSemaphoreValueCount = 1 + binary_signal_count;
	timeline_info.pSignalSemaphoreValues = signal_values;

	VkSubmitInfo submit_info = { VK_STRUCTURE_TYPE_SUBMIT_INFO };
	submit_info.pNext = &timeline_info;
	submit_info.waitSemaphoreCount = wait_count;
	submit_info.pWaitSemaphores = wait_count ? wait_semaphores : NULL;
	submit_info.pWaitDstStageMask = wait_count ? wait_stages : NULL;
	submit_info.commandBufferCount = command_buffer_count;
	submit_info.pCommandBuffers = command_buffer_count ? command_buffers : NULL;
	submit_info.signalSemaphoreCount = 1 + binary_signal_count;
	submit_info.pSignalSemaphores = signal_semaphores;

	VkResult result = vkQueueSubmit(queue->vk_queue, 1, &submit_info, VK_NULL_HANDLE);
	if (result != VK_SUCCESS) {
		if (command_buffers != stack_command_buffers) { free(command_buffers); }
		rtvk_throwf(rtvk_error_from_vk(result), NULL);
		return (struct rtvk_timepoint){ queue, queue->submitted_value };
	}
	if (command_buffers != stack_command_buffers) { free(command_buffers); }

	queue->wait_count = 0;
	queue->binary_wait_count = 0;
	queue->binary_signal_count = 0;
	struct rtvk_submitted_batch* head = queue->pending_head;
	struct rtvk_submitted_batch* tail = queue->pending_tail;
	queue->pending_head = NULL;
	queue->pending_tail = NULL;
	rtvk_queue_push_submitted_list(queue, head, tail);
	queue->submitted_value = value;
	return (struct rtvk_timepoint){ queue, value };
}
struct rtvk_timepoint rtvk_queue_signal(struct rtvk_context* ctx, struct rtvk_queue* queue) {
	return rtvk_queue_submit(ctx, queue, NULL);
}

bool rtvk_queue_signal_binary_on_next_flush(struct rtvk_queue* queue, VkSemaphore semaphore) {
	if (!queue || !semaphore) { return false; }
	if (queue->binary_signal_count >= sizeof(queue->binary_signals) / sizeof(queue->binary_signals[0])) {
		rtvk_throwf(RT_IMPROPER_USAGE, "queue has too many binary signal semaphores");
		return false;
	}
	queue->binary_signals[queue->binary_signal_count++] = semaphore;
	return true;
}

struct rtvk_timepoint rtvk_queue_wait_binary(struct rtvk_context* ctx, struct rtvk_queue* queue, VkSemaphore semaphore) {
	if (!queue || !semaphore) { return (struct rtvk_timepoint){ NULL, 0 }; }
	if (queue->binary_wait_count >= sizeof(queue->binary_waits) / sizeof(queue->binary_waits[0])) {
		rtvk_throwf(RT_IMPROPER_USAGE, "queue has too many binary wait semaphores");
		return (struct rtvk_timepoint){ queue, queue->timeline_value };
	}

	rtvk_queue_collect(ctx, queue);
	u64 value = queue->timeline_value + 1;
	queue->wait_count = 0;
	queue->timeline_value = value;
	queue->binary_waits[queue->binary_wait_count++] = semaphore;
	struct rtvk_submitted_batch* batch = rtvk_queue_create_batch(ctx, NULL, value);
	if (!batch) { return (struct rtvk_timepoint){ queue, queue->submitted_value }; }
	rtvk_queue_push_pending_batch(queue, batch);
	return (struct rtvk_timepoint){ queue, value };
}

void rtvk_queue_wait(struct rtvk_context* ctx, struct rtvk_queue* queue, struct rtvk_timepoint timepoint) {
	if (!queue || !timepoint.queue || timepoint.value == 0) { return; }
	if (queue == timepoint.queue && timepoint.value > queue->submitted_value) { return; }
	if (timepoint.value > timepoint.queue->submitted_value) {
		rtvk_queue_flush(ctx, timepoint.queue);
	}
	if (queue->wait_count >= sizeof(queue->wait_timepoints) / sizeof(queue->wait_timepoints[0])) {
		rtvk_throwf(RT_IMPROPER_USAGE, "queue has too many wait timepoints");
		return;
	}
	queue->wait_timepoints[queue->wait_count++] = timepoint;
}

struct rtvk_timepoint rtvk_queue_signal_binary_after_timepoint(struct rtvk_queue* queue, u64 wait_value, VkSemaphore semaphore) {
	if (!queue || !semaphore || wait_value == 0) { return (struct rtvk_timepoint){ NULL, 0 }; }

	rtvk_queue_collect(queue->base.ctx, queue);

	u64 signal_value = queue->timeline_value + 1;
	u64 signal_values[2] = { 0, signal_value };
	VkSemaphore signal_semaphores[2];
	signal_semaphores[0] = semaphore;
	signal_semaphores[1] = queue->vk_timeline;

	VkTimelineSemaphoreSubmitInfo timeline_info = { VK_STRUCTURE_TYPE_TIMELINE_SEMAPHORE_SUBMIT_INFO };
	timeline_info.pNext = NULL;
	timeline_info.waitSemaphoreValueCount = 1;
	timeline_info.pWaitSemaphoreValues = &wait_value;
	timeline_info.signalSemaphoreValueCount = 2;
	timeline_info.pSignalSemaphoreValues = signal_values;

	VkPipelineStageFlags wait_stage = rtvk_queue_wait_stage(queue);
	VkSubmitInfo submit_info = { VK_STRUCTURE_TYPE_SUBMIT_INFO };
	submit_info.pNext = &timeline_info;
	submit_info.waitSemaphoreCount = 1;
	submit_info.pWaitSemaphores = &queue->vk_timeline;
	submit_info.pWaitDstStageMask = &wait_stage;
	submit_info.commandBufferCount = 0;
	submit_info.pCommandBuffers = NULL;
	submit_info.signalSemaphoreCount = 2;
	submit_info.pSignalSemaphores = signal_semaphores;

	VkResult result = vkQueueSubmit(queue->vk_queue, 1, &submit_info, VK_NULL_HANDLE);
	if (result != VK_SUCCESS) {
		rtvk_throwf(rtvk_error_from_vk(result), NULL);
		return (struct rtvk_timepoint){ queue, queue->timeline_value };
	}

	queue->timeline_value = signal_value;
	queue->submitted_value = signal_value;
	rtvk_queue_collect(queue->base.ctx, queue);
	return (struct rtvk_timepoint){ queue, signal_value };
}

struct rtvk_queue* rtvk_queue_query_present(struct rtvk_context* ctx, VkSurfaceKHR surface) {
	for (u32 i = 0; i < ctx->queue_count; i++) {
		VkBool32 supported = VK_FALSE;
		VkResult result = vkGetPhysicalDeviceSurfaceSupportKHR(ctx->vk_physical_device, ctx->queues[i]->family_index, surface, &supported);
		if (result != VK_SUCCESS) {
			rtvk_throwf(rtvk_error_from_vk(result), NULL);
			return NULL;
		}
		if (supported) { return ctx->queues[i]; }
	}
	return NULL;
}

void rtvk_timepoint_wait(struct rtvk_context* ctx, struct rtvk_timepoint timepoint) {
	if (rtvk_timepoint_complete(timepoint)) { return; }
	if (timepoint.value > timepoint.queue->submitted_value) {
		rtvk_queue_flush(ctx, timepoint.queue);
	}

	VkSemaphoreWaitInfo wait_info = { VK_STRUCTURE_TYPE_SEMAPHORE_WAIT_INFO };
	wait_info.pNext = NULL;
	wait_info.flags = 0;
	wait_info.semaphoreCount = 1;
	wait_info.pSemaphores = &timepoint.queue->vk_timeline;
	wait_info.pValues = &timepoint.value;

	VkResult result = vkWaitSemaphores(ctx->vk_device, &wait_info, UINT64_MAX);
	if (result != VK_SUCCESS) {
		rtvk_throwf(rtvk_error_from_vk(result), NULL);
	}
	rtvk_queue_collect(ctx, timepoint.queue);
}
bool rtvk_timepoint_reached(struct rtvk_context* ctx, struct rtvk_timepoint timepoint) {
	if (rtvk_timepoint_complete(timepoint)) { return true; }

	u64 value = 0;
	VkResult result = vkGetSemaphoreCounterValue(ctx->vk_device, timepoint.queue->vk_timeline, &value);
	if (result != VK_SUCCESS) {
		rtvk_throwf(rtvk_error_from_vk(result), NULL);
		return false;
	}
	return value >= timepoint.value;
}
bool rtvk_timepoint_complete(struct rtvk_timepoint timepoint) {
	return !timepoint.queue || timepoint.value == 0;
}


