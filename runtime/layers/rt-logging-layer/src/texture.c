#include "procs.h"

/*===============================================================================================*/
/*                                                                                               */
/*===============================================================================================*/

RT_EXPORT rt_texture rtTextureCreate(void) { return rtlog_rtTextureCreate(); }
RT_EXPORT void rtTextureDestroy(rt_texture texture) { rtlog_rtTextureDestroy(texture); }
RT_EXPORT rt_texture_view rtTextureViewCreate(void) { return rtlog_rtTextureViewCreate(); }
RT_EXPORT void rtTextureViewBind(rt_texture_view texture_view, rt_texture texture) { rtlog_rtTextureViewBind(texture_view, texture); }
RT_EXPORT void rtTextureViewDestroy(rt_texture_view texture_view) { rtlog_rtTextureViewDestroy(texture_view); }
RT_EXPORT void rtTextureViewFilter(rt_texture_view texture_view, enum rt_filter mag_filter, enum rt_filter min_filter, enum rt_mip_filter mip_filter) { rtlog_rtTextureViewFilter(texture_view, mag_filter, min_filter, mip_filter); }
RT_EXPORT void rtTextureViewAddress(rt_texture_view texture_view, enum rt_address_mode address_u, enum rt_address_mode address_v, enum rt_address_mode address_w) { rtlog_rtTextureViewAddress(texture_view, address_u, address_v, address_w); }
RT_EXPORT void rtTextureViewAnisotropy(rt_texture_view texture_view, u32 max_anisotropy) { rtlog_rtTextureViewAnisotropy(texture_view, max_anisotropy); }
RT_EXPORT void rtTextureViewLod(rt_texture_view texture_view, f32 min_lod, f32 max_lod, f32 lod_bias) { rtlog_rtTextureViewLod(texture_view, min_lod, max_lod, lod_bias); }

RT_EXPORT rt_timepoint rtTextureCopy(rt_texture src_texture, u32 src_mip, rt_texture dst_texture, u32 dst_mip) { return rtlog_rtTextureCopy(src_texture, src_mip, dst_texture, dst_mip); }
RT_EXPORT rt_timepoint rtTextureData(rt_texture texture, enum rt_texture_type type, u32 mip, u32 width, u32 height, u32 depth, enum rt_format format, const void* data) { return rtlog_rtTextureData(texture, type, mip, width, height, depth, format, data); }
RT_EXPORT rt_timepoint rtTextureSubcopy(rt_texture src_texture, u32 src_mip, u32 src_x, u32 src_y, u32 src_z, rt_texture dst_texture, u32 dst_mip, u32 dst_x, u32 dst_y, u32 dst_z, u32 width, u32 height, u32 depth) { return rtlog_rtTextureSubcopy(src_texture, src_mip, src_x, src_y, src_z, dst_texture, dst_mip, dst_x, dst_y, dst_z, width, height, depth); }
RT_EXPORT rt_timepoint rtTextureSubdata(rt_texture texture, u32 mip, u32 offset_x, u32 offset_y, u32 offset_z, u32 width, u32 height, u32 depth, const void* data) { return rtlog_rtTextureSubdata(texture, mip, offset_x, offset_y, offset_z, width, height, depth, data); }
RT_EXPORT rt_timepoint rtTextureViewCopyToBuffer(rt_texture_view texture_view, rt_buffer buffer) { return rtlog_rtTextureViewCopyToBuffer(texture_view, buffer); }
RT_EXPORT rt_extent_3d rtTextureViewExtent(rt_texture_view texture_view) { return rtlog_rtTextureViewExtent(texture_view); }

/*===============================================================================================*/
/*                                                                                               */
/*===============================================================================================*/

rt_texture rtlog_rtTextureCreate(void) {
	u64 start_ns = rtlog_now_ns();

	rtlog_printf("rtTextureCreate()\n");
	rt_texture result = next_rtTextureCreate();
	rtlog_printf("rtTextureCreate -> %s [%s]\n", rtlog_pointer(result), rtlog_elapsed(start_ns));
	rtlog_error("rtTextureCreate");
	return result;
}

void rtlog_rtTextureDestroy(rt_texture texture) {
	u64 start_ns = rtlog_now_ns();
	rtlog_printf("rtTextureDestroy(texture=%s)\n", rtlog_pointer(texture));
	next_rtTextureDestroy(texture);
	rtlog_printf("rtTextureDestroy completed in %s\n", rtlog_elapsed(start_ns));
	rtlog_error("rtTextureDestroy");
}

