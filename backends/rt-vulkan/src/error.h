#ifndef RTVK_ERROR_H
#define RTVK_ERROR_H

#include "config.h"
#include "types.h"

#include <stdarg.h>
#include <vulkan/vulkan.h>

RTVK_EXTERN_C_ENTER

/*===============================================================================================*/
/*                                                                                               */
/*===============================================================================================*/

RTVK_API void rtSetOutput(PFN_rtOutput output, void* user_data);
RTVK_API enum rt_error rtError(void);
RTVK_API const char* rtErrorMessage(void);
RTVK_API void rtClearError(void);

/*===============================================================================================*/
/*                                                                                               */
/*===============================================================================================*/

RTVK_API void rtvk_printf(const char* format, ...);
RTVK_API void rtvk_vprintf(const char* format, va_list args);
RTVK_API void rtvk_throwf(enum rt_error error, const char* format, ...);
RTVK_API enum rt_error rtvk_error_from_vk(VkResult result);
RTVK_API enum rt_error rtvk_error(void);

#define RTVK_CHECK_ALLOC(ptr, bytes, what)                  \
	do {                                                    \
		if (!(ptr)) {                                       \
			rtvk_throwf(                                     \
				RT_OUT_OF_HOST_MEMORY,                       \
				"failed to allocate %zu bytes for %s",       \
				(usize)(bytes),                               \
				(what));                                     \
		}                                                   \
	} while (0)

RTVK_EXTERN_C_EXIT
#endif /* RTVK_ERROR_H */


