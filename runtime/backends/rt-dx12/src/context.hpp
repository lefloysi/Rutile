#pragma once

#include "types.hpp"

#include <d3d12.h>
#include <dxgi1_6.h>

struct rtdx_context_flags {
	unsigned presentation : 1;
};

struct rtdx_queue;

struct rtdx_context {
	IDXGIFactory6* dxgi_factory;
	IDXGIAdapter1* dxgi_adapter;
	ID3D12Device* d3d_device;
	rtdx_queue** queues;
	u32 queue_count;
	rtdx_context_flags flags;
	bool allow_tearing;
	bool shutting_down;
};

extern rtdx_context* current_context;

rtdx_context* rtdx_get_current_context();
rtdx_context* rtdx_create_context(rtdx_context_flags flags);
void rtdx_context_init(rtdx_context* ctx);
void rtdx_context_finish(rtdx_context* ctx);
void rtdx_context_destroy(rtdx_context* ctx);

template <typename T>
inline void rtdx_release(T** object) {
	if (*object) {
		(*object)->Release();
		*object = nullptr;
	}
}
