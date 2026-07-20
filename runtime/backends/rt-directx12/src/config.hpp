#pragma once

#if defined(_WIN32)
#define RTDX_API extern "C" __declspec(dllexport)
#else
#define RTDX_API extern "C" __attribute__((visibility("default")))
#endif

inline constexpr unsigned RTDX_MAX_FRAMES_IN_FLIGHT = 3;
