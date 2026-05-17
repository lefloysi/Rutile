#ifndef RT_EXT_GLFW_H
#define RT_EXT_GLFW_H

/*
 * RT_EXT_GLFW extension package.
 */

#include "rutile.h"
#include "rt_ext_swapchain.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct GLFWwindow GLFWwindow;
typedef void (*PFN_rtSwapchainBindWindowGLFW)(rt_swapchain swapchain, GLFWwindow* window);

extern PFN_rtSwapchainBindWindowGLFW rt_rtSwapchainBindWindowGLFW;
bool rtLoad_RT_EXT_GLFW(void);

#ifndef RT_NO_API_WRAPPERS
static inline void rtSwapchainBindWindowGLFW(rt_swapchain swapchain, GLFWwindow* window) { rt_rtSwapchainBindWindowGLFW(swapchain, window); }
#endif

#ifdef RUTILE_IMPL

PFN_rtSwapchainBindWindowGLFW rt_rtSwapchainBindWindowGLFW = NULL;

bool rtLoad_RT_EXT_GLFW(void) {
	rt_rtSwapchainBindWindowGLFW = (PFN_rtSwapchainBindWindowGLFW)rtGetProc("rtSwapchainBindWindowGLFW");
	return rt_rtSwapchainBindWindowGLFW != NULL;
}

#endif /* RUTILE_IMPL */

#ifdef __cplusplus
}
#endif /* __cplusplus */
#endif /* RT_EXT_GLFW_H */
