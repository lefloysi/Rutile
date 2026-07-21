#pragma once

#include "config.hpp"
#include "error.hpp"
#include "resource/buffer.hpp"
#include "resource/command_buffer.hpp"
#include "glfw/swapchain.hpp"
#include "resource/graphics_program.hpp"
#include "resource/queue.hpp"
#include "resource/swapchain.hpp"
#include "resource/texture.hpp"
#include "types.hpp"

RTDX_API void rtInit(const char* const* features, u32 feature_count);
RTDX_API void rtExit();
RTDX_API const char* rtGetName();
RTDX_API rt_format_usage rtQueryFormatCapabilities(rt_format format);
