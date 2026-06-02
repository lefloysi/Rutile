#include "texture.h"
#include "context.h"
#include "error.h"
#include "extension/swapchain/swapchain.h"
#include "resource/buffer.h"
#include "resource/queue.h"

#include <stdlib.h>
#include <string.h>

/*===============================================================================================*/
/*                                                                                               */
/*===============================================================================================*/

rt_texture rtTextureCreate(void) {
	struct rtvk_texture* texture = rtvk_texture_create(rtvk_get_current_context());
	return rtvk_texture_to_handle(texture);
}

void rtTextureDestroy(rt_texture texture) {
	rtvk_texture_destroy(rtvk_get_current_context(), rtvk_texture_from_handle(texture));
}

rt_texture_view rtTextureViewCreate(rt_texture texture) {
	struct rtvk_texture_view* view = rtvk_texture_view_create_for_texture(
		rtvk_get_current_context(),
		rtvk_texture_from_handle(texture));
	return rtvk_texture_view_to_handle(view);
}

void rtTextureViewDestroy(rt_texture_view texture_view) {
	rtvk_texture_view_destroy(rtvk_get_current_context(), rtvk_texture_view_from_handle(texture_view));
}

void rtTextureViewFilter(rt_texture_view texture_view, enum rt_filter mag_filter, enum rt_filter min_filter, enum rt_mip_filter mip_filter) {
	rtvk_texture_view_filter(rtvk_texture_view_from_handle(texture_view), mag_filter, min_filter, mip_filter);
}

void rtTextureViewAddress(rt_texture_view texture_view, enum rt_address_mode address_u, enum rt_address_mode address_v, enum rt_address_mode address_w) {
	rtvk_texture_view_address(rtvk_texture_view_from_handle(texture_view), address_u, address_v, address_w);
}

void rtTextureViewAnisotropy(rt_texture_view texture_view, u32 max_anisotropy) {
	rtvk_texture_view_anisotropy(rtvk_texture_view_from_handle(texture_view), max_anisotropy);
}

void rtTextureViewLod(rt_texture_view texture_view, f32 min_lod, f32 max_lod, f32 lod_bias) {
	rtvk_texture_view_lod(rtvk_texture_view_from_handle(texture_view), min_lod, max_lod, lod_bias);
}

static VkSamplerAddressMode rtvk_sampler_address_mode(enum rt_address_mode mode) {
	switch (mode) {
	case RT_ADDRESS_CLAMP: return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	case RT_ADDRESS_MIRROR: return VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT;
	default: return VK_SAMPLER_ADDRESS_MODE_REPEAT;
	}
}

static void rtvk_texture_view_normalize_sampler(struct rtvk_texture_view* view) {
	if (!view->mag_filter) {
		view->mag_filter = RT_FILTER_LINEAR;
	}
	if (!view->min_filter) {
		view->min_filter = RT_FILTER_LINEAR;
	}
	if (!view->mip_filter) {
		view->mip_filter = RT_MIP_FILTER_NONE;
	}
	if (!view->address_u) {
		view->address_u = RT_ADDRESS_REPEAT;
	}
	if (!view->address_v) {
		view->address_v = RT_ADDRESS_REPEAT;
	}
	if (!view->address_w) {
		view->address_w = RT_ADDRESS_REPEAT;
	}
	if (!view->max_anisotropy) {
		view->max_anisotropy = 1;
	}
}

static bool rtvk_sampler_create(struct rtvk_context* ctx, struct rtvk_texture_view* view, VkSampler* sampler) {
	if (!sampler) {
		rtvk_throwf(RT_IMPROPER_USAGE, "sampler output is NULL");
		return false;
	}

	rtvk_texture_view_normalize_sampler(view);
	*sampler = VK_NULL_HANDLE;

	VkSamplerCreateInfo sampler_info = { VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO };
	sampler_info.magFilter = view->mag_filter == RT_FILTER_NEAREST ? VK_FILTER_NEAREST : VK_FILTER_LINEAR;
	sampler_info.minFilter = view->min_filter == RT_FILTER_NEAREST ? VK_FILTER_NEAREST : VK_FILTER_LINEAR;
	sampler_info.mipmapMode = view->mip_filter == RT_MIP_FILTER_LINEAR ? VK_SAMPLER_MIPMAP_MODE_LINEAR : VK_SAMPLER_MIPMAP_MODE_NEAREST;
	sampler_info.addressModeU = rtvk_sampler_address_mode(view->address_u);
	sampler_info.addressModeV = rtvk_sampler_address_mode(view->address_v);
	sampler_info.addressModeW = rtvk_sampler_address_mode(view->address_w);
	sampler_info.mipLodBias = view->lod_bias;
	sampler_info.anisotropyEnable = view->max_anisotropy > 1 ? VK_TRUE : VK_FALSE;
	sampler_info.maxAnisotropy = (f32)view->max_anisotropy;
	sampler_info.compareEnable = VK_FALSE;
	sampler_info.compareOp = VK_COMPARE_OP_ALWAYS;
	sampler_info.minLod = view->min_lod;
	sampler_info.maxLod = view->max_lod;
	sampler_info.borderColor = VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK;
	sampler_info.unnormalizedCoordinates = VK_FALSE;

	VkResult result = vkCreateSampler(ctx->vk_device, &sampler_info, VK_ALLOCATOR, sampler);
	if (result != VK_SUCCESS) {
		rtvk_throwf(rtvk_error_from_vk(result), NULL);
		return false;
	}
	return true;
}

