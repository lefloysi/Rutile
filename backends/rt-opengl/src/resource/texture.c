#include "texture.h"

#include "system/atomic.h"
#include "context.h"
#include "error.h"
#include "execution/execution.h"

#include <stdlib.h>
#include <string.h>

/*===============================================================================================*/
/*                                                                                               */
/*===============================================================================================*/

rt_texture rtTextureCreate(void) {
	rtgl_begin_errorable_operation();
	return rtgl_texture_to_handle(rtgl_texture_create(rtgl_get_current_context()));
}

void rtTextureDestroy(rt_texture texture) {
	rtgl_texture_destroy(rtgl_get_current_context(), rtgl_texture_from_handle(texture));
}

rt_texture_view rtTextureViewCreate(void) {
	rtgl_begin_errorable_operation();
	return rtgl_texture_view_to_handle(rtgl_texture_view_create(rtgl_get_current_context()));
}

void rtTextureResize(rt_texture texture, enum rt_texture_type type, enum rt_format format, rt_extent_3d extent, usize mip_count) {
	rtgl_begin_errorable_operation();
	struct rtgl_texture* internal = rtgl_texture_from_handle(texture);
	if (!internal || !extent.width || !extent.height || !extent.depth || !mip_count) {
		return;
	}
	rtgl_texture_data(rtgl_get_current_context(), internal, type, mip_count, (u32)extent.width, (u32)extent.height, (u32)extent.depth, format, NULL);
}

void rtTextureViewSetTexture(rt_texture_view texture_view, rt_texture texture) {
	rtgl_begin_errorable_operation();
	rtgl_texture_view_bind(rtgl_get_current_context(), rtgl_texture_view_from_handle(texture_view), rtgl_texture_from_handle(texture));
}

void rtTextureViewDestroy(rt_texture_view texture_view) {
	rtgl_texture_view_destroy(rtgl_get_current_context(), rtgl_texture_view_from_handle(texture_view));
}

void rtTextureViewSetFilter(rt_texture_view texture_view, enum rt_filter mag_filter, enum rt_filter min_filter, enum rt_mip_filter mip_filter) {
	rtgl_begin_errorable_operation();
	rtgl_texture_view_filter(rtgl_get_current_context(), rtgl_texture_view_from_handle(texture_view), mag_filter, min_filter, mip_filter);
}

void rtTextureViewSetAddress(rt_texture_view texture_view, enum rt_address_mode address_u, enum rt_address_mode address_v, enum rt_address_mode address_w) {
	rtgl_begin_errorable_operation();
	rtgl_texture_view_address(rtgl_get_current_context(), rtgl_texture_view_from_handle(texture_view), address_u, address_v, address_w);
}

void rtTextureViewSetAnisotropy(rt_texture_view texture_view, usize max_anisotropy) {
	rtgl_begin_errorable_operation();
	rtgl_texture_view_anisotropy(rtgl_get_current_context(), rtgl_texture_view_from_handle(texture_view), (u32)max_anisotropy);
}

void rtTextureViewSetLod(rt_texture_view texture_view, f32 min_lod, f32 max_lod, f32 lod_bias) {
	rtgl_begin_errorable_operation();
	rtgl_texture_view_lod(rtgl_get_current_context(), rtgl_texture_view_from_handle(texture_view), min_lod, max_lod, lod_bias);
}

