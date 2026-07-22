#include "glfw/swapchain.hpp"

#include "context.hpp"
#include "error.hpp"
#include "glfw/glfw.hpp"
#include "resource/swapchain_win32.hpp"

void rtSwapchainBindWindowGLFW(rt_swapchain swapchain, GLFWwindow* window) {
	int width = 0;
	int height = 0;

	if (!window) {
		rtdx_throwf(RT_IMPROPER_USAGE, "rtSwapchainBindWindowGLFW window is NULL");
		return;
	}
	rtdx_init_glfw_platform();
	if (rtError() != RT_SUCCESS) {
		return;
	}

	rtClearError();
	rtdx_glfw_get_framebuffer_size(window, &width, &height);
	if (!rtdx_swapchain_create_for_hwnd(
		rtdx_get_current_context(),
		rtdx_swapchain_from_handle(swapchain),
		rtdx_glfw_get_hwnd(window),
		(u32)width,
		(u32)height
	)) {
		return;
	}
}
