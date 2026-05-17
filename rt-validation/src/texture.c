#include "procs.h"
#include "logger.h"

#define RTVAL_DROP(message) rtval_printf("[validation] %s, dropping call\n", message)

/*===============================================================================================*/
/*                                                                                               */
/*===============================================================================================*/

RT_EXPORT rt_texture rtTextureCreate(void) { return rtval_rtTextureCreate(); }
RT_EXPORT void rtTextureDestroy(rt_texture texture) { rtval_rtTextureDestroy(texture); }
RT_EXPORT rt_texture_view rtTextureViewCreate(rt_texture texture) { return rtval_rtTextureViewCreate(texture); }
RT_EXPORT void rtTextureViewDestroy(rt_texture_view texture_view) { rtval_rtTextureViewDestroy(texture_view); }
RT_EXPORT void rtTextureViewFilter(rt_texture_view texture_view, enum rt_filter mag_filter, enum rt_filter min_filter, enum rt_mip_filter mip_filter) { rtval_rtTextureViewFilter(texture_view, mag_filter, min_filter, mip_filter); }
RT_EXPORT void rtTextureViewAddress(rt_texture_view texture_view, enum rt_address_mode address_u, enum rt_address_mode address_v, enum rt_address_mode address_w) { rtval_rtTextureViewAddress(texture_view, address_u, address_v, address_w); }
RT_EXPORT void rtTextureViewAnisotropy(rt_texture_view texture_view, u32 max_anisotropy) { rtval_rtTextureViewAnisotropy(texture_view, max_anisotropy); }
RT_EXPORT void rtTextureViewLod(rt_texture_view texture_view, f32 min_lod, f32 max_lod, f32 lod_bias) { rtval_rtTextureViewLod(texture_view, min_lod, max_lod, lod_bias); }

RT_EXPORT rt_timepoint rtTextureCopy(rt_queue queue, rt_texture src_texture, u32 src_mip, rt_texture dst_texture, u32 dst_mip) { return rtval_rtTextureCopy(queue, src_texture, src_mip, dst_texture, dst_mip); }
RT_EXPORT rt_timepoint rtTextureData(rt_queue queue, rt_texture texture, enum rt_texture_type type, u32 mip, u32 offset_x, u32 offset_y, u32 offset_z, enum rt_format format, const void* data) { return rtval_rtTextureData(queue, texture, type, mip, offset_x, offset_y, offset_z, format, data); }
RT_EXPORT rt_timepoint rtTextureSubcopy(rt_queue queue, rt_texture src_texture, u32 src_mip, u32 src_x, u32 src_y, u32 src_z, rt_texture dst_texture, u32 dst_mip, u32 dst_x, u32 dst_y, u32 dst_z, u32 width, u32 height, u32 depth) { return rtval_rtTextureSubcopy(queue, src_texture, src_mip, src_x, src_y, src_z, dst_texture, dst_mip, dst_x, dst_y, dst_z, width, height, depth); }
RT_EXPORT rt_timepoint rtTextureSubdata(rt_queue queue, rt_texture texture, u32 mip, u32 offset_x, u32 offset_y, u32 offset_z, u32 width, u32 height, u32 depth, const void* data) { return rtval_rtTextureSubdata(queue, texture, mip, offset_x, offset_y, offset_z, width, height, depth, data); }
RT_EXPORT rt_timepoint rtTextureViewCopyToBuffer(rt_queue queue, rt_texture_view texture_view, rt_buffer buffer) { return rtval_rtTextureViewCopyToBuffer(queue, texture_view, buffer); }
RT_EXPORT rt_extent_3d rtTextureViewExtent(rt_texture_view texture_view) { return rtval_rtTextureViewExtent(texture_view); }

/*===============================================================================================*/
/*                                                                                               */
/*===============================================================================================*/

rt_texture rtval_rtTextureCreate(void) {
	rt_texture texture = rtval_next_rtTextureCreate();
	rtval_report_error("rtTextureCreate");
	return texture;
}

void rtval_rtTextureDestroy(rt_texture texture) {
	rtval_next_rtTextureDestroy(texture);
	rtval_report_error("rtTextureDestroy");
}

rt_texture_view rtval_rtTextureViewCreate(rt_texture texture) {
	if (!texture) {
		RTVAL_DROP("texture_view_create: NULL texture");
		return RT_NULL_HANDLE;
	}

	rt_texture_view texture_view = rtval_next_rtTextureViewCreate(texture);
	rtval_report_error("rtTextureViewCreate");
	return texture_view;
}

void rtval_rtTextureViewDestroy(rt_texture_view texture_view) {
	rtval_next_rtTextureViewDestroy(texture_view);
	rtval_report_error("rtTextureViewDestroy");
}

static bool rtval_texture_view_state_valid(rt_texture_view texture_view, const char* call) {
	if (!texture_view) {
		RTVAL_DROP(call);
		return false;
	}
	return true;
}

void rtval_rtTextureViewFilter(rt_texture_view texture_view, enum rt_filter mag_filter, enum rt_filter min_filter, enum rt_mip_filter mip_filter) {
	if (!rtval_texture_view_state_valid(texture_view, "texture_view_filter: NULL texture view")) { return; }
	rtval_next_rtTextureViewFilter(texture_view, mag_filter, min_filter, mip_filter);
	rtval_report_error("rtTextureViewFilter");
}

