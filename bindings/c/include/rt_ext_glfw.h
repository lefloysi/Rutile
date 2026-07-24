#ifndef RT_EXT_GLFW_H
#define RT_EXT_GLFW_H

/*
 * RT_EXT_GLFW extension package.
 */

#include "rt_ext_swapchain.h"
#include "rutile.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct GLFWwindow GLFWwindow;
typedef bool (*PFN_rtInit_RT_EXT_GLFW)(void);
typedef void (*PFN_rtSwapchainBindWindowGLFW)(rt_swapchain swapchain, GLFWwindow* window);

extern PFN_rtInit_RT_EXT_GLFW rt_rtInit_RT_EXT_GLFW;
extern PFN_rtSwapchainBindWindowGLFW rt_rtSwapchainBindWindowGLFW;
enum rt_error rtLoad_RT_EXT_GLFW(void);

#ifndef RT_NO_API_WRAPPERS
static inline void rtSwapchainBindWindowGLFW(rt_swapchain swapchain, GLFWwindow* window) { rt_rtSwapchainBindWindowGLFW(swapchain, window); }
#endif

#ifdef RUTILE_IMPL

PFN_rtInit_RT_EXT_GLFW rt_rtInit_RT_EXT_GLFW = NULL;
PFN_rtSwapchainBindWindowGLFW rt_rtSwapchainBindWindowGLFW = NULL;

static void rt__clear_RT_EXT_GLFW(void) {
	rt_rtInit_RT_EXT_GLFW = NULL;
	rt_rtSwapchainBindWindowGLFW = NULL;
}

enum rt_error rtLoad_RT_EXT_GLFW(void) {
	PFN_rtInit_RT_EXT_GLFW init;
	PFN_rtSwapchainBindWindowGLFW bind_window;

	rt__clear_RT_EXT_GLFW();
	init = (PFN_rtInit_RT_EXT_GLFW)rtGetProc("rtInit_RT_EXT_GLFW");
	if (init && !init()) {
		enum rt_error err = rtError();
		return err != RT_SUCCESS ? err : RT_UNSUPPORTED_FEATURE;
	}
	bind_window = (PFN_rtSwapchainBindWindowGLFW)rtGetProc("rtSwapchainBindWindowGLFW");
	if (!bind_window) {
		rt__clear_RT_EXT_GLFW();
		return RT_EXTENSION_NOT_PRESENT;
	}
	rt_rtInit_RT_EXT_GLFW = init;
	rt_rtSwapchainBindWindowGLFW = bind_window;
	return RT_SUCCESS;
}

/* Neither rt-vulkan nor rt-directx12 link GLFW themselves; they resolve the GLFW
 * entry points they need at runtime from whatever module the host process
 * loaded. When the host statically links GLFW into its executable, the linker
 * has to be told to actually export those symbols, otherwise GetProcAddress on
 * the EXE returns NULL. We do that here on MSVC via #pragma comment(linker,
 * ...) so the user only needs to include this header with RUTILE_IMPL defined.
 *
 * The Vulkan-flavored entries (CreateWindowSurface, VulkanSupported) are
 * required by rt-vulkan; glfwGetWin32Window is required by rt-directx12. The rest
 * are shared. */
#if defined(_WIN32) && defined(_MSC_VER)
#pragma comment(linker, "/EXPORT:glfwCreateWindowSurface")
#pragma comment(linker, "/EXPORT:glfwGetFramebufferSize")
#pragma comment(linker, "/EXPORT:glfwGetError")
#pragma comment(linker, "/EXPORT:glfwVulkanSupported")
#pragma comment(linker, "/EXPORT:glfwGetWin32Window")
#endif

#endif /* RUTILE_IMPL */

#ifdef __cplusplus
}
#endif /* __cplusplus */
#endif /* RT_EXT_GLFW_H */
