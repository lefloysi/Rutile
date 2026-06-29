#ifndef RTDX_FRAMEBUFFER_H
#define RTDX_FRAMEBUFFER_H

#include "texture.h"

#define RTDX_MAX_FRAMEBUFFER_COLOR_ATTACHMENTS 8

/*===============================================================================================*/
/*                                                                                               */
/*===============================================================================================*/

struct rtdx_framebuffer {
	struct rtdx_resource_base base;
	struct rtdx_texture_view* color_views[RTDX_MAX_FRAMEBUFFER_COLOR_ATTACHMENTS];
	struct rtdx_texture_view* depth_view;
	struct rtdx_texture_view* stencil_view;
	u32 color_texture_count;
};
RTDX_DECLARE_NEW_RESOURCE(framebuffer)

RTDX_EXTERN_C_ENTER
RTDX_API rt_framebuffer rtFramebufferCreate(void);
RTDX_API void rtFramebufferDestroy(rt_framebuffer framebuffer);
RTDX_API rt_texture_view rtFramebufferColorView(rt_framebuffer framebuffer, u32 slot);
RTDX_API void rtFramebufferSetColorView(rt_framebuffer framebuffer, u32 slot, rt_texture_view view);
RTDX_API void rtFramebufferDepthView(rt_framebuffer framebuffer, rt_texture_view view);
bool rtdx_texture_view_valid(struct rtdx_texture_view* view);
RTDX_EXTERN_C_EXIT
void rtdx_framebuffer_set_color_view(struct rtdx_context* ctx, struct rtdx_framebuffer* framebuffer, u32 slot, struct rtdx_texture_view* view);
void rtdx_framebuffer_set_depth_view(struct rtdx_context* ctx, struct rtdx_framebuffer* framebuffer, struct rtdx_texture_view* view);
struct rtdx_texture_view* rtdx_framebuffer_color_view(struct rtdx_framebuffer* framebuffer, u32 slot);
bool rtdx_framebuffer_valid(struct rtdx_framebuffer* framebuffer);

#endif /* RTDX_FRAMEBUFFER_H */
