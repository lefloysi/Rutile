#include "framebuffer.h"
#include "context.h"
#include "error.h"

/*===============================================================================================*/
/*                                                                                               */
/*===============================================================================================*/

RTVK_DEFINE_RESOURCE_PRIVATE(framebuffer)

rt_framebuffer rtFramebufferCreate(void) {
	return rtvk_framebuffer_to_handle(rtvk_framebuffer_create(rtvk_get_current_context()));
}

void rtFramebufferDestroy(rt_framebuffer framebuffer) {
	rtvk_framebuffer_destroy(rtvk_get_current_context(), rtvk_framebuffer_from_handle(framebuffer));
}

rt_texture_view rtFramebufferColorView(rt_framebuffer framebuffer, u32 slot) {
	struct rtvk_texture_view* view = rtvk_framebuffer_color_view(rtvk_framebuffer_from_handle(framebuffer), slot);
	return rtvk_texture_view_to_handle(view);
}

void rtFramebufferSetColorView(rt_framebuffer framebuffer, u32 slot, rt_texture_view view) {
	rtvk_framebuffer_set_color_view(
		rtvk_get_current_context(),
		rtvk_framebuffer_from_handle(framebuffer),
		slot,
		rtvk_texture_view_from_handle(view));
}

void rtFramebufferDepthView(rt_framebuffer framebuffer, rt_texture_view view) {
	rtvk_framebuffer_set_depth_view(
		rtvk_get_current_context(),
		rtvk_framebuffer_from_handle(framebuffer),
		rtvk_texture_view_from_handle(view));
}

void rtvk_framebuffer_init(struct rtvk_context* ctx, struct rtvk_framebuffer* framebuffer) {
	rtvk_init_resource_base(ctx, RTVK_RESOURCE_BASE(framebuffer), RT_RESOURCE_FRAMEBUFFER);
}
void rtvk_framebuffer_finish(struct rtvk_context* ctx, struct rtvk_framebuffer* framebuffer) {
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
	rtvk_finish_resource_base(ctx, RTVK_RESOURCE_BASE(framebuffer));
}

bool rtvk_texture_view_valid(struct rtvk_texture_view* view) {
	return view && view->base.type == RT_RESOURCE_TEXTURE_VIEW && view->vk_image_view;
}

void rtvk_framebuffer_set_color_view(struct rtvk_context* ctx, struct rtvk_framebuffer* framebuffer, u32 slot, struct rtvk_texture_view* view) {
	if (slot >= RTVK_MAX_FRAMEBUFFER_COLOR_ATTACHMENTS) {
		rtvk_throwf(RT_UNSUPPORTED_FEATURE, "framebuffer requested color attachment %u, max is %u", slot, RTVK_MAX_FRAMEBUFFER_COLOR_ATTACHMENTS);
		return;
	}

	if (view && !rtvk_texture_view_valid(view)) {
		rtvk_throwf(RT_IMPROPER_USAGE, "framebuffer color texture view is invalid");
		return;
	}
	if (view && !(rtvk_texture_format_aspect(view->vk_format) & VK_IMAGE_ASPECT_COLOR_BIT)) {
		rtvk_throwf(RT_IMPROPER_USAGE, "framebuffer color texture view format has no color aspect");
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
	if (!framebuffer) {
		rtvk_throwf(RT_IMPROPER_USAGE, "framebuffer is NULL");
		return NULL;
	}
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
	if (!framebuffer) {
		rtvk_throwf(RT_IMPROPER_USAGE, "framebuffer is NULL");
		return;
	}
	if (view && !rtvk_texture_view_valid(view)) {
		rtvk_throwf(RT_IMPROPER_USAGE, "framebuffer depth texture view is invalid");
		return;
	}
	if (view && !(rtvk_texture_format_aspect(view->vk_format) & VK_IMAGE_ASPECT_DEPTH_BIT)) {
		rtvk_throwf(RT_IMPROPER_USAGE, "framebuffer depth texture view format has no depth aspect");
		return;
	}
	if (view) {
		rtvk_retain_resource(view);
	}
	if (framebuffer->depth_view) {
		rtvk_release_resource(framebuffer->depth_view);
	}
	framebuffer->depth_view = view;
}

void rtvk_framebuffer_set_stencil_view(struct rtvk_context* ctx, struct rtvk_framebuffer* framebuffer, struct rtvk_texture_view* view) {
	if (view) {
		rtvk_retain_resource(view);
	}
	if (framebuffer->stencil_view) {
		rtvk_release_resource(framebuffer->stencil_view);
	}
	framebuffer->stencil_view = view;
}

bool rtvk_framebuffer_valid(struct rtvk_framebuffer* framebuffer) {
	for (u32 i = 0; i < framebuffer->color_texture_count; i++) {
		if (!rtvk_texture_view_valid(framebuffer->color_views[i])) {
			return false;
		}
	}
	if (framebuffer->depth_view && !rtvk_texture_view_valid(framebuffer->depth_view)) {
		return false;
	}
	if (framebuffer->stencil_view && !rtvk_texture_view_valid(framebuffer->stencil_view)) {
		return false;
	}
	return true;
}
