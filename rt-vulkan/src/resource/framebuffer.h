#ifndef RTVK_FRAMEBUFFER_H
#define RTVK_FRAMEBUFFER_H

#include "texture.h"

#define RTVK_MAX_FRAMEBUFFER_COLOR_ATTACHMENTS 8

/*===============================================================================================*/
/*                                                                                               */
/*===============================================================================================*/

RTVK_API rt_framebuffer rtFramebufferCreate(void);
RTVK_API void rtFramebufferDestroy(rt_framebuffer framebuffer);
RTVK_API rt_texture_view rtFramebufferColorView(rt_framebuffer framebuffer, u32 slot);
RTVK_API void rtFramebufferSetColorView(rt_framebuffer framebuffer, u32 slot, rt_texture_view view);
RTVK_API void rtFramebufferDepthView(rt_framebuffer framebuffer, rt_texture_view view);

/*===============================================================================================*/
/*                                                                                               */
/*===============================================================================================*/

struct rtvk_framebuffer {
	struct rtvk_resource_base base;
	struct rtvk_texture_view* color_views[RTVK_MAX_FRAMEBUFFER_COLOR_ATTACHMENTS];
	struct rtvk_texture_view* depth_view;
	struct rtvk_texture_view* stencil_view;

	u32 color_texture_count;
};
RTVK_DECLARE_NEW_RESOURCE(framebuffer)


bool rtvk_texture_view_valid(struct rtvk_texture_view* view);
void rtvk_framebuffer_set_color_view(struct rtvk_context* ctx, struct rtvk_framebuffer* framebuffer, u32 slot, struct rtvk_texture_view* view);
void rtvk_framebuffer_set_depth_view(struct rtvk_context* ctx, struct rtvk_framebuffer* framebuffer, struct rtvk_texture_view* view);
void rtvk_framebuffer_set_stencil_view(struct rtvk_context* ctx, struct rtvk_framebuffer* framebuffer, struct rtvk_texture_view* view);
struct rtvk_texture_view* rtvk_framebuffer_color_view(struct rtvk_framebuffer* framebuffer, u32 slot);
bool rtvk_framebuffer_valid(struct rtvk_framebuffer* framebuffer);

#endif