rt_texture_view rtlog_rtTextureViewCreate(void) {
	u64 start_ns = rtlog_now_ns();

	rtlog_printf("rtTextureViewCreate()\n");
	rt_texture_view result = next_rtTextureViewCreate();
	rtlog_printf("rtTextureViewCreate -> %s [%s]\n", rtlog_pointer(result), rtlog_elapsed(start_ns));
	rtlog_error("rtTextureViewCreate");
	return result;
}

void rtlog_rtTextureViewBind(rt_texture_view texture_view, rt_texture texture) {
	u64 start_ns = rtlog_now_ns();
	rtlog_printf("rtTextureViewBind(texture_view=%s, texture=%s)\n", rtlog_pointer(texture_view), rtlog_pointer(texture));
	next_rtTextureViewBind(texture_view, texture);
	rtlog_printf("rtTextureViewBind completed in %s\n", rtlog_elapsed(start_ns));
	rtlog_error("rtTextureViewBind");
}

void rtlog_rtTextureViewDestroy(rt_texture_view texture_view) {
	u64 start_ns = rtlog_now_ns();
	rtlog_printf("rtTextureViewDestroy(texture_view=%s)\n", rtlog_pointer(texture_view));
	next_rtTextureViewDestroy(texture_view);
	rtlog_printf("rtTextureViewDestroy completed in %s\n", rtlog_elapsed(start_ns));
	rtlog_error("rtTextureViewDestroy");
}

void rtlog_rtTextureViewFilter(rt_texture_view texture_view, enum rt_filter mag_filter, enum rt_filter min_filter, enum rt_mip_filter mip_filter) {
	u64 start_ns = rtlog_now_ns();
	rtlog_printf("rtTextureViewFilter(texture_view=%s)\n", rtlog_pointer(texture_view));
	next_rtTextureViewFilter(texture_view, mag_filter, min_filter, mip_filter);
	rtlog_printf("rtTextureViewFilter completed in %s\n", rtlog_elapsed(start_ns));
	rtlog_error("rtTextureViewFilter");
}

void rtlog_rtTextureViewAddress(rt_texture_view texture_view, enum rt_address_mode address_u, enum rt_address_mode address_v, enum rt_address_mode address_w) {
	u64 start_ns = rtlog_now_ns();
	rtlog_printf("rtTextureViewAddress(texture_view=%s)\n", rtlog_pointer(texture_view));
	next_rtTextureViewAddress(texture_view, address_u, address_v, address_w);
	rtlog_printf("rtTextureViewAddress completed in %s\n", rtlog_elapsed(start_ns));
	rtlog_error("rtTextureViewAddress");
}

void rtlog_rtTextureViewAnisotropy(rt_texture_view texture_view, u32 max_anisotropy) {
	u64 start_ns = rtlog_now_ns();
	rtlog_printf("rtTextureViewAnisotropy(texture_view=%s, max_anisotropy=%u)\n", rtlog_pointer(texture_view), max_anisotropy);
	next_rtTextureViewAnisotropy(texture_view, max_anisotropy);
	rtlog_printf("rtTextureViewAnisotropy completed in %s\n", rtlog_elapsed(start_ns));
	rtlog_error("rtTextureViewAnisotropy");
}

void rtlog_rtTextureViewLod(rt_texture_view texture_view, f32 min_lod, f32 max_lod, f32 lod_bias) {
	u64 start_ns = rtlog_now_ns();
	rtlog_printf("rtTextureViewLod(texture_view=%s, min_lod=%f, max_lod=%f, lod_bias=%f)\n", rtlog_pointer(texture_view), min_lod, max_lod, lod_bias);
	next_rtTextureViewLod(texture_view, min_lod, max_lod, lod_bias);
	rtlog_printf("rtTextureViewLod completed in %s\n", rtlog_elapsed(start_ns));
	rtlog_error("rtTextureViewLod");
}

rt_timepoint rtlog_rtTextureCopy(rt_texture src_texture, u32 src_mip, rt_texture dst_texture, u32 dst_mip) {
	u64 start_ns = rtlog_now_ns();

	rtlog_printf("rtTextureCopy(src_texture=%s, src_mip=%u, dst_texture=%s, dst_mip=%u)\n", rtlog_pointer(src_texture), src_mip, rtlog_pointer(dst_texture), dst_mip);
	rt_timepoint result = next_rtTextureCopy(src_texture, src_mip, dst_texture, dst_mip);
	rtlog_printf("rtTextureCopy -> %s [%s]\n", rtlog_timepoint(result), rtlog_elapsed(start_ns));
	rtlog_error("rtTextureCopy");
	return result;
}