rt_timepoint rtTextureCopy(rt_queue queue, rt_texture src_texture, u32 src_mip, rt_texture dst_texture, u32 dst_mip) {
	struct rtvk_timepoint timepoint = rtvk_texture_copy(
		rtvk_get_current_context(),
		rtvk_queue_from_handle(queue),
		rtvk_texture_from_handle(src_texture),
		src_mip,
		rtvk_texture_from_handle(dst_texture),
		dst_mip);
	return rtvk_timepoint_to_public(timepoint);
}
rt_timepoint rtTextureData(rt_queue queue, rt_texture texture, enum rt_texture_type type, u32 mip, u32 offset_x, u32 offset_y, u32 offset_z, enum rt_format format, const void* data) {
	struct rtvk_timepoint timepoint = rtvk_texture_data(
		rtvk_get_current_context(),
		rtvk_queue_from_handle(queue),
		rtvk_texture_from_handle(texture),
		type,
		mip,
		offset_x,
		offset_y,
		offset_z,
		format,
		data);
	return rtvk_timepoint_to_public(timepoint);
}
rt_timepoint rtTextureSubcopy(rt_queue queue, rt_texture src_texture, u32 src_mip, u32 src_x, u32 src_y, u32 src_z, rt_texture dst_texture, u32 dst_mip, u32 dst_x, u32 dst_y, u32 dst_z, u32 width, u32 height, u32 depth) {
	struct rtvk_timepoint timepoint = rtvk_texture_subcopy(
		rtvk_get_current_context(),
		rtvk_queue_from_handle(queue),
		rtvk_texture_from_handle(src_texture),
		src_mip,
		src_x,
		src_y,
		src_z,
		rtvk_texture_from_handle(dst_texture),
		dst_mip,
		dst_x,
		dst_y,
		dst_z,
		width,
		height,
		depth);
	return rtvk_timepoint_to_public(timepoint);
}
rt_timepoint rtTextureSubdata(rt_queue queue, rt_texture texture, u32 mip, u32 offset_x, u32 offset_y, u32 offset_z, u32 width, u32 height, u32 depth, const void* data) {
	struct rtvk_timepoint timepoint = rtvk_texture_subdata(
		rtvk_get_current_context(),
		rtvk_queue_from_handle(queue),
		rtvk_texture_from_handle(texture),
		mip,
		offset_x,
		offset_y,
		offset_z,
		width,
		height,
		depth,
		data);
	return rtvk_timepoint_to_public(timepoint);
}

rt_timepoint rtTextureViewCopyToBuffer(rt_queue queue, rt_texture_view texture_view, rt_buffer buffer) {
	struct rtvk_timepoint timepoint = rtvk_texture_view_copy_to_buffer(
		rtvk_get_current_context(),
		rtvk_queue_from_handle(queue),
		rtvk_texture_view_from_handle(texture_view),
		rtvk_buffer_from_handle(buffer));
	return rtvk_timepoint_to_public(timepoint);
}

rt_extent_3d rtTextureViewExtent(rt_texture_view texture_view) {
	rt_extent_3d extent = { 0, 0, 0 };
	struct rtvk_texture_view* view = rtvk_texture_view_from_handle(texture_view);
	if (!view || !view->texture || !view->vk_image_view) {
		rtvk_throwf(RT_IMPROPER_USAGE, "texture view extent query source is invalid");
		return extent;
	}
	extent.width = view->width;
	extent.height = view->height;
	extent.depth = view->depth;
	return extent;
}

/*===============================================================================================*/
/*                                                                                               */
/*===============================================================================================*/

RTVK_DEFINE_RESOURCE_PRIVATE(texture)
RTVK_DEFINE_RESOURCE_PRIVATE(texture_view)

static VkImageViewType rtvk_texture_view_type(enum rt_texture_type type) {
	switch (type) {
	case RT_TEXTURE_1D: return VK_IMAGE_VIEW_TYPE_1D;
	case RT_TEXTURE_2D: return VK_IMAGE_VIEW_TYPE_2D;
	case RT_TEXTURE_3D: return VK_IMAGE_VIEW_TYPE_3D;
	case RT_TEXTURE_1D_ARRAY: return VK_IMAGE_VIEW_TYPE_1D_ARRAY;
	case RT_TEXTURE_2D_ARRAY: return VK_IMAGE_VIEW_TYPE_2D_ARRAY;
	default: return VK_IMAGE_VIEW_TYPE_2D;
	}
}

static VkFormat rtvk_texture_format(enum rt_format format) {
	switch (format) {
	case RT_RGBA8_UNORM: return VK_FORMAT_R8G8B8A8_UNORM;
	case RT_D16_UNORM: return VK_FORMAT_D16_UNORM;
	case RT_D32_SFLOAT: return VK_FORMAT_D32_SFLOAT;
	case RT_S8_UINT: return VK_FORMAT_S8_UINT;
	case RT_D24_UNORM_S8_UINT: return VK_FORMAT_D24_UNORM_S8_UINT;
	case RT_D32_SFLOAT_S8_UINT: return VK_FORMAT_D32_SFLOAT_S8_UINT;
	default: return VK_FORMAT_UNDEFINED;
	}
}

static u32 rtvk_texture_format_bytes_per_pixel(enum rt_format format) {
	switch (format) {
	case RT_RGBA8_UNORM: return 4;
	case RT_D16_UNORM: return 2;
	case RT_D32_SFLOAT: return 4;
	case RT_S8_UINT: return 1;
	case RT_D24_UNORM_S8_UINT: return 4;
	case RT_D32_SFLOAT_S8_UINT: return 5;
	default: return 0;
	}
}

static bool rtvk_format_has_depth(VkFormat format) {
	return format == VK_FORMAT_D16_UNORM || format == VK_FORMAT_D32_SFLOAT ||
		   format == VK_FORMAT_D24_UNORM_S8_UINT || format == VK_FORMAT_D32_SFLOAT_S8_UINT;
}

static bool rtvk_format_has_stencil(VkFormat format) {
	return format == VK_FORMAT_S8_UINT || format == VK_FORMAT_D24_UNORM_S8_UINT ||
		   format == VK_FORMAT_D32_SFLOAT_S8_UINT;
}

VkImageAspectFlags rtvk_texture_format_aspect(VkFormat format) {
	VkImageAspectFlags aspect = 0;
	if (rtvk_format_has_depth(format)) {
		aspect |= VK_IMAGE_ASPECT_DEPTH_BIT;
	}
	if (rtvk_format_has_stencil(format)) {
		aspect |= VK_IMAGE_ASPECT_STENCIL_BIT;
	}
	return aspect ? aspect : VK_IMAGE_ASPECT_COLOR_BIT;
}

static u32 rtvk_texture_view_bytes_per_pixel(VkFormat format) {
	switch (format) {
	case VK_FORMAT_R8G8B8A8_UNORM:
	case VK_FORMAT_R8G8B8A8_SRGB:
	case VK_FORMAT_B8G8R8A8_UNORM:
	case VK_FORMAT_B8G8R8A8_SRGB:
		return 4;
	default:
		return 0;
	}
}

static bool rtvk_texture_view_needs_bgra_swizzle(VkFormat format) {
	return format == VK_FORMAT_B8G8R8A8_UNORM || format == VK_FORMAT_B8G8R8A8_SRGB;
}

static VkAccessFlags rtvk_texture_layout_access(VkImageLayout layout) {
	switch (layout) {
	case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL: return VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
	case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL:
		return VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
	case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL: return VK_ACCESS_TRANSFER_READ_BIT;
	case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL: return VK_ACCESS_TRANSFER_WRITE_BIT;
	case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL: return VK_ACCESS_SHADER_READ_BIT;
	default: return 0;
	}
}

