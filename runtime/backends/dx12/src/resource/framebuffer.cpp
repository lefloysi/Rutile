#include "framebuffer.h"
#include "context.h"
#include "error.h"

/*===============================================================================================*/
/*                                                                                               */
/*===============================================================================================*/

RTDX_DEFINE_RESOURCE_PRIVATE(framebuffer)

rt_framebuffer rtFramebufferCreate(void) {
	return rtdx_framebuffer_to_handle(rtdx_framebuffer_create(rtdx_get_current_context()));
}

void rtFramebufferDestroy(rt_framebuffer framebuffer) {
	rtdx_framebuffer_destroy(rtdx_get_current_context(), rtdx_framebuffer_from_handle(framebuffer));
}

rt_texture_view rtFramebufferColorView(rt_framebuffer framebuffer, u32 slot) {
	struct rtdx_texture_view *view = rtdx_framebuffer_color_view(rtdx_framebuffer_from_handle(framebuffer), slot);
	return rtdx_texture_view_to_handle(view);
}

void rtFramebufferSetColorView(rt_framebuffer framebuffer, u32 slot, rt_texture_view view) {
	rtdx_framebuffer_set_color_view(
		rtdx_get_current_context(),
		rtdx_framebuffer_from_handle(framebuffer),
		slot,
		rtdx_texture_view_from_handle(view)
	);
}

void rtFramebufferDepthView(rt_framebuffer framebuffer, rt_texture_view view) {
	rtdx_framebuffer_set_depth_view(
		rtdx_get_current_context(),
		rtdx_framebuffer_from_handle(framebuffer),
		rtdx_texture_view_from_handle(view)
	);
}

/*===============================================================================================*/
/*                                                                                               */
/*===============================================================================================*/

void rtdx_framebuffer_init(struct rtdx_context *ctx, struct rtdx_framebuffer *framebuffer) {
	rtdx_init_resource_base(ctx, RTDX_RESOURCE_BASE(framebuffer), RT_RESOURCE_FRAMEBUFFER);
}

void rtdx_framebuffer_finish(struct rtdx_context *ctx, struct rtdx_framebuffer *framebuffer) {
	for (u32 i = 0; i < framebuffer->color_texture_count; i++) {
		framebuffer->color_views[i] = NULL;
	}
	framebuffer->color_texture_count = 0;
	framebuffer->depth_view = NULL;
	framebuffer->stencil_view = NULL;
	rtdx_finish_resource_base(ctx, RTDX_RESOURCE_BASE(framebuffer));
}

bool rtdx_texture_view_valid(struct rtdx_texture_view *view) {
	return view && view->base.type == RT_RESOURCE_TEXTURE_VIEW && view->d3d_resource;
}

void rtdx_framebuffer_set_color_view(struct rtdx_context *ctx, struct rtdx_framebuffer *framebuffer, u32 slot, struct rtdx_texture_view *view) {
	if (slot >= RTDX_MAX_FRAMEBUFFER_COLOR_ATTACHMENTS) {
		rtdx_throwf(RT_UNSUPPORTED_FEATURE, "framebuffer requested color attachment %u, max is %u", slot, RTDX_MAX_FRAMEBUFFER_COLOR_ATTACHMENTS);
		return;
	}

	if (view && !rtdx_texture_view_valid(view)) {
		rtdx_throwf(RT_IMPROPER_USAGE, "framebuffer color texture view is invalid");
		return;
	}
	if (view && !view->rtv.ptr) {
		rtdx_throwf(RT_IMPROPER_USAGE, "framebuffer color texture view has no render target view");
		return;
	}
	if (view && rtdx_texture_format_is_depth(view->dxgi_format)) {
		rtdx_throwf(RT_IMPROPER_USAGE, "framebuffer color texture view format has no color aspect");
		return;
	}

	framebuffer->color_views[slot] = view;
	if (view && slot >= framebuffer->color_texture_count) {
		framebuffer->color_texture_count = slot + 1;
	}
	while (framebuffer->color_texture_count && !framebuffer->color_views[framebuffer->color_texture_count - 1]) {
		framebuffer->color_texture_count--;
	}
}

struct rtdx_texture_view *rtdx_framebuffer_color_view(struct rtdx_framebuffer *framebuffer, u32 slot) {
	if (!framebuffer) {
		rtdx_throwf(RT_IMPROPER_USAGE, "framebuffer is NULL");
		return nullptr;
	}
	if (slot >= RTDX_MAX_FRAMEBUFFER_COLOR_ATTACHMENTS) {
		rtdx_throwf(RT_UNSUPPORTED_FEATURE, "framebuffer requested color attachment %u, max is %u", slot, RTDX_MAX_FRAMEBUFFER_COLOR_ATTACHMENTS);
		return nullptr;
	}

	if (slot >= framebuffer->color_texture_count) {
		return nullptr;
	}
	return framebuffer->color_views[slot];
}

void rtdx_framebuffer_set_depth_view(struct rtdx_context *ctx, struct rtdx_framebuffer *framebuffer, struct rtdx_texture_view *view) {
	if (!framebuffer) {
		rtdx_throwf(RT_IMPROPER_USAGE, "framebuffer is NULL");
		return;
	}
	if (view && !rtdx_texture_view_valid(view)) {
		rtdx_throwf(RT_IMPROPER_USAGE, "framebuffer depth texture view is invalid");
		return;
	}
	if (view && !view->dsv.ptr) {
		rtdx_throwf(RT_IMPROPER_USAGE, "framebuffer depth texture view has no depth stencil view");
		return;
	}
	if (view && !rtdx_texture_format_is_depth(view->dxgi_format)) {
		rtdx_throwf(RT_IMPROPER_USAGE, "framebuffer depth texture view format has no depth aspect");
		return;
	}
	framebuffer->depth_view = view;
}

bool rtdx_framebuffer_valid(struct rtdx_framebuffer *framebuffer) {
	for (u32 i = 0; i < framebuffer->color_texture_count; i++) {
		if (!rtdx_texture_view_valid(framebuffer->color_views[i])) {
			return false;
		}
	}
	if (framebuffer->depth_view && !rtdx_texture_view_valid(framebuffer->depth_view)) {
		return false;
	}
	if (framebuffer->stencil_view && !rtdx_texture_view_valid(framebuffer->stencil_view)) {
		return false;
	}
	return true;
}