rt_extent_3d rtTextureViewExtent(rt_texture_view texture_view) {
	rtgl_begin_errorable_operation();
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
	case RT_RG8_UNORM:
		return GL_RG8;
	case RT_RGB8_UNORM:
		return GL_RGB8;
	case RT_RGBA8_UNORM:
		return GL_RGBA8;
	case RT_R16_UNORM:
		return GL_R16;
	case RT_RG16_UNORM:
		return GL_RG16;
	case RT_RGB16_UNORM:
		return GL_RGB16;
	case RT_RGBA16_UNORM:
		return GL_RGBA16;
	case RT_R16_SFLOAT:
		return GL_R16F;
	case RT_RG16_SFLOAT:
		return GL_RG16F;
	case RT_RGB16_SFLOAT:
		return GL_RGB16F;
	case RT_RGBA16_SFLOAT:
		return GL_RGBA16F;
	case RT_R32_SFLOAT:
		return GL_R32F;
	case RT_RG32_SFLOAT:
		return GL_RG32F;
	case RT_RGB32_SFLOAT:
		return GL_RGB32F;
	case RT_RGBA32_SFLOAT:
		return GL_RGBA32F;
	case RT_R8_SINT:
		return GL_R8I;
	case RT_RG8_SINT:
		return GL_RG8I;
	case RT_RGB8_SINT:
		return GL_RGB8I;
	case RT_RGBA8_SINT:
		return GL_RGBA8I;
	case RT_R16_SINT:
		return GL_R16I;
	case RT_RG16_SINT:
		return GL_RG16I;
	case RT_RGB16_SINT:
		return GL_RGB16I;
	case RT_RGBA16_SINT:
		return GL_RGBA16I;
	case RT_R32_SINT:
		return GL_R32I;
	case RT_RG32_SINT:
		return GL_RG32I;
	case RT_RGB32_SINT:
		return GL_RGB32I;
	case RT_RGBA32_SINT:
		return GL_RGBA32I;
	case RT_R8_UINT:
		return GL_R8UI;
	case RT_RG8_UINT:
		return GL_RG8UI;
	case RT_RGB8_UINT:
		return GL_RGB8UI;
	case RT_RGBA8_UINT:
		return GL_RGBA8UI;
	case RT_R16_UINT:
		return GL_R16UI;
	case RT_RG16_UINT:
		return GL_RG16UI;
	case RT_RGB16_UINT:
		return GL_RGB16UI;
	case RT_RGBA16_UINT:
		return GL_RGBA16UI;
	case RT_R32_UINT:
		return GL_R32UI;
	case RT_RG32_UINT:
		return GL_RG32UI;
	case RT_RGB32_UINT:
		return GL_RGB32UI;
	case RT_RGBA32_UINT:
		return GL_RGBA32UI;
	case RT_D16_UNORM:
		return GL_DEPTH_COMPONENT16;
	case RT_D32_SFLOAT:
		return GL_DEPTH_COMPONENT32F;
	case RT_S8_UINT:
		return GL_STENCIL_INDEX8;
	case RT_D24_UNORM_S8_UINT:
		return GL_DEPTH24_STENCIL8;
	case RT_D32_SFLOAT_S8_UINT:
		return GL_DEPTH32F_STENCIL8;
	default:
		return GL_NONE;
	}
}

GLenum rtgl_texture_upload_format(enum rt_format format) {
	switch (format) {
	case RT_R8_UNORM:
	case RT_R16_UNORM:
	case RT_R16_SFLOAT:
	case RT_R32_SFLOAT:
		return GL_RED;
	case RT_RG8_UNORM:
	case RT_RG16_UNORM:
	case RT_RG16_SFLOAT:
	case RT_RG32_SFLOAT:
		return GL_RG;
	case RT_RGB8_UNORM:
	case RT_RGB16_UNORM:
	case RT_RGB16_SFLOAT:
	case RT_RGB32_SFLOAT:
		return GL_RGB;
	case RT_RGBA8_UNORM:
	case RT_RGBA16_UNORM:
	case RT_RGBA16_SFLOAT:
	case RT_RGBA32_SFLOAT:
		return GL_RGBA;
	case RT_R8_SINT:
	case RT_R16_SINT:
	case RT_R32_SINT:
		return GL_RED_INTEGER;
	case RT_RG8_SINT:
	case RT_RG16_SINT:
	case RT_RG32_SINT:
		return GL_RG_INTEGER;
	case RT_RGB8_SINT:
	case RT_RGB16_SINT:
	case RT_RGB32_SINT:
		return GL_RGB_INTEGER;
	case RT_RGBA8_SINT:
	case RT_RGBA16_SINT:
	case RT_RGBA32_SINT:
		return GL_RGBA_INTEGER;
	case RT_R8_UINT:
	case RT_R16_UINT:
	case RT_R32_UINT:
		return GL_RED_INTEGER;
	case RT_RG8_UINT:
	case RT_RG16_UINT:
	case RT_RG32_UINT:
		return GL_RG_INTEGER;
	case RT_RGB8_UINT:
	case RT_RGB16_UINT:
	case RT_RGB32_UINT:
		return GL_RGB_INTEGER;
	case RT_RGBA8_UINT:
	case RT_RGBA16_UINT:
	case RT_RGBA32_UINT:
		return GL_RGBA_INTEGER;
	case RT_D16_UNORM:
	case RT_D32_SFLOAT:
		return GL_DEPTH_COMPONENT;
	case RT_S8_UINT:
		return GL_STENCIL_INDEX;
	case RT_D24_UNORM_S8_UINT:
	case RT_D32_SFLOAT_S8_UINT:
		return GL_DEPTH_STENCIL;
	default:
		return GL_NONE;
	}
}

