#include "resource/glfw/glfw.h"

#include "context.h"
#include "error.h"
#include "platform/glfw.h"

bool rtInit_RT_EXT_GLFW(void) {
	return rtgl_init_glfw_platform();
}

void rtSwapchainBindWindowGLFW(rt_swapchain swapchain, GLFWwindow* window) {
	rtgl_swapchain_bind_window_glfw(
		rtgl_get_current_context(),
		rtgl_swapchain_from_handle(swapchain),
		window
	);
}

void rtgl_swapchain_bind_window_glfw(struct rtgl_context* ctx, struct rtgl_swapchain* swapchain, GLFWwindow* window) {
	u32 width = 0;
	u32 height = 0;
	struct gl_surface* surface;

	if (!ctx || !swapchain || !window) {
		rtgl_throwf(RT_IMPROPER_USAGE, "swapchain, context, and GLFW window must be valid");
		return;
	}
	surface = rtgl_create_glfw_surface(ctx->gl_context, window, &width, &height);
	if (!surface) {
		return;
	}
	rtgl_swapchain_init_from_surface(ctx, swapchain, surface, width, height);
}
