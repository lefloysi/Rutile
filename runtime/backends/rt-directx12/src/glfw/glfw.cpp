#include "glfw/glfw.hpp"
#include "error.hpp"

#include <windows.h>
#include <cassert>

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

	rtdx_glfw.get_framebuffer_size = reinterpret_cast<PFN_rtdx_glfwGetFramebufferSize>(rtdx_glfw_symbol("glfwGetFramebufferSize"));
	rtdx_glfw.get_win32_window = reinterpret_cast<PFN_rtdx_glfwGetWin32Window>(rtdx_glfw_symbol("glfwGetWin32Window"));

	if (!rtdx_glfw.get_framebuffer_size || !rtdx_glfw.get_win32_window) {
		rtdx_throwf(RT_UNSUPPORTED_PLATFORM, "GLFW symbols are not exported by the executable or available from glfw3.dll");
		return false;
	}

	rtdx_glfw.resolved = true;
	return true;
}

void rtdx_init_glfw_platform() {
	rtdx_glfw_resolve();
}

rtdx_native_window rtdx_glfw_get_native_window(GLFWwindow* window) {
	assert(rtdx_glfw.get_win32_window);
	assert(window);
	return rtdx_glfw.get_win32_window(window);
}

void rtdx_glfw_get_framebuffer_size(GLFWwindow* window, int* width, int* height) {
	assert(rtdx_glfw.get_framebuffer_size);
	assert(window);
	rtdx_glfw.get_framebuffer_size(window, width, height);
}
