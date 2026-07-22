#include "ext/glfw/swapchain.h"

#include "context.h"
#include "error.h"
#include "execution.h"

bool rtInit_RT_EXT_GLFW(void) {
	rtgl_init_glfw_platform();
	return rtgl_error() == RT_SUCCESS;
}

void rtSwapchainBindWindowGLFW(rt_swapchain swapchain, GLFWwindow* window) {
	int width = 0;
	int height = 0;

	glfwGetFramebufferSize(window, &width, &height);
	rtgl_swapchain_bind_window_glfw(rtgl_get_current_context(), rtgl_swapchain_from_handle(swapchain), window, (u32)width, (u32)height);
}

void rtgl_swapchain_bind_window_glfw(struct rtgl_context* ctx, struct rtgl_swapchain* swapchain, GLFWwindow* window, u32 width, u32 height) {
	struct gl_surface* surface = rtgl_execution_glfw_surface_create(ctx, window);

	if (!surface) {
		return;
	}
	swapchain->surface = surface;
	rtgl_swapchain_create_images(ctx, swapchain, width, height);
	rtgl_printf("rt-opengl: swapchain bound to surface (%ux%u)\n", width, height);
}
