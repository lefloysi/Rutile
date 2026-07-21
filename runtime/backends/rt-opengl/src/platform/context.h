#ifndef RTGL_PLATFORM_CONTEXT_H
#define RTGL_PLATFORM_CONTEXT_H
#include "config.h"
#include "types.h"

/*
** Context management
**
** All contexts are created headless. A context has no notion of a window
** at creation time. If a window is needed later, attach one via
** struct gl_surface and make it current against it.
*/
struct gl_context;

/*
** presentation: whether this context may ever have a window-backed
** surface attached to it. Decided once, at creation, because on Linux
** the underlying EGL display is bound to a platform (X11 / Wayland /
** bare device) at creation time and cannot change afterward. If false,
** the context is permanently headless (e.g. a compute-only backend).
*/
struct gl_context* rtgl_create_glcontext(u08 major, u08 minor, bool presentation, struct gl_context* share);
void rtgl_destroy_glcontext(struct gl_context* context);
struct gl_context* rtgl_get_current_glcontext(void);

/*
** Surface management
**
** A surface represents a real, window-backed drawable - analogous to
** VkSurfaceKHR. There is no such thing as a "headless surface"; headless
** is expressed by passing NULL to make_current. Only create one once a
** native window handle actually exists.
*/
struct gl_surface;

void rtgl_destroy_glsurface(struct gl_surface* surface);

/*
** Current context / surface binding (thread-local)
**
** surface == NULL means headless:
**   EGL   - eglMakeCurrent with EGL_NO_SURFACE
**   macOS - setView:nil
**   WGL   - internal bootstrap drawable owned by gl_context, never
**           exposed here
*/
void rtgl_make_glcontext_current(struct gl_context* context, struct gl_surface* surface);
void rtgl_release_current_context(void);

/*
** Proc loading - requires a context current on the calling thread (a
** WGL constraint, applied universally here so no platform-specific
** exception exists).
*/
typedef void* rtgl_proc_t;
typedef rtgl_proc_t (*PFN_rtglLoadProc)(void* userptr, const char* name);
rtgl_proc_t rtgl_load_proc(struct gl_context* context, const char* name);

#endif /* RTGL_PLATFORM_CONTEXT_H */
