#ifndef RTVK_GLFW_SWAPCHAIN_H
#define RTVK_GLFW_SWAPCHAIN_H

#include "config.h"
#define RT_NO_API_WRAPPERS
#include "rt_ext_glfw.h"
#undef RT_NO_API_WRAPPERS
#include "types.h"

RTVK_API void rtSwapchainBindWindowGLFW(rt_swapchain swapchain, GLFWwindow* window);

void rtvk_swapchain_bind_window_glfw(struct rtvk_context* ctx, struct rtvk_swapchain* swapchain, GLFWwindow* window);

#endif /* RTVK_GLFW_SWAPCHAIN_H */
