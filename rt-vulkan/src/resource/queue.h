#ifndef RTVK_QUEUE_H
#define RTVK_QUEUE_H

#include "config.h"
#include "resource.h"
#include "command_buffer.h"

#include <vk_mem_alloc.h>

/*===============================================================================================*/
/*                                                                                               */
/*===============================================================================================*/

RTVK_API rt_queue rtQueueQuery(enum rt_queue_capability capability);
RTVK_API void rtQueueWait(rt_queue queue, rt_timepoint timepoint);
RTVK_API rt_timepoint rtQueueSubmit(rt_queue queue, rt_command_buffer command_buffer);
RTVK_API rt_timepoint rtQueueFlush(rt_queue queue);
RTVK_API void rtTimepointWait(rt_timepoint timepoint);
RTVK_API bool rtTimepointReached(rt_timepoint timepoint);

/*===============================================================================================*/
/*                                                                                               */
/*===============================================================================================*/

struct rtvk_submitted_batch {
	struct rtvk_command_buffer* command_buffer_node;
	struct rtvk_submitted_batch* next;
	u64 value;
};

struct rtvk_queue {
	struct rtvk_resource_base base;

	VkQueue vk_queue;
	VkSemaphore vk_timeline;
	VkCommandPool copy_command_pool;
	VkCommandBuffer copy_command_buffer;
	struct rtvk_timepoint copy_command_timepoint;
	VkCommandPool upload_command_pool;
	VkCommandBuffer upload_command_buffer;
	VkBuffer upload_staging_buffer;
	VmaAllocation upload_staging_allocation;
	u64 upload_staging_size;
	struct rtvk_timepoint upload_command_timepoint;

	struct rtvk_submitted_batch* submitted_head;
	struct rtvk_submitted_batch* submitted_tail;
	struct rtvk_submitted_batch* pending_head;
	struct rtvk_submitted_batch* pending_tail;
	struct rtvk_timepoint wait_timepoints[8];
	VkSemaphore binary_waits[8];

	u64 timeline_value;
	u64 submitted_value;
	enum rt_queue_capability capability;
	u32 family_index;
	u32 queue_index;
	u32 wait_count;
	u32 binary_wait_count;
};

static inline struct rtvk_queue* rtvk_queue_from_handle(rt_queue queue) { return (struct rtvk_queue*)queue; }
static inline rt_queue rtvk_queue_to_handle(struct rtvk_queue* queue) { return (rt_queue)queue; }

struct rtvk_queue* rtvk_queue_create(struct rtvk_context* ctx, VkQueue vk_queue, enum rt_queue_capability capability, u32 family_index, u32 queue_index);
void rtvk_queue_destroy(struct rtvk_context* ctx, struct rtvk_queue* queue);
bool rtvk_queue_init(struct rtvk_context* ctx, struct rtvk_queue* queue, VkQueue vk_queue, enum rt_queue_capability capability, u32 family_index, u32 queue_index);
void rtvk_queue_finish(struct rtvk_context* ctx, struct rtvk_queue* queue);
struct rtvk_queue* rtvk_queue_query(struct rtvk_context* ctx, enum rt_queue_capability capability);
struct rtvk_queue* rtvk_queue_query_present(struct rtvk_context* ctx, VkSurfaceKHR surface);
VkPipelineStageFlags rtvk_queue_wait_stage(struct rtvk_queue* queue);
void rtvk_queue_wait(struct rtvk_context* ctx, struct rtvk_queue* queue, struct rtvk_timepoint timepoint);
struct rtvk_timepoint rtvk_queue_submit(struct rtvk_context* ctx, struct rtvk_queue* queue, struct rtvk_command_buffer* command_buffer);
struct rtvk_timepoint rtvk_queue_flush(struct rtvk_context* ctx, struct rtvk_queue* queue);
struct rtvk_timepoint rtvk_queue_signal(struct rtvk_context* ctx, struct rtvk_queue* queue);
struct rtvk_timepoint rtvk_queue_wait_binary(struct rtvk_context* ctx, struct rtvk_queue* queue, VkSemaphore semaphore);
struct rtvk_timepoint rtvk_queue_signal_binary_after_timepoint(struct rtvk_queue* queue, u64 wait_value, VkSemaphore semaphore);
void rtvk_queue_collect(struct rtvk_context* ctx, struct rtvk_queue* queue);
void rtvk_timepoint_wait(struct rtvk_context* ctx, struct rtvk_timepoint timepoint);
bool rtvk_timepoint_reached(struct rtvk_context* ctx, struct rtvk_timepoint timepoint);

#endif