static VkPipelineStageFlags rtvk_texture_layout_stage(VkImageLayout layout) {
	switch (layout) {
	case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL: return VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL:
		return VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
	case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL: return VK_PIPELINE_STAGE_TRANSFER_BIT;
	case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL: return VK_PIPELINE_STAGE_TRANSFER_BIT;
	case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL: return VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
	default: return VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
	}
}

static struct rtvk_queue* rtvk_texture_graphics_queue(struct rtvk_context* ctx, struct rtvk_queue* queue) {
	if (queue && queue->capability == RT_QUEUE_GRAPHICS) {
		return queue;
	}
	return rtvk_queue_query(ctx, RT_QUEUE_GRAPHICS);
}

void rtvk_texture_init(struct rtvk_context* ctx, struct rtvk_texture* texture) {
	rtvk_init_resource_base(ctx, RTVK_RESOURCE_BASE(texture), RT_RESOURCE_TEXTURE);
}

void rtvk_texture_view_init(struct rtvk_context* ctx, struct rtvk_texture_view* view) {
	rtvk_init_resource_base(ctx, RTVK_RESOURCE_BASE(view), RT_RESOURCE_TEXTURE_VIEW);
	rtvk_texture_view_normalize_sampler(view);
}

void rtvk_texture_finish(struct rtvk_context* ctx, struct rtvk_texture* texture) {
	rtvk_texture_node_release(texture->active);
	texture->active = NULL;

	struct rtvk_texture* node = texture->next;
	while (node) {
		struct rtvk_texture* next = node->next;
		node->next = NULL;
		rtvk_texture_node_release(node);
		node = next;
	}
	texture->next = NULL;
	rtvk_finish_resource_base(ctx, RTVK_RESOURCE_BASE(texture));
}

static struct rtvk_texture* rtvk_texture_node_create(struct rtvk_context* ctx) {
	struct rtvk_texture* node = RTVK_ALLOC_RESOURCE(struct rtvk_texture);
	if (!node) {
		rtvk_throwf(RT_OUT_OF_HOST_MEMORY, "failed to allocate texture metadata");
		return NULL;
	}

	rtvk_init_resource_base(ctx, RTVK_RESOURCE_BASE(node), RT_RESOURCE_TEXTURE);
	node->vk_layout = VK_IMAGE_LAYOUT_UNDEFINED;
	return node;
}

void rtvk_texture_node_retain(struct rtvk_texture* texture) {
	if (!texture) {
		return;
	}
	rtvk_resource_retain(RTVK_RESOURCE_BASE(texture));
}

void rtvk_texture_node_release(struct rtvk_texture* texture) {
	if (!texture) {
		return;
	}
	if (rtvk_atomic_dec(&texture->base.ref_count) != 0) {
		return;
	}

	struct rtvk_context* ctx = texture->base.ctx;
	if (texture->vk_image && texture->vma_allocation) {
		vmaDestroyImage(ctx->vma_allocator, texture->vk_image, texture->vma_allocation);
	}
	rtvk_finish_resource_base(ctx, RTVK_RESOURCE_BASE(texture));
	rtvk_atomic_bool_finish(&texture->base.zombie);
	rtvk_atomic_u32_finish(&texture->base.job_count);
	rtvk_atomic_u32_finish(&texture->base.ref_count);
	RTVK_FREE_RESOURCE(texture);
}

static void rtvk_texture_recycle_node(struct rtvk_texture* texture, struct rtvk_texture* node) {
	if (!node) {
		return;
	}
	node->next = texture->next;
	texture->next = node;
}

static void rtvk_texture_collect_nodes(struct rtvk_texture* texture) {
	struct rtvk_texture** link = &texture->next;
	while (*link) {
		struct rtvk_texture* node = *link;
		if (rtvk_atomic_load(&node->base.ref_count) == 1) {
			*link = node->next;
			node->next = NULL;
			rtvk_texture_node_release(node);
			continue;
		}
		link = &node->next;
	}
}

void rtvk_texture_view_finish(struct rtvk_context* ctx, struct rtvk_texture_view* view) {
	if (view->vk_sampler) {
		vkDestroySampler(ctx->vk_device, view->vk_sampler, VK_ALLOCATOR);
		view->vk_sampler = VK_NULL_HANDLE;
	}
	while (view->retired_samplers) {
		rtvk_retired_sampler* retired = view->retired_samplers;
		view->retired_samplers = retired->next;
		vkDestroySampler(ctx->vk_device, retired->vk_sampler, VK_ALLOCATOR);
		free(retired);
	}
	if (view->vk_image_view) {
		vkDestroyImageView(ctx->vk_device, view->vk_image_view, VK_ALLOCATOR);
		view->vk_image_view = VK_NULL_HANDLE;
	}
	if (view->texture) {
		rtvk_texture_node_release(view->texture);
		view->texture = NULL;
	}
	view->vk_format = VK_FORMAT_UNDEFINED;
	rtvk_finish_resource_base(ctx, RTVK_RESOURCE_BASE(view));
}

struct rtvk_texture* rtvk_texture_create_for_swapchain_image(struct rtvk_context* ctx, VkImage image, VkFormat format, u32 width, u32 height) {
	struct rtvk_texture* texture = rtvk_texture_create(ctx);
	if (!texture) {
		return NULL;
	}

	struct rtvk_texture* node = rtvk_texture_node_create(ctx);
	if (!node) {
		rtvk_texture_destroy(ctx, texture);
		return NULL;
	}

	node->vk_image = image;
	node->vk_format = format;
	node->vk_layout = VK_IMAGE_LAYOUT_UNDEFINED;
	node->type = RT_TEXTURE_2D;
	node->swapchain_image = true;
	node->width = width;
	node->height = height;
	node->depth = 1;
	texture->active = node;
	return texture;
}

static struct rtvk_texture_view* rtvk_texture_view_create_for_image(struct rtvk_context* ctx, struct rtvk_texture* texture) {
	struct rtvk_texture* node = texture ? texture->active : NULL;
	struct rtvk_texture_view* view = rtvk_texture_view_create(ctx);
	if (!view) {
		return NULL;
	}

	VkImageViewCreateInfo view_info = { VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO };
	u32 layer_count = 1;
	if ((node->type == RT_TEXTURE_1D_ARRAY || node->type == RT_TEXTURE_2D_ARRAY) && node->depth) {
		layer_count = node->depth;
	}

