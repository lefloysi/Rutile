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
RTVK_API void* rtBufferMap(rt_buffer buffer, u64 offset, u64 size);
RTVK_API void rtBufferUnmap(rt_buffer buffer);

/*===============================================================================================*/
/*                                                                                               */
/*===============================================================================================*/

struct rtvk_buffer {
	struct rtvk_resource_base base;
	struct rtvk_buffer* active;
	struct rtvk_buffer* next;

	VkBuffer vk_buffer;
	VmaAllocation vma_allocation;

	void* shadow_data;

	u64 size;
	enum rt_buffer_mode mode;
	enum rt_buffer_usage usage;
	atomic_u32 ref_count;
};
RTVK_DECLARE_NEW_RESOURCE(buffer)

struct rtvk_timepoint rtvk_buffer_data(struct rtvk_context* ctx, struct rtvk_buffer* buffer, enum rt_buffer_mode mode, enum rt_buffer_usage usage, u64 size, const void* data);
struct rtvk_timepoint rtvk_buffer_subdata(struct rtvk_context* ctx, struct rtvk_buffer* buffer, u64 offset, u64 size, const void* data);
void rtvk_buffer_read(struct rtvk_context* ctx, struct rtvk_buffer* buffer, u64 offset, u64 size, void* data);
void* rtvk_buffer_map(struct rtvk_context* ctx, struct rtvk_buffer* buffer, u64 offset, u64 size);
void rtvk_buffer_unmap(struct rtvk_context* ctx, struct rtvk_buffer* buffer);
void rtvk_buffer_node_retain(struct rtvk_buffer* buffer);
void rtvk_buffer_node_release(struct rtvk_buffer* buffer);

#endif


