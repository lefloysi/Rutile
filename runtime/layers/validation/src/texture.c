#include "texture.h"
#include "buffer.h"
#include "logger.h"
#include "queue.h"
#include "texture_view.h"

#define RTVAL_DROP(message) rtval_printf("[validation] %s, dropping call\n", message)

/*===============================================================================================*/
/*                                                                                               */
/*===============================================================================================*/

RT_EXPORT rt_texture rtTextureCreate(void) {
	return rtval_texture_to_handle(rtval_texture_create());
}

RT_EXPORT void rtTextureDestroy(rt_texture texture) {
	rtval_texture_destroy(rtval_texture_from_handle(texture));
}

RT_EXPORT rt_texture_view rtTextureViewCreate(rt_texture texture) {
	return rtval_texture_view_to_handle(rtval_texture_view_create(rtval_texture_from_handle(texture)));
}

RT_EXPORT void rtTextureViewDestroy(rt_texture_view texture_view) {
	rtval_texture_view_destroy(rtval_texture_view_from_handle(texture_view));
}

RT_EXPORT void rtTextureViewFilter(rt_texture_view texture_view, enum rt_filter mag_filter, enum rt_filter min_filter, enum rt_mip_filter mip_filter) {
	rtval_texture_view_filter(rtval_texture_view_from_handle(texture_view), mag_filter, min_filter, mip_filter);
}

RT_EXPORT void rtTextureViewAddress(rt_texture_view texture_view, enum rt_address_mode address_u, enum rt_address_mode address_v, enum rt_address_mode address_w) {
	rtval_texture_view_address(rtval_texture_view_from_handle(texture_view), address_u, address_v, address_w);
}

RT_EXPORT void rtTextureViewAnisotropy(rt_texture_view texture_view, u32 max_anisotropy) {
	rtval_texture_view_anisotropy(rtval_texture_view_from_handle(texture_view), max_anisotropy);
}

RT_EXPORT void rtTextureViewLod(rt_texture_view texture_view, f32 min_lod, f32 max_lod, f32 lod_bias) {
	rtval_texture_view_lod(rtval_texture_view_from_handle(texture_view), min_lod, max_lod, lod_bias);
}

RT_EXPORT rt_extent_3d rtTextureViewExtent(rt_texture_view texture_view) {
	return rtval_texture_view_extent(rtval_texture_view_from_handle(texture_view));
}

RT_EXPORT rt_timepoint rtTextureCopy(rt_queue queue, rt_texture src_texture, u32 src_mip, rt_texture dst_texture, u32 dst_mip) {
	return rtval_texture_copy(
		rtval_queue_from_handle(queue),
		rtval_texture_from_handle(src_texture),
		src_mip,
		rtval_texture_from_handle(dst_texture),
		dst_mip
	);
}

RT_EXPORT rt_timepoint rtTextureData(rt_queue queue, rt_texture texture, enum rt_texture_type type, u32 mip, u32 offset_x, u32 offset_y, u32 offset_z, enum rt_format format, const void* data) {
	return rtval_texture_data(
		rtval_queue_from_handle(queue),
		rtval_texture_from_handle(texture),
		type,
		mip,
		offset_x,
		offset_y,
		offset_z,
		format,
		data
	);
}

RT_EXPORT rt_timepoint rtTextureSubcopy(rt_queue queue, rt_texture src_texture, u32 src_mip, u32 src_x, u32 src_y, u32 src_z, rt_texture dst_texture, u32 dst_mip, u32 dst_x, u32 dst_y, u32 dst_z, u32 width, u32 height, u32 depth) {
	return rtval_texture_subcopy(
		rtval_queue_from_handle(queue),
		rtval_texture_from_handle(src_texture),
		src_mip,
		src_x,
		src_y,
		src_z,
		rtval_texture_from_handle(dst_texture),
		dst_mip,
		dst_x,
		dst_y,
		dst_z,
		width,
		height,
		depth
	);
}

