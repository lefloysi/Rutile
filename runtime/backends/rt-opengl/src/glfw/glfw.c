#include "glfw/glfw.h"

#include "error.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <assert.h>

typedef struct rtgl_glfw_procs {
	HWND (*glfwGetWin32Window)(GLFWwindow* window);
	void (*glfwGetFramebufferSize)(GLFWwindow* window, int* width, int* height);
} rtgl_glfw_procs;

static rtgl_glfw_procs glfw_procs;

struct gl_surface* rtgl_create_wgl_surface(struct gl_context* context, HWND window);

static HMODULE rtgl_glfw_module(void) {
	HMODULE module = GetModuleHandleA("glfw3.dll");
	return module ? module : GetModuleHandleA(NULL);
}

static void* rtgl_glfw_load_proc(HMODULE module, const char* name) {
	return module ? (void*)GetProcAddress(module, name) : NULL;
}

#define RTGL_GLFW_LOAD_PROC(cast, name)                                                             \
	do {                                                                                            \
		glfw_procs.name = cast rtgl_glfw_load_proc(module, #name);                                  \
		if (!glfw_procs.name) {                                                                     \
			rtgl_throwf(RT_UNSUPPORTED_PLATFORM, "%s not found in executable or loaded glfw3.dll", #name); \
			return;                                                                                 \
		}                                                                                           \
	} while (0)

void rtgl_init_glfw_platform(void) {
	HMODULE module;

	if (glfw_procs.glfwGetWin32Window && glfw_procs.glfwGetFramebufferSize) {
		return;
	}

	module = rtgl_glfw_module();
	RTGL_GLFW_LOAD_PROC((HWND (*)(GLFWwindow*)), glfwGetWin32Window);
	RTGL_GLFW_LOAD_PROC((void (*)(GLFWwindow*, int*, int*)), glfwGetFramebufferSize);
}

#undef RTGL_GLFW_LOAD_PROC

HWND glfwGetWin32Window(GLFWwindow* window) {
	assert(glfw_procs.glfwGetWin32Window);
	assert(window);
	return glfw_procs.glfwGetWin32Window(window);
}

struct gl_surface* rtgl_create_glfw_surface(struct gl_context* context, GLFWwindow* window) {
	return rtgl_create_wgl_surface(context, glfwGetWin32Window(window));
}

void glfwGetFramebufferSize(GLFWwindow* window, int* width, int* height) {
	assert(glfw_procs.glfwGetFramebufferSize);
	assert(window);
	glfw_procs.glfwGetFramebufferSize(window, width, height);
}
