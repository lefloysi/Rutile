#ifndef RTOPENGL_ERROR_H
#define RTOPENGL_ERROR_H

#include "config.h"
#include "types.h"

#include <stdarg.h>

RTGL_EXTERN_C_ENTER

/*===============================================================================================*/
/*                                                                                               */
/*===============================================================================================*/

RTGL_API void rtSetOutput(PFN_rtOutput output, void* user_data);
RTGL_API enum rt_error rtError(void);
RTGL_API const char* rtErrorMessage(void);
RTGL_API void rtClearError(void);

/*===============================================================================================*/
/*                                                                                               */
/*===============================================================================================*/

RTGL_API void rtgl_printf(const char* format, ...);
RTGL_API void rtgl_vprintf(const char* format, va_list args);
RTGL_API void rtgl_throwf(enum rt_error error, const char* format, ...);
void rtgl_set_output(PFN_rtOutput output, void* user_data);
enum rt_error rtgl_error();
const char* rtgl_error_message();
void rtgl_clear_error();

RTGL_EXTERN_C_EXIT
#endif /* RTOPENGL_ERROR_H */