	view_info.pNext = NULL;
	view_info.flags = 0;
	view_info.image = node->vk_image;
	view_info.viewType = rtvk_texture_view_type(node->type);
	view_info.format = node->vk_format;
	view_info.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
	view_info.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
	view_info.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
	view_info.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
	view_info.subresourceRange.aspectMask = rtvk_texture_format_aspect(node->vk_format);
	view_info.subresourceRange.baseMipLevel = 0;
	view_info.subresourceRange.levelCount = 1;
	view_info.subresourceRange.baseArrayLayer = 0;
	view_info.subresourceRange.layerCount = layer_count;

	VkResult result = vkCreateImageView(ctx->vk_device, &view_info, VK_ALLOCATOR, &view->vk_image_view);
	if (result != VK_SUCCESS) {
		rtvk_texture_view_destroy(ctx, view);
		rtvk_throwf(rtvk_error_from_vk(result), NULL);
		return NULL;
	}

	rtvk_texture_node_retain(node);
	view->texture = node;
	view->vk_format = node->vk_format;
	view->width = node->width;
	view->height = node->height;
	view->depth = node->depth;
	return view;
}

struct rtvk_texture_view* rtvk_texture_view_create_for_texture(struct rtvk_context* ctx, struct rtvk_texture* texture) {
	if (!texture || !texture->active || !texture->active->vk_image) {
		rtvk_throwf(RT_IMPROPER_USAGE, "texture view source texture is invalid");
		return NULL;
	}
	return rtvk_texture_view_create_for_image(ctx, texture);
}

struct rtvk_texture_view* rtvk_texture_view_create_for_swapchain(struct rtvk_context* ctx, struct rtvk_texture* texture) {
	if (!texture || !texture->active || !texture->active->vk_image) {
		rtvk_throwf(RT_IMPROPER_USAGE, "swapchain texture view source texture is invalid");
		return NULL;
	}
	return rtvk_texture_view_create_for_image(ctx, texture);
}

static bool rtvk_texture_view_sampler_valid(struct rtvk_texture_view* texture_view) {
	if (!texture_view) {
		rtvk_throwf(RT_IMPROPER_USAGE, "texture view is NULL");
		return false;
	}
	return true;
}

bool rtvk_texture_view_prepare_sampler(struct rtvk_context* ctx, struct rtvk_texture_view* texture_view) {
	if (!rtvk_texture_view_sampler_valid(texture_view)) {
		return false;
	}
	if (texture_view->vk_sampler) {
		return true;
	}

	VkSampler vk_sampler = VK_NULL_HANDLE;
	if (!rtvk_sampler_create(ctx, texture_view, &vk_sampler)) {
		return false;
	}
	texture_view->vk_sampler = vk_sampler;
	return true;
}

static void rtvk_texture_view_retire_sampler(struct rtvk_texture_view* texture_view, VkSampler sampler) {
	if (!sampler) {
		return;
	}

	rtvk_retired_sampler* retired = malloc(sizeof(*retired));
	if (!retired) {
		rtvk_throwf(RT_OUT_OF_HOST_MEMORY, "failed to retire sampler");
		return;
	}
	retired->vk_sampler = sampler;
	retired->next = texture_view->retired_samplers;
	texture_view->retired_samplers = retired;
}

static bool rtvk_texture_view_recreate_sampler(struct rtvk_texture_view* texture_view) {
	struct rtvk_context* ctx = texture_view->base.ctx;
	VkSampler vk_sampler = VK_NULL_HANDLE;
	if (!rtvk_sampler_create(ctx, texture_view, &vk_sampler)) {
		return false;
	}
	if (texture_view->vk_sampler) {
		rtvk_texture_view_retire_sampler(texture_view, texture_view->vk_sampler);
	}
	texture_view->vk_sampler = vk_sampler;
	return true;
}

void rtvk_texture_view_filter(
	struct rtvk_texture_view* texture_view,
	enum rt_filter mag_filter,
	enum rt_filter min_filter,
	enum rt_mip_filter mip_filter) {
	if (!rtvk_texture_view_sampler_valid(texture_view)) {
		return;
	}
	if (texture_view->mag_filter == mag_filter &&
		texture_view->min_filter == min_filter &&
		texture_view->mip_filter == mip_filter) {
		return;
	}
	texture_view->mag_filter = mag_filter;
	texture_view->min_filter = min_filter;
	texture_view->mip_filter = mip_filter;
	rtvk_texture_view_normalize_sampler(texture_view);
	rtvk_texture_view_recreate_sampler(texture_view);
}

void rtvk_texture_view_address(
	struct rtvk_texture_view* texture_view,
	enum rt_address_mode address_u,
	enum rt_address_mode address_v,
	enum rt_address_mode address_w) {
	if (!rtvk_texture_view_sampler_valid(texture_view)) {
		return;
	}
	if (texture_view->address_u == address_u &&
		texture_view->address_v == address_v &&
		texture_view->address_w == address_w) {
		return;
	}
	texture_view->address_u = address_u;
	texture_view->address_v = address_v;
	texture_view->address_w = address_w;
	rtvk_texture_view_normalize_sampler(texture_view);
	rtvk_texture_view_recreate_sampler(texture_view);
}

void rtvk_texture_view_anisotropy(struct rtvk_texture_view* texture_view, u32 max_anisotropy) {
	if (!rtvk_texture_view_sampler_valid(texture_view)) {
		return;
	}
	if (texture_view->max_anisotropy == max_anisotropy) {
		return;
	}
	texture_view->max_anisotropy = max_anisotropy;
	rtvk_texture_view_normalize_sampler(texture_view);
	rtvk_texture_view_recreate_sampler(texture_view);
}

void rtvk_texture_view_lod(
	struct rtvk_texture_view* texture_view,
	f32 min_lod,
	f32 max_lod,
	f32 lod_bias) {
	if (!rtvk_texture_view_sampler_valid(texture_view)) {
		return;
	}
	if (texture_view->min_lod == min_lod &&
		texture_view->max_lod == max_lod &&
		texture_view->lod_bias == lod_bias) {
		return;
	}
	texture_view->min_lod = min_lod;
	texture_view->max_lod = max_lod;
	texture_view->lod_bias = lod_bias;
	rtvk_texture_view_normalize_sampler(texture_view);
	rtvk_texture_view_recreate_sampler(texture_view);
}

struct rtvk_timepoint rtvk_texture_copy(struct rtvk_context* ctx, struct rtvk_queue* queue, struct rtvk_texture* src_texture, u32 src_mip, struct rtvk_texture* dst_texture, u32 dst_mip) {
	struct rtvk_timepoint timepoint = { queue, 0 };
	rtvk_throwf(RT_UNSUPPORTED_FEATURE, "texture copy is not implemented yet");
	return timepoint;
}

