#include "extension/swapchain/glfw/glfw.h"
#include "context.h"
#include "error.h"

#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>
#include <windows.h>

/*===============================================================================================*/
/*                                                                                               */
/*===============================================================================================*/

void rtSwapchainBindWindowGLFW(rt_swapchain swapchain, GLFWwindow* window) {
	int width = 0;
	int height = 0;
	HWND hwnd;

	if (!window) {
		rtdx_throwf(RT_IMPROPER_USAGE, "rtSwapchainBindWindowGLFW window is NULL");
		return;
	}

	glfwGetFramebufferSize(window, &width, &height);
	hwnd = glfwGetWin32Window(window);
	if (!hwnd) {
		rtdx_throwf(RT_UNSUPPORTED_PLATFORM, "GLFW window has no Win32 HWND");
		return;
	}

	rtdx_swapchain_create_for_hwnd(
		rtdx_get_current_context(),
		rtdx_swapchain_from_handle(swapchain),
		hwnd,
		(u32)width,
		(u32)height);
}


