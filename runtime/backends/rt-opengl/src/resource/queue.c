#include "resource/queue.h"

#include "context.h"
#include "execution.h"
#include "resource/command_buffer.h"
#include "resource/framebuffer.h"
#include "resource/swapchain.h"
#include "resource/texture.h"

/*===============================================================================================*/
/*                                                                                               */
/*===============================================================================================*/

rt_queue rtQueueQuery(enum rt_queue_capability capability) {
	return rtgl_queue_to_handle(rtgl_queue_query(rtgl_get_current_context(), capability));
}

void rtQueueWait(rt_queue queue, rt_timepoint timepoint) {
	rtTimepointWait(timepoint);
}

rt_timepoint rtQueueSubmit(rt_queue queue, rt_command_buffer command_buffer) {
	struct rtgl_queue* internal = rtgl_queue_from_handle(queue);
	return rtgl_timepoint_to_public(rtgl_command_buffer_submit(internal->base.ctx, internal, rtgl_command_buffer_from_handle(command_buffer)));
}

rt_timepoint rtQueueFlush(rt_queue queue) {
	struct rtgl_queue* internal = rtgl_queue_from_handle(queue);
	rtgl_timepoint_wait(internal->base.ctx, (struct rtgl_timepoint){ internal, internal->submitted_value });
	return rtgl_timepoint_to_public((struct rtgl_timepoint){ internal, internal->submitted_value });
}

void rtTimepointWait(rt_timepoint timepoint) {
	rtgl_timepoint_wait(rtgl_get_current_context(), (struct rtgl_timepoint){ rtgl_queue_from_handle(timepoint.queue), timepoint.value });
}

bool rtTimepointReached(rt_timepoint timepoint) {
	return rtgl_timepoint_reached(rtgl_get_current_context(), (struct rtgl_timepoint){ rtgl_queue_from_handle(timepoint.queue), timepoint.value });
}

/*===============================================================================================*/
/*                                                                                               */
/*===============================================================================================*/

RTGL_DEFINE_RESOURCE_PRIVATE(queue)

void rtgl_queue_init(struct rtgl_context* ctx, struct rtgl_queue* queue) {
	rtgl_init_resource_base(ctx, RTGL_RESOURCE_BASE(queue), RTGL_RESOURCE_QUEUE);
	queue->completion_condition = rt_condition_create();
	if (!queue->completion_condition) {
		rtgl_throwf(RT_OUT_OF_HOST_MEMORY, "failed to create OpenGL queue completion condition");
		return;
	}
	queue->capability = RT_QUEUE_GRAPHICS;
}

void rtgl_queue_finish(struct rtgl_queue* queue) {
	if (queue->completion_condition) {
		rt_condition_destroy(queue->completion_condition);
		queue->completion_condition = NULL;
	}
	rtgl_finish_resource_base(RTGL_RESOURCE_BASE(queue));
}

struct rtgl_queue* rtgl_queue_query(struct rtgl_context* ctx, enum rt_queue_capability capability) {
	return rtgl_context_graphics_queue(ctx);
}

struct rtgl_timepoint rtgl_queue_signal(struct rtgl_queue* queue) {
	u64 value = ++queue->submitted_value;
	return (struct rtgl_timepoint){ queue, value };
}

void rtgl_queue_complete(struct rtgl_queue* queue, u64 value) {
	struct rtgl_context* ctx = queue->base.ctx;
	rtgl_execution_lock(ctx);
	if (queue->completed_value < value) {
		queue->completed_value = value;
		if (queue->completion_condition) {
			rt_condition_broadcast(queue->completion_condition);
		}
	}
	rtgl_execution_unlock(ctx);
}

struct rtgl_timepoint rtgl_queue_present(struct rtgl_queue* queue, struct rtgl_swapchain* swapchain, struct rtgl_framebuffer* framebuffer) {
	return rtgl_execution_present(queue->base.ctx, queue, swapchain, framebuffer);
}

void rtgl_timepoint_wait(struct rtgl_context* ctx, struct rtgl_timepoint timepoint) {
	if (!timepoint.queue || timepoint.value == 0 || rtgl_timepoint_reached(ctx, timepoint)) {
		return;
	}
	rtgl_execution_lock(ctx);
	while (timepoint.queue->completed_value < timepoint.value) {
		rt_condition_wait(timepoint.queue->completion_condition, ctx->execution.work_lock);
	}
	rtgl_execution_unlock(ctx);
}

bool rtgl_timepoint_reached(struct rtgl_context* ctx, struct rtgl_timepoint timepoint) {
	bool reached;

	if (!timepoint.queue || timepoint.value == 0) {
		return true;
	}
	rtgl_execution_lock(ctx);
	reached = timepoint.queue->completed_value >= timepoint.value;
	rtgl_execution_unlock(ctx);
	return reached;
}
