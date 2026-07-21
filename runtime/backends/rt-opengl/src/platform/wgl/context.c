#include "platform/context.h"

#include "config.h"
#include "error.h"

#include <stdlib.h>
#include <windows.h>

/*
** struct gl_context holds the real WGL context plus the bootstrap
** drawable it was created against. The bootstrap window is a
** message-only window (HWND_MESSAGE parent) - it can never be shown,
** needs no monitor, and works identically whether or not this process
** ever attaches a real window later. This is why "presentation" does
** not change anything about how the context itself is built on
** Windows - only Linux's EGL platform choice cares about it.
*/
struct gl_context {
	HWND bootstrap_window;
	HDC bootstrap_dc;
	HGLRC hglrc;
	int pixel_format;
	PIXELFORMATDESCRIPTOR pfd;
};

/*
** struct gl_surface wraps a real, caller-owned native window. It only
** ever exists once the caller actually has a window to attach - see
** platform/context.h for why there is no such thing as a "headless
** surface".
*/
struct gl_surface {
	HWND window;
	HDC dc;
};

typedef HGLRC(WINAPI* PFN_wglCreateContextAttribsARB)(HDC, HGLRC, const int*);

/*
** Tracks the context current on this thread. WGL has no way to go
** from an HGLRC back to our gl_context wrapper, so we keep our own
** thread-local record instead of asking the driver.
*/
static __declspec(thread) struct gl_context* rtgl_tls_current_context = NULL;

static bool rtgl_register_bootstrap_class(void) {
	WNDCLASSA wc = { 0 };
	ATOM atom;

	wc.lpfnWndProc = DefWindowProcA;
	wc.hInstance = GetModuleHandleA(NULL);
	wc.lpszClassName = "rtopengl_bootstrap_window";

	atom = RegisterClassA(&wc);
	if (!atom && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) {
		rtgl_throwf(RT_PLATFORM_FAILURE, "failed to register OpenGL bootstrap window class");
		return false;
	}
	return true;
}

struct gl_context* rtgl_create_glcontext(u08 major, u08 minor, bool presentation, struct gl_context* share) {
	struct gl_context* context = NULL;
	HWND hwnd = NULL;
	HDC hdc = NULL;
	HGLRC legacy = NULL;
	HGLRC hglrc = NULL;
	PFN_wglCreateContextAttribsARB wglCreateContextAttribsARB = NULL;
	PIXELFORMATDESCRIPTOR pfd = { 0 };
	int pixel_format;

	/*
	** presentation is unused here - a message-only window works
	** identically whether or not a real window is attached later,
	** so Windows has no branch to make. It is accepted purely to
	** keep the signature uniform across platforms.
	*/
	(void)presentation;
	rtgl_printf("rt-opengl: WGL creating hidden bootstrap window\n");

	if (!rtgl_register_bootstrap_class()) {
		goto fail;
	}

	/*
	** HWND_MESSAGE makes this a message-only window: it cannot be
	** shown, has no taskbar entry, no z-order, and needs no display
	** attached. It still yields a normal HWND/HDC pair that WGL
	** accepts everywhere a real window's would be accepted.
	*/
	hwnd = CreateWindowExA(0, "rtopengl_bootstrap_window", "", 0, 0, 0, 0, 0, HWND_MESSAGE, NULL, GetModuleHandleA(NULL), NULL);
	if (!hwnd) {
		rtgl_throwf(RT_PLATFORM_FAILURE, "failed to create OpenGL bootstrap window");
		goto fail;
	}

	hdc = GetDC(hwnd);
	if (!hdc) {
		rtgl_throwf(RT_PLATFORM_FAILURE, "failed to acquire OpenGL bootstrap device context");
		goto fail;
	}

	pfd.nSize = sizeof(pfd);
	pfd.nVersion = 1;
	pfd.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
	pfd.iPixelType = PFD_TYPE_RGBA;
	pfd.cColorBits = 32;
	pfd.cAlphaBits = 8;
	pfd.cDepthBits = 24;
	pfd.cStencilBits = 8;

	pixel_format = ChoosePixelFormat(hdc, &pfd);
	if (!pixel_format || !SetPixelFormat(hdc, pixel_format, &pfd)) {
		rtgl_throwf(RT_PLATFORM_FAILURE, "failed to choose a OpenGL pixel format");
		goto fail;
	}

	legacy = wglCreateContext(hdc);
	if (!legacy) {
		rtgl_throwf(RT_PLATFORM_FAILURE, "failed to create bootstrap GL context");
		goto fail;
	}

	if (!wglMakeCurrent(hdc, legacy)) {
		rtgl_throwf(RT_PLATFORM_FAILURE, "failed to make bootstrap GL context current");
		goto fail;
	}

	wglCreateContextAttribsARB = (PFN_wglCreateContextAttribsARB)wglGetProcAddress("wglCreateContextAttribsARB");
	if (!wglCreateContextAttribsARB) {
		rtgl_throwf(RT_PLATFORM_FAILURE, "driver does not support wglCreateContextAttribsARB");
		goto fail;
	}
	rtgl_printf("rt-opengl: WGL bootstrap context active, creating GL %u.%u core context\n", major, minor);

	{
		HGLRC share_hglrc = share ? share->hglrc : NULL;
		const int attribs[] = {
			0x2091 /* WGL_CONTEXT_MAJOR_VERSION_ARB */, major, 0x2092 /* WGL_CONTEXT_MINOR_VERSION_ARB */, minor, 0x9126 /* WGL_CONTEXT_PROFILE_MASK_ARB */, 0x00000001 /* WGL_CONTEXT_CORE_PROFILE_BIT_ARB */, 0
		};

		hglrc = wglCreateContextAttribsARB(hdc, share_hglrc, attribs);
		if (!hglrc) {
			rtgl_throwf(RT_PLATFORM_FAILURE, "failed to create GL %u.%u core context", major, minor);
			goto fail;
		}

		wglMakeCurrent(NULL, NULL);
		wglDeleteContext(legacy);
		legacy = NULL;

		if (!wglMakeCurrent(hdc, hglrc)) {
			rtgl_throwf(RT_PLATFORM_FAILURE, "failed to activate GL %u.%u context", major, minor);
			goto fail;
		}
	}

