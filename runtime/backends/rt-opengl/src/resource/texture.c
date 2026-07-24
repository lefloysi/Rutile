#include "texture.h"

#include "context.h"
#include "error.h"
#include "execution.h"

/*===============================================================================================*/
/*                                                                                               */
/*===============================================================================================*/

rt_texture rtTextureCreate(void) {
	return rtgl_texture_to_handle(rtgl_texture_create(rtgl_get_current_context()));
}

void rtTextureDestroy(rt_texture texture) {
	rtgl_texture_destroy(rtgl_get_current_context(), rtgl_texture_from_handle(texture));
}

rt_texture_view rtTextureViewCreate(void) {
	return rtgl_texture_view_to_handle(rtgl_texture_view_create(rtgl_get_current_context()));
}

void rtTextureViewBind(rt_texture_view texture_view, rt_texture texture) {
	rtgl_texture_view_bind(rtgl_get_current_context(), rtgl_texture_view_from_handle(texture_view), rtgl_texture_from_handle(texture));
}

void rtTextureViewDestroy(rt_texture_view texture_view) {
	rtgl_texture_view_destroy(rtgl_get_current_context(), rtgl_texture_view_from_handle(texture_view));
}

void rtTextureViewFilter(rt_texture_view texture_view, enum rt_filter mag_filter, enum rt_filter min_filter, enum rt_mip_filter mip_filter) {
	rtgl_texture_view_filter(rtgl_get_current_context(), rtgl_texture_view_from_handle(texture_view), mag_filter, min_filter, mip_filter);
}

void rtTextureViewAddress(rt_texture_view texture_view, enum rt_address_mode address_u, enum rt_address_mode address_v, enum rt_address_mode address_w) {
	rtgl_texture_view_address(rtgl_get_current_context(), rtgl_texture_view_from_handle(texture_view), address_u, address_v, address_w);
}

void rtTextureViewAnisotropy(rt_texture_view texture_view, u32 max_anisotropy) {
	rtgl_texture_view_anisotropy(rtgl_get_current_context(), rtgl_texture_view_from_handle(texture_view), max_anisotropy);
}

void rtTextureViewLod(rt_texture_view texture_view, f32 min_lod, f32 max_lod, f32 lod_bias) {
	rtgl_texture_view_lod(rtgl_get_current_context(), rtgl_texture_view_from_handle(texture_view), min_lod, max_lod, lod_bias);
}

rt_timepoint rtTextureCopy(rt_texture src_texture, u32 src_mip, rt_texture dst_texture, u32 dst_mip) {
	rtgl_throwf(RT_UNSUPPORTED_FEATURE, "OpenGL texture copy is not implemented");
	return (rt_timepoint){ RT_NULL_HANDLE, 0 };
}

rt_timepoint rtTextureData(rt_texture texture, enum rt_texture_type type, u32 mip, u32 width, u32 height, u32 depth, enum rt_format format, const void* data) {
	return rtgl_timepoint_to_public(rtgl_texture_data(rtgl_get_current_context(), rtgl_texture_from_handle(texture), type, mip, width, height, depth, format, data));
}

rt_timepoint rtTextureSubcopy(rt_texture src_texture, u32 src_mip, u32 src_x, u32 src_y, u32 src_z, rt_texture dst_texture, u32 dst_mip, u32 dst_x, u32 dst_y, u32 dst_z, u32 width, u32 height, u32 depth) {
	rtgl_throwf(RT_UNSUPPORTED_FEATURE, "OpenGL texture subcopy is not implemented");
	return (rt_timepoint){ RT_NULL_HANDLE, 0 };
}

rt_timepoint rtTextureSubdata(rt_texture texture, u32 mip, u32 offset_x, u32 offset_y, u32 offset_z, u32 width, u32 height, u32 depth, const void* data) {
	return rtgl_timepoint_to_public(rtgl_texture_subdata(
		rtgl_get_current_context(),
		rtgl_texture_from_handle(texture),
		mip,
		offset_x,
		offset_y,
		offset_z,
		width,
		height,
		depth,
		data));
}

rt_timepoint rtTextureViewCopyToBuffer(rt_texture_view texture_view, rt_buffer buffer) {
	rtgl_throwf(RT_UNSUPPORTED_FEATURE, "OpenGL texture view copy to buffer is not implemented");
	return (rt_timepoint){ RT_NULL_HANDLE, 0 };
}

