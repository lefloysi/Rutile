#pragma once

#include "resource/swapchain.hpp"

#include <windows.h>

bool rtdx_swapchain_create_for_hwnd(rtdx_context* ctx, rtdx_swapchain* swapchain, HWND hwnd, u32 width, u32 height);
