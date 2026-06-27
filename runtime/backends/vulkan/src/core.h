#ifndef RTVK_CORE_H
#define RTVK_CORE_H

#include "config.h"
#include "error.h"
#include "resource/buffer.h"
#include "resource/command_buffer.h"
#include "resource/graphics_program.h"
#include "resource/queue.h"
#include "resource/swapchain.h"
#include "resource/texture.h"
#include "types.h"

/*===============================================================================================*/
/*                                                                                               */
/*===============================================================================================*/

RTVK_API void rtInit(const char* const* features, u32 feature_count);
RTVK_API void rtExit(void);
RTVK_API const char* rtGetName(void);
RTVK_API enum rt_format_usage rtQueryFormatCapabilities(enum rt_format format);

#endif /* RTVK_CORE_H */