rt_extent_3d rtTextureViewExtent(rt_texture_view texture_view) {
	return rtgl_texture_view_extent(rtgl_get_current_context(), rtgl_texture_view_from_handle(texture_view));
}

/*===============================================================================================*/
/*                                                                                               */
/*===============================================================================================*/

RTGL_DEFINE_RESOURCE_PRIVATE(texture)
RTGL_DEFINE_RESOURCE_PRIVATE(texture_view)

GLenum rtgl_texture_internal_format(enum rt_format format) {
	switch (format) {
	case RT_R8_UNORM:
		return GL_R8;
	case RT_RGBA8_UNORM:
		return GL_RGBA8;
	case RT_D32_SFLOAT:
		return GL_DEPTH_COMPONENT32F;
	default:
		return GL_NONE;
	}
}

GLenum rtgl_texture_upload_format(enum rt_format format) {
	switch (format) {
	case RT_R8_UNORM:
		return GL_RED;
	case RT_RGBA8_UNORM:
		return GL_RGBA;
	case RT_D32_SFLOAT:
		return GL_DEPTH_COMPONENT;
	default:
		return GL_NONE;
	}
}

GLenum rtgl_texture_upload_type(enum rt_format format) {
	switch (format) {
	case RT_R8_UNORM:
	case RT_RGBA8_UNORM:
		return GL_UNSIGNED_BYTE;
	case RT_D32_SFLOAT:
		return GL_FLOAT;
	default:
		return GL_NONE;
	}
}

void rtgl_texture_init(struct rtgl_context* ctx, struct rtgl_texture* texture) {
	rtgl_init_resource_base(ctx, RTGL_RESOURCE_BASE(texture), RTGL_RESOURCE_TEXTURE);
	texture->base.type = RT_TEXTURE_2D;
	texture->base.depth = 1;
	texture->base.mip_levels = 1;
	texture->base.gl_target = GL_TEXTURE_2D;
	rtgl_execution_texture_create(ctx, &texture->base);
}

void rtgl_texture_view_init(struct rtgl_context* ctx, struct rtgl_texture_view* view) {
	rtgl_init_resource_base(ctx, RTGL_RESOURCE_BASE(view), RTGL_RESOURCE_TEXTURE_VIEW);
	view->mag_filter = RT_FILTER_LINEAR;
	view->min_filter = RT_FILTER_LINEAR;
	view->mip_filter = RT_MIP_FILTER_NONE;
	view->address_u = RT_ADDRESS_REPEAT;
	view->address_v = RT_ADDRESS_REPEAT;
	view->address_w = RT_ADDRESS_REPEAT;
	view->max_anisotropy = 1;
	view->min_lod = 0.0f;
	view->max_lod = 1000.0f;
	view->lod_bias = 0.0f;
	view->parameters_applied = false;
}

void rtgl_texture_finish(struct rtgl_texture* texture) {
	rtgl_execution_texture_delete(texture->base.base.ctx, &texture->base);
	rtgl_finish_resource_base(RTGL_RESOURCE_BASE(texture));
}

void rtgl_texture_view_finish(struct rtgl_texture_view* view) {
	view->image = NULL;
	rtgl_finish_resource_base(RTGL_RESOURCE_BASE(view));
}

struct rtgl_timepoint rtgl_texture_data(struct rtgl_context* ctx, struct rtgl_texture* texture, enum rt_texture_type type, u32 mip, u32 width, u32 height, u32 depth, enum rt_format format, const void* data) {
	struct rtgl_timepoint timepoint = { NULL, 0 };
	GLenum internal_format = rtgl_texture_internal_format(format);
	if (texture->base.gl_texture) {
		rtgl_execution_texture_delete(ctx, &texture->base);
		rtgl_execution_texture_create(ctx, &texture->base);
	}
	texture->base.type = type;
	texture->base.format = format;
	texture->base.width = width;
	texture->base.height = height;
	texture->base.depth = depth;
	texture->base.mip_levels = 1;
	texture->base.gl_target = GL_TEXTURE_2D;
	texture->base.gl_internal_format = internal_format;
	rtgl_execution_texture_data(ctx, &texture->base, data);
	return timepoint;
}

