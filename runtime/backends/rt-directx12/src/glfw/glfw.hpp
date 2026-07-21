#pragma once

#include "resource/swapchain.hpp"
#define RT_NO_API_WRAPPERS
#include "rt_ext_glfw.h"
#undef RT_NO_API_WRAPPERS

void rtdx_init_glfw_platform();
rtdx_native_window rtdx_glfw_get_native_window(GLFWwindow* window);
void rtdx_glfw_get_framebuffer_size(GLFWwindow* window, int* width, int* height);
