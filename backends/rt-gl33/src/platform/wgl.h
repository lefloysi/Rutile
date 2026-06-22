#ifndef RTGL33_PLATFORM_WGL_H
#define RTGL33_PLATFORM_WGL_H

#include "../config.h"

#include <windows.h>

RTGL_EXTERN_C_ENTER

/*===============================================================================================*/
/*                                                                                               */
/*===============================================================================================*/

typedef struct rtgl_wgl_context_t* rtgl_wgl_context;

typedef struct rtgl_wgl_context_desc {
	bool shared;
	bool worker;
} rtgl_wgl_context_desc;

/*===============================================================================================*/
/*                                                                                               */
/*===============================================================================================*/

RTGL_API rtgl_wgl_context rtgl_wgl_context_create(void);
RTGL_API rtgl_wgl_context rtgl_wgl_context_create_shared(rtgl_wgl_context share);
RTGL_API void rtgl_wgl_context_destroy(rtgl_wgl_context context);

RTGL_API void rtgl_wgl_context_make_current(rtgl_wgl_context context);
RTGL_API void rtgl_wgl_context_clear_current(void);

RTGL_API bool rtgl_wgl_context_attach_window(rtgl_wgl_context context, HWND window);
RTGL_API void rtgl_wgl_context_detach_window(rtgl_wgl_context context, HWND window);

RTGL_API bool rtgl_wgl_context_owns_window(rtgl_wgl_context context, HWND window);

RTGL_EXTERN_C_EXIT

#endif /* RTGL33_PLATFORM_WGL_H */
