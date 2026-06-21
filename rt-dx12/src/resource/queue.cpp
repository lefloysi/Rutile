#include "queue.h"
#include "context.h"
#include "error.h"
#include "resource/swapchain.h"
#include "resource/command_buffer.h"

#include <stdlib.h>

rt_queue rtQueueQuery(enum rt_queue_capability capability) {
	struct rtdx_queue* queue = rtdx_queue_query(rtdx_get_current_context(), capability);
	return rtdx_queue_to_handle(queue);
}

rt_timepoint rtQueueSubmit(rt_queue queue, rt_command_buffer command_buffer) {
	struct rtdx_timepoint timepoint = rtdx_queue_submit(
		rtdx_get_current_context(),
		rtdx_queue_from_handle(queue),
		rtdx_command_buffer_from_handle(command_buffer));
	return rtdx_timepoint_to_public(timepoint);
}

rt_timepoint rtQueueFlush(rt_queue queue) {
	struct rtdx_timepoint timepoint = rtdx_queue_flush(rtdx_get_current_context(), rtdx_queue_from_handle(queue));
	return rtdx_timepoint_to_public(timepoint);
}

void rtQueueWait(rt_queue queue, rt_timepoint timepoint) {
	struct rtdx_timepoint internal = { rtdx_queue_from_handle(timepoint.queue), timepoint.value };
	rtdx_queue_wait(rtdx_get_current_context(), rtdx_queue_from_handle(queue), internal);
}

void rtTimepointWait(rt_timepoint timepoint) {
	struct rtdx_timepoint internal = { rtdx_queue_from_handle(timepoint.queue), timepoint.value };
	rtdx_timepoint_wait(rtdx_get_current_context(), internal);
}

bool rtTimepointReached(rt_timepoint timepoint) {
	struct rtdx_timepoint internal = { rtdx_queue_from_handle(timepoint.queue), timepoint.value };
	return rtdx_timepoint_reached(rtdx_get_current_context(), internal);
}

/*===============================================================================================*/
/*                                                                                               */
/*===============================================================================================*/

struct rtdx_queue* rtdx_queue_create(struct rtdx_context* ctx, enum rt_queue_capability capability) {
	struct rtdx_queue* queue = RTDX_ALLOC_RESOURCE(struct rtdx_queue);
	if (!queue) { return NULL; }

	if (!rtdx_queue_init(ctx, queue, capability)) {
		rtdx_queue_finish(ctx, queue);
		RTDX_FREE_RESOURCE(queue);
		return NULL;
	}

	return queue;
}

void rtdx_queue_destroy(struct rtdx_context* ctx, struct rtdx_queue* queue) {
	if (!queue) { return; }
	rtdx_queue_finish(ctx, queue);
	rtdx_resource_retire(RTDX_RESOURCE_BASE(queue));
}

bool rtdx_queue_init(struct rtdx_context* ctx, struct rtdx_queue* queue, enum rt_queue_capability capability) {
	rtdx_init_resource_base(ctx, RTDX_RESOURCE_BASE(queue), RT_RESOURCE_QUEUE);
	queue->capability = capability;

	D3D12_COMMAND_QUEUE_DESC queue_info = {};
	queue_info.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
	queue_info.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL;
	queue_info.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
	queue_info.NodeMask = 0;

	HRESULT result = ctx->d3d_device->CreateCommandQueue(&queue_info, IID_PPV_ARGS(&queue->d3d_queue));
	if (FAILED(result)) {
		rtdx_throwf(rtdx_error_from_hresult(result), "CreateCommandQueue failed: 0x%08x", (u32)result);
		return false;
	}

	result = ctx->d3d_device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&queue->d3d_fence));
	if (FAILED(result)) {
		rtdx_throwf(rtdx_error_from_hresult(result), "CreateFence failed: 0x%08x", (u32)result);
		return false;
	}

	queue->fence_event = CreateEventA(NULL, FALSE, FALSE, NULL);
	if (!queue->fence_event) {
		rtdx_throwf(RT_PLATFORM_FAILURE, "CreateEventA failed");
		return false;
	}

	return true;
}

