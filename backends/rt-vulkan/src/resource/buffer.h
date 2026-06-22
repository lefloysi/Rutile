#ifndef RTVK_BUFFER_H
#define RTVK_BUFFER_H

#include "config.h"
#include "resource.h"

#include <vulkan/vulkan.h>
#include <vk_mem_alloc.h>

/*===============================================================================================*/
/*                                                                                               */
/*===============================================================================================*/

RTVK_API rt_buffer rtBufferCreate(void);
RTVK_API void rtBufferDestroy(rt_buffer buffer);
RTVK_API rt_timepoint rtBufferData(rt_buffer buffer, enum rt_buffer_mode mode, enum rt_buffer_usage usage, u64 size, const void* data);
RTVK_API rt_timepoint rtBufferSubdata(rt_buffer buffer, u64 offset, u64 size, const void* data);
RTVK_API void rtBufferRead(rt_buffer buffer, u64 offset, u64 size, void* data);

/*===============================================================================================*/
/*                                                                                               */
/*===============================================================================================*/

struct rtvk_buffer {
	struct rtvk_resource_base base;
	struct rtvk_buffer* active;
	struct rtvk_buffer* next;

	VkBuffer vk_buffer;
	VmaAllocation vma_allocation;

	u64 size;
	enum rt_buffer_mode mode;
	enum rt_buffer_usage usage;
};
RTVK_DECLARE_NEW_RESOURCE(buffer)

void rtvk_buffer_node_retain(struct rtvk_buffer* buffer);
void rtvk_buffer_node_release(struct rtvk_buffer* buffer);

struct rtvk_timepoint rtvk_buffer_data(struct rtvk_context* ctx, struct rtvk_buffer* buffer, enum rt_buffer_mode mode, enum rt_buffer_usage usage, u64 size, const void* data);
struct rtvk_timepoint rtvk_buffer_subdata(struct rtvk_context* ctx, struct rtvk_buffer* buffer, u64 offset, u64 size, const void* data);

enum rt_buffer_usage rtvk_default_buffer_usage(void);
bool rtvk_buffer_is_staging(enum rt_buffer_usage usage);
bool rtvk_buffer_uses_host_storage(enum rt_buffer_mode mode, enum rt_buffer_usage usage);
bool rtvk_buffer_needs_graphics_upload_queue(enum rt_buffer_usage usage);
struct rtvk_queue* rtvk_buffer_upload_queue(struct rtvk_context* ctx, enum rt_buffer_usage usage);
VkBufferUsageFlags rtvk_buffer_vk_usage(enum rt_buffer_mode mode, enum rt_buffer_usage usage);
VkAccessFlags rtvk_buffer_vk_access(enum rt_buffer_usage usage);
VkPipelineStageFlags rtvk_buffer_vk_stage(enum rt_buffer_usage usage);
VkPipelineStageFlags rtvk_buffer_vk_stage_for_queue(enum rt_buffer_usage usage, struct rtvk_queue* queue);

struct rtvk_buffer* rtvk_buffer_node_create(struct rtvk_context* ctx, u64 size, enum rt_buffer_mode mode, enum rt_buffer_usage usage);
void rtvk_buffer_recycle_node(struct rtvk_buffer* buffer, struct rtvk_buffer* node);
struct rtvk_buffer* rtvk_buffer_take_reusable_node(struct rtvk_buffer* buffer, u64 size, enum rt_buffer_mode mode, enum rt_buffer_usage usage);

void rtvk_buffer_write_host(struct rtvk_context* ctx, struct rtvk_buffer* buffer, u64 offset, u64 size, const void* data);
void rtvk_buffer_upload_command(struct rtvk_context* ctx, struct rtvk_queue* queue);
void rtvk_buffer_upload_staging(struct rtvk_context* ctx, struct rtvk_queue* queue, u64 size);
struct rtvk_timepoint rtvk_buffer_upload_static(struct rtvk_context* ctx, struct rtvk_queue* queue, struct rtvk_buffer* buffer, u64 offset, u64 size, const void* data);

void rtvk_buffer_read(struct rtvk_context* ctx, struct rtvk_buffer* buffer, u64 offset, u64 size, void* data);

#endif


