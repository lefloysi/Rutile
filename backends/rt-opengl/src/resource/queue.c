#include "resource/queue.h"

#include "context.h"
#include "error.h"
#include "execution/execution.h"
#include "resource/command_buffer.h"
#include "resource/framebuffer.h"
#include "resource/swapchain.h"
#include "resource/texture.h"

#include <stdlib.h>

#define RTGL_TIMEPOINT_QUEUE_SHIFT 56
#define RTGL_TIMEPOINT_VALUE_MASK UINT64_C(0x00ffffffffffffff)

/*===============================================================================================*/
/*                                                                                               */
/*===============================================================================================*/
rt_queue rtQueueCreate(enum rt_queue_capability capability) {
	rtgl_begin_errorable_operation();
	return rtgl_queue_to_handle(rtgl_queue_create_virtual(rtgl_get_current_context(), capability));
}

void rtQueueDestroy(rt_queue queue) {
	struct rtgl_queue* internal = rtgl_queue_from_handle(queue);
	if (!internal) {
		return;
	}
	rtgl_mutex_lock(&internal->submit_lock);
	rtgl_execution_lock(internal->base.ctx);
	internal->active = false;
	rtgl_execution_unlock(internal->base.ctx);
	rtgl_mutex_unlock(&internal->submit_lock);
}

rt_timepoint rtQueueSubmit(rt_queue queue, rt_command_buffer command_buffer) {
	rtgl_begin_errorable_operation();
	struct rtgl_queue* internal = rtgl_queue_from_handle(queue);
	if (!internal) {
		return (rt_timepoint){ 0 };
	}
	rtgl_mutex_lock(&internal->submit_lock);
	rtgl_execution_lock(internal->base.ctx);
	const bool active = internal->active;
	rtgl_execution_unlock(internal->base.ctx);
	if (!active) {
		rtgl_mutex_unlock(&internal->submit_lock);
		return (rt_timepoint){ 0 };
	}
	rt_timepoint submitted = rtgl_command_buffer_submit(internal->base.ctx, internal, rtgl_command_buffer_from_handle(command_buffer));
	rtgl_mutex_unlock(&internal->submit_lock);
	return submitted;
}

void rtQueueWait(rt_queue queue, rt_timepoint timepoint) {
	rtgl_begin_errorable_operation();
	struct rtgl_queue* internal = rtgl_queue_from_handle(queue);
	if (!internal) {
		return;
	}
	rtgl_mutex_lock(&internal->submit_lock);
	if (internal->wait_count == internal->wait_capacity) {
		usize capacity = internal->wait_capacity ? internal->wait_capacity * 2 : 8;
		rt_timepoint* wait_timepoints = (rt_timepoint*)realloc(internal->wait_timepoints, capacity * sizeof(*wait_timepoints));
		if (!wait_timepoints) {
			rtgl_mutex_unlock(&internal->submit_lock);
			rtgl_throwf(RT_OUT_OF_HOST_MEMORY, "failed to allocate OpenGL queue wait timepoints");
			return;
		}
		internal->wait_timepoints = wait_timepoints;
		internal->wait_capacity = capacity;
	}
	internal->wait_timepoints[internal->wait_count++] = timepoint;
	rtgl_mutex_unlock(&internal->submit_lock);
}

rt_timepoint rtQueueFlush(rt_queue queue) {
	struct rtgl_queue* internal = rtgl_queue_from_handle(queue);
	if (!internal) {
		return (rt_timepoint){ 0 };
	}
	rtgl_mutex_lock(&internal->submit_lock);
	rtgl_execution_lock(internal->base.ctx);
	rt_timepoint timepoint = { ((u64)internal->identifier << RTGL_TIMEPOINT_QUEUE_SHIFT) | internal->submitted_value };
	rtgl_execution_unlock(internal->base.ctx);
	rtgl_mutex_unlock(&internal->submit_lock);
	rtgl_timepoint_wait(internal->base.ctx, timepoint);
	return timepoint;
}

void rtTimepointWait(rt_timepoint timepoint) {
	rtgl_begin_errorable_operation();
	rtgl_timepoint_wait(rtgl_get_current_context(), timepoint);
}

bool rtTimepointReached(rt_timepoint timepoint) {
	return rtgl_timepoint_reached(rtgl_get_current_context(), timepoint);
}

/*===============================================================================================*/
/*                                                                                               */
/*===============================================================================================*/
RTGL_DEFINE_RESOURCE_PRIVATE(queue)

void rtgl_queue_init(struct rtgl_context* ctx, struct rtgl_queue* queue) {
	rtgl_init_resource_base(ctx, RTGL_RESOURCE_BASE(queue), RTGL_RESOURCE_QUEUE);
	rtgl_condition_init(&queue->completion_condition);
	rtgl_mutex_init(&queue->submit_lock);
	queue->capability = RT_QUEUE_GRAPHICS;
	queue->active = true;
}

