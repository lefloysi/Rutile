#include "procs.h"

/*===============================================================================================*/
/*                                                                                               */
/*===============================================================================================*/

RT_EXPORT rt_swapchain rtSwapchainCreate(void) { return rtlog_rtSwapchainCreate(); }
RT_EXPORT void rtSwapchainDestroy(rt_swapchain swapchain) { rtlog_rtSwapchainDestroy(swapchain); }
RT_EXPORT void rtSwapchainResize(rt_swapchain swapchain, u32 width, u32 height) { rtlog_rtSwapchainResize(swapchain, width, height); }
RT_EXPORT void rtSwapchainSetVsync(rt_swapchain swapchain, bool enabled) { rtlog_rtSwapchainSetVsync(swapchain, enabled); }
RT_EXPORT rt_swapchain_acquire_result rtSwapchainAcquire(rt_swapchain swapchain) { return rtlog_rtSwapchainAcquire(swapchain); }
RT_EXPORT void rtSwapchainPresent(rt_swapchain swapchain, rt_timepoint rendered) { rtlog_rtSwapchainPresent(swapchain, rendered); }
RT_EXPORT void rtSwapchainBindWindowGLFW(rt_swapchain swapchain, GLFWwindow* window) { rtlog_rtSwapchainBindWindowGLFW(swapchain, window); }

/*===============================================================================================*/
/*                                                                                               */
/*===============================================================================================*/

rt_swapchain rtlog_rtSwapchainCreate(void) {
	u64 start_ns = rtlog_now_ns();

	rtlog_printf("rtSwapchainCreate()\n");
	rt_swapchain result = next_rtSwapchainCreate();
	rtlog_printf("rtSwapchainCreate -> %s [%s]\n", rtlog_pointer(result), rtlog_elapsed(start_ns));
	rtlog_error("rtSwapchainCreate");
	return result;
}

void rtlog_rtSwapchainDestroy(rt_swapchain swapchain) {
	u64 start_ns = rtlog_now_ns();
	rtlog_printf("rtSwapchainDestroy(swapchain=%s)\n", rtlog_pointer(swapchain));
	next_rtSwapchainDestroy(swapchain);
	rtlog_printf("rtSwapchainDestroy completed in %s\n", rtlog_elapsed(start_ns));
	rtlog_error("rtSwapchainDestroy");
}

void rtlog_rtSwapchainResize(rt_swapchain swapchain, u32 width, u32 height) {
	u64 start_ns = rtlog_now_ns();
	rtlog_printf("rtSwapchainResize(swapchain=%s, width=%u, height=%u)\n", rtlog_pointer(swapchain), width, height);
	if (!next_rtSwapchainResize) {
		rtlog_printf("rtSwapchainResize missing next function in %s\n", rtlog_elapsed(start_ns));
		rtlog_error("rtSwapchainResize");
		return;
	}
	next_rtSwapchainResize(swapchain, width, height);
	rtlog_printf("rtSwapchainResize completed in %s\n", rtlog_elapsed(start_ns));
	rtlog_error("rtSwapchainResize");
}

void rtlog_rtSwapchainSetVsync(rt_swapchain swapchain, bool enabled) {
	u64 start_ns = rtlog_now_ns();
	rtlog_printf("rtSwapchainSetVsync(swapchain=%s, enabled=%s)\n", rtlog_pointer(swapchain), enabled ? "true" : "false");
	if (!next_rtSwapchainSetVsync) {
		rtlog_printf("rtSwapchainSetVsync missing next function in %s\n", rtlog_elapsed(start_ns));
		rtlog_error("rtSwapchainSetVsync");
		return;
	}
	next_rtSwapchainSetVsync(swapchain, enabled);
	rtlog_printf("rtSwapchainSetVsync completed in %s\n", rtlog_elapsed(start_ns));
	rtlog_error("rtSwapchainSetVsync");
}

rt_swapchain_acquire_result rtlog_rtSwapchainAcquire(rt_swapchain swapchain) {
	u64 start_ns = rtlog_now_ns();
	rt_swapchain_acquire_result result = { RT_NULL_HANDLE, { RT_NULL_HANDLE, 0 } };

	rtlog_printf("rtSwapchainAcquire(swapchain=%s)\n", rtlog_pointer(swapchain));
	if (!next_rtSwapchainAcquire) {
		rtlog_printf("rtSwapchainAcquire -> missing next function [%s]\n", rtlog_elapsed(start_ns));
		rtlog_error("rtSwapchainAcquire");
		return result;
	}
	result = next_rtSwapchainAcquire(swapchain);
	rtlog_printf("rtSwapchainAcquire -> {framebuffer=%s, timepoint=%s} [%s]\n", rtlog_pointer(result.framebuffer), rtlog_timepoint(result.timepoint), rtlog_elapsed(start_ns));
	rtlog_error("rtSwapchainAcquire");
	return result;
}

void rtlog_rtSwapchainPresent(rt_swapchain swapchain, rt_timepoint rendered) {
	u64 start_ns = rtlog_now_ns();
	rtlog_printf("rtSwapchainPresent(swapchain=%s, rendered=%s)\n", rtlog_pointer(swapchain), rtlog_timepoint(rendered));
	if (!next_rtSwapchainPresent) {
		rtlog_printf("rtSwapchainPresent missing next function in %s\n", rtlog_elapsed(start_ns));
		rtlog_error("rtSwapchainPresent");
		return;
	}
	next_rtSwapchainPresent(swapchain, rendered);
	rtlog_printf("rtSwapchainPresent completed in %s\n", rtlog_elapsed(start_ns));
	rtlog_error("rtSwapchainPresent");
}

void rtlog_rtSwapchainBindWindowGLFW(rt_swapchain swapchain, GLFWwindow* window) {
	u64 start_ns = rtlog_now_ns();
	rtlog_printf("rtSwapchainBindWindowGLFW(swapchain=%s, window=%s)\n", rtlog_pointer(swapchain), rtlog_pointer(window));
	if (!next_rtSwapchainBindWindowGLFW) {
		rtlog_printf("rtSwapchainBindWindowGLFW missing next function in %s\n", rtlog_elapsed(start_ns));
		rtlog_error("rtSwapchainBindWindowGLFW");
		return;
	}
	next_rtSwapchainBindWindowGLFW(swapchain, window);
	rtlog_printf("rtSwapchainBindWindowGLFW completed in %s\n", rtlog_elapsed(start_ns));
	rtlog_error("rtSwapchainBindWindowGLFW");
}


