#include "procs.h"

RT_EXPORT rt_framebuffer rtFramebufferCreate(void) {
	return rtlog_rtFramebufferCreate();
}

RT_EXPORT void rtFramebufferDestroy(rt_framebuffer framebuffer) {
	rtlog_rtFramebufferDestroy(framebuffer);
}

RT_EXPORT rt_texture_view rtFramebufferColorView(rt_framebuffer framebuffer, u32 slot) {
	return rtlog_rtFramebufferColorView(framebuffer, slot);
}

RT_EXPORT void rtFramebufferSetColorView(rt_framebuffer framebuffer, u32 slot, rt_texture_view view) {
	rtlog_rtFramebufferSetColorView(framebuffer, slot, view);
}

RT_EXPORT void rtFramebufferDepthView(rt_framebuffer framebuffer, rt_texture_view view) {
	rtlog_rtFramebufferDepthView(framebuffer, view);
}

rt_framebuffer rtlog_rtFramebufferCreate(void) {
	u64 start_ns = rtlog_now_ns();

	rtlog_printf("rtFramebufferCreate()\n");
	rt_framebuffer result = next_rtFramebufferCreate();
	rtlog_printf("rtFramebufferCreate -> %s [%s]\n", rtlog_pointer(result), rtlog_elapsed(start_ns));
	rtlog_error("rtFramebufferCreate");
	return result;
}

void rtlog_rtFramebufferDestroy(rt_framebuffer framebuffer) {
	u64 start_ns = rtlog_now_ns();

	rtlog_printf("rtFramebufferDestroy(framebuffer=%s)\n", rtlog_pointer(framebuffer));
	next_rtFramebufferDestroy(framebuffer);
	rtlog_printf("rtFramebufferDestroy completed in %s\n", rtlog_elapsed(start_ns));
	rtlog_error("rtFramebufferDestroy");
}

rt_texture_view rtlog_rtFramebufferColorView(rt_framebuffer framebuffer, u32 slot) {
	u64 start_ns = rtlog_now_ns();

	rtlog_printf("rtFramebufferColorView(framebuffer=%s, slot=%u)\n", rtlog_pointer(framebuffer), slot);
	rt_texture_view result = next_rtFramebufferColorView(framebuffer, slot);
	rtlog_printf("rtFramebufferColorView -> %s [%s]\n", rtlog_pointer(result), rtlog_elapsed(start_ns));
	rtlog_error("rtFramebufferColorView");
	return result;
}

void rtlog_rtFramebufferSetColorView(rt_framebuffer framebuffer, u32 slot, rt_texture_view view) {
	u64 start_ns = rtlog_now_ns();

	rtlog_printf("rtFramebufferSetColorView(framebuffer=%s, slot=%u, view=%s)\n", rtlog_pointer(framebuffer), slot, rtlog_pointer(view));
	next_rtFramebufferSetColorView(framebuffer, slot, view);
	rtlog_printf("rtFramebufferSetColorView completed in %s\n", rtlog_elapsed(start_ns));
	rtlog_error("rtFramebufferSetColorView");
}

void rtlog_rtFramebufferDepthView(rt_framebuffer framebuffer, rt_texture_view view) {
	u64 start_ns = rtlog_now_ns();

	rtlog_printf("rtFramebufferDepthView(framebuffer=%s, view=%s)\n", rtlog_pointer(framebuffer), rtlog_pointer(view));
	next_rtFramebufferDepthView(framebuffer, view);
	rtlog_printf("rtFramebufferDepthView completed in %s\n", rtlog_elapsed(start_ns));
	rtlog_error("rtFramebufferDepthView");
}