	context = calloc(1, sizeof(struct gl_context));
	RTGL_CHECK_ALLOC(context, sizeof(struct gl_context), "OpenGL platform context");
	if (!context) {
		goto fail;
	}

	context->bootstrap_window = hwnd;
	context->bootstrap_dc = hdc;
	context->hglrc = hglrc;
	context->pixel_format = pixel_format;
	context->pfd = pfd;

	rtgl_tls_current_context = context;
	rtgl_printf("rt-opengl: WGL context created (pixel_format=%d)\n", pixel_format);
	return context;

fail:
	if (legacy) {
		if (wglGetCurrentContext() == legacy) {
			wglMakeCurrent(NULL, NULL);
		}
		wglDeleteContext(legacy);
	}
	if (hglrc) {
		if (wglGetCurrentContext() == hglrc) {
			wglMakeCurrent(NULL, NULL);
		}
		wglDeleteContext(hglrc);
	}
	if (hdc && hwnd) {
		ReleaseDC(hwnd, hdc);
	}
	if (hwnd) {
		DestroyWindow(hwnd);
	}
	return NULL;
}

void rtgl_destroy_glcontext(struct gl_context* context) {
	if (!context) {
		return;
	}
	if (wglGetCurrentContext() == context->hglrc) {
		wglMakeCurrent(NULL, NULL);
		rtgl_tls_current_context = NULL;
	}
	if (context->hglrc) {
		wglDeleteContext(context->hglrc);
	}
	if (context->bootstrap_dc && context->bootstrap_window) {
		ReleaseDC(context->bootstrap_window, context->bootstrap_dc);
	}
	if (context->bootstrap_window) {
		DestroyWindow(context->bootstrap_window);
	}
	rtgl_printf("rt-opengl: WGL context destroyed\n");
	free(context);
}

struct gl_context* rtgl_get_current_glcontext(void) {
	return rtgl_tls_current_context;
}

/*
** window is an HWND on this platform. The window's
** pixel format is set to match the context's exactly - a WGL
** requirement, not a stylistic choice: a context can only be made
** current against a drawable whose pixel format is compatible with
** the one it was created with.
*/
struct gl_surface* rtgl_create_wgl_surface(struct gl_context* context, HWND window) {
	struct gl_surface* surface;
	HDC hdc;

	hdc = GetDC(window);
	if (!hdc) {
		rtgl_throwf(RT_PLATFORM_FAILURE, "failed to acquire device context for window surface");
		return NULL;
	}

	if (!SetPixelFormat(hdc, context->pixel_format, &context->pfd)) {
		/*
		** SetPixelFormat can only be called once per HDC for the
		** lifetime of the window. If the caller already set a
		** compatible format themselves this fails harmlessly;
		** anything else is a real mismatch.
		*/
		if (GetPixelFormat(hdc) != context->pixel_format) {
			rtgl_throwf(RT_PLATFORM_FAILURE, "failed to set matching pixel format on window surface");
			ReleaseDC(window, hdc);
			return NULL;
		}
	}

	surface = calloc(1, sizeof(struct gl_surface));
	RTGL_CHECK_ALLOC(surface, sizeof(struct gl_surface), "OpenGL window surface");
	if (!surface) {
		ReleaseDC(window, hdc);
		return NULL;
	}

	surface->window = window;
	surface->dc = hdc;
	rtgl_printf("rt-opengl: WGL window surface created (hwnd=%p, pixel_format=%d)\n", (void*)window, context->pixel_format);
	return surface;
}

void rtgl_destroy_glsurface(struct gl_surface* surface) {
	if (!surface) {
		return;
	}
	if (surface->dc && surface->window) {
		ReleaseDC(surface->window, surface->dc);
	}
	rtgl_printf("rt-opengl: WGL window surface destroyed\n");
	free(surface);
}

/*
** surface == NULL means headless: fall back to the bootstrap drawable
** owned by the context itself, never exposed to the caller.
*/
void rtgl_make_glcontext_current(struct gl_context* context, struct gl_surface* surface) {
	if (!context) {
		wglMakeCurrent(NULL, NULL);
		rtgl_tls_current_context = NULL;
		return;
	}

	HDC hdc = surface ? surface->dc : context->bootstrap_dc;
	if (!wglMakeCurrent(hdc, context->hglrc)) {
		rtgl_throwf(RT_PLATFORM_FAILURE, "failed to make OpenGL context current");
		return;
	}
	rtgl_tls_current_context = context;
}

void rtgl_release_current_context(void) {
	wglMakeCurrent(NULL, NULL);
	rtgl_tls_current_context = NULL;
}

/*
** context is required to be current on the calling thread before
** this is called - a WGL constraint, applied universally per
** platform/context.h so no platform-specific exception exists.
*/
rtgl_proc_t rtgl_load_proc(struct gl_context* context, const char* name) {
	rtgl_proc_t proc;

	(void)context;

	proc = (rtgl_proc_t)wglGetProcAddress(name);
	if (proc) {
		return proc;
	}
	/*
	** wglGetProcAddress only resolves entry points beyond GL 1.1;
	** anything the ICD exports directly (older core functions) has
	** to come from the DLL itself.
	*/
	return (rtgl_proc_t)GetProcAddress(GetModuleHandleA("opengl32.dll"), name);
}