static bool rtvk_texture_upload_command(struct rtvk_context* ctx, struct rtvk_queue* queue) {
	if (queue->upload_command_pool && queue->upload_command_buffer) {
		rtvk_queue_collect(ctx, queue);
		if (queue->upload_command_timepoint.queue && queue->upload_command_timepoint.value > queue->completed_value) {
			rtvk_queue_retire_upload_resources(ctx, queue, true, false);
		}
	}
	if (queue->upload_command_pool && queue->upload_command_buffer) {
		queue->upload_command_timepoint = (struct rtvk_timepoint){ NULL, 0 };
		vkResetCommandPool(ctx->vk_device, queue->upload_command_pool, 0);
		return true;
	}

	VkCommandPoolCreateInfo pool_info = { VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO };
	pool_info.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT | VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
	pool_info.queueFamilyIndex = queue->family_index;
	VkResult result = vkCreateCommandPool(ctx->vk_device, &pool_info, VK_ALLOCATOR, &queue->upload_command_pool);
	if (result != VK_SUCCESS) {
		rtvk_throwf(rtvk_error_from_vk(result), NULL);
		return false;
	}

	VkCommandBufferAllocateInfo alloc_info = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO };
	alloc_info.commandPool = queue->upload_command_pool;
	alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	alloc_info.commandBufferCount = 1;
	result = vkAllocateCommandBuffers(ctx->vk_device, &alloc_info, &queue->upload_command_buffer);
	if (result != VK_SUCCESS) {
		vkDestroyCommandPool(ctx->vk_device, queue->upload_command_pool, VK_ALLOCATOR);
		queue->upload_command_pool = VK_NULL_HANDLE;
		rtvk_throwf(rtvk_error_from_vk(result), NULL);
		return false;
	}
	return true;
}

