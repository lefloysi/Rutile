#include "resource/glfw/glfw.h"
#include "context.h"
#include "error.h"

#include <volk.h>

#if defined(_WIN32)
#  define WIN32_LEAN_AND_MEAN
#  include <windows.h>
#  include <psapi.h>
#else
#  include <dlfcn.h>
#endif

/*
 * rt-vulkan deliberately does NOT link GLFW. The host application brings its
 * own GLFW (statically linked into the executable, or loaded as glfw3.dll),
 * and we resolve the GLFW entry points we need from whatever module the host
 * already has in its address space. This avoids the "two static GLFWs in the
 * same process" trap where each module has its own uninitialised copy of
 * GLFW's global state.
 */

typedef struct GLFWwindow GLFWwindow;

typedef VkResult (*PFN_glfwCreateWindowSurface)(VkInstance, GLFWwindow*, const VkAllocationCallbacks*, VkSurfaceKHR*);
typedef void     (*PFN_glfwGetFramebufferSize)(GLFWwindow*, int*, int*);
typedef int      (*PFN_glfwGetError)(const char**);
typedef int      (*PFN_glfwVulkanSupported)(void);

struct rtvk_glfw_procs {
	int resolved;
	int ok;
	const char* failure_reason;
	PFN_glfwCreateWindowSurface create_window_surface;
	PFN_glfwGetFramebufferSize  get_framebuffer_size;
	PFN_glfwGetError            get_error;
	PFN_glfwVulkanSupported     vulkan_supported;
};

static struct rtvk_glfw_procs g_glfw;

#if defined(_WIN32)
static void* rtvk_glfw_find_proc(const char* name) {
	HMODULE modules[512];
	HANDLE process = GetCurrentProcess();
	DWORD needed = 0;

	/* Look at every module loaded into the process and return the first
	 * one that exports the requested symbol. This finds glfw whether the
	 * EXE linked it statically and re-exported it, or whether it lives in
	 * glfw3.dll, or any other module that re-exports it. */
	if (!EnumProcessModules(process, modules, sizeof(modules), &needed)) {
		return NULL;
	}
	DWORD count = needed / sizeof(HMODULE);
	if (count > (sizeof(modules) / sizeof(modules[0]))) {
		count = (DWORD)(sizeof(modules) / sizeof(modules[0]));
	}
	for (DWORD i = 0; i < count; ++i) {
		FARPROC proc = GetProcAddress(modules[i], name);
		if (proc) {
			return (void*)proc;
		}
	}
	/* Last resort: try glfw3.dll explicitly in case it's lazily loaded later. */
	HMODULE glfw_dll = GetModuleHandleA("glfw3.dll");
	if (glfw_dll) {
		return (void*)GetProcAddress(glfw_dll, name);
	}
	return NULL;
}
#else
static void* rtvk_glfw_find_proc(const char* name) {
	/* RTLD_DEFAULT searches every library already loaded by the process. */
	return dlsym(RTLD_DEFAULT, name);
}
#endif

static void rtvk_glfw_resolve(void) {
	if (g_glfw.resolved) {
		return;
	}
	g_glfw.resolved = 1;

	g_glfw.create_window_surface = (PFN_glfwCreateWindowSurface)rtvk_glfw_find_proc("glfwCreateWindowSurface");
	g_glfw.get_framebuffer_size  = (PFN_glfwGetFramebufferSize)rtvk_glfw_find_proc("glfwGetFramebufferSize");
	g_glfw.get_error             = (PFN_glfwGetError)rtvk_glfw_find_proc("glfwGetError");
	g_glfw.vulkan_supported      = (PFN_glfwVulkanSupported)rtvk_glfw_find_proc("glfwVulkanSupported");

	if (!g_glfw.create_window_surface) {
		g_glfw.failure_reason = "glfwCreateWindowSurface not found in any loaded module - is GLFW linked into the host executable?";
		return;
	}
	if (!g_glfw.get_framebuffer_size) {
		g_glfw.failure_reason = "glfwGetFramebufferSize not found in any loaded module";
		return;
	}
	g_glfw.ok = 1;
}

void rtSwapchainBindWindowGLFW(rt_swapchain swapchain, GLFWwindow* window) {
	rtvk_swapchain_bind_window_glfw(
		rtvk_get_current_context(),
		rtvk_swapchain_from_handle(swapchain),
		window
	);
}

void rtvk_swapchain_bind_window_glfw(struct rtvk_context* ctx, struct rtvk_swapchain* swapchain, GLFWwindow* window) {
	VkSurfaceKHR surface = VK_NULL_HANDLE;
	int width = 0;
	int height = 0;
	VkResult result;

	if (!ctx || !swapchain || !window) {
		rtvk_throwf(RT_IMPROPER_USAGE, "swapchain, context, and GLFW window must be valid");
		return;
	}

	rtvk_glfw_resolve();
	if (!g_glfw.ok) {
		rtvk_throwf(
			RT_UNSUPPORTED_PLATFORM,
			"rt-vulkan could not locate GLFW in this process: %s",
			g_glfw.failure_reason ? g_glfw.failure_reason : "unknown reason"
		);
		return;
	}

	result = g_glfw.create_window_surface(ctx->vk_instance, window, VK_ALLOCATOR, &surface);
	if (result != VK_SUCCESS) {
		const char* glfw_msg = NULL;
		int glfw_code = 0;
		if (g_glfw.get_error) {
			glfw_code = g_glfw.get_error(&glfw_msg);
		}
		rtvk_throwf(
			rtvk_error_from_vk(result),
			"glfwCreateWindowSurface failed: %s (GLFW %d: %s)",
			rtvk_vk_result_name(result),
			glfw_code,
			glfw_msg ? glfw_msg : "no GLFW error message"
		);
		return;
	}

	g_glfw.get_framebuffer_size(window, &width, &height);
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
