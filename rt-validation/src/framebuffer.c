#include "logger.h"
#include "procs.h"

#define RTVAL_DROP(message) rtval_printf("[validation] %s, dropping call\n", message)

RT_EXPORT rt_framebuffer rtFramebufferCreate(void) {
	return rtval_rtFramebufferCreate();
}

RT_EXPORT void rtFramebufferDestroy(rt_framebuffer framebuffer) {
	rtval_rtFramebufferDestroy(framebuffer);
}

RT_EXPORT rt_texture_view rtFramebufferColorView(rt_framebuffer framebuffer, u32 slot) {
	return rtval_rtFramebufferColorView(framebuffer, slot);
}

RT_EXPORT void rtFramebufferSetColorView(rt_framebuffer framebuffer, u32 slot, rt_texture_view view) {
	rtval_rtFramebufferSetColorView(framebuffer, slot, view);
}

RT_EXPORT void rtFramebufferDepthView(rt_framebuffer framebuffer, rt_texture_view view) {
	rtval_rtFramebufferDepthView(framebuffer, view);
}

rt_framebuffer rtval_rtFramebufferCreate(void) {
	if (!rtval_next_rtFramebufferCreate) {
		RTVAL_DROP("framebuffer_create: next function is NULL");
		return RT_NULL_HANDLE;
	}

	rt_framebuffer result = rtval_next_rtFramebufferCreate();
	rtval_report_error("rtFramebufferCreate");
	return result;
}

void rtval_rtFramebufferDestroy(rt_framebuffer framebuffer) {
	if (!framebuffer) {
		return;
	}
	if (!rtval_next_rtFramebufferDestroy) {
		RTVAL_DROP("framebuffer_destroy: next function is NULL");
		return;
	}

	rtval_next_rtFramebufferDestroy(framebuffer);
	rtval_report_error("rtFramebufferDestroy");
}

rt_texture_view rtval_rtFramebufferColorView(rt_framebuffer framebuffer, u32 slot) {
	if (!framebuffer) {
		RTVAL_DROP("framebuffer_color_view: NULL framebuffer");
		return RT_NULL_HANDLE;
	}
	if (!rtval_next_rtFramebufferColorView) {
		RTVAL_DROP("framebuffer_color_view: next function is NULL");
		return RT_NULL_HANDLE;
	}

	rt_texture_view result = rtval_next_rtFramebufferColorView(framebuffer, slot);
	rtval_report_error("rtFramebufferColorView");
	return result;
}

void rtval_rtFramebufferSetColorView(rt_framebuffer framebuffer, u32 slot, rt_texture_view view) {
	if (!framebuffer) {
		RTVAL_DROP("framebuffer_set_color_view: NULL framebuffer");
		return;
	}
	if (!rtval_next_rtFramebufferSetColorView) {
		RTVAL_DROP("framebuffer_set_color_view: next function is NULL");
		return;
	}

	rtval_next_rtFramebufferSetColorView(framebuffer, slot, view);
	rtval_report_error("rtFramebufferSetColorView");
}

void rtval_rtFramebufferDepthView(rt_framebuffer framebuffer, rt_texture_view view) {
	if (!framebuffer) {
		RTVAL_DROP("framebuffer_depth_view: NULL framebuffer");
		return;
	}
	if (!rtval_next_rtFramebufferDepthView) {
		RTVAL_DROP("framebuffer_depth_view: next function is NULL");
		return;
	}

	rtval_next_rtFramebufferDepthView(framebuffer, view);
	rtval_report_error("rtFramebufferDepthView");
}

#undef RTVAL_DROP
