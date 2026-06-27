#ifndef RTDX_ERROR_H
#define RTDX_ERROR_H

#include "config.h"
#include "types.h"

#include <stdarg.h>
#include <windows.h>

/*===============================================================================================*/
/*                                                                                               */
/*===============================================================================================*/

RTDX_EXTERN_C_ENTER

RTDX_API void rtSetOutput(PFN_rtOutput output, void* user_data);
RTDX_API enum rt_error rtError(void);
RTDX_API const char* rtErrorMessage(void);
RTDX_API void rtClearError(void);

RTDX_API void rtdx_printf(const char* format, ...);
RTDX_API void rtdx_vprintf(const char* format, va_list args);
RTDX_API void rtdx_throwf(enum rt_error error, const char* format, ...);
RTDX_API enum rt_error rtdx_error_from_hresult(HRESULT result);
RTDX_API const char* rtdx_hresult_name(HRESULT result);

RTDX_EXTERN_C_EXIT
#endif /* RTDX_ERROR_H */