GLenum rtgl_texture_upload_type(enum rt_format format) {
	switch (format) {
	case RT_R8_UNORM:
	case RT_RG8_UNORM:
	case RT_RGB8_UNORM:
	case RT_RGBA8_UNORM:
	case RT_R8_UINT:
	case RT_RG8_UINT:
	case RT_RGB8_UINT:
	case RT_RGBA8_UINT:
		return GL_UNSIGNED_BYTE;
	case RT_R8_SINT:
	case RT_RG8_SINT:
	case RT_RGB8_SINT:
	case RT_RGBA8_SINT:
		return GL_BYTE;
	case RT_R16_UNORM:
	case RT_RG16_UNORM:
	case RT_RGB16_UNORM:
	case RT_RGBA16_UNORM:
	case RT_R16_UINT:
	case RT_RG16_UINT:
	case RT_RGB16_UINT:
	case RT_RGBA16_UINT:
	case RT_D16_UNORM:
		return GL_UNSIGNED_SHORT;
	case RT_R16_SINT:
	case RT_RG16_SINT:
	case RT_RGB16_SINT:
	case RT_RGBA16_SINT:
		return GL_SHORT;
	case RT_R16_SFLOAT:
	case RT_RG16_SFLOAT:
	case RT_RGB16_SFLOAT:
	case RT_RGBA16_SFLOAT:
		return GL_HALF_FLOAT;
	case RT_R32_SFLOAT:
	case RT_RG32_SFLOAT:
	case RT_RGB32_SFLOAT:
	case RT_RGBA32_SFLOAT:
	case RT_D32_SFLOAT:
		return GL_FLOAT;
	case RT_R32_SINT:
	case RT_RG32_SINT:
	case RT_RGB32_SINT:
	case RT_RGBA32_SINT:
		return GL_INT;
	case RT_R32_UINT:
	case RT_RG32_UINT:
	case RT_RGB32_UINT:
	case RT_RGBA32_UINT:
		return GL_UNSIGNED_INT;
	case RT_S8_UINT:
		return GL_UNSIGNED_BYTE;
	case RT_D24_UNORM_S8_UINT:
		return GL_UNSIGNED_INT_24_8;
	case RT_D32_SFLOAT_S8_UINT:
		return GL_FLOAT_32_UNSIGNED_INT_24_8_REV;
	default:
		return GL_NONE;
	}
}

void rtgl_texture_init(struct rtgl_context* ctx, struct rtgl_texture* texture) {
	rtgl_init_resource_base(ctx, RTGL_RESOURCE_BASE(texture), RTGL_RESOURCE_TEXTURE);
}