void rtgl_queue_finish(struct rtgl_queue* queue) {
	rtgl_condition_finish(&queue->completion_condition);
	rtgl_mutex_finish(&queue->submit_lock);
	free(queue->wait_timepoints);
	queue->wait_timepoints = NULL;
	queue->wait_count = 0;
	queue->wait_capacity = 0;
	rtgl_finish_resource_base(RTGL_RESOURCE_BASE(queue));
}

struct rtgl_queue* rtgl_queue_create_virtual(struct rtgl_context* ctx, enum rt_queue_capability capability) {
	if (!ctx) {
		return NULL;
	}
	rtgl_execution_lock(ctx);
	if (ctx->queue_count == 256) {
		rtgl_execution_unlock(ctx);
		return NULL;
	}
	if (ctx->queue_count == ctx->queue_capacity) {
		u32 capacity = ctx->queue_capacity ? ctx->queue_capacity * 2 : 4;
		struct rtgl_queue** queues = (struct rtgl_queue**)realloc(ctx->queues, sizeof(*queues) * capacity);
		RTGL_CHECK_ALLOC(queues, sizeof(*queues) * capacity, "OpenGL virtual queue registry");
		if (!queues) {
			rtgl_execution_unlock(ctx);
			return NULL;
		}
		ctx->queues = queues;
		ctx->queue_capacity = capacity;
	}
	struct rtgl_queue* queue = rtgl_queue_create(ctx);
	if (queue) {
		queue->identifier = (u08)ctx->queue_count;
		queue->capability = capability;
		ctx->queues[ctx->queue_count++] = queue;
	}
	rtgl_execution_unlock(ctx);
	return queue;
}

struct rtgl_queue* rtgl_queue_from_timepoint(struct rtgl_context* ctx, rt_timepoint timepoint) {
	if (!ctx || timepoint.value == 0) {
		return NULL;
	}
	const u64 identifier = timepoint.value >> RTGL_TIMEPOINT_QUEUE_SHIFT;
	rtgl_execution_lock(ctx);
	struct rtgl_queue* queue = identifier < ctx->queue_count ? ctx->queues[identifier] : NULL;
	rtgl_execution_unlock(ctx);
	return queue;
}

rt_timepoint rtgl_queue_signal(struct rtgl_queue* queue) {
	rtgl_execution_lock(queue->base.ctx);
	const u64 value = ++queue->submitted_value;
	rtgl_execution_unlock(queue->base.ctx);
	return (rt_timepoint){ ((u64)queue->identifier << RTGL_TIMEPOINT_QUEUE_SHIFT) | value };
}

u64 rtgl_timepoint_queue_value(rt_timepoint timepoint) {
	return timepoint.value & RTGL_TIMEPOINT_VALUE_MASK;
}

void rtgl_queue_complete(struct rtgl_queue* queue, u64 value) {
	struct rtgl_context* ctx = queue->base.ctx;
	rtgl_execution_lock(ctx);
	if (queue->completed_value < value) {
		queue->completed_value = value;
		rtgl_condition_broadcast(&queue->completion_condition);
		rt_event_signal(ctx->execution.work_event);
	}
	rtgl_execution_unlock(ctx);
}

rt_timepoint rtgl_queue_present(struct rtgl_queue* queue, struct rtgl_swapchain* swapchain, struct rtgl_framebuffer* framebuffer) {
	return rtgl_execution_present(queue->base.ctx, queue, swapchain, framebuffer);
}

void rtgl_timepoint_wait(struct rtgl_context* ctx, rt_timepoint timepoint) {
	struct rtgl_queue* queue = rtgl_queue_from_timepoint(ctx, timepoint);
	const u64 value = rtgl_timepoint_queue_value(timepoint);
	if (!queue || value == 0) {
		return;
	}
	rtgl_execution_lock(ctx);
	while (queue->completed_value < value) {
		rtgl_condition_wait(&queue->completion_condition, &ctx->execution.work_lock);
	}
	rtgl_execution_unlock(ctx);
}

bool rtgl_timepoint_reached(struct rtgl_context* ctx, rt_timepoint timepoint) {
	struct rtgl_queue* queue = rtgl_queue_from_timepoint(ctx, timepoint);
	const u64 value = rtgl_timepoint_queue_value(timepoint);
	if (!queue || value == 0) {
		return true;
	}
	rtgl_execution_lock(ctx);
	const bool reached = queue->completed_value >= value;
	rtgl_execution_unlock(ctx);
	return reached;
}