RT_EXPORT rt_timepoint rtTextureSubdata(rt_queue queue, rt_texture texture, u32 mip, u32 offset_x, u32 offset_y, u32 offset_z, u32 width, u32 height, u32 depth, const void* data) {
	return rtval_texture_subdata(
		rtval_queue_from_handle(queue),
		rtval_texture_from_handle(texture),
		mip,
		offset_x,
		offset_y,
		offset_z,
		width,
		height,
		depth,
		data
	);
}

RT_EXPORT rt_timepoint rtTextureViewCopyToBuffer(rt_queue queue, rt_texture_view texture_view, rt_buffer buffer) {
	return rtval_texture_view_copy_to_buffer(
		rtval_queue_from_handle(queue),
		rtval_texture_view_from_handle(texture_view),
		rtval_buffer_from_handle(buffer)
	);
}

/*===============================================================================================*/
/*                                                                                               */
/*===============================================================================================*/

struct rtval_texture* rtval_texture_create(void) {
	rt_texture backend = rtval_next_rtTextureCreate();
	if (!backend) {
		rtval_report_error("rtTextureCreate");
		return NULL;
	}
	struct rtval_texture* handle = rtval_handle_create(RTVAL_HANDLE_TYPE_TEXTURE);
	if (!handle) {
		rtval_next_rtTextureDestroy(backend);
		return NULL;
	}
	struct rtval_texture* state = RTVAL_PAYLOAD(handle, struct rtval_texture);
	state->backend = backend;
	rtval_report_error("rtTextureCreate");
	return handle;
}

void rtval_texture_destroy(struct rtval_texture* texture) {
	if (!texture) {
		return;
	}
	struct rtval_texture* state = RTVAL_PAYLOAD(texture, struct rtval_texture);
	if (!state) {
		RTVAL_DROP("rtTextureDestroy: invalid handle");
		return;
	}
	rtval_next_rtTextureDestroy(state->backend);
	rtval_handle_destroy(texture);
}

struct rtval_texture_view* rtval_texture_view_create(struct rtval_texture* texture) {
	struct rtval_texture* tex_state = RTVAL_PAYLOAD(texture, struct rtval_texture);
	if (!tex_state) {
		RTVAL_DROP("rtTextureViewCreate: invalid texture");
		return NULL;
	}
	rt_texture_view backend = rtval_next_rtTextureViewCreate(tex_state->backend);
	if (!backend) {
		rtval_report_error("rtTextureViewCreate");
		return NULL;
	}
	struct rtval_texture_view* handle = rtval_handle_create(RTVAL_HANDLE_TYPE_TEXTURE_VIEW);
	if (!handle) {
		rtval_next_rtTextureViewDestroy(backend);
		return NULL;
	}
	struct rtval_texture_view* state = RTVAL_PAYLOAD(handle, struct rtval_texture_view);
	state->backend = backend;
	rtval_report_error("rtTextureViewCreate");
	return handle;
}

void rtval_texture_view_destroy(struct rtval_texture_view* view) {
	if (!view) {
		return;
	}
	struct rtval_texture_view* state = RTVAL_PAYLOAD(view, struct rtval_texture_view);
	if (!state) {
		RTVAL_DROP("rtTextureViewDestroy: invalid handle");
		return;
	}
	rtval_next_rtTextureViewDestroy(state->backend);
	rtval_handle_destroy(view);
}

void rtval_texture_view_filter(struct rtval_texture_view* view, enum rt_filter mag_filter, enum rt_filter min_filter, enum rt_mip_filter mip_filter) {
	struct rtval_texture_view* state = RTVAL_PAYLOAD(view, struct rtval_texture_view);
	if (!state) {
		RTVAL_DROP("rtTextureViewFilter: invalid handle");
		return;
	}
	rtval_next_rtTextureViewFilter(state->backend, mag_filter, min_filter, mip_filter);
	rtval_report_error("rtTextureViewFilter");
}