usize rtgl_texture_format_size(enum rt_format format) {
	switch (format) {
	case RT_R8_UNORM:
	case RT_R8_SINT:
	case RT_R8_UINT:
	case RT_S8_UINT:
		return 1;
	case RT_RG8_UNORM:
	case RT_RG8_SINT:
	case RT_RG8_UINT:
	case RT_R16_UNORM:
	case RT_R16_SFLOAT:
	case RT_R16_SINT:
	case RT_R16_UINT:
	case RT_D16_UNORM:
		return 2;
	case RT_RGB8_UNORM:
	case RT_RGB8_SINT:
	case RT_RGB8_UINT:
		return 3;
	case RT_RGBA8_UNORM:
	case RT_RGBA8_SINT:
	case RT_RGBA8_UINT:
	case RT_RG16_UNORM:
	case RT_RG16_SFLOAT:
	case RT_RG16_SINT:
	case RT_RG16_UINT:
	case RT_R32_SFLOAT:
	case RT_R32_SINT:
	case RT_R32_UINT:
	case RT_D32_SFLOAT:
	case RT_D24_UNORM_S8_UINT:
		return 4;
	case RT_RGB16_UNORM:
	case RT_RGB16_SFLOAT:
	case RT_RGB16_SINT:
	case RT_RGB16_UINT:
		return 6;
	case RT_RGBA16_UNORM:
	case RT_RGBA16_SFLOAT:
	case RT_RGBA16_SINT:
	case RT_RGBA16_UINT:
	case RT_RG32_SFLOAT:
	case RT_RG32_SINT:
	case RT_RG32_UINT:
	case RT_D32_SFLOAT_S8_UINT:
		return 8;
	case RT_RGB32_SFLOAT:
	case RT_RGB32_SINT:
	case RT_RGB32_UINT:
		return 12;
	case RT_RGBA32_SFLOAT:
	case RT_RGBA32_SINT:
	case RT_RGBA32_UINT:
		return 16;
	default:
		return 0;
	}
}

usize rtgl_texture_format_aspect_size(enum rt_format format, enum rt_texture_aspect_flag aspects) {
	if (format == RT_D24_UNORM_S8_UINT)
		return aspects == RT_TEXTURE_ASPECT_STENCIL ? 1 : 4;
	if (format == RT_D32_SFLOAT_S8_UINT)
		return aspects == RT_TEXTURE_ASPECT_STENCIL ? 1 : aspects == RT_TEXTURE_ASPECT_DEPTH ? 4
																							 : 8;
	return rtgl_texture_format_size(format);
}

GLenum rtgl_texture_upload_format_aspect(enum rt_format format, enum rt_texture_aspect_flag aspects) {
	if ((format == RT_D24_UNORM_S8_UINT || format == RT_D32_SFLOAT_S8_UINT) && aspects == RT_TEXTURE_ASPECT_DEPTH)
		return GL_DEPTH_COMPONENT;
	if ((format == RT_D24_UNORM_S8_UINT || format == RT_D32_SFLOAT_S8_UINT) && aspects == RT_TEXTURE_ASPECT_STENCIL)
		return GL_STENCIL_INDEX;
	return rtgl_texture_upload_format(format);
}

GLenum rtgl_texture_upload_type_aspect(enum rt_format format, enum rt_texture_aspect_flag aspects) {
	if ((format == RT_D24_UNORM_S8_UINT || format == RT_D32_SFLOAT_S8_UINT) && aspects == RT_TEXTURE_ASPECT_STENCIL)
		return GL_UNSIGNED_BYTE;
	if (format == RT_D24_UNORM_S8_UINT && aspects == RT_TEXTURE_ASPECT_DEPTH)
		return GL_UNSIGNED_INT;
	if (format == RT_D32_SFLOAT_S8_UINT && aspects == RT_TEXTURE_ASPECT_DEPTH)
		return GL_FLOAT;
	return rtgl_texture_upload_type(format);
}

static GLenum rtgl_texture_target(enum rt_texture_type type) {
	switch (type) {
	case RT_TEXTURE_1D:
		return GL_TEXTURE_1D;
	case RT_TEXTURE_2D:
		return GL_TEXTURE_2D;
	case RT_TEXTURE_3D:
		return GL_TEXTURE_3D;
	case RT_TEXTURE_1D_ARRAY:
		return GL_TEXTURE_1D_ARRAY;
	case RT_TEXTURE_2D_ARRAY:
		return GL_TEXTURE_2D_ARRAY;
	default:
		return GL_NONE;
	}
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
}

