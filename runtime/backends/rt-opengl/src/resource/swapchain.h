#ifndef RTOPENGL_SWAPCHAIN_H
#define RTOPENGL_SWAPCHAIN_H

#include "config.h"
#include "platform/context.h"
#include "types.h"

RTGL_EXTERN_C_ENTER

RTGL_API rt_swapchain rtSwapchainCreate(void);
RTGL_API void rtSwapchainDestroy(rt_swapchain swapchain);
RTGL_API void rtSwapchainResize(rt_swapchain swapchain, u32 width, u32 height);
RTGL_API rt_swapchain_acquire_result rtSwapchainAcquire(rt_swapchain swapchain);
RTGL_API void rtSwapchainPresent(rt_swapchain swapchain, rt_timepoint rendered);

RTGL_EXTERN_C_EXIT

struct rtgl_context;

struct rtgl_swapchain {
	struct gl_surface* surface;
	u32 width;
	u32 height;
	bool bound;
};

static inline struct rtgl_swapchain* rtgl_swapchain_from_handle(rt_swapchain swapchain) {
	return (struct rtgl_swapchain*)swapchain;
}

static inline rt_swapchain rtgl_swapchain_to_handle(struct rtgl_swapchain* swapchain) {
	return (rt_swapchain)swapchain;
}

struct rtgl_swapchain* rtgl_swapchain_create(struct rtgl_context* ctx);
void rtgl_swapchain_destroy(struct rtgl_context* ctx, struct rtgl_swapchain* swapchain);
void rtgl_swapchain_init_from_window(struct rtgl_context* ctx, struct rtgl_swapchain* swapchain, native_window_handle_t window, u32 width, u32 height);
void rtgl_swapchain_init_from_surface(struct rtgl_context* ctx, struct rtgl_swapchain* swapchain, struct gl_surface* surface, u32 width, u32 height);

#endif /* RTOPENGL_SWAPCHAIN_H */
