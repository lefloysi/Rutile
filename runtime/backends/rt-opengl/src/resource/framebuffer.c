#include "framebuffer.h"

#include "context.h"
#include "error.h"
#include "execution.h"

/*===============================================================================================*/
/*                                                                                               */
/*===============================================================================================*/

rt_framebuffer rtFramebufferCreate(void) {
	return rtgl_framebuffer_to_handle(rtgl_framebuffer_create(rtgl_get_current_context()));
}

void rtFramebufferDestroy(rt_framebuffer framebuffer) {
	rtgl_framebuffer_destroy(rtgl_get_current_context(), rtgl_framebuffer_from_handle(framebuffer));
}

rt_texture_view rtFramebufferColorView(rt_framebuffer framebuffer, u32 slot) {
	return rtgl_texture_view_to_handle(rtgl_framebuffer_color_view(rtgl_framebuffer_from_handle(framebuffer), slot));
}

void rtFramebufferSetColorView(rt_framebuffer framebuffer, u32 slot, rt_texture_view view) {
	rtgl_framebuffer_set_color_view(rtgl_get_current_context(), rtgl_framebuffer_from_handle(framebuffer), slot, rtgl_texture_view_from_handle(view));
}

void rtFramebufferDepthView(rt_framebuffer framebuffer, rt_texture_view view) {
	rtgl_framebuffer_set_depth_view(rtgl_get_current_context(), rtgl_framebuffer_from_handle(framebuffer), rtgl_texture_view_from_handle(view));
}

/*===============================================================================================*/
/*                                                                                               */
/*===============================================================================================*/

RTGL_DEFINE_RESOURCE_PRIVATE(framebuffer)

void rtgl_framebuffer_init(struct rtgl_context* ctx, struct rtgl_framebuffer* framebuffer) {
	rtgl_init_resource_base(ctx, RTGL_RESOURCE_BASE(framebuffer), RTGL_RESOURCE_FRAMEBUFFER);
	rtgl_execution_framebuffer_create(ctx, framebuffer);
}

void rtgl_framebuffer_finish(struct rtgl_framebuffer* framebuffer) {
	rtgl_execution_framebuffer_delete(framebuffer->base.ctx, framebuffer);
	for (u32 i = 0; i < framebuffer->color_texture_count; i++) {
		rtgl_release_resource(framebuffer->color_views[i]);
		framebuffer->color_views[i] = NULL;
	}
	framebuffer->color_texture_count = 0;
	rtgl_release_resource(framebuffer->depth_view);
	framebuffer->depth_view = NULL;
	rtgl_release_resource(framebuffer->stencil_view);
	framebuffer->stencil_view = NULL;
	rtgl_finish_resource_base(RTGL_RESOURCE_BASE(framebuffer));
}

struct rtgl_texture_view* rtgl_framebuffer_color_view(struct rtgl_framebuffer* framebuffer, u32 slot) {
	return slot < framebuffer->color_texture_count ? framebuffer->color_views[slot] : NULL;
}

void rtgl_framebuffer_set_color_view(struct rtgl_context* ctx, struct rtgl_framebuffer* framebuffer, u32 slot, struct rtgl_texture_view* view) {
	if (view) {
		rtgl_retain_resource(view);
	}
	rtgl_release_resource(framebuffer->color_views[slot]);
	framebuffer->color_views[slot] = view;
	if (view && slot >= framebuffer->color_texture_count) {
		framebuffer->color_texture_count = slot + 1;
	}
	while (framebuffer->color_texture_count && !framebuffer->color_views[framebuffer->color_texture_count - 1]) {
		framebuffer->color_texture_count--;
	}
	rtgl_execution_framebuffer_attach_color(ctx, framebuffer, slot, view);
}

void rtgl_framebuffer_set_depth_view(struct rtgl_context* ctx, struct rtgl_framebuffer* framebuffer, struct rtgl_texture_view* view) {
	if (view) {
		rtgl_retain_resource(view);
	}
	rtgl_release_resource(framebuffer->depth_view);
	framebuffer->depth_view = view;
	rtgl_execution_framebuffer_attach_depth(ctx, framebuffer, view);
}

bool rtgl_framebuffer_valid(struct rtgl_framebuffer* framebuffer) {
	return framebuffer && framebuffer->gl_framebuffer && framebuffer->color_texture_count && rtgl_texture_view_valid(framebuffer->color_views[0]);
}