void rtTextureViewRead(rt_texture_view texture_view, rt_texture_range range, u08* data, usize data_size) {
	rtgl_begin_errorable_operation();
	struct rtgl_texture_view* view = rtgl_texture_view_from_handle(texture_view);
	if (!rtgl_texture_view_valid(view) || !data || !range.extent.width || !range.extent.height || !range.extent.depth || !range.mip_count || !range.layer_count) {
		return;
	}
	const usize bytes_per_texel = rtgl_texture_format_aspect_size(view->image->format, range.aspects);
	const usize layer_count = view->image->type == RT_TEXTURE_1D_ARRAY || view->image->type == RT_TEXTURE_2D_ARRAY ? range.layer_count : 1;
	usize required_size = 0;
	for (usize mip = 0; mip < range.mip_count; mip++) {
		const usize width = range.extent.width >> mip ? range.extent.width >> mip : 1;
		const usize height = range.extent.height >> mip ? range.extent.height >> mip : 1;
		const usize depth = range.extent.depth >> mip ? range.extent.depth >> mip : 1;
		const usize image_width = view->image->width >> (range.base_mip + mip) ? view->image->width >> (range.base_mip + mip) : 1;
		const usize image_height = view->image->height >> (range.base_mip + mip) ? view->image->height >> (range.base_mip + mip) : 1;
		const usize image_depth = view->image->depth >> (range.base_mip + mip) ? view->image->depth >> (range.base_mip + mip) : 1;
		if ((range.offset.width >> mip) > image_width || width > image_width - (range.offset.width >> mip) || ((view->image->type == RT_TEXTURE_2D || view->image->type == RT_TEXTURE_2D_ARRAY || view->image->type == RT_TEXTURE_3D) && ((range.offset.height >> mip) > image_height || height > image_height - (range.offset.height >> mip))) || (view->image->type == RT_TEXTURE_3D && ((range.offset.depth >> mip) > image_depth || depth > image_depth - (range.offset.depth >> mip)))) {
			return;
		}
		required_size += width * height * depth * layer_count * bytes_per_texel;
	}
	const bool array = view->image->type == RT_TEXTURE_1D_ARRAY || view->image->type == RT_TEXTURE_2D_ARRAY;
	enum rt_texture_aspect_flag supported_aspects = RT_TEXTURE_ASPECT_COLOR;
	if (view->image->format == RT_D16_UNORM || view->image->format == RT_D32_SFLOAT)
		supported_aspects = RT_TEXTURE_ASPECT_DEPTH;
	if (view->image->format == RT_S8_UINT)
		supported_aspects = RT_TEXTURE_ASPECT_STENCIL;
	if (view->image->format == RT_D24_UNORM_S8_UINT || view->image->format == RT_D32_SFLOAT_S8_UINT)
		supported_aspects = (enum rt_texture_aspect_flag)(RT_TEXTURE_ASPECT_DEPTH | RT_TEXTURE_ASPECT_STENCIL);
	if (!bytes_per_texel || !range.aspects || (range.aspects & ~supported_aspects) || data_size < required_size || range.base_mip >= view->image->mip_levels || range.mip_count > view->image->mip_levels - range.base_mip || (array && (range.base_layer >= view->image->depth || range.layer_count > view->image->depth - range.base_layer)) || (!array && (range.base_layer || range.layer_count != 1)) || ((view->image->type == RT_TEXTURE_1D || view->image->type == RT_TEXTURE_1D_ARRAY) && (range.offset.height || range.offset.depth || range.extent.height != 1 || range.extent.depth != 1)) || ((view->image->type == RT_TEXTURE_2D || view->image->type == RT_TEXTURE_2D_ARRAY) && (range.offset.depth || range.extent.depth != 1))) {
		return;
	}
	rtgl_execution_texture_read(rtgl_get_current_context(), view->image, range, data, data_size);
}

void rtgl_texture_finish(struct rtgl_texture* texture) {
	rtgl_texture_image_release(texture->base.ctx, texture->image);
	texture->image = NULL;
	struct rtgl_image_base* image = texture->reusable_images;
	while (image) {
		struct rtgl_image_base* next = image->next;
		image->next = NULL;
		rtgl_texture_image_release(texture->base.ctx, image);
		image = next;
	}
	texture->reusable_images = NULL;
	rtgl_finish_resource_base(RTGL_RESOURCE_BASE(texture));
}

