#pragma once

#include "config.hpp"
#include "resource/framebuffer.hpp"
#include "resource/queue.hpp"
#include "resource/texture.hpp"
#include "sync.h"

#include <dxgi1_6.h>

RTDX_API rt_swapchain rtSwapchainCreate();
RTDX_API void rtSwapchainDestroy(rt_swapchain swapchain);
RTDX_API void rtSwapchainResize(rt_swapchain swapchain, u32 width, u32 height);
RTDX_API void rtSwapchainSetVsync(rt_swapchain swapchain, bool enabled);
RTDX_API rt_swapchain_acquire_result rtSwapchainAcquire(rt_swapchain swapchain);
RTDX_API void rtSwapchainPresent(rt_swapchain swapchain, rt_timepoint rendered);

struct rtdx_swapchain_frame {
	rtdx_queue* present_queue;

	ID3D12CommandAllocator* present_allocator;
	ID3D12GraphicsCommandList* present_command_list;

	u64 present_value;
	bool has_present_timepoint;
};

struct rtdx_swapchain {
	rtdx_resource_base base;

	IDXGISwapChain3* dxgi_swapchain;
	ID3D12DescriptorHeap* rtv_heap;
	void* frame_latency_object;

	rtdx_texture* textures[RTDX_MAX_FRAMES_IN_FLIGHT];
	rtdx_texture_view* texture_views[RTDX_MAX_FRAMES_IN_FLIGHT];
	rtdx_framebuffer* framebuffers[RTDX_MAX_FRAMES_IN_FLIGHT];
	rtdx_swapchain_frame frames[RTDX_MAX_FRAMES_IN_FLIGHT];

	UINT rtv_descriptor_size;
	DXGI_FORMAT dxgi_format;
	u32 width;
	u32 height;
	u32 current_image_index;
	bool frame_acquired;
	bool vsync;
	rt_mutex* frame_lock;
	rt_condition* frame_condition;
};

inline rtdx_swapchain* rtdx_swapchain_from_handle(rt_swapchain swapchain) { return reinterpret_cast<rtdx_swapchain*>(swapchain); }
inline rt_swapchain rtdx_swapchain_to_handle(rtdx_swapchain* swapchain) { return reinterpret_cast<rt_swapchain>(swapchain); }

rtdx_swapchain* rtdx_swapchain_create(rtdx_context* ctx);
void rtdx_swapchain_destroy(rtdx_context* ctx, rtdx_swapchain* swapchain);
void rtdx_swapchain_init(rtdx_context* ctx, rtdx_swapchain* swapchain);
void rtdx_swapchain_finish(rtdx_context* ctx, rtdx_swapchain* swapchain);
bool rtdx_swapchain_create_for_dxgi_swapchain(rtdx_context* ctx, rtdx_swapchain* swapchain, IDXGISwapChain3* dxgi_swapchain, u32 width, u32 height);
bool rtdx_swapchain_create_framebuffers(rtdx_context* ctx, rtdx_swapchain* swapchain);
bool rtdx_swapchain_resize(rtdx_context* ctx, rtdx_swapchain* swapchain, u32 width, u32 height);
rt_swapchain_acquire_result rtdx_swapchain_acquire(rtdx_context* ctx, rtdx_swapchain* swapchain);
void rtdx_swapchain_present(rtdx_context* ctx, rtdx_swapchain* swapchain, rtdx_timepoint rendered);
void rtdx_swapchain_wait_frame(rtdx_context* ctx, rtdx_swapchain_frame* frame);
