#ifndef RTVK_SWAPCHAIN_GLFW_H
#define RTVK_SWAPCHAIN_GLFW_H

#include "config.h"
#include "resource/swapchain.h"
#include "rt_ext_glfw.h"
#include "types.h"

/*===============================================================================================*/
/*                                                                                               */
/*===============================================================================================*/

RTVK_API void rtSwapchainBindWindowGLFW(rt_swapchain swapchain, GLFWwindow* window);

/*===============================================================================================*/
/*                                                                                               */
/*===============================================================================================*/

void rtvk_swapchain_bind_window_glfw(struct rtvk_context* ctx, struct rtvk_swapchain* swapchain, GLFWwindow* window);

#endif
