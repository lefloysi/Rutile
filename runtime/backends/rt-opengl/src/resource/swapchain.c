#include "resource/swapchain.h"

#include "context.h"
#include "error.h"

#include <stdlib.h>

rt_swapchain rtSwapchainCreate(void) {
	return rtgl_swapchain_to_handle(rtgl_swapchain_create(rtgl_get_current_context()));
}

void rtSwapchainDestroy(rt_swapchain swapchain) {
	rtgl_swapchain_destroy(rtgl_get_current_context(), rtgl_swapchain_from_handle(swapchain));
}

void rtSwapchainResize(rt_swapchain swapchain, u32 width, u32 height) {
	struct rtgl_swapchain* state = rtgl_swapchain_from_handle(swapchain);
	if (!state) {
		rtgl_throwf(RT_IMPROPER_USAGE, "swapchain resize requires a valid swapchain");
		return;
	}
	state->width = width;
	state->height = height;
}

rt_swapchain_acquire_result rtSwapchainAcquire(rt_swapchain swapchain) {
	rt_swapchain_acquire_result result = { RT_NULL_HANDLE, { RT_NULL_HANDLE, 0 } };
	struct rtgl_swapchain* state = rtgl_swapchain_from_handle(swapchain);
	if (!state || !state->bound) {
		rtgl_throwf(RT_IMPROPER_USAGE, "swapchain acquire requires a bound swapchain");
	}
	return result;
}

void rtSwapchainPresent(rt_swapchain swapchain, rt_timepoint rendered) {
	(void)rendered;
	struct rtgl_swapchain* state = rtgl_swapchain_from_handle(swapchain);
	if (!state || !state->bound) {
		rtgl_throwf(RT_IMPROPER_USAGE, "swapchain present requires a bound swapchain");
		return;
	}
}

struct rtgl_swapchain* rtgl_swapchain_create(struct rtgl_context* ctx) {
	(void)ctx;
	struct rtgl_swapchain* swapchain = calloc(1, sizeof(*swapchain));
	RTGL_CHECK_ALLOC(swapchain, sizeof(*swapchain), "OpenGL swapchain");
	if (swapchain) {
		rtgl_printf("rt-opengl: swapchain created\n");
	}
	return swapchain;
}

void rtgl_swapchain_destroy(struct rtgl_context* ctx, struct rtgl_swapchain* swapchain) {
	(void)ctx;
	if (!swapchain) {
		return;
	}
	if (swapchain->surface) {
		rtgl_destroy_glsurface(swapchain->surface);
		swapchain->surface = NULL;
	}
	rtgl_printf("rt-opengl: swapchain destroyed\n");
	free(swapchain);
}

void rtgl_swapchain_init_from_surface(struct rtgl_context* ctx, struct rtgl_swapchain* swapchain, struct gl_surface* surface, u32 width, u32 height) {
	if (!ctx || !ctx->gl_context || !swapchain || !surface) {
		rtgl_throwf(RT_IMPROPER_USAGE, "swapchain, context, and GL surface must be valid");
		return;
	}
	if (swapchain->bound) {
		rtgl_throwf(RT_IMPROPER_USAGE, "OpenGL swapchain is already bound to a window");
		return;
	}
	swapchain->surface = surface;
	swapchain->width = width;
	swapchain->height = height;
	swapchain->bound = true;
	rtgl_printf("rt-opengl: swapchain bound to surface (%ux%u)\n", width, height);
}
