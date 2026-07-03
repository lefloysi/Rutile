#pragma once

#include "texture.hpp"

inline constexpr u32 RTDX_MAX_FRAMEBUFFER_COLOR_ATTACHMENTS = 8;

struct rtdx_framebuffer {
	rtdx_resource_base base;
	rtdx_texture_view* color_views[RTDX_MAX_FRAMEBUFFER_COLOR_ATTACHMENTS];
	rtdx_texture_view* depth_view;
	rtdx_texture_view* stencil_view;
	u32 color_texture_count;
};
RTDX_DECLARE_NEW_RESOURCE(framebuffer)

RTDX_API rt_framebuffer rtFramebufferCreate();
RTDX_API void rtFramebufferDestroy(rt_framebuffer framebuffer);
RTDX_API rt_texture_view rtFramebufferColorView(rt_framebuffer framebuffer, u32 slot);
RTDX_API void rtFramebufferSetColorView(rt_framebuffer framebuffer, u32 slot, rt_texture_view view);
RTDX_API void rtFramebufferDepthView(rt_framebuffer framebuffer, rt_texture_view view);

bool rtdx_texture_view_valid(rtdx_texture_view* view);
void rtdx_framebuffer_set_color_view(rtdx_context* ctx, rtdx_framebuffer* framebuffer, u32 slot, rtdx_texture_view* view);
void rtdx_framebuffer_set_depth_view(rtdx_context* ctx, rtdx_framebuffer* framebuffer, rtdx_texture_view* view);
rtdx_texture_view* rtdx_framebuffer_color_view(rtdx_framebuffer* framebuffer, u32 slot);
bool rtdx_framebuffer_valid(rtdx_framebuffer* framebuffer);
