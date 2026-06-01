#include "procs.h"
#include "logger.h"

#define RTVAL_DROP(message) rtval_printf("[validation] %s, dropping call\n", message)

/*===============================================================================================*/
/*                                                                                               */
/*===============================================================================================*/

RT_EXPORT rt_swapchain rtSwapchainCreate(void) { return rtval_rtSwapchainCreate(); }
RT_EXPORT void rtSwapchainDestroy(rt_swapchain swapchain) { rtval_rtSwapchainDestroy(swapchain); }
RT_EXPORT void rtSwapchainResize(rt_swapchain swapchain, u32 width, u32 height) { rtval_rtSwapchainResize(swapchain, width, height); }
RT_EXPORT void rtSwapchainSetVsync(rt_swapchain swapchain, bool enabled) { rtval_rtSwapchainSetVsync(swapchain, enabled); }
RT_EXPORT rt_swapchain_acquire_result rtSwapchainAcquire(rt_swapchain swapchain) { return rtval_rtSwapchainAcquire(swapchain); }
RT_EXPORT void rtSwapchainPresent(rt_swapchain swapchain, rt_timepoint rendered) { rtval_rtSwapchainPresent(swapchain, rendered); }
RT_EXPORT void rtSwapchainBindWindowGLFW(rt_swapchain swapchain, GLFWwindow* window) { rtval_rtSwapchainBindWindowGLFW(swapchain, window); }

/*===============================================================================================*/
/*                                                                                               */
/*===============================================================================================*/

rt_swapchain rtval_rtSwapchainCreate(void) {
	rt_swapchain swapchain = rtval_next_rtSwapchainCreate();
	rtval_report_error("rtSwapchainCreate");
	return swapchain;
}

void rtval_rtSwapchainDestroy(rt_swapchain swapchain) {
	rtval_next_rtSwapchainDestroy(swapchain);
	rtval_report_error("rtSwapchainDestroy");
}

void rtval_rtSwapchainResize(rt_swapchain swapchain, u32 width, u32 height) {
	if (!swapchain) {
		RTVAL_DROP("swapchain_resize: NULL swapchain");
		return;
	}
	if (width == 0 || height == 0) {
		RTVAL_DROP("swapchain_resize: zero extent");
		return;
	}
	if (!rtval_next_rtSwapchainResize) {
		RTVAL_DROP("swapchain_resize: next function is NULL");
		return;
	}

	rtval_next_rtSwapchainResize(swapchain, width, height);
	rtval_report_error("rtSwapchainResize");
}

void rtval_rtSwapchainSetVsync(rt_swapchain swapchain, bool enabled) {
	if (!swapchain) {
		RTVAL_DROP("swapchain_set_vsync: NULL swapchain");
		return;
	}
	if (!rtval_next_rtSwapchainSetVsync) {
		RTVAL_DROP("swapchain_set_vsync: next function is NULL");
		return;
	}

	rtval_next_rtSwapchainSetVsync(swapchain, enabled);
	rtval_report_error("rtSwapchainSetVsync");
}

rt_swapchain_acquire_result rtval_rtSwapchainAcquire(rt_swapchain swapchain) {
	rt_swapchain_acquire_result result = { RT_NULL_HANDLE, { RT_NULL_HANDLE, 0 } };

	if (!swapchain) {
		rtval_printf("[validation] swapchain_acquire: NULL handle\n");
		return result;
	}
	if (!rtval_next_rtSwapchainAcquire) {
		rtval_printf("[validation] swapchain_acquire: next function is NULL\n");
		return result;
	}

	result = rtval_next_rtSwapchainAcquire(swapchain);
	rtval_report_error("rtSwapchainAcquire");
	return result;
}

void rtval_rtSwapchainPresent(rt_swapchain swapchain, rt_timepoint rendered) {
	if (!swapchain) {
		RTVAL_DROP("swapchain_present: NULL swapchain");
		return;
	}
	if (!rendered.queue || rendered.value == 0) {
		RTVAL_DROP("swapchain_present: missing render timepoint");
		return;
	}
	if (!rtval_next_rtSwapchainPresent) {
		RTVAL_DROP("swapchain_present: next function is NULL");
		return;
	}

	rtval_next_rtSwapchainPresent(swapchain, rendered);
	rtval_report_error("rtSwapchainPresent");
}

void rtval_rtSwapchainBindWindowGLFW(rt_swapchain swapchain, GLFWwindow* window) {
	if (!swapchain) {
		RTVAL_DROP("glfw bind: NULL swapchain");
		return;
	}
	if (!window) {
		RTVAL_DROP("glfw bind: NULL window");
		return;
	}
	if (!rtval_next_rtSwapchainBindWindowGLFW) {
		RTVAL_DROP("glfw bind: next function is NULL");
		return;
	}

	rtval_next_rtSwapchainBindWindowGLFW(swapchain, window);
	rtval_report_error("rtSwapchainBindWindowGLFW");
}

#undef RTVAL_DROP


