#include "swapchain.h"
#include "logger.h"
#include "framebuffer.h"

#define RTVAL_DROP(message) rtval_printf("[validation] %s, dropping call\n", message)

/*===============================================================================================*/
/*                                                                                               */
/*===============================================================================================*/

RT_EXPORT rt_swapchain rtSwapchainCreate(void) {
	return rtval_swapchain_to_handle(rtval_swapchain_create());
}

RT_EXPORT void rtSwapchainDestroy(rt_swapchain swapchain) {
	rtval_swapchain_destroy(RTVAL_PAYLOAD(swapchain, struct rtval_swapchain));
}

RT_EXPORT void rtSwapchainResize(rt_swapchain swapchain, u32 width, u32 height) {
	rtval_swapchain_resize(RTVAL_PAYLOAD(swapchain, struct rtval_swapchain), width, height);
}

RT_EXPORT rt_swapchain_acquire_result rtSwapchainAcquire(rt_swapchain swapchain) {
	return rtval_swapchain_acquire(RTVAL_PAYLOAD(swapchain, struct rtval_swapchain));
}

RT_EXPORT void rtSwapchainPresent(rt_swapchain swapchain, rt_timepoint rendered) {
	rtval_swapchain_present(RTVAL_PAYLOAD(swapchain, struct rtval_swapchain), rendered);
}

RT_EXPORT void rtSwapchainBindWindowGLFW(rt_swapchain swapchain, GLFWwindow* window) {
	rtval_swapchain_bind_window_glfw(RTVAL_PAYLOAD(swapchain, struct rtval_swapchain), window);
}

/*===============================================================================================*/
/*                                                                                               */
/*===============================================================================================*/

struct rtval_swapchain* rtval_swapchain_create(void) {
	rt_swapchain backend = rtval_next_rtSwapchainCreate();
	if (!backend) { rtval_report_error("rtSwapchainCreate"); return NULL; }
	struct rtval_swapchain* handle = rtval_handle_create(RTVAL_HANDLE_TYPE_SWAPCHAIN);
	if (!handle) { rtval_next_rtSwapchainDestroy(backend); return NULL; }
	struct rtval_swapchain* state = RTVAL_PAYLOAD(handle, struct rtval_swapchain);
	state->backend = backend;
	state->current_framebuffer = NULL;
	state->has_current_framebuffer = false;
	rtval_report_error("rtSwapchainCreate");
	return handle;
}

void rtval_swapchain_destroy(struct rtval_swapchain* swapchain) {
	if (!swapchain) { return; }
	rt_swapchain backend = swapchain->backend;
	if (swapchain->current_framebuffer) {
		rtval_handle_destroy(swapchain->current_framebuffer);
		swapchain->current_framebuffer = NULL;
	}
	swapchain->has_current_framebuffer = false;
	rtval_handle_destroy(swapchain);
	rtval_next_rtSwapchainDestroy(backend);
}

void rtval_swapchain_resize(struct rtval_swapchain* swapchain, u32 width, u32 height) {
	if (!swapchain) { RTVAL_DROP("rtSwapchainResize: NULL handle"); return; }
	if (width == 0 || height == 0) { RTVAL_DROP("rtSwapchainResize: zero extent"); return; }

	rtval_next_rtSwapchainResize(swapchain->backend, width, height);
	rtval_report_error("rtSwapchainResize");
}

rt_swapchain_acquire_result rtval_swapchain_acquire(struct rtval_swapchain* swapchain) {
	if (!swapchain) { RTVAL_DROP("rtSwapchainAcquire: NULL handle"); return (rt_swapchain_acquire_result){ RT_NULL_HANDLE, { 0 } }; }
	if (swapchain->has_current_framebuffer) {
		RTVAL_DROP("rtSwapchainAcquire: framebuffer already acquired");
		return (rt_swapchain_acquire_result){ RT_NULL_HANDLE, { 0 } };
	}

	rt_swapchain_acquire_result result = rtval_next_rtSwapchainAcquire(swapchain->backend);
	rtval_report_error("rtSwapchainAcquire");
	if (!result.framebuffer) {
		rtval_printf("[validation] rtSwapchainAcquire: backend returned NULL framebuffer, current_framebuffer=%p has_current_framebuffer=%d\n",
			swapchain->current_framebuffer,
			swapchain->has_current_framebuffer ? 1 : 0);
		RTVAL_DROP("rtSwapchainAcquire: backend returned NULL framebuffer");
		return (rt_swapchain_acquire_result){ RT_NULL_HANDLE, { 0 } };
	}
	swapchain->current_framebuffer = rtval_framebuffer_wrap(result.framebuffer);
	if (!swapchain->current_framebuffer) {
		rtval_printf("[validation] rtSwapchainAcquire: wrap failed for backend framebuffer=%p\n", result.framebuffer);
		RTVAL_DROP("rtSwapchainAcquire: failed to wrap framebuffer");
		return (rt_swapchain_acquire_result){ RT_NULL_HANDLE, { 0 } };
	}
	swapchain->has_current_framebuffer = true;
	return (rt_swapchain_acquire_result){
		rtval_framebuffer_to_handle(swapchain->current_framebuffer),
		rtval_timepoint_wrap(result.timepoint)
	};
}

void rtval_swapchain_present(struct rtval_swapchain* swapchain, rt_timepoint rendered) {
	if (!swapchain) { RTVAL_DROP("rtSwapchainPresent: NULL handle"); return; }
	if (!rendered.queue || rendered.value == 0) { RTVAL_DROP("rtSwapchainPresent: missing render timepoint"); return; }
	if (!swapchain->has_current_framebuffer || !swapchain->current_framebuffer) {
		RTVAL_DROP("rtSwapchainPresent: called without a successful rtSwapchainAcquire");
		return;
	}

	rtval_next_rtSwapchainPresent(swapchain->backend, rtval_timepoint_unwrap(rendered));
	rtval_report_error("rtSwapchainPresent");
	rtval_handle_destroy(swapchain->current_framebuffer);
	swapchain->current_framebuffer = NULL;
	swapchain->has_current_framebuffer = false;
}

void rtval_swapchain_bind_window_glfw(struct rtval_swapchain* swapchain, GLFWwindow* window) {
	if (!swapchain) { RTVAL_DROP("rtSwapchainBindWindowGLFW: NULL handle"); return; }
	if (!window) { RTVAL_DROP("rtSwapchainBindWindowGLFW: NULL window"); return; }

	rtval_next_rtSwapchainBindWindowGLFW(swapchain->backend, window);
	rtval_report_error("rtSwapchainBindWindowGLFW");
}

#undef RTVAL_DROP
