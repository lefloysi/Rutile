#ifndef RTOPENGL_PLATFORM_GLFW_H
#define RTOPENGL_PLATFORM_GLFW_H

#include "platform/context.h"
#define RT_NO_API_WRAPPERS
#include "rt_ext_glfw.h"
#undef RT_NO_API_WRAPPERS
#include "types.h"

bool rtgl_init_glfw_platform(void);
struct gl_surface* rtgl_create_glfw_surface(struct gl_context* context, GLFWwindow* window, u32* width, u32* height);

#endif /* RTOPENGL_PLATFORM_GLFW_H */
