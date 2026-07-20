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
bool rtLoad_RT_EXT_GLFW(void);

#ifndef RT_NO_API_WRAPPERS
static inline void rtSwapchainBindWindowGLFW(rt_swapchain swapchain, GLFWwindow* window) { rt_rtSwapchainBindWindowGLFW(swapchain, window); }
#endif

#ifdef RUTILE_IMPL

PFN_rtInit_RT_EXT_GLFW rt_rtInit_RT_EXT_GLFW = NULL;
PFN_rtSwapchainBindWindowGLFW rt_rtSwapchainBindWindowGLFW = NULL;

bool rtLoad_RT_EXT_GLFW(void) {
	rt_rtInit_RT_EXT_GLFW = (PFN_rtInit_RT_EXT_GLFW)rtGetProc("rtInit_RT_EXT_GLFW");
	rt_rtSwapchainBindWindowGLFW = (PFN_rtSwapchainBindWindowGLFW)rtGetProc("rtSwapchainBindWindowGLFW");
	return rt_rtSwapchainBindWindowGLFW != NULL && (!rt_rtInit_RT_EXT_GLFW || rt_rtInit_RT_EXT_GLFW());
}

/* Neither rt-vk13 nor rt-dx12 link GLFW themselves; they resolve the GLFW
 * entry points they need at runtime from whatever module the host process
 * loaded. When the host statically links GLFW into its executable, the linker
 * has to be told to actually export those symbols, otherwise GetProcAddress on
 * the EXE returns NULL. We do that here on MSVC via #pragma comment(linker,
 * ...) so the user only needs to include this header with RUTILE_IMPL defined.
 *
 * The Vulkan-flavored entries (CreateWindowSurface, VulkanSupported) are
 * required by rt-vk13; glfwGetWin32Window is required by rt-dx12. The rest
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