void rtval_rtTextureViewAddress(rt_texture_view texture_view, enum rt_address_mode address_u, enum rt_address_mode address_v, enum rt_address_mode address_w) {
	if (!rtval_texture_view_state_valid(texture_view, "texture_view_address: NULL texture view")) { return; }
	rtval_next_rtTextureViewAddress(texture_view, address_u, address_v, address_w);
	rtval_report_error("rtTextureViewAddress");
}

void rtval_rtTextureViewAnisotropy(rt_texture_view texture_view, u32 max_anisotropy) {
	if (!rtval_texture_view_state_valid(texture_view, "texture_view_anisotropy: NULL texture view")) { return; }
	rtval_next_rtTextureViewAnisotropy(texture_view, max_anisotropy);
	rtval_report_error("rtTextureViewAnisotropy");
}

void rtval_rtTextureViewLod(rt_texture_view texture_view, f32 min_lod, f32 max_lod, f32 lod_bias) {
	if (!rtval_texture_view_state_valid(texture_view, "texture_view_lod: NULL texture view")) { return; }
	rtval_next_rtTextureViewLod(texture_view, min_lod, max_lod, lod_bias);
	rtval_report_error("rtTextureViewLod");
}

rt_timepoint rtval_rtTextureCopy(rt_queue queue, rt_texture src_texture, u32 src_mip, rt_texture dst_texture, u32 dst_mip) {
	rt_timepoint timepoint = { queue, 0 };
	if (!queue) {
		RTVAL_DROP("texture_copy: NULL queue");
		return timepoint;
	}
	if (!src_texture || !dst_texture) {
		RTVAL_DROP("texture_copy: NULL texture");
		return timepoint;
	}

	timepoint = rtval_next_rtTextureCopy(queue, src_texture, src_mip, dst_texture, dst_mip);
	rtval_report_error("rtTextureCopy");
	return timepoint;
}

rt_timepoint rtval_rtTextureData(rt_queue queue, rt_texture texture, enum rt_texture_type type, u32 mip, u32 offset_x, u32 offset_y, u32 offset_z, enum rt_format format, const void* data) {
	rt_timepoint timepoint = { queue, 0 };
	if (!queue) {
		RTVAL_DROP("texture_data: NULL queue");
		return timepoint;
	}
	if (!texture) {
		RTVAL_DROP("texture_data: NULL texture");
		return timepoint;
	}
	timepoint = rtval_next_rtTextureData(queue, texture, type, mip, offset_x, offset_y, offset_z, format, data);
	rtval_report_error("rtTextureData");
	return timepoint;
}

rt_timepoint rtval_rtTextureSubcopy(rt_queue queue, rt_texture src_texture, u32 src_mip, u32 src_x, u32 src_y, u32 src_z, rt_texture dst_texture, u32 dst_mip, u32 dst_x, u32 dst_y, u32 dst_z, u32 width, u32 height, u32 depth) {
	rt_timepoint timepoint = { queue, 0 };
	if (!queue) {
		RTVAL_DROP("texture_subcopy: NULL queue");
		return timepoint;
	}
	if (!src_texture || !dst_texture) {
		RTVAL_DROP("texture_subcopy: NULL texture");
		return timepoint;
	}
	if (width == 0 || height == 0 || depth == 0) {
		RTVAL_DROP("texture_subcopy: zero extent");
		return timepoint;
	}

	timepoint = rtval_next_rtTextureSubcopy(queue, src_texture, src_mip, src_x, src_y, src_z, dst_texture, dst_mip, dst_x, dst_y, dst_z, width, height, depth);
	rtval_report_error("rtTextureSubcopy");
	return timepoint;
}

rt_timepoint rtval_rtTextureSubdata(rt_queue queue, rt_texture texture, u32 mip, u32 offset_x, u32 offset_y, u32 offset_z, u32 width, u32 height, u32 depth, const void* data) {
	rt_timepoint timepoint = { queue, 0 };
	if (!queue) {
		RTVAL_DROP("texture_subdata: NULL queue");
		return timepoint;
	}
	if (!texture) {
		RTVAL_DROP("texture_subdata: NULL texture");
		return timepoint;
	}
	if (!data) {
		RTVAL_DROP("texture_subdata: NULL data");
		return timepoint;
	}
	if (width == 0 || height == 0 || depth == 0) {
		RTVAL_DROP("texture_subdata: zero extent");
		return timepoint;
	}

	timepoint = rtval_next_rtTextureSubdata(queue, texture, mip, offset_x, offset_y, offset_z, width, height, depth, data);
	rtval_report_error("rtTextureSubdata");
	return timepoint;
}

rt_timepoint rtval_rtTextureViewCopyToBuffer(rt_queue queue, rt_texture_view texture_view, rt_buffer buffer) {
	rt_timepoint timepoint = { queue, 0 };
	if (!queue) {
		RTVAL_DROP("texture_view_copy_to_buffer: NULL queue");
		return timepoint;
	}
	if (!texture_view || !buffer) {
		RTVAL_DROP("texture_view_copy_to_buffer: NULL resource");
		return timepoint;
	}

	timepoint = rtval_next_rtTextureViewCopyToBuffer(queue, texture_view, buffer);
	rtval_report_error("rtTextureViewCopyToBuffer");
	return timepoint;
}

rt_extent_3d rtval_rtTextureViewExtent(rt_texture_view texture_view) {
	rt_extent_3d empty = { 0, 0, 0 };
	if (!texture_view) {
		RTVAL_DROP("texture_view_extent: NULL texture view");
		return empty;
	}

	rt_extent_3d extent = rtval_next_rtTextureViewExtent(texture_view);
	rtval_report_error("rtTextureViewExtent");
	return extent;
}

#undef RTVAL_DROP