rt_timepoint rtlog_rtTextureData(rt_texture texture, enum rt_texture_type type, u32 mip, u32 width, u32 height, u32 depth, enum rt_format format, const void* data) {
	u64 start_ns = rtlog_now_ns();

	rtlog_printf("rtTextureData(texture=%s, type=%d, mip=%u, size=(%u,%u,%u), format=%d, data=%s)\n", rtlog_pointer(texture), (i32)type, mip, width, height, depth, (i32)format, rtlog_pointer(data));
	rt_timepoint result = next_rtTextureData(texture, type, mip, width, height, depth, format, data);
	rtlog_printf("rtTextureData -> %s [%s]\n", rtlog_timepoint(result), rtlog_elapsed(start_ns));
	rtlog_error("rtTextureData");
	return result;
}

rt_timepoint rtlog_rtTextureSubcopy(rt_texture src_texture, u32 src_mip, u32 src_x, u32 src_y, u32 src_z, rt_texture dst_texture, u32 dst_mip, u32 dst_x, u32 dst_y, u32 dst_z, u32 width, u32 height, u32 depth) {
	u64 start_ns = rtlog_now_ns();

	rtlog_printf("rtTextureSubcopy(src_texture=%s, src_mip=%u, src=(%u,%u,%u), dst_texture=%s, dst_mip=%u, dst=(%u,%u,%u), size=(%u,%u,%u))\n", rtlog_pointer(src_texture), src_mip, src_x, src_y, src_z, rtlog_pointer(dst_texture), dst_mip, dst_x, dst_y, dst_z, width, height, depth);
	rt_timepoint result = next_rtTextureSubcopy(src_texture, src_mip, src_x, src_y, src_z, dst_texture, dst_mip, dst_x, dst_y, dst_z, width, height, depth);
	rtlog_printf("rtTextureSubcopy -> %s [%s]\n", rtlog_timepoint(result), rtlog_elapsed(start_ns));
	rtlog_error("rtTextureSubcopy");
	return result;
}

rt_timepoint rtlog_rtTextureSubdata(rt_texture texture, u32 mip, u32 offset_x, u32 offset_y, u32 offset_z, u32 width, u32 height, u32 depth, const void* data) {
	u64 start_ns = rtlog_now_ns();

	rtlog_printf("rtTextureSubdata(texture=%s, mip=%u, offset=(%u,%u,%u), size=(%u,%u,%u), data=%s)\n", rtlog_pointer(texture), mip, offset_x, offset_y, offset_z, width, height, depth, rtlog_pointer(data));
	rt_timepoint result = next_rtTextureSubdata(texture, mip, offset_x, offset_y, offset_z, width, height, depth, data);
	rtlog_printf("rtTextureSubdata -> %s [%s]\n", rtlog_timepoint(result), rtlog_elapsed(start_ns));
	rtlog_error("rtTextureSubdata");
	return result;
}

rt_timepoint rtlog_rtTextureViewCopyToBuffer(rt_texture_view texture_view, rt_buffer buffer) {
	u64 start_ns = rtlog_now_ns();

	rtlog_printf("rtTextureViewCopyToBuffer(texture_view=%s, buffer=%s)\n", rtlog_pointer(texture_view), rtlog_pointer(buffer));
	rt_timepoint result = next_rtTextureViewCopyToBuffer(texture_view, buffer);
	rtlog_printf("rtTextureViewCopyToBuffer -> %s [%s]\n", rtlog_timepoint(result), rtlog_elapsed(start_ns));
	rtlog_error("rtTextureViewCopyToBuffer");
	return result;
}

rt_extent_3d rtlog_rtTextureViewExtent(rt_texture_view texture_view) {
	u64 start_ns = rtlog_now_ns();

	rtlog_printf("rtTextureViewExtent(texture_view=%s)\n", rtlog_pointer(texture_view));
	rt_extent_3d result = next_rtTextureViewExtent(texture_view);
	rtlog_printf("rtTextureViewExtent -> (%u,%u,%u) [%s]\n", result.width, result.height, result.depth, rtlog_elapsed(start_ns));
	rtlog_error("rtTextureViewExtent");
	return result;
}
