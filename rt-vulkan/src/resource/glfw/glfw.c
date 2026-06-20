#include "resource/glfw/glfw.h"
#include "context.h"
#include "error.h"

#if defined(_WIN32)
#  include <windows.h>
#else
#  include <dlfcn.h>
#endif

typedef VkResult (*PFN_rtvk_glfwCreateWindowSurface)(
	VkInstance instance,
	GLFWwindow* window,
	const VkAllocationCallbacks* allocator,
	VkSurfaceKHR* surface);
typedef void (*PFN_rtvk_glfwGetFramebufferSize)(GLFWwindow* window, int* width, int* height);

typedef struct rtvk_glfw_procs {
	PFN_rtvk_glfwCreateWindowSurface create_window_surface;
	PFN_rtvk_glfwGetFramebufferSize get_framebuffer_size;
	bool resolved;
} rtvk_glfw_procs;

static rtvk_glfw_procs rtvk_glfw;

/*===============================================================================================*/
/*                                                                                               */
/*===============================================================================================*/

void rtSwapchainBindWindowGLFW(rt_swapchain swapchain, GLFWwindow* window) {
	rtvk_swapchain_bind_window_glfw(
		rtvk_get_current_context(),
		rtvk_swapchain_from_handle(swapchain),
		window
	);
}

/*===============================================================================================*/
/*                                                                                               */
/*===============================================================================================*/

static void* rtvk_glfw_symbol(const char* name) {
#if defined(_WIN32)
	HMODULE module = GetModuleHandleA(NULL);
	void* symbol = module ? (void*)GetProcAddress(module, name) : NULL;
	if (symbol) { return symbol; }
	module = GetModuleHandleA("glfw3.dll");
	if (!module) {
		module = LoadLibraryA("glfw3.dll");
	}
	return module ? (void*)GetProcAddress(module, name) : NULL;
#else
	void* symbol = dlsym(RTLD_DEFAULT, name);
	if (symbol) { return symbol; }
	void* module = dlopen(NULL, RTLD_LAZY | RTLD_LOCAL);
	symbol = module ? dlsym(module, name) : NULL;
	if (symbol) { return symbol; }
	module = dlopen("libglfw.so.3", RTLD_LAZY | RTLD_LOCAL);
	if (!module) {
		module = dlopen("libglfw.so", RTLD_LAZY | RTLD_LOCAL);
	}
	return module ? dlsym(module, name) : NULL;
#endif
}

static bool rtvk_glfw_resolve(void) {
	if (rtvk_glfw.resolved) {
		return rtvk_glfw.create_window_surface && rtvk_glfw.get_framebuffer_size;
	}

	rtvk_glfw.create_window_surface = (PFN_rtvk_glfwCreateWindowSurface)rtvk_glfw_symbol("glfwCreateWindowSurface");
	rtvk_glfw.get_framebuffer_size = (PFN_rtvk_glfwGetFramebufferSize)rtvk_glfw_symbol("glfwGetFramebufferSize");

	if (!rtvk_glfw.create_window_surface || !rtvk_glfw.get_framebuffer_size) {
		rtvk_throwf(RT_UNSUPPORTED_PLATFORM, "GLFW symbols are not exported by the executable or available from glfw3.dll");
		return false;
	}

	rtvk_glfw.resolved = true;
	return true;
}

void rtvk_swapchain_bind_window_glfw(struct rtvk_context* ctx, struct rtvk_swapchain* swapchain, GLFWwindow* window) {
	VkSurfaceKHR surface;
	int width;
	int height;
	VkResult result;

	if (!ctx || !swapchain || !window) {
		rtvk_throwf(RT_IMPROPER_USAGE, "swapchain, context, and GLFW window must be valid");
		return;
	}
	if (!rtvk_glfw_resolve()) { return; }

	result = rtvk_glfw.create_window_surface(ctx->vk_instance, window, VK_ALLOCATOR, &surface);
	if (result != VK_SUCCESS) {
		rtvk_throwf(rtvk_error_from_vk(result), NULL);
		return;
	}

	rtvk_glfw.get_framebuffer_size(window, &width, &height);
	if (!rtvk_swapchain_create_for_surface(ctx, swapchain, surface, (u32)width, (u32)height)) { return; }
}