void rtval_texture_view_address(struct rtval_texture_view* view, enum rt_address_mode address_u, enum rt_address_mode address_v, enum rt_address_mode address_w) {
	struct rtval_texture_view* state = RTVAL_PAYLOAD(view, struct rtval_texture_view);
	if (!state) {
		RTVAL_DROP("rtTextureViewAddress: invalid handle");
		return;
	}
	rtval_next_rtTextureViewAddress(state->backend, address_u, address_v, address_w);
	rtval_report_error("rtTextureViewAddress");
}

void rtval_texture_view_anisotropy(struct rtval_texture_view* view, u32 max_anisotropy) {
	struct rtval_texture_view* state = RTVAL_PAYLOAD(view, struct rtval_texture_view);
	if (!state) {
		RTVAL_DROP("rtTextureViewAnisotropy: invalid handle");
		return;
	}
	rtval_next_rtTextureViewAnisotropy(state->backend, max_anisotropy);
	rtval_report_error("rtTextureViewAnisotropy");
}

void rtval_texture_view_lod(struct rtval_texture_view* view, f32 min_lod, f32 max_lod, f32 lod_bias) {
	struct rtval_texture_view* state = RTVAL_PAYLOAD(view, struct rtval_texture_view);
	if (!state) {
		RTVAL_DROP("rtTextureViewLod: invalid handle");
		return;
	}
	rtval_next_rtTextureViewLod(state->backend, min_lod, max_lod, lod_bias);
	rtval_report_error("rtTextureViewLod");
}

rt_extent_3d rtval_texture_view_extent(struct rtval_texture_view* view) {
	rt_extent_3d empty = {0, 0, 0};
	struct rtval_texture_view* state = RTVAL_PAYLOAD(view, struct rtval_texture_view);
	if (!state) {
		RTVAL_DROP("rtTextureViewExtent: invalid handle");
		return empty;
	}
	rt_extent_3d extent = rtval_next_rtTextureViewExtent(state->backend);
	rtval_report_error("rtTextureViewExtent");
	return extent;
}

rt_timepoint rtval_texture_copy(struct rtval_queue* queue, struct rtval_texture* src, u32 src_mip, struct rtval_texture* dst, u32 dst_mip) {
	rt_timepoint timepoint = {RT_NULL_HANDLE, 0};
	struct rtval_queue* q = RTVAL_PAYLOAD(queue, struct rtval_queue);
	if (!q) {
		RTVAL_DROP("rtTextureCopy: invalid queue");
		return timepoint;
	}
	struct rtval_texture* s = RTVAL_PAYLOAD(src, struct rtval_texture);
	struct rtval_texture* d = RTVAL_PAYLOAD(dst, struct rtval_texture);
	if (!s || !d) {
		RTVAL_DROP("rtTextureCopy: invalid texture");
		return timepoint;
	}

	timepoint = rtval_next_rtTextureCopy(q->backend, s->backend, src_mip, d->backend, dst_mip);
	rtval_report_error("rtTextureCopy");
	return rtval_timepoint_wrap(timepoint);
}

rt_timepoint rtval_texture_data(struct rtval_queue* queue, struct rtval_texture* texture, enum rt_texture_type type, u32 mip, u32 offset_x, u32 offset_y, u32 offset_z, enum rt_format format, const void* data) {
	rt_timepoint timepoint = {RT_NULL_HANDLE, 0};
	struct rtval_queue* q = RTVAL_PAYLOAD(queue, struct rtval_queue);
	if (!q) {
		RTVAL_DROP("rtTextureData: invalid queue");
		return timepoint;
	}
	struct rtval_texture* t = RTVAL_PAYLOAD(texture, struct rtval_texture);
	if (!t) {
		RTVAL_DROP("rtTextureData: invalid texture");
		return timepoint;
	}

	timepoint = rtval_next_rtTextureData(q->backend, t->backend, type, mip, offset_x, offset_y, offset_z, format, data);
	rtval_report_error("rtTextureData");
	return rtval_timepoint_wrap(timepoint);
}