struct rtgl_timepoint rtgl_texture_subdata(struct rtgl_context* ctx, struct rtgl_texture* texture, u32 mip, u32 offset_x, u32 offset_y, u32 offset_z, u32 width, u32 height, u32 depth, const void* data) {
	struct rtgl_timepoint timepoint = { NULL, 0 };
	if (!texture || !texture->base.gl_texture) {
		rtgl_throwf(RT_IMPROPER_USAGE, "rtTextureSubdata requires an initialized texture");
		return timepoint;
	}
	if (!data || width == 0 || height == 0 || depth == 0) {
		return timepoint;
	}
	if (texture->base.type != RT_TEXTURE_2D || offset_z != 0 || depth != 1) {
		rtgl_throwf(RT_UNSUPPORTED_FEATURE, "OpenGL texture subdata currently supports 2D texture regions only");
		return timepoint;
	}
	if (mip >= texture->base.mip_levels ||
		offset_x > texture->base.width || offset_y > texture->base.height ||
		width > texture->base.width - offset_x || height > texture->base.height - offset_y) {
		rtgl_throwf(RT_IMPROPER_USAGE, "rtTextureSubdata region is outside the texture bounds");
		return timepoint;
	}
	rtgl_execution_texture_subdata(ctx, &texture->base, mip, offset_x, offset_y, offset_z, width, height, depth, data);
	return timepoint;
}

void rtgl_texture_view_bind(struct rtgl_context* ctx, struct rtgl_texture_view* view, struct rtgl_texture* texture) {
	if (!texture) {
		return;
	}
	rtgl_texture_view_bind_image(ctx, view, &texture->base);
}

void rtgl_texture_view_bind_image(struct rtgl_context* ctx, struct rtgl_texture_view* view, struct rtgl_image_base* image) {
	(void)ctx;
	if (!view || !image) {
		return;
	}
	view->image = image;
	view->parameters_applied = false;
}

void rtgl_texture_view_image_data(struct rtgl_context* ctx, struct rtgl_texture_view* view, enum rt_texture_type type, u32 mip, u32 width, u32 height, u32 depth, enum rt_format format, const void* data) {
	(void)ctx;
	(void)view;
	(void)type;
	(void)mip;
	(void)width;
	(void)height;
	(void)depth;
	(void)format;
	(void)data;
	rtgl_throwf(RT_IMPROPER_USAGE, "OpenGL texture views do not own image data");
}

void rtgl_texture_view_image_data_internal(struct rtgl_context* ctx, struct rtgl_texture_view* view, enum rt_texture_type type, u32 mip, u32 width, u32 height, u32 depth, enum rt_format format, GLenum internal_format, const void* data) {
	(void)ctx;
	(void)view;
	(void)type;
	(void)mip;
	(void)width;
	(void)height;
	(void)depth;
	(void)format;
	(void)internal_format;
	(void)data;
	rtgl_throwf(RT_IMPROPER_USAGE, "OpenGL texture views do not own image data");
}

void rtgl_texture_view_filter(struct rtgl_context* ctx, struct rtgl_texture_view* view, enum rt_filter mag_filter, enum rt_filter min_filter, enum rt_mip_filter mip_filter) {
	view->mag_filter = mag_filter;
	view->min_filter = min_filter;
	view->mip_filter = mip_filter;
	view->parameters_applied = false;
}

void rtgl_texture_view_address(struct rtgl_context* ctx, struct rtgl_texture_view* view, enum rt_address_mode address_u, enum rt_address_mode address_v, enum rt_address_mode address_w) {
	view->address_u = address_u;
	view->address_v = address_v;
	view->address_w = address_w;
	view->parameters_applied = false;
}

void rtgl_texture_view_anisotropy(struct rtgl_context* ctx, struct rtgl_texture_view* view, u32 max_anisotropy) {
	view->max_anisotropy = max_anisotropy;
	view->parameters_applied = false;
}

void rtgl_texture_view_lod(struct rtgl_context* ctx, struct rtgl_texture_view* view, f32 min_lod, f32 max_lod, f32 lod_bias) {
	view->min_lod = min_lod;
	view->max_lod = max_lod;
	view->lod_bias = lod_bias;
	view->parameters_applied = false;
}

rt_extent_3d rtgl_texture_view_extent(struct rtgl_context* ctx, struct rtgl_texture_view* view) {
	(void)ctx;
	return view && view->image ? (rt_extent_3d){ view->image->width, view->image->height, view->image->depth } : (rt_extent_3d){ 0, 0, 0 };
}

bool rtgl_texture_view_valid(struct rtgl_texture_view* view) {
	return view && !view->base.zombie && view->image && !view->image->base.zombie && view->image->gl_texture;
}
