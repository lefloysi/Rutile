#pragma once

#include "config.hpp"
#define RT_NO_API_WRAPPERS
#include "rt_ext_glfw.h"
#undef RT_NO_API_WRAPPERS
#include "types.hpp"

RTDX_API void rtSwapchainBindWindowGLFW(rt_swapchain swapchain, GLFWwindow* window);
