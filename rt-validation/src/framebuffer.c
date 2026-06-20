#include "framebuffer.h"
#include "logger.h"
#include "texture_view.h"

#define RTVAL_DROP(message) rtval_printf("[validation] %s, dropping call\n", message)

/*===============================================================================================*/
/*                                                                                               */
/*===============================================================================================*/

RT_EXPORT rt_framebuffer rtFramebufferCreate(void) {
	return rtval_framebuffer_to_handle(rtval_framebuffer_create());
}

RT_EXPORT void rtFramebufferDestroy(rt_framebuffer framebuffer) {
	rtval_framebuffer_destroy(rtval_framebuffer_from_handle(framebuffer));
}

RT_EXPORT rt_texture_view rtFramebufferColorView(rt_framebuffer framebuffer, u32 slot) {
	return rtval_framebuffer_color_view(rtval_framebuffer_from_handle(framebuffer), slot);
}

RT_EXPORT void rtFramebufferSetColorView(rt_framebuffer framebuffer, u32 slot, rt_texture_view view) {
	rtval_framebuffer_set_color_view(
		rtval_framebuffer_from_handle(framebuffer),
		slot,
		rtval_texture_view_from_handle(view)
	);
}

RT_EXPORT void rtFramebufferDepthView(rt_framebuffer framebuffer, rt_texture_view view) {
	rtval_framebuffer_set_depth_view(
		rtval_framebuffer_from_handle(framebuffer),
		rtval_texture_view_from_handle(view)
	);
}

/*===============================================================================================*/
/*                                                                                               */
/*===============================================================================================*/

struct rtval_framebuffer* rtval_framebuffer_create(void) {
	rt_framebuffer backend = rtval_next_rtFramebufferCreate();
	if (!backend) {
		rtval_report_error("rtFramebufferCreate");
		return NULL;
	}
	struct rtval_framebuffer* handle = rtval_handle_create(RTVAL_HANDLE_TYPE_FRAMEBUFFER);
	if (!handle) {
		rtval_next_rtFramebufferDestroy(backend);
		return NULL;
	}
	struct rtval_framebuffer* state = RTVAL_PAYLOAD(handle, struct rtval_framebuffer);
	state->backend = backend;
	rtval_report_error("rtFramebufferCreate");
	return handle;
}

struct rtval_framebuffer* rtval_framebuffer_wrap(rt_framebuffer backend) {
	if (!backend) { return NULL; }
	struct rtval_framebuffer* handle = rtval_handle_create(RTVAL_HANDLE_TYPE_FRAMEBUFFER);
	if (!handle) { return NULL; }
	struct rtval_framebuffer* state = RTVAL_PAYLOAD(handle, struct rtval_framebuffer);
	state->backend = backend;
	rtval_report_error("rtFramebufferWrap");
	return handle;
}

void rtval_framebuffer_destroy(struct rtval_framebuffer* framebuffer) {
	if (!framebuffer) {
		RTVAL_DROP("rtFramebufferDestroy: NULL handle");
		return;
	}
	struct rtval_framebuffer* framebuffer_state = RTVAL_PAYLOAD(framebuffer, struct rtval_framebuffer);
	if (!framebuffer_state) {
		RTVAL_DROP("rtFramebufferDestroy: invalid handle");
		return;
	}
	rtval_next_rtFramebufferDestroy(framebuffer_state->backend);
	rtval_handle_destroy(framebuffer);
}

rt_texture_view rtval_framebuffer_color_view(struct rtval_framebuffer* framebuffer, u32 slot) {
	if (!framebuffer) {
		RTVAL_DROP("rtFramebufferColorView: NULL handle");
		return RT_NULL_HANDLE;
	}
	struct rtval_framebuffer* framebuffer_state = RTVAL_PAYLOAD(framebuffer, struct rtval_framebuffer);
	if (!framebuffer_state) {
		RTVAL_DROP("rtFramebufferColorView: invalid handle");
		return RT_NULL_HANDLE;
	}
	rt_texture_view result = rtval_next_rtFramebufferColorView(framebuffer_state->backend, slot);
	rtval_report_error("rtFramebufferColorView");
	return result;
}

void rtval_framebuffer_set_color_view(struct rtval_framebuffer* framebuffer, u32 slot, struct rtval_texture_view* view) {
	if (!framebuffer) {
		RTVAL_DROP("rtFramebufferSetColorView: NULL handle");
		return;
	}
	struct rtval_framebuffer* framebuffer_state = RTVAL_PAYLOAD(framebuffer, struct rtval_framebuffer);
	if (!framebuffer_state) {
		RTVAL_DROP("rtFramebufferSetColorView: invalid handle");
		return;
	}
	rt_texture_view view_backend = RT_NULL_HANDLE;
	if (view) {
		struct rtval_texture_view* view_state = RTVAL_PAYLOAD(view, struct rtval_texture_view);
		if (!view_state) {
			RTVAL_DROP("rtFramebufferSetColorView: invalid view handle");
			return;
		}
		view_backend = view_state->backend;
	}
	rtval_next_rtFramebufferSetColorView(framebuffer_state->backend, slot, view_backend);
	rtval_report_error("rtFramebufferSetColorView");
}

void rtval_framebuffer_set_depth_view(struct rtval_framebuffer* framebuffer, struct rtval_texture_view* view) {
	if (!framebuffer) {
		RTVAL_DROP("rtFramebufferDepthView: NULL handle");
		return;
	}
	struct rtval_framebuffer* framebuffer_state = RTVAL_PAYLOAD(framebuffer, struct rtval_framebuffer);
	if (!framebuffer_state) {
		RTVAL_DROP("rtFramebufferDepthView: invalid handle");
		return;
	}
	rt_texture_view view_backend = RT_NULL_HANDLE;
	if (view) {
		struct rtval_texture_view* view_state = RTVAL_PAYLOAD(view, struct rtval_texture_view);
		if (!view_state) {
			RTVAL_DROP("rtFramebufferDepthView: invalid view handle");
			return;
		}
		view_backend = view_state->backend;
	}
	rtval_next_rtFramebufferDepthView(framebuffer_state->backend, view_backend);
	rtval_report_error("rtFramebufferDepthView");
}

#undef RTVAL_DROP