void rtdx_queue_finish(struct rtdx_context* ctx, struct rtdx_queue* queue) {
	if (!queue) { return; }
	rtdx_queue_collect(ctx, queue);
	rtdx_release(&queue->upload_command_list);
	rtdx_release(&queue->upload_allocator);
	rtdx_release(&queue->upload_buffer);
	queue->upload_buffer_size = 0;
	queue->upload_fence_value = 0;
	if (queue->fence_event) {
		CloseHandle(queue->fence_event);
		queue->fence_event = NULL;
	}
	rtdx_release(&queue->d3d_fence);
	rtdx_release(&queue->d3d_queue);
	rtdx_finish_resource_base(ctx, RTDX_RESOURCE_BASE(queue));
}

struct rtdx_queue* rtdx_queue_query(struct rtdx_context* ctx, enum rt_queue_capability capability) {
	if (!ctx) { return NULL; }
	for (u32 i = 0; i < ctx->queue_count; i++) {
		if (ctx->queues[i]->capability == capability) { return ctx->queues[i]; }
	}
	return NULL;
}

static bool rtdx_timepoint_complete(struct rtdx_timepoint timepoint) {
	return !timepoint.queue || timepoint.value == 0;
}

static u64 rtdx_queue_completed_value(struct rtdx_queue* queue) {
	if (!queue || !queue->d3d_fence) { return 0; }
	return queue->d3d_fence->GetCompletedValue();
}

static struct rtdx_submitted_batch* rtdx_queue_create_batch(struct rtdx_context* ctx, struct rtdx_command_buffer* command_buffer, u64 value) {
	if (!command_buffer) { return NULL; }

	struct rtdx_submitted_batch* batch = (struct rtdx_submitted_batch*)calloc(1, sizeof(*batch));
	if (!batch) {
		rtdx_throwf(RT_OUT_OF_HOST_MEMORY, "failed to allocate submitted batch metadata");
		return NULL;
	}

	batch->value = value;
	batch->command_buffer_node = rtdx_command_buffer_active_node(command_buffer);
	rtdx_command_buffer_node_retain(batch->command_buffer_node);
	return batch;
}

static void rtdx_queue_push_batch(struct rtdx_queue* queue, struct rtdx_submitted_batch* batch) {
	if (!batch) { return; }
	if (queue->submitted_tail) {
		queue->submitted_tail->next = batch;
	} else {
		queue->submitted_head = batch;
	}
	queue->submitted_tail = batch;
}

void rtdx_queue_collect(struct rtdx_context* ctx, struct rtdx_queue* queue) {
	if (!queue) { return; }

	u64 completed_value = rtdx_queue_completed_value(queue);
	while (queue->submitted_head && queue->submitted_head->value <= completed_value) {
		struct rtdx_submitted_batch* batch = queue->submitted_head;
		queue->submitted_head = batch->next;
		if (!queue->submitted_head) {
			queue->submitted_tail = NULL;
		}
		if (ctx && ctx->shutting_down) {
			free(batch);
			continue;
		}
		rtdx_command_buffer_node_release(batch->command_buffer_node);
		free(batch);
	}
}

struct rtdx_timepoint rtdx_queue_submit(struct rtdx_context* ctx, struct rtdx_queue* queue, struct rtdx_command_buffer* command_buffer) {
	if (!queue) { return { NULL, 0 }; }

	if (command_buffer && !rtdx_command_buffer_active_node(command_buffer)) {
		rtdx_throwf(RT_IMPROPER_USAGE, "command buffer has not been recorded");
		return { queue, queue->fence_value };
	}

	rtdx_queue_collect(ctx, queue);

