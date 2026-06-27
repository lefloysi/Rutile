#ifndef RTGL33_PLATFORM_CONTEXT_H
#define RTGL33_PLATFORM_CONTEXT_H

#include "config.h"
#include "types.h"

/*===============================================================================================*/
/*                                                                                               */
/*===============================================================================================*/

typedef struct gl_context gl_context;
typedef void *rtgl_proc_t;
typedef rtgl_proc_t (*PFN_rtglLoadProc)(void *userptr, const char *name);

gl_context *rtgl_create_glcontext(u08 major, u08 minor, bool core_profile, gl_context *share);
void rtgl_destroy_glcontext(gl_context *context);
void rtgl_make_glcontext_current(gl_context *context);
rtgl_proc_t rtgl_load_proc(const char *name);
void rtgl_release_current_context(void);

#endif /* RTGL33_PLATFORM_CONTEXT_H */
