#ifndef RTGL_TEXTURE_H
#define RTGL_TEXTURE_H

#include "config.h"
#include "glad/gl.h"
#include "resource.h"

RTGL_EXTERN_C_ENTER

/*===============================================================================================*/
/*                                                                                               */
/*===============================================================================================*/

RTGL_API rt_texture rtTextureCreate(void);
RTGL_API void rtTextureDestroy(rt_texture texture);
RTGL_API rt_timepoint rtTextureCopy(rt_texture src_texture, u32 src_mip, rt_texture dst_texture, u32 dst_mip);
RTGL_API rt_timepoint rtTextureData(rt_texture texture, enum rt_texture_type type, u32 mip, u32 width, u32 height, u32 depth, enum rt_format format, const void* data);
RTGL_API rt_timepoint rtTextureSubcopy(rt_texture src_texture, u32 src_mip, u32 src_x, u32 src_y, u32 src_z, rt_texture dst_texture, u32 dst_mip, u32 dst_x, u32 dst_y, u32 dst_z, u32 width, u32 height, u32 depth);
RTGL_API rt_timepoint rtTextureSubdata(rt_texture texture, u32 mip, u32 offset_x, u32 offset_y, u32 offset_z, u32 width, u32 height, u32 depth, const void* data);
RTGL_API rt_timepoint rtTextureViewCopyToBuffer(rt_texture_view texture_view, rt_buffer buffer);
RTGL_API rt_extent_3d rtTextureViewExtent(rt_texture_view texture_view);

RTGL_API void rtTextureViewDestroy(rt_texture_view texture_view);
RTGL_API rt_texture_view rtTextureViewCreate(void);
RTGL_API void rtTextureViewBind(rt_texture_view texture_view, rt_texture texture);
RTGL_API void rtTextureViewFilter(rt_texture_view texture_view, enum rt_filter mag_filter, enum rt_filter min_filter, enum rt_mip_filter mip_filter);
RTGL_API void rtTextureViewAddress(rt_texture_view texture_view, enum rt_address_mode address_u, enum rt_address_mode address_v, enum rt_address_mode address_w);
RTGL_API void rtTextureViewAnisotropy(rt_texture_view texture_view, u32 max_anisotropy);
RTGL_API void rtTextureViewLod(rt_texture_view texture_view, f32 min_lod, f32 max_lod, f32 lod_bias);

/*===============================================================================================*/
/*                                                                                               */
/*===============================================================================================*/

struct rtgl_image_base {
	struct rtgl_resource_base base;
	GLuint gl_texture;
	GLenum gl_target;
	GLenum gl_internal_format;
	enum rt_texture_type type;
	enum rt_format format;
	u32 width;
	u32 height;
	u32 depth;
	u32 mip_levels;
};

struct rtgl_texture {
	struct rtgl_image_base base;
};
RTGL_DECLARE_NEW_RESOURCE(texture)

struct rtgl_texture_view {
	struct rtgl_resource_base base;
	struct rtgl_image_base* image;
	enum rt_filter mag_filter;
	enum rt_filter min_filter;
	enum rt_mip_filter mip_filter;
	enum rt_address_mode address_u;
	enum rt_address_mode address_v;
	enum rt_address_mode address_w;
	u32 max_anisotropy;
	f32 min_lod;
	f32 max_lod;
	f32 lod_bias;
};
RTGL_DECLARE_NEW_RESOURCE(texture_view)

void rtgl_texture_view_bind(struct rtgl_context* ctx, struct rtgl_texture_view* view, struct rtgl_texture* texture);
void rtgl_texture_view_bind_image(struct rtgl_context* ctx, struct rtgl_texture_view* view, struct rtgl_image_base* image);
void rtgl_texture_view_image_data(struct rtgl_context* ctx, struct rtgl_texture_view* view, enum rt_texture_type type, u32 mip, u32 width, u32 height, u32 depth, enum rt_format format, const void* data);
void rtgl_texture_view_image_data_internal(struct rtgl_context* ctx, struct rtgl_texture_view* view, enum rt_texture_type type, u32 mip, u32 width, u32 height, u32 depth, enum rt_format format, GLenum internal_format, const void* data);
void rtgl_texture_view_filter(struct rtgl_context* ctx, struct rtgl_texture_view* view, enum rt_filter mag_filter, enum rt_filter min_filter, enum rt_mip_filter mip_filter);
void rtgl_texture_view_address(struct rtgl_context* ctx, struct rtgl_texture_view* view, enum rt_address_mode address_u, enum rt_address_mode address_v, enum rt_address_mode address_w);
void rtgl_texture_view_anisotropy(struct rtgl_context* ctx, struct rtgl_texture_view* view, u32 max_anisotropy);
void rtgl_texture_view_lod(struct rtgl_context* ctx, struct rtgl_texture_view* view, f32 min_lod, f32 max_lod, f32 lod_bias);
rt_extent_3d rtgl_texture_view_extent(struct rtgl_context* ctx, struct rtgl_texture_view* view);
struct rtgl_timepoint rtgl_texture_data(struct rtgl_context* ctx, struct rtgl_texture* texture, enum rt_texture_type type, u32 mip, u32 width, u32 height, u32 depth, enum rt_format format, const void* data);
GLenum rtgl_texture_upload_format(enum rt_format format);
GLenum rtgl_texture_upload_type(enum rt_format format);
bool rtgl_texture_view_valid(struct rtgl_texture_view* view);

RTGL_EXTERN_C_EXIT
#endif /* RTGL_TEXTURE_H */