static bool rtvk_texture_upload_staging(struct rtvk_context* ctx, struct rtvk_queue* queue, u64 size) {
	if (queue->upload_staging_buffer && queue->upload_staging_size >= size) {
		rtvk_queue_collect(ctx, queue);
		if (queue->upload_command_timepoint.queue && queue->upload_command_timepoint.value > queue->completed_value) {
			rtvk_queue_retire_upload_resources(ctx, queue, false, true);
		}
	}
	if (queue->upload_staging_buffer && queue->upload_staging_size >= size) {
		return true;
	}

	rtvk_queue_collect(ctx, queue);
	if (queue->upload_staging_buffer) {
		if (queue->upload_command_timepoint.queue && queue->upload_command_timepoint.value > queue->completed_value) {
			rtvk_queue_retire_upload_resources(ctx, queue, false, true);
		} else {
			vmaDestroyBuffer(ctx->vma_allocator, queue->upload_staging_buffer, queue->upload_staging_allocation);
			queue->upload_staging_buffer = VK_NULL_HANDLE;
			queue->upload_staging_allocation = NULL;
			queue->upload_staging_size = 0;
		}
	}
	VkBufferCreateInfo buffer_info = { VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
	buffer_info.size = size;
	buffer_info.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
	buffer_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

	VmaAllocationCreateInfo alloc_info = { 0 };
	alloc_info.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT;
	alloc_info.usage = VMA_MEMORY_USAGE_AUTO_PREFER_HOST;

	VkResult result = vmaCreateBuffer(
		ctx->vma_allocator,
		&buffer_info,
		&alloc_info,
		&queue->upload_staging_buffer,
		&queue->upload_staging_allocation,
		NULL);
	if (result != VK_SUCCESS) {
		rtvk_throwf(rtvk_error_from_vk(result), NULL);
		return false;
	}
	queue->upload_staging_size = size;
	return true;
}

struct rtvk_timepoint rtvk_texture_data(struct rtvk_context* ctx, struct rtvk_queue* queue, struct rtvk_texture* texture, enum rt_texture_type type, u32 mip, u32 offset_x, u32 offset_y, u32 offset_z, enum rt_format format, const void* data) {
	queue = rtvk_texture_graphics_queue(ctx, queue);
	struct rtvk_timepoint timepoint = { queue, 0 };
	if (!queue) {
		rtvk_throwf(RT_IMPROPER_USAGE, "texture data upload requires a graphics queue");
		return timepoint;
	}
	if (!texture) {
		rtvk_throwf(RT_IMPROPER_USAGE, "texture is NULL");
		return timepoint;
	}
	if (type != RT_TEXTURE_2D || mip != 0 || offset_z > 1) {
		rtvk_throwf(RT_UNSUPPORTED_FEATURE, "texture data currently supports only 2D mip 0 textures");
		return timepoint;
	}
	if (offset_x == 0 || offset_y == 0) {
		rtvk_throwf(RT_IMPROPER_USAGE, "texture dimensions must be nonzero");
		return timepoint;
	}

	VkFormat vk_format = rtvk_texture_format(format);
	u32 bytes_per_pixel = rtvk_texture_format_bytes_per_pixel(format);
	if (vk_format == VK_FORMAT_UNDEFINED || bytes_per_pixel == 0) {
		rtvk_throwf(RT_UNSUPPORTED_FEATURE, "unsupported texture format");
		return timepoint;
	}

	rtvk_queue_collect(ctx, queue);
	rtvk_texture_collect_nodes(texture);

	struct rtvk_texture* node = rtvk_texture_node_create(ctx);
	if (!node) {
		return timepoint;
	}

	VkImageCreateInfo image_info = { VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO };
	image_info.imageType = VK_IMAGE_TYPE_2D;
	image_info.format = vk_format;
	image_info.extent.width = offset_x;
	image_info.extent.height = offset_y;
	image_info.extent.depth = 1;
	image_info.mipLevels = 1;
	image_info.arrayLayers = 1;
	image_info.samples = VK_SAMPLE_COUNT_1_BIT;
	image_info.tiling = VK_IMAGE_TILING_OPTIMAL;
	image_info.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
	if (rtvk_format_has_depth(vk_format) || rtvk_format_has_stencil(vk_format)) {
		image_info.usage |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
	} else {
		image_info.usage |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
	}
	image_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	image_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

	VmaAllocationCreateInfo allocation_info = { 0 };
	allocation_info.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;

	VkResult result = vmaCreateImage(ctx->vma_allocator, &image_info, &allocation_info, &node->vk_image, &node->vma_allocation, NULL);
	if (result != VK_SUCCESS) {
		rtvk_texture_node_release(node);
		rtvk_throwf(rtvk_error_from_vk(result), NULL);
		return timepoint;
	}

	node->vk_format = vk_format;
	node->vk_layout = VK_IMAGE_LAYOUT_UNDEFINED;
	node->type = type;
	node->width = offset_x;
	node->height = offset_y;
	node->depth = 1;
	const VkImageAspectFlags aspect = rtvk_texture_format_aspect(vk_format);
	if ((rtvk_format_has_depth(vk_format) || rtvk_format_has_stencil(vk_format)) && !data) {
		rtvk_texture_recycle_node(texture, texture->active);
		texture->active = node;
		return timepoint;
	}

	u64 upload_size = (u64)offset_x * offset_y * bytes_per_pixel;
	if (data && upload_size && !rtvk_texture_upload_staging(ctx, queue, upload_size)) {
		rtvk_texture_node_release(node);
		return timepoint;
	}

	if (data && upload_size) {
		VmaAllocationInfo staging_info;
		vmaGetAllocationInfo(ctx->vma_allocator, queue->upload_staging_allocation, &staging_info);
		memcpy(staging_info.pMappedData, data, (usize)upload_size);
		vmaFlushAllocation(ctx->vma_allocator, queue->upload_staging_allocation, 0, upload_size);
	}

	if (!rtvk_texture_upload_command(ctx, queue)) {
		rtvk_texture_node_release(node);
		return timepoint;
	}
	VkCommandBuffer command_buffer = queue->upload_command_buffer;

	VkCommandBufferBeginInfo begin_info = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
	begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
	result = vkBeginCommandBuffer(command_buffer, &begin_info);
	if (result != VK_SUCCESS) {
		rtvk_texture_node_release(node);
		rtvk_throwf(rtvk_error_from_vk(result), NULL);
		return timepoint;
	}

	VkImageMemoryBarrier barrier = { VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER };
	barrier.srcAccessMask = 0;
	barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	barrier.dstAccessMask = data ? VK_ACCESS_TRANSFER_WRITE_BIT : VK_ACCESS_SHADER_READ_BIT;
	barrier.newLayout = data ? VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL : VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.image = node->vk_image;
	barrier.subresourceRange.aspectMask = aspect;
	barrier.subresourceRange.baseMipLevel = 0;
	barrier.subresourceRange.levelCount = 1;
	barrier.subresourceRange.baseArrayLayer = 0;
	barrier.subresourceRange.layerCount = 1;
	vkCmdPipelineBarrier(command_buffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
						 data ? VK_PIPELINE_STAGE_TRANSFER_BIT : VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
						 0, 0, NULL, 0, NULL, 1, &barrier);

	if (data && upload_size) {
		VkBufferImageCopy copy = { 0 };
		copy.bufferOffset = 0;
		copy.imageSubresource.aspectMask = aspect;
		copy.imageSubresource.mipLevel = 0;
		copy.imageSubresource.baseArrayLayer = 0;
		copy.imageSubresource.layerCount = 1;
		copy.imageExtent.width = offset_x;
		copy.imageExtent.height = offset_y;
		copy.imageExtent.depth = 1;
		vkCmdCopyBufferToImage(
			command_buffer,
			queue->upload_staging_buffer,
			node->vk_image,
			VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			1,
			&copy);

		barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
		barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
		barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		vkCmdPipelineBarrier(command_buffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
							 0, 0, NULL, 0, NULL, 1, &barrier);
	}

	result = vkEndCommandBuffer(command_buffer);
	if (result == VK_SUCCESS) {
		u64 value = queue->timeline_value + 1;
		VkTimelineSemaphoreSubmitInfo timeline_info = { VK_STRUCTURE_TYPE_TIMELINE_SEMAPHORE_SUBMIT_INFO };
		timeline_info.signalSemaphoreValueCount = 1;
		timeline_info.pSignalSemaphoreValues = &value;

		VkSubmitInfo submit_info = { VK_STRUCTURE_TYPE_SUBMIT_INFO };
		submit_info.pNext = &timeline_info;
		submit_info.commandBufferCount = 1;
		submit_info.pCommandBuffers = &command_buffer;
		submit_info.signalSemaphoreCount = 1;
		submit_info.pSignalSemaphores = &queue->vk_timeline;
		result = vkQueueSubmit(queue->vk_queue, 1, &submit_info, VK_NULL_HANDLE);
		if (result == VK_SUCCESS) {
			queue->timeline_value = value;
			queue->submitted_value = value;
			timepoint = (struct rtvk_timepoint){ queue, value };
			queue->upload_command_timepoint = timepoint;
		}
	}

	if (result != VK_SUCCESS) {
		rtvk_texture_node_release(node);
		rtvk_throwf(rtvk_error_from_vk(result), NULL);
		return timepoint;
	}

	node->vk_layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	rtvk_texture_recycle_node(texture, texture->active);
	texture->active = node;
	return timepoint;
}

struct rtvk_timepoint rtvk_texture_subcopy(struct rtvk_context* ctx, struct rtvk_queue* queue, struct rtvk_texture* src_texture, u32 src_mip, u32 src_x, u32 src_y, u32 src_z, struct rtvk_texture* dst_texture, u32 dst_mip, u32 dst_x, u32 dst_y, u32 dst_z, u32 width, u32 height, u32 depth) {
	struct rtvk_timepoint timepoint = { queue, 0 };
	rtvk_throwf(RT_UNSUPPORTED_FEATURE, "texture subcopy is not implemented yet");
	return timepoint;
}

struct rtvk_timepoint rtvk_texture_subdata(struct rtvk_context* ctx, struct rtvk_queue* queue, struct rtvk_texture* texture, u32 mip, u32 offset_x, u32 offset_y, u32 offset_z, u32 width, u32 height, u32 depth, const void* data) {
	queue = rtvk_texture_graphics_queue(ctx, queue);
	struct rtvk_timepoint timepoint = { queue, 0 };
	if (!queue) {
		rtvk_throwf(RT_IMPROPER_USAGE, "texture subdata upload requires a graphics queue");
		return timepoint;
	}
	if (!texture || !texture->active || !texture->active->vk_image) {
		rtvk_throwf(RT_IMPROPER_USAGE, "texture subdata target is invalid");
		return timepoint;
	}
	if (width == 0 || height == 0 || depth == 0) {
		return timepoint;
	}
	if (!data) {
		rtvk_throwf(RT_IMPROPER_USAGE, "texture subdata source data is NULL");
		return timepoint;
	}

	struct rtvk_texture* node = texture->active;
	if (node->type != RT_TEXTURE_2D || mip != 0 || offset_z != 0 || depth != 1) {
		rtvk_throwf(RT_UNSUPPORTED_FEATURE, "texture subdata currently supports only 2D mip 0 textures");
		return timepoint;
	}
	if (offset_x > node->width || offset_y > node->height ||
		width > node->width - offset_x || height > node->height - offset_y) {
		rtvk_throwf(RT_IMPROPER_USAGE, "texture subdata region is out of bounds");
		return timepoint;
	}

	u32 bytes_per_pixel = rtvk_texture_view_bytes_per_pixel(node->vk_format);
	if (bytes_per_pixel == 0) {
		rtvk_throwf(RT_UNSUPPORTED_FEATURE, "texture subdata does not support this Vulkan format");
		return timepoint;
	}

	u64 upload_size = (u64)width * height * depth * bytes_per_pixel;
	rtvk_queue_collect(ctx, queue);
	rtvk_queue_flush(ctx, queue);
	if (!rtvk_texture_upload_staging(ctx, queue, upload_size)) {
		return timepoint;
	}

	VmaAllocationInfo staging_info;
	vmaGetAllocationInfo(ctx->vma_allocator, queue->upload_staging_allocation, &staging_info);
	memcpy(staging_info.pMappedData, data, (usize)upload_size);
	vmaFlushAllocation(ctx->vma_allocator, queue->upload_staging_allocation, 0, upload_size);

	if (!rtvk_texture_upload_command(ctx, queue)) {
		return timepoint;
	}
	VkCommandBuffer command_buffer = queue->upload_command_buffer;

	VkCommandBufferBeginInfo begin_info = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
	begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
	VkResult result = vkBeginCommandBuffer(command_buffer, &begin_info);
	if (result != VK_SUCCESS) {
		rtvk_throwf(rtvk_error_from_vk(result), NULL);
		return timepoint;
	}

	VkImageLayout original_layout = node->vk_layout;
	VkImageLayout restore_layout = original_layout == VK_IMAGE_LAYOUT_UNDEFINED ?
								   VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL :
								   original_layout;
	const VkImageAspectFlags aspect = rtvk_texture_format_aspect(node->vk_format);

	VkImageMemoryBarrier barrier = { VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER };
	barrier.srcAccessMask = rtvk_texture_layout_access(original_layout);
	barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
	barrier.oldLayout = original_layout;
	barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
	barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.image = node->vk_image;
	barrier.subresourceRange.aspectMask = aspect;
	barrier.subresourceRange.baseMipLevel = 0;
	barrier.subresourceRange.levelCount = 1;
	barrier.subresourceRange.baseArrayLayer = 0;
	barrier.subresourceRange.layerCount = 1;
	vkCmdPipelineBarrier(command_buffer, rtvk_texture_layout_stage(original_layout),
						 VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, NULL, 0, NULL, 1, &barrier);

	VkBufferImageCopy copy = { 0 };
	copy.bufferOffset = 0;
	copy.bufferRowLength = 0;
	copy.bufferImageHeight = 0;
	copy.imageSubresource.aspectMask = aspect;
	copy.imageSubresource.mipLevel = 0;
	copy.imageSubresource.baseArrayLayer = 0;
	copy.imageSubresource.layerCount = 1;
	copy.imageOffset.x = offset_x;
	copy.imageOffset.y = offset_y;
	copy.imageOffset.z = offset_z;
	copy.imageExtent.width = width;
	copy.imageExtent.height = height;
	copy.imageExtent.depth = depth;
	vkCmdCopyBufferToImage(
		command_buffer,
		queue->upload_staging_buffer,
		node->vk_image,
		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
		1,
		&copy);

	barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
	barrier.dstAccessMask = rtvk_texture_layout_access(restore_layout);
	barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
	barrier.newLayout = restore_layout;
	VkPipelineStageFlags restore_stage = rtvk_texture_layout_stage(restore_layout);
	if (restore_stage == VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT) {
		restore_stage = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
	}
	vkCmdPipelineBarrier(command_buffer, VK_PIPELINE_STAGE_TRANSFER_BIT, restore_stage,
						 0, 0, NULL, 0, NULL, 1, &barrier);

	result = vkEndCommandBuffer(command_buffer);
	if (result == VK_SUCCESS) {
		u64 value = queue->timeline_value + 1;
		VkTimelineSemaphoreSubmitInfo timeline_info = { VK_STRUCTURE_TYPE_TIMELINE_SEMAPHORE_SUBMIT_INFO };
		timeline_info.signalSemaphoreValueCount = 1;
		timeline_info.pSignalSemaphoreValues = &value;

		VkSubmitInfo submit_info = { VK_STRUCTURE_TYPE_SUBMIT_INFO };
		submit_info.pNext = &timeline_info;
		submit_info.commandBufferCount = 1;
		submit_info.pCommandBuffers = &command_buffer;
		submit_info.signalSemaphoreCount = 1;
		submit_info.pSignalSemaphores = &queue->vk_timeline;
		result = vkQueueSubmit(queue->vk_queue, 1, &submit_info, VK_NULL_HANDLE);
		if (result == VK_SUCCESS) {
			queue->timeline_value = value;
			queue->submitted_value = value;
			timepoint = (struct rtvk_timepoint){ queue, value };
			queue->upload_command_timepoint = timepoint;
			node->vk_layout = restore_layout;
		}
	}

	if (result != VK_SUCCESS) {
		rtvk_throwf(rtvk_error_from_vk(result), NULL);
		return timepoint;
	}

	return timepoint;
}

static bool rtvk_texture_copy_buffer_command(struct rtvk_context* ctx, struct rtvk_queue* queue) {
	if (queue->copy_command_pool && queue->copy_command_buffer) {
		rtvk_queue_collect(ctx, queue);
		queue->copy_command_timepoint = (struct rtvk_timepoint){ NULL, 0 };
		vkResetCommandPool(ctx->vk_device, queue->copy_command_pool, 0);
		return true;
	}

	VkCommandPoolCreateInfo pool_info = { VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO };
	pool_info.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT | VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
	pool_info.queueFamilyIndex = queue->family_index;

	VkResult result = vkCreateCommandPool(ctx->vk_device, &pool_info, VK_ALLOCATOR, &queue->copy_command_pool);
	if (result != VK_SUCCESS) {
		rtvk_throwf(rtvk_error_from_vk(result), NULL);
		return false;
	}

	VkCommandBufferAllocateInfo alloc_info = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO };
	alloc_info.commandPool = queue->copy_command_pool;
	alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	alloc_info.commandBufferCount = 1;
	result = vkAllocateCommandBuffers(ctx->vk_device, &alloc_info, &queue->copy_command_buffer);
	if (result != VK_SUCCESS) {
		vkDestroyCommandPool(ctx->vk_device, queue->copy_command_pool, VK_ALLOCATOR);
		queue->copy_command_pool = VK_NULL_HANDLE;
		rtvk_throwf(rtvk_error_from_vk(result), NULL);
		return false;
	}
	return true;
}

