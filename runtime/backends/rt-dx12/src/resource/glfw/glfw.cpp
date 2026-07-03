#include "resource/glfw/glfw.hpp"
#include "context.hpp"
#include "error.hpp"

#include <windows.h>

using PFN_rtdx_glfwGetFramebufferSize = void (*)(GLFWwindow* window, int* width, int* height);
using PFN_rtdx_glfwGetWin32Window = HWND (*)(GLFWwindow* window);

struct rtdx_glfw_procs {
	PFN_rtdx_glfwGetFramebufferSize get_framebuffer_size = nullptr;
	PFN_rtdx_glfwGetWin32Window get_win32_window = nullptr;
	bool resolved = false;
};

static rtdx_glfw_procs rtdx_glfw;

/*===============================================================================================*/
/*                                                                                               */
/*===============================================================================================*/

static void* rtdx_glfw_symbol(const char* name) {
	HMODULE module = GetModuleHandleA(NULL);
	void* symbol = module ? reinterpret_cast<void*>(GetProcAddress(module, name)) : nullptr;
	if (symbol) {
		return symbol;
	}

	module = GetModuleHandleA("glfw3.dll");
	if (!module) {
		module = LoadLibraryA("glfw3.dll");
	}
	return module ? reinterpret_cast<void*>(GetProcAddress(module, name)) : nullptr;
}

static bool rtdx_glfw_resolve() {
	if (rtdx_glfw.resolved) {
		return rtdx_glfw.get_framebuffer_size && rtdx_glfw.get_win32_window;
	}

	rtdx_glfw.get_framebuffer_size =
		reinterpret_cast<PFN_rtdx_glfwGetFramebufferSize>(rtdx_glfw_symbol("glfwGetFramebufferSize"));
	rtdx_glfw.get_win32_window =
		reinterpret_cast<PFN_rtdx_glfwGetWin32Window>(rtdx_glfw_symbol("glfwGetWin32Window"));

	if (!rtdx_glfw.get_framebuffer_size || !rtdx_glfw.get_win32_window) {
		rtdx_throwf(RT_UNSUPPORTED_PLATFORM, "GLFW symbols are not exported by the executable or available from glfw3.dll");
		return false;
	}

	rtdx_glfw.resolved = true;
	return true;
}

void rtSwapchainBindWindowGLFW(rt_swapchain swapchain, GLFWwindow* window) {
	int width = 0;
	int height = 0;
	HWND hwnd;

	if (!window) {
		rtdx_throwf(RT_IMPROPER_USAGE, "rtSwapchainBindWindowGLFW window is NULL");
		return;
	}
	if (!rtdx_glfw_resolve()) {
		return;
	}

	rtClearError();
	rtdx_glfw.get_framebuffer_size(window, &width, &height);
	hwnd = rtdx_glfw.get_win32_window(window);
	if (!hwnd) {
		rtdx_throwf(RT_UNSUPPORTED_PLATFORM, "GLFW window has no Win32 HWND");
		return;
	}

	if (!rtdx_swapchain_create_for_hwnd(
		rtdx_get_current_context(),
		rtdx_swapchain_from_handle(swapchain),
		hwnd,
		(u32)width,
		(u32)height
	)) {
		return;
	}
}
