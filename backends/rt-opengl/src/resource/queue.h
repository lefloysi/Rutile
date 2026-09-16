#ifndef RTGL_QUEUE_H
#define RTGL_QUEUE_H

#include "config.h"
#include "resource.h"
#include "system/sync.h"

RTGL_EXTERN_C_ENTER

struct rtgl_swapchain;
struct rtgl_framebuffer;

/*===============================================================================================*/
/*                                                                                               */
/*===============================================================================================*/

RTGL_API rt_queue rtQueueCreate(enum rt_queue_capability capability);
RTGL_API void rtQueueDestroy(rt_queue queue);
RTGL_API rt_timepoint rtQueueSubmit(rt_queue queue, rt_command_buffer command_buffer);
RTGL_API void rtQueueWait(rt_queue queue, rt_timepoint timepoint);
RTGL_API rt_timepoint rtQueueFlush(rt_queue queue);
RTGL_API void rtTimepointWait(rt_timepoint timepoint);
RTGL_API bool rtTimepointReached(rt_timepoint timepoint);

/*===============================================================================================*/
/*                                                                                               */
/*===============================================================================================*/

struct rtgl_queue {
	struct rtgl_resource_base base;
	struct rtgl_condition completion_condition;
	struct rtgl_mutex submit_lock;
	rt_timepoint* wait_timepoints;
	usize wait_count;
	usize wait_capacity;
	u64 submitted_value;
	u64 completed_value;
	u08 identifier;
	bool active;
	enum rt_queue_capability capability;
};
RTGL_DECLARE_NEW_RESOURCE(queue)

struct rtgl_queue* rtgl_queue_create_virtual(struct rtgl_context* ctx, enum rt_queue_capability capability);
struct rtgl_queue* rtgl_queue_from_timepoint(struct rtgl_context* ctx, rt_timepoint timepoint);
rt_timepoint rtgl_queue_signal(struct rtgl_queue* queue);
u64 rtgl_timepoint_queue_value(rt_timepoint timepoint);
void rtgl_queue_complete(struct rtgl_queue* queue, u64 value);
rt_timepoint rtgl_queue_present(struct rtgl_queue* queue, struct rtgl_swapchain* swapchain, struct rtgl_framebuffer* framebuffer);
void rtgl_timepoint_wait(struct rtgl_context* ctx, rt_timepoint timepoint);
bool rtgl_timepoint_reached(struct rtgl_context* ctx, rt_timepoint timepoint);

RTGL_EXTERN_C_EXIT
#endif /* RTGL_QUEUE_H */