struct rtvk_timepoint rtvk_texture_view_copy_to_buffer(struct rtvk_context* ctx, struct rtvk_queue* queue, struct rtvk_texture_view* texture_view, struct rtvk_buffer* buffer) {
	queue = rtvk_texture_graphics_queue(ctx, queue);
	struct rtvk_timepoint timepoint = { queue, 0 };
	if (!queue) {
		rtvk_throwf(RT_IMPROPER_USAGE, "texture view copy requires a graphics queue");
		return timepoint;
	}
	struct rtvk_texture* texture = texture_view ? texture_view->texture : NULL;
	if (!texture_view || !texture || !texture->vk_image || !texture_view->vk_image_view) {
		rtvk_throwf(RT_IMPROPER_USAGE, "texture view copy source is invalid");
		return timepoint;
	}
	if (!buffer) {
		rtvk_throwf(RT_IMPROPER_USAGE, "texture view copy requires a valid destination buffer");
		return timepoint;
	}
	if (!(buffer->usage & RT_BUFFER_USAGE_TRANSFER_DST) && !(buffer->usage & RT_BUFFER_USAGE_STAGING)) {
		rtvk_throwf(RT_IMPROPER_USAGE, "destination buffer usage must allow transfer writes");
		return timepoint;
	}
	if (buffer->mode != RT_BUFFER_DYNAMIC && !(buffer->usage & RT_BUFFER_USAGE_STAGING)) {
		rtvk_throwf(RT_IMPROPER_USAGE, "texture view copy requires a host-visible destination buffer");
		return timepoint;
	}

