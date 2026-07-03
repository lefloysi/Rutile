#ifndef RTDX_GLFW_H
#define RTDX_GLFW_H

#include "config.h"
#include "resource/swapchain.h"
#include "rt_ext_glfw.h"

/*===============================================================================================*/
/*                                                                                               */
/*===============================================================================================*/

RTDX_EXTERN_C_ENTER
RTDX_API void rtSwapchainBindWindowGLFW(rt_swapchain swapchain, GLFWwindow* window);
RTDX_EXTERN_C_EXIT

#endif /* RTDX_GLFW_H */
