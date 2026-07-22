#pragma once

#define RT_NO_API_WRAPPERS
#include "rt_ext_glfw.h"
#undef RT_NO_API_WRAPPERS

#include <windows.h>

void rtdx_init_glfw_platform();
HWND rtdx_glfw_get_hwnd(GLFWwindow* window);
void rtdx_glfw_get_framebuffer_size(GLFWwindow* window, int* width, int* height);
