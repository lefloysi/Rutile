#ifndef RTOPENGL_SWAPCHAIN_GLFW_H
#define RTOPENGL_SWAPCHAIN_GLFW_H

#include "config.h"
#include "resource/swapchain.h"
#define RT_NO_API_WRAPPERS
#include "rt_ext_glfw.h"
#undef RT_NO_API_WRAPPERS
#include "types.h"

RTGL_EXTERN_C_ENTER

RTGL_API bool rtInit_RT_EXT_GLFW(void);
RTGL_API void rtSwapchainBindWindowGLFW(rt_swapchain swapchain, GLFWwindow* window);

RTGL_EXTERN_C_EXIT

void rtgl_swapchain_bind_window_glfw(struct rtgl_context* ctx, struct rtgl_swapchain* swapchain, GLFWwindow* window);

#endif /* RTOPENGL_SWAPCHAIN_GLFW_H */