	u32 bytes_per_pixel = rtvk_texture_view_bytes_per_pixel(texture_view->vk_format);
	if (bytes_per_pixel == 0) {
		rtvk_throwf(RT_UNSUPPORTED_FEATURE, "texture view copy does not support this Vulkan format");
		return timepoint;
	}

	u64 total_size = (u64)texture_view->width * texture_view->height * bytes_per_pixel;
	if (!buffer->active || buffer->active->size < total_size) {
		rtvk_buffer_data(ctx, buffer, buffer->mode, buffer->usage, total_size, NULL);
	}
	if (!buffer->active) {
		return timepoint;
	}

	rtvk_queue_flush(ctx, queue);
	if (!rtvk_texture_copy_buffer_command(ctx, queue)) {
		return timepoint;
	}
	VkCommandBuffer command_buffer = queue->copy_command_buffer;

	VkCommandBufferBeginInfo begin_info = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
	begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
	VkResult result = vkBeginCommandBuffer(command_buffer, &begin_info);
	if (result != VK_SUCCESS) {
		rtvk_throwf(rtvk_error_from_vk(result), NULL);
		return timepoint;
	}

	VkImageLayout original_layout = texture->vk_layout;
	VkImageMemoryBarrier barrier = { VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER };
	barrier.srcAccessMask = rtvk_texture_layout_access(original_layout);
	barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
	barrier.oldLayout = original_layout;
	barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
	barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.image = texture->vk_image;
	barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	barrier.subresourceRange.baseMipLevel = 0;
	barrier.subresourceRange.levelCount = 1;
	barrier.subresourceRange.baseArrayLayer = 0;
	barrier.subresourceRange.layerCount = 1;
	vkCmdPipelineBarrier(command_buffer, rtvk_texture_layout_stage(original_layout),
						 VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, NULL, 0, NULL, 1, &barrier);

	VkBufferImageCopy copy = { 0 };
	copy.bufferOffset = 0;
	copy.bufferRowLength = 0;
	copy.bufferImageHeight = 0;
	copy.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	copy.imageSubresource.mipLevel = 0;
	copy.imageSubresource.baseArrayLayer = 0;
	copy.imageSubresource.layerCount = 1;
	copy.imageExtent.width = texture_view->width;
	copy.imageExtent.height = texture_view->height;
	copy.imageExtent.depth = 1;
	vkCmdCopyImageToBuffer(
		command_buffer,
		texture->vk_image,
		VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
		buffer->active->vk_buffer,
		1,
		&copy);

	barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
	barrier.dstAccessMask = rtvk_texture_layout_access(original_layout);
	barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
	barrier.newLayout = original_layout;
	VkPipelineStageFlags restore_stage = rtvk_texture_layout_stage(original_layout);
	if (restore_stage == VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT) {
		restore_stage = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
	}
	vkCmdPipelineBarrier(command_buffer, VK_PIPELINE_STAGE_TRANSFER_BIT, restore_stage, 0, 0, NULL,
						 0, NULL, 1, &barrier);

	result = vkEndCommandBuffer(command_buffer);
	if (result == VK_SUCCESS) {
		u64 value = queue->timeline_value + 1;
		VkTimelineSemaphoreSubmitInfo timeline_info = { VK_STRUCTURE_TYPE_TIMELINE_SEMAPHORE_SUBMIT_INFO };
		timeline_info.signalSemaphoreValueCount = 1;
		timeline_info.pSignalSemaphoreValues = &value;

		VkSubmitInfo submit_info = { VK_STRUCTURE_TYPE_SUBMIT_INFO };
		submit_info.pNext = &timeline_info;
		submit_info.commandBufferCount = 1;
		submit_info.pCommandBuffers = &command_buffer;
		submit_info.signalSemaphoreCount = 1;
		submit_info.pSignalSemaphores = &queue->vk_timeline;
		result = vkQueueSubmit(queue->vk_queue, 1, &submit_info, VK_NULL_HANDLE);
		if (result == VK_SUCCESS) {
			queue->timeline_value = value;
			queue->submitted_value = value;
			timepoint = (struct rtvk_timepoint){ queue, value };
			queue->copy_command_timepoint = timepoint;
		}
	}

	rtvk_queue_collect(ctx, queue);
	if (result != VK_SUCCESS) {
		rtvk_throwf(rtvk_error_from_vk(result), NULL);
		return timepoint;
	}

	return timepoint;
}
