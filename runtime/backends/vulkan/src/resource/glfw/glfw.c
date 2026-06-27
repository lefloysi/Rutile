#include "resource/glfw/glfw.h"
#include "context.h"
#include "error.h"

#include <GLFW/glfw3.h>

void rtSwapchainBindWindowGLFW(rt_swapchain swapchain, GLFWwindow *window) {
	rtvk_swapchain_bind_window_glfw(
		rtvk_get_current_context(),
		rtvk_swapchain_from_handle(swapchain),
		window
	);
}

void rtvk_swapchain_bind_window_glfw(struct rtvk_context *ctx, struct rtvk_swapchain *swapchain, GLFWwindow *window) {
	VkSurfaceKHR surface = VK_NULL_HANDLE;
	int width = 0;
	int height = 0;
	VkResult result;

	if (!ctx || !swapchain || !window) {
		rtvk_throwf(RT_IMPROPER_USAGE, "swapchain, context, and GLFW window must be valid");
		return;
	}

	result = glfwCreateWindowSurface(ctx->vk_instance, window, VK_ALLOCATOR, &surface);
	if (result != VK_SUCCESS) {
		rtvk_throwf(rtvk_error_from_vk(result), NULL);
		return;
	}

	glfwGetFramebufferSize(window, &width, &height);
	rtvk_swapchain_create_for_surface(ctx, swapchain, surface, (u32)width, (u32)height);
}