rt_timepoint rtval_texture_subcopy(struct rtval_queue* queue, struct rtval_texture* src, u32 src_mip, u32 src_x, u32 src_y, u32 src_z, struct rtval_texture* dst, u32 dst_mip, u32 dst_x, u32 dst_y, u32 dst_z, u32 width, u32 height, u32 depth) {
	rt_timepoint timepoint = {RT_NULL_HANDLE, 0};
	struct rtval_queue* q = RTVAL_PAYLOAD(queue, struct rtval_queue);
	if (!q) {
		RTVAL_DROP("rtTextureSubcopy: invalid queue");
		return timepoint;
	}
	struct rtval_texture* s = RTVAL_PAYLOAD(src, struct rtval_texture);
	struct rtval_texture* d = RTVAL_PAYLOAD(dst, struct rtval_texture);
	if (!s || !d) {
		RTVAL_DROP("rtTextureSubcopy: invalid texture");
		return timepoint;
	}
	if (width == 0 || height == 0 || depth == 0) {
		RTVAL_DROP("rtTextureSubcopy: zero extent");
		return timepoint;
	}

	timepoint = rtval_next_rtTextureSubcopy(q->backend, s->backend, src_mip, src_x, src_y, src_z, d->backend, dst_mip, dst_x, dst_y, dst_z, width, height, depth);
	rtval_report_error("rtTextureSubcopy");
	return rtval_timepoint_wrap(timepoint);
}

rt_timepoint rtval_texture_subdata(struct rtval_queue* queue, struct rtval_texture* texture, u32 mip, u32 offset_x, u32 offset_y, u32 offset_z, u32 width, u32 height, u32 depth, const void* data) {
	rt_timepoint timepoint = {RT_NULL_HANDLE, 0};
	struct rtval_queue* q = RTVAL_PAYLOAD(queue, struct rtval_queue);
	if (!q) {
		RTVAL_DROP("rtTextureSubdata: invalid queue");
		return timepoint;
	}
	struct rtval_texture* t = RTVAL_PAYLOAD(texture, struct rtval_texture);
	if (!t) {
		RTVAL_DROP("rtTextureSubdata: invalid texture");
		return timepoint;
	}
	if (!data) {
		RTVAL_DROP("rtTextureSubdata: NULL data");
		return timepoint;
	}
	if (width == 0 || height == 0 || depth == 0) {
		RTVAL_DROP("rtTextureSubdata: zero extent");
		return timepoint;
	}

	timepoint = rtval_next_rtTextureSubdata(q->backend, t->backend, mip, offset_x, offset_y, offset_z, width, height, depth, data);
	rtval_report_error("rtTextureSubdata");
	return rtval_timepoint_wrap(timepoint);
}

rt_timepoint rtval_texture_view_copy_to_buffer(struct rtval_queue* queue, struct rtval_texture_view* view, struct rtval_buffer* buffer) {
	rt_timepoint timepoint = {RT_NULL_HANDLE, 0};
	struct rtval_queue* q = RTVAL_PAYLOAD(queue, struct rtval_queue);
	if (!q) {
		RTVAL_DROP("rtTextureViewCopyToBuffer: invalid queue");
		return timepoint;
	}
	struct rtval_texture_view* v = RTVAL_PAYLOAD(view, struct rtval_texture_view);
	struct rtval_buffer* b = RTVAL_PAYLOAD(buffer, struct rtval_buffer);
	if (!v || !b) {
		RTVAL_DROP("rtTextureViewCopyToBuffer: invalid resource");
		return timepoint;
	}

	timepoint = rtval_next_rtTextureViewCopyToBuffer(q->backend, v->backend, b->backend);
	rtval_report_error("rtTextureViewCopyToBuffer");
	return rtval_timepoint_wrap(timepoint);
}

#undef RTVAL_DROP
