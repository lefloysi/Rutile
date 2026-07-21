#ifndef RTVK_GLFW_GLFW_H
#define RTVK_GLFW_GLFW_H

#include "config.h"
#define RT_NO_API_WRAPPERS
#include "rt_ext_glfw.h"
#undef RT_NO_API_WRAPPERS
#include <volk.h>

struct rtvk_context;

void rtvk_init_glfw_platform(void);
VkSurfaceKHR rtvk_create_glfw_surface(struct rtvk_context* ctx, GLFWwindow* window);
void rtvk_glfw_get_framebuffer_size(GLFWwindow* window, int* width, int* height);
void rtvk_glfw_get_error(int* code, const char** message);

#endif /* RTVK_GLFW_GLFW_H */