void rtgl_texture_view_finish(struct rtgl_texture_view* view) {
	if (view->gl_sampler) {
		rtgl_execution_texture_view_delete_sampler(view->base.ctx, view);
	}
	if (view->texture) {
		struct rtgl_texture_view** current = &view->texture->views;
		while (*current && *current != view) {
			current = &(*current)->texture_next;
		}
		if (*current) {
			*current = view->texture_next;
		}
		rtgl_release_resource(view->texture);
	}
	view->texture_next = NULL;
	view->image = NULL;
	rtgl_finish_resource_base(RTGL_RESOURCE_BASE(view));
}

static struct rtgl_image_base* rtgl_texture_image_create(struct rtgl_context* ctx, enum rt_texture_type type, usize mip_count, u32 width, u32 height, u32 depth, enum rt_format format) {
	struct rtgl_image_base* image = (struct rtgl_image_base*)calloc(1, sizeof(*image));
	if (!image) {
		return NULL;
	}
	image->base.ctx = ctx;
	image->ref_count = 1;
	image->heap_owned = true;
	image->type = type;
	image->format = format;
	image->width = width;
	image->height = height;
	image->depth = depth;
	image->mip_levels = (u32)mip_count;
	image->gl_target = rtgl_texture_target(type);
	image->gl_internal_format = rtgl_texture_internal_format(format);
	if (image->gl_target == GL_NONE || image->gl_internal_format == GL_NONE) {
		free(image);
		return NULL;
	}
	rtgl_execution_texture_create(ctx, image);
	rtgl_execution_texture_data(ctx, image, NULL);
	return image;
}

void rtgl_texture_image_retain(struct rtgl_image_base* image) {
	if (image && image->heap_owned) {
		rt_atomic_inc(&image->ref_count);
	}
}

void rtgl_texture_image_release(struct rtgl_context* ctx, struct rtgl_image_base* image) {
	if (!image || !image->heap_owned || !rt_atomic_load(&image->ref_count) || rt_atomic_dec(&image->ref_count)) {
		return;
	}
	rtgl_execution_texture_delete(ctx, image);
	free(image);
}

static void rtgl_texture_update_views(struct rtgl_texture* texture) {
	for (struct rtgl_texture_view* view = texture->views; view; view = view->texture_next) {
		view->image = texture->image;
	}
}

static void rtgl_texture_recycle_image(struct rtgl_texture* texture, struct rtgl_image_base* image) {
	if (!image)
		return;
	image->next = texture->reusable_images;
	texture->reusable_images = image;
}

static struct rtgl_image_base* rtgl_texture_take_reusable_image(struct rtgl_texture* texture, enum rt_texture_type type, usize mip_count, u32 width, u32 height, u32 depth, enum rt_format format) {
	struct rtgl_image_base** link = &texture->reusable_images;
	while (*link) {
		struct rtgl_image_base* image = *link;
		if (rt_atomic_load(&image->ref_count) == 1 && image->type == type && image->mip_levels == mip_count && image->width == width && image->height == height && image->depth == depth && image->format == format) {
			*link = image->next;
			image->next = NULL;
			return image;
		}
		link = &image->next;
	}
	return NULL;
}

void rtgl_texture_image_copy(struct rtgl_image_base* dst, const struct rtgl_image_base* src) {
	if (!dst || !src || !dst->gl_texture || !src->gl_texture) {
		return;
	}
	glMemoryBarrier(GL_TEXTURE_UPDATE_BARRIER_BIT);
	const u32 mip_levels = dst->mip_levels < src->mip_levels ? dst->mip_levels : src->mip_levels;
	for (u32 mip = 0; mip < mip_levels; mip++) {
		u32 width = src->width >> mip;
		u32 height = src->height >> mip;
		u32 depth = src->depth >> mip;
		if (!width)
			width = 1;
		if (!height)
			height = 1;
		if (!depth)
			depth = 1;
		if (src->type == RT_TEXTURE_1D) {
			height = 1;
			depth = 1;
		} else if (src->type == RT_TEXTURE_2D) {
			depth = 1;
		} else if (src->type == RT_TEXTURE_1D_ARRAY) {
			height = src->depth;
			depth = 1;
		} else if (src->type == RT_TEXTURE_2D_ARRAY) {
			depth = src->depth;
		}
		glCopyImageSubData(src->gl_texture, src->gl_target, (GLint)mip, 0, 0, 0, dst->gl_texture, dst->gl_target, (GLint)mip, 0, 0, 0, (GLsizei)width, (GLsizei)height, (GLsizei)depth);
	}
}

