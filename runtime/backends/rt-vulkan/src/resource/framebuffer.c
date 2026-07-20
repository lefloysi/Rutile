#include "framebuffer.h"
#include "context.h"
#include "error.h"
#include <assert.h>

/*===============================================================================================*/
/*                                                                                               */
/*===============================================================================================*/

rt_framebuffer rtFramebufferCreate(void) {
	return rtvk_framebuffer_to_handle(rtvk_framebuffer_create(rtvk_get_current_context()));
}

void rtFramebufferDestroy(rt_framebuffer framebuffer) {
	rtvk_framebuffer_destroy(
		rtvk_get_current_context(), 
		rtvk_framebuffer_from_handle(framebuffer)
	);
}

rt_texture_view rtFramebufferColorView(rt_framebuffer framebuffer, u32 slot) {
	return rtvk_texture_view_to_handle(rtvk_framebuffer_color_view(rtvk_framebuffer_from_handle(framebuffer), slot));
}

void rtFramebufferSetColorView(rt_framebuffer framebuffer, u32 slot, rt_texture_view view) {
	rtvk_framebuffer_set_color_view(
		rtvk_get_current_context(),
		rtvk_framebuffer_from_handle(framebuffer),
		slot,
		rtvk_texture_view_from_handle(view)
	);
}

void rtFramebufferDepthView(rt_framebuffer framebuffer, rt_texture_view view) {
	rtvk_framebuffer_set_depth_view(
		rtvk_get_current_context(),
		rtvk_framebuffer_from_handle(framebuffer),
		rtvk_texture_view_from_handle(view)
	);
}

/*===============================================================================================*/
/*                                                                                               */
/*===============================================================================================*/

RTVK_DEFINE_RESOURCE_PRIVATE(framebuffer)

void rtvk_framebuffer_init(struct rtvk_context* ctx, struct rtvk_framebuffer* framebuffer) {
	rtvk_init_resource_base(ctx, RTVK_RESOURCE_BASE(framebuffer), RT_RESOURCE_FRAMEBUFFER);
}
void rtvk_framebuffer_finish(struct rtvk_framebuffer* framebuffer) {
	for (u32 i = 0; i < framebuffer->color_texture_count; i++) {
		if (framebuffer->color_views[i]) {
			rtvk_release_resource(framebuffer->color_views[i]);
		}
		framebuffer->color_views[i] = NULL;
	}
	framebuffer->color_texture_count = 0;
	if (framebuffer->depth_view) {
		rtvk_release_resource(framebuffer->depth_view);
	}
	framebuffer->depth_view = NULL;
	if (framebuffer->stencil_view) {
		rtvk_release_resource(framebuffer->stencil_view);
	}
	framebuffer->stencil_view = NULL;
	rtvk_finish_resource_base(RTVK_RESOURCE_BASE(framebuffer));
}

void rtvk_framebuffer_set_color_view(struct rtvk_context* ctx, struct rtvk_framebuffer* framebuffer, u32 slot, struct rtvk_texture_view* view) {
	assert(framebuffer);
	if (slot >= RTVK_MAX_FRAMEBUFFER_COLOR_ATTACHMENTS) {
		rtvk_throwf(RT_UNSUPPORTED_FEATURE, "framebuffer requested color attachment %u, max is %u", slot, RTVK_MAX_FRAMEBUFFER_COLOR_ATTACHMENTS);
		return;
	}

	if (view) {
		rtvk_retain_resource(view);
	}
	if (framebuffer->color_views[slot]) {
		rtvk_release_resource(framebuffer->color_views[slot]);
	}
	framebuffer->color_views[slot] = view;
	if (view && slot >= framebuffer->color_texture_count) {
		framebuffer->color_texture_count = slot + 1;
	}
	while (framebuffer->color_texture_count && !framebuffer->color_views[framebuffer->color_texture_count - 1]) {
		framebuffer->color_texture_count--;
	}
}

struct rtvk_texture_view* rtvk_framebuffer_color_view(struct rtvk_framebuffer* framebuffer, u32 slot) {
	assert(framebuffer);
	if (slot >= RTVK_MAX_FRAMEBUFFER_COLOR_ATTACHMENTS) {
		rtvk_throwf(RT_UNSUPPORTED_FEATURE, "framebuffer requested color attachment %u, max is %u", slot, RTVK_MAX_FRAMEBUFFER_COLOR_ATTACHMENTS);
		return NULL;
	}

	if (slot >= framebuffer->color_texture_count) {
		return NULL;
	}
	return framebuffer->color_views[slot];
}

void rtvk_framebuffer_set_depth_view(struct rtvk_context* ctx, struct rtvk_framebuffer* framebuffer, struct rtvk_texture_view* view) {
	assert(framebuffer);
	if (view) {
		rtvk_retain_resource(view);
	}
	if (framebuffer->depth_view) {
		rtvk_release_resource(framebuffer->depth_view);
	}
	framebuffer->depth_view = view;
}

void rtvk_framebuffer_set_stencil_view(struct rtvk_context* ctx, struct rtvk_framebuffer* framebuffer, struct rtvk_texture_view* view) {
	assert(framebuffer);
	if (view) {
		rtvk_retain_resource(view);
	}
	if (framebuffer->stencil_view) {
		rtvk_release_resource(framebuffer->stencil_view);
	}
	framebuffer->stencil_view = view;
}
