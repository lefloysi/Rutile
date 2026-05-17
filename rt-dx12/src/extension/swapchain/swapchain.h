#ifndef RTDX_SWAPCHAIN_H
#define RTDX_SWAPCHAIN_H

#include "config.h"
#include "resource/framebuffer.h"
#include "resource/queue.h"
#include "resource/texture.h"

#include <dxgi1_6.h>
#include <windows.h>

/*===============================================================================================*/
/*                                                                                               */
/*===============================================================================================*/

RTDX_EXTERN_C_ENTER
RTDX_API rt_swapchain rtSwapchainCreate(void);
RTDX_API void rtSwapchainDestroy(rt_swapchain swapchain);
RTDX_API void rtSwapchainResize(rt_swapchain swapchain, u32 width, u32 height);
RTDX_API rt_swapchain_acquire_result rtSwapchainAcquire(rt_swapchain swapchain);
RTDX_API void rtSwapchainPresent(rt_swapchain swapchain, rt_timepoint rendered);
RTDX_EXTERN_C_EXIT

/*===============================================================================================*/
/*                                                                                               */
/*===============================================================================================*/

struct rtdx_swapchain_frame {
	struct rtdx_queue* present_queue;

	ID3D12CommandAllocator* present_allocator;
	ID3D12GraphicsCommandList* present_command_list;

	u64 present_value;
	bool has_present_timepoint;
};

struct rtdx_swapchain {
	struct rtdx_resource_base base;

	IDXGISwapChain3* dxgi_swapchain;
	ID3D12DescriptorHeap* rtv_heap;
	HANDLE frame_latency_object;

	struct rtdx_texture* textures[RTDX_MAX_FRAMES_IN_FLIGHT];
	struct rtdx_texture_view* texture_views[RTDX_MAX_FRAMES_IN_FLIGHT];
	struct rtdx_framebuffer* framebuffers[RTDX_MAX_FRAMES_IN_FLIGHT];
	struct rtdx_swapchain_frame frames[RTDX_MAX_FRAMES_IN_FLIGHT];

	HWND hwnd;
	UINT rtv_descriptor_size;
	DXGI_FORMAT dxgi_format;
	u32 width;
	u32 height;
	u32 current_image_index;
	bool frame_acquired;
	CRITICAL_SECTION frame_lock;
	CONDITION_VARIABLE frame_condition;
};

static inline struct rtdx_swapchain* rtdx_swapchain_from_handle(rt_swapchain swapchain) { return (struct rtdx_swapchain*)swapchain; }
static inline rt_swapchain rtdx_swapchain_to_handle(struct rtdx_swapchain* swapchain) { return (rt_swapchain)swapchain; }

struct rtdx_swapchain* rtdx_swapchain_create(struct rtdx_context* ctx);
void rtdx_swapchain_destroy(struct rtdx_context* ctx, struct rtdx_swapchain* swapchain);
void rtdx_swapchain_init(struct rtdx_context* ctx, struct rtdx_swapchain* swapchain);
void rtdx_swapchain_finish(struct rtdx_context* ctx, struct rtdx_swapchain* swapchain);
bool rtdx_swapchain_create_for_hwnd(struct rtdx_context* ctx, struct rtdx_swapchain* swapchain, HWND hwnd, u32 width, u32 height);
bool rtdx_swapchain_resize(struct rtdx_context* ctx, struct rtdx_swapchain* swapchain, u32 width, u32 height);
rt_swapchain_acquire_result rtdx_swapchain_acquire(struct rtdx_context* ctx, struct rtdx_swapchain* swapchain);
void rtdx_swapchain_present(struct rtdx_context* ctx, struct rtdx_swapchain* swapchain, struct rtdx_timepoint rendered);
void rtdx_swapchain_wait_frame(struct rtdx_context* ctx, struct rtdx_swapchain_frame* frame);

#endif /* RTDX_SWAPCHAIN_H */


