#ifndef RTDX_QUEUE_H
#define RTDX_QUEUE_H

#include "config.h"
#include "resource.h"

#include <d3d12.h>
#include <windows.h>

/*===============================================================================================*/
/*                                                                                               */
/*===============================================================================================*/

RTDX_EXTERN_C_ENTER
RTDX_API rt_queue rtQueueQuery(enum rt_queue_capability capability);
RTDX_API void rtQueueWait(rt_queue queue, rt_timepoint timepoint);
RTDX_API rt_timepoint rtQueueSubmit(rt_queue queue, rt_command_buffer command_buffer);
RTDX_API rt_timepoint rtQueueFlush(rt_queue queue);
RTDX_API void rtTimepointWait(rt_timepoint timepoint);
RTDX_API bool rtTimepointReached(rt_timepoint timepoint);
RTDX_EXTERN_C_EXIT

/*===============================================================================================*/
/*                                                                                               */
/*===============================================================================================*/

struct rtdx_command_buffer;

struct rtdx_submitted_batch {
	struct rtdx_command_buffer* command_buffer_node;
	struct rtdx_submitted_batch* next;
	u64 value;
};

struct rtdx_queue {
	struct rtdx_resource_base base;

	ID3D12CommandQueue* d3d_queue;
	ID3D12Fence* d3d_fence;
	ID3D12CommandAllocator* upload_allocator;
	ID3D12GraphicsCommandList* upload_command_list;
	ID3D12Resource* upload_buffer;
	HANDLE fence_event;

	struct rtdx_timepoint wait_timepoints[8];
	struct rtdx_submitted_batch* submitted_head;
	struct rtdx_submitted_batch* submitted_tail;

	u64 fence_value;
	u64 upload_fence_value;
	u64 upload_buffer_size;
	enum rt_queue_capability capability;
	u32 wait_count;
};

static inline struct rtdx_queue* rtdx_queue_from_handle(rt_queue queue) { return (struct rtdx_queue*)queue; }
static inline rt_queue rtdx_queue_to_handle(struct rtdx_queue* queue) { return (rt_queue)queue; }

struct rtdx_queue* rtdx_queue_create(struct rtdx_context* ctx, enum rt_queue_capability capability);
void rtdx_queue_destroy(struct rtdx_context* ctx, struct rtdx_queue* queue);
bool rtdx_queue_init(struct rtdx_context* ctx, struct rtdx_queue* queue, enum rt_queue_capability capability);
void rtdx_queue_finish(struct rtdx_context* ctx, struct rtdx_queue* queue);
struct rtdx_queue* rtdx_queue_query(struct rtdx_context* ctx, enum rt_queue_capability capability);
void rtdx_queue_wait(struct rtdx_context* ctx, struct rtdx_queue* queue, struct rtdx_timepoint timepoint);
struct rtdx_timepoint rtdx_queue_submit(struct rtdx_context* ctx, struct rtdx_queue* queue, struct rtdx_command_buffer* command_buffer);
struct rtdx_timepoint rtdx_queue_flush(struct rtdx_context* ctx, struct rtdx_queue* queue);
struct rtdx_timepoint rtdx_queue_signal(struct rtdx_context* ctx, struct rtdx_queue* queue);
void rtdx_queue_wait_idle(struct rtdx_context* ctx, struct rtdx_queue* queue);
void rtdx_queue_collect(struct rtdx_context* ctx, struct rtdx_queue* queue);
void rtdx_timepoint_wait(struct rtdx_context* ctx, struct rtdx_timepoint timepoint);
bool rtdx_timepoint_reached(struct rtdx_context* ctx, struct rtdx_timepoint timepoint);

#endif /* RTDX_QUEUE_H */
