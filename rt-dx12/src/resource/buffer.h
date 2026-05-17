#ifndef RTDX_BUFFER_H
#define RTDX_BUFFER_H

#include "config.h"
#include "resource.h"

#include <d3d12.h>

/*===============================================================================================*/
/*                                                                                               */
/*===============================================================================================*/

RTDX_EXTERN_C_ENTER
RTDX_API rt_buffer rtBufferCreate(void);
RTDX_API void rtBufferDestroy(rt_buffer buffer);
RTDX_API rt_timepoint rtBufferData(rt_buffer buffer, enum rt_buffer_mode mode, enum rt_buffer_usage usage, u64 size, const void* data);
RTDX_API rt_timepoint rtBufferSubdata(rt_buffer buffer, u64 offset, u64 size, const void* data);
RTDX_API void rtBufferRead(rt_buffer buffer, u64 offset, u64 size, void* data);
RTDX_API void* rtBufferMap(rt_buffer buffer, u64 offset, u64 size);
RTDX_API void rtBufferUnmap(rt_buffer buffer);
RTDX_EXTERN_C_EXIT

/*===============================================================================================*/
/*                                                                                               */
/*===============================================================================================*/

struct rtdx_buffer {
	struct rtdx_resource_base base;
	struct rtdx_buffer_storage* storage;
	struct rtdx_buffer_storage* reusable_storage;
	enum rt_buffer_mode mode;
	enum rt_buffer_usage usage;
};
RTDX_DECLARE_NEW_RESOURCE(buffer)

struct rtdx_buffer_storage {
	struct rtdx_context* ctx;
	struct rtdx_buffer_storage* next;

	ID3D12Resource* d3d_resource;
	D3D12_VERTEX_BUFFER_VIEW vertex_view;
	D3D12_RESOURCE_STATES state;

	void* shadow_data;

	u64 size;
	enum rt_buffer_mode mode;
	enum rt_buffer_usage usage;
	u32 ref_count;
};

struct rtdx_timepoint rtdx_buffer_data(struct rtdx_context* ctx, struct rtdx_buffer* buffer, enum rt_buffer_mode mode, enum rt_buffer_usage usage, u64 size, const void* data);
struct rtdx_timepoint rtdx_buffer_subdata(struct rtdx_context* ctx, struct rtdx_buffer* buffer, u64 offset, u64 size, const void* data);
void rtdx_buffer_read(struct rtdx_context* ctx, struct rtdx_buffer* buffer, u64 offset, u64 size, void* data);
void* rtdx_buffer_map(struct rtdx_context* ctx, struct rtdx_buffer* buffer, u64 offset, u64 size);
void rtdx_buffer_unmap(struct rtdx_context* ctx, struct rtdx_buffer* buffer);
void rtdx_buffer_storage_retain(struct rtdx_buffer_storage* storage);
void rtdx_buffer_storage_release(struct rtdx_buffer_storage* storage);

#endif /* RTDX_BUFFER_H */


