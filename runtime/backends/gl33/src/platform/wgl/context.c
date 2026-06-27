#include "platform/context.h"

#include "error.h"
#include "config.h"

#include <stdlib.h>
#include <windows.h>

struct gl_context {
	HWND bootstrap_window;
	HDC bootstrap_dc;
	HGLRC hglrc;
};

typedef HGLRC (WINAPI* PFN_wglCreateContextAttribsARB)(HDC, HGLRC, const int*);

static gl_context* rtgl_context_bootstrap(u08 major, u08 minor, bool core_profile, gl_context* share) {
	gl_context* context = NULL;
	HWND hwnd = NULL;
	HDC hdc = NULL;
	HGLRC legacy = NULL;
	HGLRC hglrc = NULL;
	PFN_wglCreateContextAttribsARB wglCreateContextAttribsARB = NULL;
	WNDCLASSA wc = { 0 };
	ATOM atom;

	wc.lpfnWndProc = DefWindowProcA;
	wc.hInstance = GetModuleHandleA(NULL);
	wc.lpszClassName = "rtgl33_hidden_window";
	atom = RegisterClassA(&wc);
	if (!atom && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) {
		rtgl_throwf(RT_PLATFORM_FAILURE, "failed to register hidden GL33 window class");
		goto fail;
	}

	hwnd = CreateWindowExA(0, "rtgl33_hidden_window", "rtgl33_hidden_window", WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, 1, 1, NULL, NULL, GetModuleHandleA(NULL), NULL);
	if (!hwnd) {
		rtgl_throwf(RT_PLATFORM_FAILURE, "failed to create hidden GL33 bootstrap window");
		goto fail;
	}

	hdc = GetDC(hwnd);
	if (!hdc) {
		rtgl_throwf(RT_PLATFORM_FAILURE, "failed to acquire hidden GL33 device context");
		goto fail;
	}

	PIXELFORMATDESCRIPTOR pfd = { 0 };
	pfd.nSize = sizeof(pfd);
	pfd.nVersion = 1;
	pfd.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
	pfd.iPixelType = PFD_TYPE_RGBA;
	pfd.cColorBits = 32;
	pfd.cAlphaBits = 8;
	pfd.cDepthBits = 24;
	pfd.cStencilBits = 8;

	int pixel_format = ChoosePixelFormat(hdc, &pfd);
	if (!pixel_format || !SetPixelFormat(hdc, pixel_format, &pfd)) {
		rtgl_throwf(RT_PLATFORM_FAILURE, "failed to choose a GL33 pixel format");
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

	hglrc = legacy;
	if (wglCreateContextAttribsARB) {
		HGLRC share_hglrc = share ? share->hglrc : NULL;
		const int attribs[] = {
			0x2091 /* WGL_CONTEXT_MAJOR_VERSION_ARB */, major,
			0x2092 /* WGL_CONTEXT_MINOR_VERSION_ARB */, minor,
			0x9126 /* WGL_CONTEXT_PROFILE_MASK_ARB */, core_profile ? 0x00000001 /* WGL_CONTEXT_CORE_PROFILE_BIT_ARB */ : 0,
			0
		};
		HGLRC modern = wglCreateContextAttribsARB(hdc, share_hglrc, attribs);
		if (modern) {
			wglMakeCurrent(NULL, NULL);
			wglDeleteContext(legacy);
			hglrc = modern;
			if (!wglMakeCurrent(hdc, hglrc)) {
				rtgl_throwf(RT_PLATFORM_FAILURE, "failed to activate GL 3.3 context");
				goto fail;
			}
		}
	}

	context = calloc(1, sizeof(gl_context));
	RTGL_CHECK_ALLOC(context, sizeof(gl_context), "GL33 platform context");
	if (!context) {
		goto fail;
	}

	context->bootstrap_window = hwnd;
	context->bootstrap_dc = hdc;
	context->hglrc = hglrc;
	return context;

fail:
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

gl_context* rtgl_create_glcontext(u08 major, u08 minor, bool core_profile, gl_context* share) {
	return rtgl_context_bootstrap(major, minor, core_profile, share);
}

void rtgl_destroy_glcontext(gl_context* context) {
	if (!context) { return; }
	if (wglGetCurrentContext() == context->hglrc) {
		wglMakeCurrent(NULL, NULL);
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
	free(context);
}

void rtgl_make_glcontext_current(gl_context* context) {
	if (!context) {
		wglMakeCurrent(NULL, NULL);
		return;
	}
	wglMakeCurrent(context->bootstrap_dc, context->hglrc);
}

void rtgl_release_current_context(void) {
	wglMakeCurrent(NULL, NULL);
}

rtgl_proc_t rtgl_load_proc(const char* name) {
	rtgl_proc_t proc = (rtgl_proc_t)wglGetProcAddress(name);
	if (proc) { return proc; }
	return (rtgl_proc_t)GetProcAddress(GetModuleHandleA("opengl32.dll"), name);
}