	u64 value = queue->fence_value + 1;
	struct rtdx_submitted_batch* batch = rtdx_queue_create_batch(ctx, command_buffer, value);
	if (command_buffer && !batch) {
		return { queue, queue->fence_value };
	}

	for (u32 i = 0; i < queue->wait_count; i++) {
		struct rtdx_timepoint wait = queue->wait_timepoints[i];
		HRESULT wait_result = queue->d3d_queue->Wait(wait.queue->d3d_fence, wait.value);
		if (FAILED(wait_result)) {
			if (batch) {
				rtdx_command_buffer_node_release(batch->command_buffer_node);
				free(batch);
			}
			rtdx_throwf(rtdx_error_from_hresult(wait_result), "ID3D12CommandQueue::Wait failed: 0x%08x", (u32)wait_result);
			return { queue, queue->fence_value };
		}
	}

	if (command_buffer) {
		struct rtdx_command_buffer* node = rtdx_command_buffer_active_node(command_buffer);
		ID3D12CommandList* lists[] = { node->d3d_command_list };
		queue->d3d_queue->ExecuteCommandLists(1, lists);
	}

	HRESULT result = queue->d3d_queue->Signal(queue->d3d_fence, value);
	if (FAILED(result)) {
		if (batch) {
			rtdx_command_buffer_node_release(batch->command_buffer_node);
			free(batch);
		}
		rtdx_throwf(rtdx_error_from_hresult(result), "ID3D12CommandQueue::Signal failed: 0x%08x", (u32)result);
		return { queue, queue->fence_value };
	}

	queue->wait_count = 0;
	queue->fence_value = value;
	rtdx_queue_push_batch(queue, batch);
	return { queue, value };
}

struct rtdx_timepoint rtdx_queue_flush(struct rtdx_context* ctx, struct rtdx_queue* queue) {
	if (!queue) { return { NULL, 0 }; }
	return { queue, queue->fence_value };
}

void rtdx_queue_wait(struct rtdx_context* ctx, struct rtdx_queue* queue, struct rtdx_timepoint timepoint) {
	if (!queue || !timepoint.queue || timepoint.value == 0) { return; }
	if (queue->wait_count >= sizeof(queue->wait_timepoints) / sizeof(queue->wait_timepoints[0])) {
		rtdx_throwf(RT_IMPROPER_USAGE, "queue has too many wait timepoints");
		return;
	}
	queue->wait_timepoints[queue->wait_count++] = timepoint;
}

struct rtdx_timepoint rtdx_queue_signal(struct rtdx_context* ctx, struct rtdx_queue* queue) {
	return rtdx_queue_submit(ctx, queue, NULL);
}

void rtdx_queue_wait_idle(struct rtdx_context* ctx, struct rtdx_queue* queue) {
	if (!queue) { return; }
	rtdx_timepoint_wait(ctx, { queue, queue->fence_value });
	rtdx_queue_collect(ctx, queue);
}

void rtdx_timepoint_wait(struct rtdx_context* ctx, struct rtdx_timepoint timepoint) {
	if (rtdx_timepoint_complete(timepoint)) { return; }
	if (timepoint.queue->d3d_fence->GetCompletedValue() >= timepoint.value) { return; }

	HRESULT result = timepoint.queue->d3d_fence->SetEventOnCompletion(timepoint.value, timepoint.queue->fence_event);
	if (FAILED(result)) {
		rtdx_throwf(rtdx_error_from_hresult(result), "ID3D12Fence::SetEventOnCompletion failed: 0x%08x", (u32)result);
		return;
	}
	WaitForSingleObject(timepoint.queue->fence_event, INFINITE);
	rtdx_queue_collect(ctx, timepoint.queue);
}

bool rtdx_timepoint_reached(struct rtdx_context* ctx, struct rtdx_timepoint timepoint) {
	if (rtdx_timepoint_complete(timepoint)) { return true; }
	return timepoint.queue->d3d_fence->GetCompletedValue() >= timepoint.value;
}


