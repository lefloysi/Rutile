#include "ext/glfw/swapchain.h"

#include "context.h"
#include "error.h"

#include <assert.h>
#include <stdint.h>

bool rtInit_RT_EXT_GLFW(void) {
	rtgl_init_glfw_platform();
	return rtgl_error() == RT_SUCCESS;
}

static bool rtgl_glfw_extent_fits_u32(int width, int height) {
	if (width <= 0 || height <= 0) {
		rtgl_throwf(RT_IMPROPER_USAGE, "glfwGetFramebufferSize returned non-positive extent (%dx%d)", width, height);
		return false;
	}
	if ((uint64_t)width > UINT32_MAX || (uint64_t)height > UINT32_MAX) {
		rtgl_throwf(RT_IMPROPER_USAGE, "glfwGetFramebufferSize returned extent outside u32 range (%dx%d)", width, height);
		return false;
	}
	return true;
}

void rtSwapchainBindWindowGLFW(rt_swapchain swapchain, GLFWwindow* window) {
	int width = 0;
	int height = 0;

	if (!window) {
		rtgl_throwf(RT_IMPROPER_USAGE, "rtSwapchainBindWindowGLFW window is NULL");
		return;
	}
	glfwGetFramebufferSize(window, &width, &height);
	if (!rtgl_glfw_extent_fits_u32(width, height)) {
		return;
	}
	rtgl_swapchain_bind_window_glfw(
		rtgl_get_current_context(),
		rtgl_swapchain_from_handle(swapchain),
		window,
		(u32)width,
		(u32)height
	);
}

typedef struct rtgl_glfw_surface_job {
	GLFWwindow* window;
	struct gl_surface* surface;
} rtgl_glfw_surface_job;

static void rtgl_glfw_exec_create_surface(struct rtgl_context* ctx, void* data) {
	rtgl_glfw_surface_job* job = (rtgl_glfw_surface_job*)data;
	job->surface = rtgl_create_glfw_surface(ctx->gl_context, job->window);
}

void rtgl_swapchain_bind_window_glfw(struct rtgl_context* ctx, struct rtgl_swapchain* swapchain, GLFWwindow* window, u32 width, u32 height) {
	rtgl_glfw_surface_job job;

	assert(ctx);
	assert(swapchain);
	assert(window);
	assert(width != 0);
	assert(height != 0);

	job.window = window;
	job.surface = NULL;
	rtgl_context_execute(ctx, rtgl_glfw_exec_create_surface, &job);
	if (!job.surface) {
		return;
	}
	rtgl_swapchain_init_from_surface(ctx, swapchain, job.surface, width, height);
}
