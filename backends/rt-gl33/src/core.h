#ifndef RTGL33_CORE_H
#define RTGL33_CORE_H

#include "types.h"
#include "config.h"
#include "error.h"

/*===============================================================================================*/
/*                                                                                               */
/*===============================================================================================*/

RTGL_API void rtInit(const char* const* features, u32 feature_count);
RTGL_API void rtExit(void);
RTGL_API const char* rtGetName(void);
RTGL_API enum rt_format_usage rtQueryFormatCapabilities(enum rt_format format);

#endif /* RTGL33_CORE_H */
