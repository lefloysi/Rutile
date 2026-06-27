#ifndef RTVAL_TEXTURE_H
#define RTVAL_TEXTURE_H

#include "handles.h"

struct rtval_texture {
	rt_texture backend;
};

struct rtval_texture *rtval_texture_create(void);
void rtval_texture_destroy(struct rtval_texture *texture);

struct rtval_texture_view *rtval_texture_view_create(struct rtval_texture *texture);
void rtval_texture_view_destroy(struct rtval_texture_view *view);
void rtval_texture_view_filter(struct rtval_texture_view *view, enum rt_filter mag_filter, enum rt_filter min_filter, enum rt_mip_filter mip_filter);
void rtval_texture_view_address(struct rtval_texture_view *view, enum rt_address_mode address_u, enum rt_address_mode address_v, enum rt_address_mode address_w);
void rtval_texture_view_anisotropy(struct rtval_texture_view *view, u32 max_anisotropy);
void rtval_texture_view_lod(struct rtval_texture_view *view, f32 min_lod, f32 max_lod, f32 lod_bias);
rt_extent_3d rtval_texture_view_extent(struct rtval_texture_view *view);

rt_timepoint rtval_texture_copy(struct rtval_queue *queue, struct rtval_texture *src, u32 src_mip, struct rtval_texture *dst, u32 dst_mip);
rt_timepoint rtval_texture_data(struct rtval_queue *queue, struct rtval_texture *texture, enum rt_texture_type type, u32 mip, u32 offset_x, u32 offset_y, u32 offset_z, enum rt_format format, const void *data);
rt_timepoint rtval_texture_subcopy(struct rtval_queue *queue, struct rtval_texture *src, u32 src_mip, u32 src_x, u32 src_y, u32 src_z, struct rtval_texture *dst, u32 dst_mip, u32 dst_x, u32 dst_y, u32 dst_z, u32 width, u32 height, u32 depth);
rt_timepoint rtval_texture_subdata(struct rtval_queue *queue, struct rtval_texture *texture, u32 mip, u32 offset_x, u32 offset_y, u32 offset_z, u32 width, u32 height, u32 depth, const void *data);
rt_timepoint rtval_texture_view_copy_to_buffer(struct rtval_queue *queue, struct rtval_texture_view *view, struct rtval_buffer *buffer);

#endif
