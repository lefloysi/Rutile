#include "glfw/swapchain.h"

#include "context.h"
#include "error.h"
#include "glfw/glfw.h"
#include "resource/swapchain.h"

void rtSwapchainBindWindowGLFW(rt_swapchain swapchain, GLFWwindow* window) {
	rtvk_swapchain_bind_window_glfw(
		rtvk_get_current_context(),
		rtvk_swapchain_from_handle(swapchain),
		window
	);
}

void rtvk_swapchain_bind_window_glfw(struct rtvk_context* ctx, struct rtvk_swapchain* swapchain, GLFWwindow* window) {
	VkSurfaceKHR surface;
	int width = 0;
	int height = 0;

	if (!ctx || !swapchain || !window) {
		rtvk_throwf(RT_IMPROPER_USAGE, "swapchain, context, and GLFW window must be valid");
		return;
	}

	rtvk_init_glfw_platform();
	if (rtvk_error() != RT_SUCCESS) {
		return;
	}

	surface = rtvk_create_glfw_surface(ctx, window);
	if (rtvk_error() != RT_SUCCESS) {
		return;
	}

	rtvk_glfw_get_framebuffer_size(window, &width, &height);
	if (width <= 0 || height <= 0) {
		rtvk_throwf(
			RT_IMPROPER_USAGE,
			"glfwGetFramebufferSize returned non-positive extent (%dx%d); window may be minimized or not yet shown",
			width,
			height
		);
		return;
	}
	rtvk_swapchain_init_from_surface(ctx, swapchain, surface, (u32)width, (u32)height);
}
