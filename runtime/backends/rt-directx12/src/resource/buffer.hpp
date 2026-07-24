#pragma once

#include "config.hpp"
#include "resource.hpp"

#include <d3d12.h>

/*===============================================================================================*/
/*                                                                                               */
/*===============================================================================================*/

RTDX_API rt_buffer rtBufferCreate();
RTDX_API void rtBufferDestroy(rt_buffer buffer);
RTDX_API rt_timepoint rtBufferData(rt_buffer buffer, rt_buffer_mode mode, rt_buffer_usage usage, u64 size, const void* data);
RTDX_API rt_timepoint rtBufferSubdata(rt_buffer buffer, u64 offset, u64 size, const void* data);
RTDX_API void rtBufferRead(rt_buffer buffer, u64 offset, u64 size, void* data);

/*===============================================================================================*/
/*                                                                                               */
/*===============================================================================================*/

struct rtdx_buffer_storage;

struct rtdx_buffer {
	rtdx_resource_base base;
	rtdx_buffer_storage* storage;
	rtdx_buffer_storage* reusable_storage;
	rt_buffer_mode mode;
	rt_buffer_usage usage;
};
RTDX_DECLARE_NEW_RESOURCE(buffer)

struct rtdx_buffer_storage {
	rtdx_context* ctx;
	rtdx_buffer_storage* next;

	ID3D12Resource* d3d_resource;
	D3D12_VERTEX_BUFFER_VIEW vertex_view;
	D3D12_RESOURCE_STATES state;

	void* shadow_data;

	u64 size;
	rt_buffer_mode mode;
	rt_buffer_usage usage;
	u32 ref_count;
};

rtdx_timepoint rtdx_buffer_data(rtdx_context* ctx, rtdx_buffer* buffer, rt_buffer_mode mode, rt_buffer_usage usage, u64 size, const void* data);
rtdx_timepoint rtdx_buffer_subdata(rtdx_context* ctx, rtdx_buffer* buffer, u64 offset, u64 size, const void* data);
void rtdx_buffer_read(rtdx_context* ctx, rtdx_buffer* buffer, u64 offset, u64 size, void* data);
void rtdx_buffer_storage_retain(rtdx_buffer_storage* storage);
void rtdx_buffer_storage_release(rtdx_buffer_storage* storage);
