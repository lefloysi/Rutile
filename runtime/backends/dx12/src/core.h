#ifndef RTDX_CORE_H
#define RTDX_CORE_H

#include "config.h"
#include "error.h"
#include "resource/buffer.h"
#include "resource/command_buffer.h"
#include "resource/glfw/glfw.h"
#include "resource/graphics_program.h"
#include "resource/queue.h"
#include "resource/swapchain.h"
#include "resource/texture.h"
#include "types.h"

/*===============================================================================================*/
/*                                                                                               */
/*===============================================================================================*/

RTDX_EXTERN_C_ENTER

RTDX_API void rtInit(const char *const *features, u32 feature_count);
RTDX_API void rtExit(void);
RTDX_API const char *rtGetName(void);
RTDX_API enum rt_format_usage rtQueryFormatCapabilities(enum rt_format format);

RTDX_EXTERN_C_EXIT
#endif /* RTDX_CORE_H */