struct rtgl_image_base* rtgl_texture_prepare_write(struct rtgl_context* ctx, struct rtgl_texture* texture, struct rtgl_image_base** copy_source) {
	if (copy_source) {
		*copy_source = NULL;
	}
	if (!texture || !texture->image) {
		return NULL;
	}
	if (rt_atomic_load(&texture->image->ref_count) == 1) {
		return texture->image;
	}
	struct rtgl_image_base* source = texture->image;
	struct rtgl_image_base* target = rtgl_texture_take_reusable_image(texture, source->type, source->mip_levels, source->width, source->height, source->depth, source->format);
	if (!target)
		target = rtgl_texture_image_create(ctx, source->type, source->mip_levels, source->width, source->height, source->depth, source->format);
	if (!target) {
		return NULL;
	}
	texture->image = target;
	rtgl_texture_recycle_image(texture, source);
	rtgl_texture_update_views(texture);
	if (copy_source) {
		*copy_source = source;
		rtgl_texture_image_retain(source);
	}
	return target;
}

rt_timepoint rtgl_texture_data(struct rtgl_context* ctx, struct rtgl_texture* texture, enum rt_texture_type type, usize mip_count, u32 width, u32 height, u32 depth, enum rt_format format, const void* data) {
	rt_timepoint timepoint = { 0 };
	if (!texture) {
		return timepoint;
	}
	struct rtgl_image_base* previous = texture->image;
	if (previous)
		rtgl_texture_recycle_image(texture, previous);
	texture->image = rtgl_texture_take_reusable_image(texture, type, mip_count, width, height, depth, format);
	if (!texture->image)
		texture->image = rtgl_texture_image_create(ctx, type, mip_count, width, height, depth, format);
	if (!texture->image) {
		texture->image = previous;
		if (previous) {
			texture->reusable_images = previous->next;
			previous->next = NULL;
		}
		return timepoint;
	}
	if (data) {
		rt_texture_range range = { 0 };
		range.mip_count = 1;
		range.layer_count = (type == RT_TEXTURE_1D_ARRAY || type == RT_TEXTURE_2D_ARRAY) ? depth : 1;
		range.extent.width = width;
		range.extent.height = type == RT_TEXTURE_1D || type == RT_TEXTURE_1D_ARRAY ? 1 : height;
		range.extent.depth = type == RT_TEXTURE_3D ? depth : 1;
		rtgl_execution_texture_subdata(ctx, texture->image, range, data);
	}
	rtgl_texture_update_views(texture);
	return timepoint;
}

void rtgl_texture_view_bind(struct rtgl_context* ctx, struct rtgl_texture_view* view, struct rtgl_texture* texture) {
	if (!view || !texture) {
		return;
	}
	if (view->texture && view->texture != texture) {
		struct rtgl_texture_view** current = &view->texture->views;
		while (*current && *current != view) {
			current = &(*current)->texture_next;
		}
		if (*current) {
			*current = view->texture_next;
		}
		rtgl_release_resource(view->texture);
		view->texture_next = NULL;
	}
	if (!view->texture) {
		rtgl_retain_resource(texture);
		view->texture = texture;
		view->texture_next = texture->views;
		texture->views = view;
	}
	rtgl_texture_view_bind_image(ctx, view, texture->image);
}
void rtgl_texture_view_bind_image(struct rtgl_context* ctx, struct rtgl_texture_view* view, struct rtgl_image_base* image) {
	(void)ctx;
	if (!view || !image) {
		return;
	}
	view->image = image;
}

void rtgl_texture_view_image_data(struct rtgl_context* ctx, struct rtgl_texture_view* view, enum rt_texture_type type, u32 mip, u32 width, u32 height, u32 depth, enum rt_format format, const void* data) {
	(void)ctx;
	(void)view;
	(void)type;
	(void)mip;
	(void)width;
	(void)height;
	(void)depth;
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
}

