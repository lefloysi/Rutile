#ifndef RTGL_FRAMEBUFFER_H
#define RTGL_FRAMEBUFFER_H

#include "texture.h"

/*===============================================================================================*/
/*                                                                                               */
/*===============================================================================================*/

#define RTGL_MAX_FRAMEBUFFER_COLOR_ATTACHMENTS 8

RTGL_API rt_framebuffer rtFramebufferCreate(void);
RTGL_API void rtFramebufferDestroy(rt_framebuffer framebuffer);
RTGL_API rt_texture_view rtFramebufferColorView(rt_framebuffer framebuffer, u32 slot);
RTGL_API void rtFramebufferSetColorView(rt_framebuffer framebuffer, u32 slot, rt_texture_view view);
RTGL_API void rtFramebufferDepthView(rt_framebuffer framebuffer, rt_texture_view view);

/*===============================================================================================*/
/*                                                                                               */
/*===============================================================================================*/

struct rtgl_framebuffer {
	struct rtgl_resource_base base;
	struct rtgl_texture_view* color_views[RTGL_MAX_FRAMEBUFFER_COLOR_ATTACHMENTS];
	struct rtgl_texture_view* depth_view;
	struct rtgl_texture_view* stencil_view;
	GLuint gl_framebuffer;
	u32 color_texture_count;
};
RTGL_DECLARE_NEW_RESOURCE(framebuffer)

struct rtgl_texture_view* rtgl_framebuffer_color_view(struct rtgl_framebuffer* framebuffer, u32 slot);
void rtgl_framebuffer_set_color_view(struct rtgl_context* ctx, struct rtgl_framebuffer* framebuffer, u32 slot, struct rtgl_texture_view* view);
void rtgl_framebuffer_set_depth_view(struct rtgl_context* ctx, struct rtgl_framebuffer* framebuffer, struct rtgl_texture_view* view);
bool rtgl_framebuffer_valid(struct rtgl_framebuffer* framebuffer);

#endif /* RTGL_FRAMEBUFFER_H */