void rtgl_texture_view_address(struct rtgl_context* ctx, struct rtgl_texture_view* view, enum rt_address_mode address_u, enum rt_address_mode address_v, enum rt_address_mode address_w) {
	view->address_u = address_u;
	view->address_v = address_v;
	view->address_w = address_w;
}

void rtgl_texture_view_anisotropy(struct rtgl_context* ctx, struct rtgl_texture_view* view, u32 max_anisotropy) {
	view->max_anisotropy = max_anisotropy;
}

void rtgl_texture_view_lod(struct rtgl_context* ctx, struct rtgl_texture_view* view, f32 min_lod, f32 max_lod, f32 lod_bias) {
	view->min_lod = min_lod;
	view->max_lod = max_lod;
	view->lod_bias = lod_bias;
}

rt_extent_3d rtgl_texture_view_extent(struct rtgl_context* ctx, struct rtgl_texture_view* view) {
	(void)ctx;
	return view && view->image ? (rt_extent_3d){ view->image->width, view->image->height, view->image->depth } : (rt_extent_3d){ 0, 0, 0 };
}

GLenum rtgl_texture_view_sampler_filter(enum rt_filter filter, enum rt_mip_filter mip_filter) {
	if (mip_filter == RT_MIP_FILTER_NONE) {
		return filter == RT_FILTER_NEAREST ? GL_NEAREST : GL_LINEAR;
	}
	if (mip_filter == RT_MIP_FILTER_NEAREST) {
		return filter == RT_FILTER_NEAREST ? GL_NEAREST_MIPMAP_NEAREST : GL_LINEAR_MIPMAP_NEAREST;
	}
	return filter == RT_FILTER_NEAREST ? GL_NEAREST_MIPMAP_LINEAR : GL_LINEAR_MIPMAP_LINEAR;
}

GLenum rtgl_texture_view_sampler_address(enum rt_address_mode mode) {
	switch (mode) {
	case RT_ADDRESS_CLAMP:
		return GL_CLAMP_TO_EDGE;
	case RT_ADDRESS_MIRROR:
		return GL_MIRRORED_REPEAT;
	case RT_ADDRESS_REPEAT:
	default:
		return GL_REPEAT;
	}
}

void rtgl_texture_view_materialize(struct rtgl_context* ctx, struct rtgl_texture_view* view) {
	(void)ctx;
	if (!view || !view->image) {
		return;
	}
	if (!view->gl_sampler) {
		glCreateSamplers(1, &view->gl_sampler);
	}
	glSamplerParameteri(view->gl_sampler, GL_TEXTURE_MAG_FILTER, view->mag_filter == RT_FILTER_NEAREST ? GL_NEAREST : GL_LINEAR);
	glSamplerParameteri(view->gl_sampler, GL_TEXTURE_MIN_FILTER, rtgl_texture_view_sampler_filter(view->min_filter, view->mip_filter));
	glSamplerParameteri(view->gl_sampler, GL_TEXTURE_WRAP_S, rtgl_texture_view_sampler_address(view->address_u));
	glSamplerParameteri(view->gl_sampler, GL_TEXTURE_WRAP_T, rtgl_texture_view_sampler_address(view->address_v));
	glSamplerParameteri(view->gl_sampler, GL_TEXTURE_WRAP_R, rtgl_texture_view_sampler_address(view->address_w));
	glSamplerParameterf(view->gl_sampler, GL_TEXTURE_MIN_LOD, view->min_lod);
	glSamplerParameterf(view->gl_sampler, GL_TEXTURE_MAX_LOD, view->max_lod);
	glSamplerParameterf(view->gl_sampler, GL_TEXTURE_LOD_BIAS, view->lod_bias);
#if defined(GL_TEXTURE_MAX_ANISOTROPY)
	glSamplerParameterf(view->gl_sampler, GL_TEXTURE_MAX_ANISOTROPY, (GLfloat)view->max_anisotropy);
#endif
}

bool rtgl_texture_view_valid(struct rtgl_texture_view* view) {
	return view && !rt_atomic_bool_load(&view->base.zombie) && view->image && view->image->gl_texture;
}
