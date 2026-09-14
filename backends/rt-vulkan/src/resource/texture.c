#include "texture.h"
#include "context.h"
#include "error.h"
#include "resource/buffer.h"
#include "resource/queue.h"
#include "resource/swapchain.h"
#include <assert.h>

#include <stdlib.h>
#include <string.h>

VkFormat rtvk_format_to_vk(enum rt_format format);

static u32 rtvk_texture_view_bytes_per_pixel(VkFormat format);
static VkAccessFlags rtvk_texture_layout_access(VkImageLayout layout);
static VkPipelineStageFlags rtvk_texture_layout_stage(VkImageLayout layout);

/*===============================================================================================*/
/*                                                                                               */
/*===============================================================================================*/

rt_texture rtTextureCreate(void) {
	rtvk_begin_errorable_operation();
	struct rtvk_texture* texture = rtvk_texture_create(rtvk_get_current_context());
	return rtvk_texture_to_handle(texture);
}

void rtTextureDestroy(rt_texture texture) {
	rtvk_texture_destroy(
		rtvk_get_current_context(),
		rtvk_texture_from_handle(texture)
	);
}

void rtTextureResize(rt_texture texture, enum rt_texture_type type, enum rt_format format, rt_extent_3d extent, usize mip_count) {
	rtvk_begin_errorable_operation();
	rtvk_texture_resize(
		rtvk_get_current_context(),
		rtvk_texture_from_handle(texture),
		type,
		format,
		extent,
		mip_count
	);
}

rt_texture_view rtTextureViewCreate(void) {
	rtvk_begin_errorable_operation();
	return rtvk_texture_view_to_handle(rtvk_texture_view_create(rtvk_get_current_context()));
}

void rtTextureViewSetTexture(rt_texture_view texture_view, rt_texture texture) {
	rtvk_begin_errorable_operation();
	rtvk_texture_view_bind(
		rtvk_get_current_context(),
		rtvk_texture_view_from_handle(texture_view),
		rtvk_texture_from_handle(texture)
	);
}

void rtTextureViewDestroy(rt_texture_view texture_view) {
	rtvk_texture_view_destroy(
		rtvk_get_current_context(),
		rtvk_texture_view_from_handle(texture_view)
	);
}

void rtTextureViewSetFilter(rt_texture_view texture_view, enum rt_filter mag_filter, enum rt_filter min_filter, enum rt_mip_filter mip_filter) {
	rtvk_begin_errorable_operation();
	rtvk_texture_view_filter(
		rtvk_texture_view_from_handle(texture_view),
		mag_filter,
		min_filter,
		mip_filter
	);
}

void rtTextureViewSetAddress(rt_texture_view texture_view, enum rt_address_mode address_u, enum rt_address_mode address_v, enum rt_address_mode address_w) {
	rtvk_begin_errorable_operation();
	rtvk_texture_view_address(
		rtvk_texture_view_from_handle(texture_view),
		address_u,
		address_v,
		address_w
	);
}

void rtTextureViewSetAnisotropy(rt_texture_view texture_view, usize max_anisotropy) {
	rtvk_begin_errorable_operation();
	rtvk_texture_view_anisotropy(
		rtvk_texture_view_from_handle(texture_view),
		max_anisotropy
	);
}

void rtTextureViewSetLod(rt_texture_view texture_view, f32 min_lod, f32 max_lod, f32 lod_bias) {
	rtvk_begin_errorable_operation();
	rtvk_texture_view_lod(
		rtvk_texture_view_from_handle(texture_view),
		min_lod,
		max_lod,
		lod_bias
	);
}

/*===============================================================================================*/
/*                                                                                               */
/*===============================================================================================*/

static VkSampler rtvk_texture_view_sampler_create(struct rtvk_context* ctx, struct rtvk_texture_view* view) {
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

	VkSampler sampler = VK_NULL_HANDLE;
	VkResult result = vkCreateSampler(ctx->vk_device, &sampler_info, VK_ALLOCATOR, &sampler);
	if (result != VK_SUCCESS) {
		rtvk_throwf(rtvk_error_from_vk(result), "Vulkan call returned %s", rtvk_vk_result_name(result));
		return VK_NULL_HANDLE;
	}
	return sampler;
}

rt_extent_3d rtTextureViewExtent(rt_texture_view texture_view) {
	rt_extent_3d extent = { 0, 0, 0 };
	struct rtvk_texture_view* view = rtvk_texture_view_from_handle(texture_view);
	if (!view || !view->vk_image_view || !view->image) {
		return extent;
	}
	extent.width = view->image->width;
	extent.height = view->image->height;
	extent.depth = view->image->depth ? view->image->depth : 1;
	return extent;
}

void rtTextureViewRead(rt_texture_view texture_view, rt_texture_range range, u08* data, usize data_size) {
	rtvk_begin_errorable_operation();
	struct rtvk_context* ctx = rtvk_get_current_context();
	struct rtvk_texture_view* view = rtvk_texture_view_from_handle(texture_view);
	if (!ctx || !view || !view->image || !data || !range.mip_count || !range.layer_count || !range.extent.width || !range.extent.height || !range.extent.depth) {
		return;
	}

	u32 bytes_per_pixel = rtvk_texture_view_bytes_per_pixel(view->image->vk_format);
	if (!bytes_per_pixel) {
		return;
	}

	usize copy_count = range.mip_count * range.layer_count;
	VkBufferImageCopy* copies = calloc(copy_count, sizeof(*copies));
	usize* data_offsets = calloc(copy_count, sizeof(*data_offsets));
	if (!copies || !data_offsets) {
		free(copies);
		free(data_offsets);
		rtvk_throwf(RT_OUT_OF_HOST_MEMORY, "failed to allocate %zu bytes for texture read regions", copy_count * (sizeof(*copies) + sizeof(*data_offsets)));
		return;
	}

	VkImageAspectFlags aspect = 0;
	if (range.aspects & RT_TEXTURE_ASPECT_COLOR) {
		aspect |= VK_IMAGE_ASPECT_COLOR_BIT;
	}
	if (range.aspects & RT_TEXTURE_ASPECT_DEPTH) {
		aspect |= VK_IMAGE_ASPECT_DEPTH_BIT;
	}
	if (range.aspects & RT_TEXTURE_ASPECT_STENCIL) {
		aspect |= VK_IMAGE_ASPECT_STENCIL_BIT;
	}
	if (!aspect) {
		aspect = rtvk_texture_format_aspect(view->image->vk_format);
	}

	usize packed_size = 0;
	VkDeviceSize staging_size = 0;
	for (usize mip = 0; mip < range.mip_count; mip++) {
		for (usize layer = 0; layer < range.layer_count; layer++) {
			usize index = mip * range.layer_count + layer;
			usize region_size = range.extent.width * range.extent.height * range.extent.depth * bytes_per_pixel;
			data_offsets[index] = packed_size;
			copies[index].bufferOffset = staging_size;
			copies[index].imageSubresource.aspectMask = aspect;
			copies[index].imageSubresource.mipLevel = (u32)(range.base_mip + mip);
			copies[index].imageSubresource.baseArrayLayer = (u32)(range.base_layer + layer);
			copies[index].imageSubresource.layerCount = 1;
			copies[index].imageOffset.x = (i32)range.offset.width;
			copies[index].imageOffset.y = (i32)range.offset.height;
			copies[index].imageOffset.z = (i32)range.offset.depth;
			copies[index].imageExtent.width = (u32)range.extent.width;
			copies[index].imageExtent.height = (u32)range.extent.height;
			copies[index].imageExtent.depth = (u32)range.extent.depth;
			packed_size += region_size;
			staging_size = (VkDeviceSize)((staging_size + region_size + 3) & ~UINT64_C(3));
		}
	}
	if (data_size < packed_size) {
		free(copies);
		free(data_offsets);
		return;
	}

	struct rtvk_queue* queue = rtvk_queue_query(ctx, RT_QUEUE_TRANSFER);
	if (!queue) {
		queue = rtvk_queue_query(ctx, RT_QUEUE_GRAPHICS);
	}
	if (!queue) {
		free(copies);
		free(data_offsets);
		return;
	}

	VkBuffer staging_buffer = VK_NULL_HANDLE;
	VmaAllocation staging_allocation = NULL;
	VkCommandPool command_pool = VK_NULL_HANDLE;
	VkCommandBuffer command_buffer = VK_NULL_HANDLE;
	VkFence fence = VK_NULL_HANDLE;
	VkResult result = VK_SUCCESS;

	rtvk_queue_lock_pair(queue, queue);
	rtvk_queue_flush_locked(ctx, queue);
	if (rtvk_error() != RT_SUCCESS) {
		goto finish;
	}

	VkBufferCreateInfo buffer_info = { VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
	buffer_info.size = staging_size;
	buffer_info.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT;
	buffer_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	VmaAllocationCreateInfo allocation_info = { 0 };
	allocation_info.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT;
	allocation_info.usage = VMA_MEMORY_USAGE_AUTO_PREFER_HOST;
	result = vmaCreateBuffer(ctx->vma_allocator, &buffer_info, &allocation_info, &staging_buffer, &staging_allocation, NULL);
	if (result != VK_SUCCESS) {
		rtvk_throwf(rtvk_error_from_vk(result), "Vulkan call returned %s", rtvk_vk_result_name(result));
		goto finish;
	}

	VkCommandPoolCreateInfo pool_info = { VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO };
	pool_info.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;
	pool_info.queueFamilyIndex = queue->family_index;
	result = vkCreateCommandPool(ctx->vk_device, &pool_info, VK_ALLOCATOR, &command_pool);
	if (result != VK_SUCCESS) {
		rtvk_throwf(rtvk_error_from_vk(result), "Vulkan call returned %s", rtvk_vk_result_name(result));
		goto finish;
	}

	VkCommandBufferAllocateInfo command_info = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO };
	command_info.commandPool = command_pool;
	command_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	command_info.commandBufferCount = 1;
	result = vkAllocateCommandBuffers(ctx->vk_device, &command_info, &command_buffer);
	if (result != VK_SUCCESS) {
		rtvk_throwf(rtvk_error_from_vk(result), "Vulkan call returned %s", rtvk_vk_result_name(result));
		goto finish;
	}

	VkCommandBufferBeginInfo begin_info = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
	begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
	result = vkBeginCommandBuffer(command_buffer, &begin_info);
	if (result != VK_SUCCESS) {
		rtvk_throwf(rtvk_error_from_vk(result), "Vulkan call returned %s", rtvk_vk_result_name(result));
		goto finish;
	}

	VkImageLayout original_layout = view->image->vk_layout;
	VkImageMemoryBarrier barrier = { VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER };
	barrier.srcAccessMask = rtvk_texture_layout_access(original_layout);
	barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
	barrier.oldLayout = original_layout;
	barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
	barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.image = view->image->vk_image;
	barrier.subresourceRange.aspectMask = aspect;
	barrier.subresourceRange.baseMipLevel = (u32)range.base_mip;
	barrier.subresourceRange.levelCount = (u32)range.mip_count;
	barrier.subresourceRange.baseArrayLayer = (u32)range.base_layer;
	barrier.subresourceRange.layerCount = (u32)range.layer_count;
	vkCmdPipelineBarrier(command_buffer, rtvk_texture_layout_stage(original_layout), VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, NULL, 0, NULL, 1, &barrier);
	vkCmdCopyImageToBuffer(command_buffer, view->image->vk_image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, staging_buffer, (u32)copy_count, copies);
	barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
	barrier.dstAccessMask = rtvk_texture_layout_access(original_layout);
	barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
	barrier.newLayout = original_layout;
	vkCmdPipelineBarrier(command_buffer, VK_PIPELINE_STAGE_TRANSFER_BIT, rtvk_texture_layout_stage(original_layout), 0, 0, NULL, 0, NULL, 1, &barrier);

	result = vkEndCommandBuffer(command_buffer);
	if (result != VK_SUCCESS) {
		rtvk_throwf(rtvk_error_from_vk(result), "Vulkan call returned %s", rtvk_vk_result_name(result));
		goto finish;
	}

	VkFenceCreateInfo fence_info = { VK_STRUCTURE_TYPE_FENCE_CREATE_INFO };
	result = vkCreateFence(ctx->vk_device, &fence_info, VK_ALLOCATOR, &fence);
	if (result != VK_SUCCESS) {
		rtvk_throwf(rtvk_error_from_vk(result), "Vulkan call returned %s", rtvk_vk_result_name(result));
		goto finish;
	}
	VkSubmitInfo submit_info = { VK_STRUCTURE_TYPE_SUBMIT_INFO };
	submit_info.commandBufferCount = 1;
	submit_info.pCommandBuffers = &command_buffer;
	if (queue->pending_head) {
		rtvk_queue_flush_locked(ctx, queue);
		if (rtvk_error() != RT_SUCCESS) {
			goto finish;
		}
	}
	result = vkQueueSubmit(queue->vk_queue, 1, &submit_info, fence);
	if (result == VK_SUCCESS) {
		result = vkWaitForFences(ctx->vk_device, 1, &fence, VK_TRUE, UINT64_MAX);
	}
	if (result != VK_SUCCESS) {
		rtvk_throwf(rtvk_error_from_vk(result), "Vulkan call returned %s", rtvk_vk_result_name(result));
		goto finish;
	}

	VmaAllocationInfo staging_info;
	vmaGetAllocationInfo(ctx->vma_allocator, staging_allocation, &staging_info);
	vmaInvalidateAllocation(ctx->vma_allocator, staging_allocation, 0, staging_size);
	for (usize index = 0; index < copy_count; index++) {
		usize region_size = range.extent.width * range.extent.height * range.extent.depth * bytes_per_pixel;
		memcpy(data + data_offsets[index], (u08*)staging_info.pMappedData + copies[index].bufferOffset, region_size);
	}

finish:
	if (fence) {
		vkDestroyFence(ctx->vk_device, fence, VK_ALLOCATOR);
	}
	if (command_pool) {
		vkDestroyCommandPool(ctx->vk_device, command_pool, VK_ALLOCATOR);
	}
	if (staging_buffer) {
		vmaDestroyBuffer(ctx->vma_allocator, staging_buffer, staging_allocation);
	}
	rtvk_queue_unlock_pair(queue, queue);
	free(copies);
	free(data_offsets);
}

u32 rtvk_view_width(const struct rtvk_texture_view* view) {
	if (!view || !view->image) {
		return 0;
	}
	return view->image->width;
}
u32 rtvk_view_height(const struct rtvk_texture_view* view) {
	if (!view || !view->image) {
		return 0;
	}
	return view->image->height;
}
VkFormat rtvk_view_format(const struct rtvk_texture_view* view) {
	if (!view || !view->image) {
		return VK_FORMAT_UNDEFINED;
	}
	return view->image->vk_format;
}
VkImageLayout rtvk_view_layout(const struct rtvk_texture_view* view) {
	if (!view || !view->image) {
		return VK_IMAGE_LAYOUT_UNDEFINED;
	}
	return view->image->vk_layout;
}

/*===============================================================================================*/
/*                                                                                               */
/*===============================================================================================*/

RTVK_DEFINE_RESOURCE_PRIVATE(texture)
RTVK_DEFINE_RESOURCE_PRIVATE(texture_view)

static VkFormat rtvk_texture_format(enum rt_format format) {
	return rtvk_format_to_vk(format);
}

static u32 rtvk_texture_format_bytes_per_pixel(enum rt_format format) {
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
	case RT_D32_SFLOAT_S8_UINT:
		return 8;
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

static bool rtvk_format_has_depth(VkFormat format) {
	return format == VK_FORMAT_D16_UNORM || format == VK_FORMAT_D32_SFLOAT || format == VK_FORMAT_D24_UNORM_S8_UINT || format == VK_FORMAT_D32_SFLOAT_S8_UINT;
}

static bool rtvk_format_has_stencil(VkFormat format) {
	return format == VK_FORMAT_S8_UINT || format == VK_FORMAT_D24_UNORM_S8_UINT || format == VK_FORMAT_D32_SFLOAT_S8_UINT;
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
	case VK_FORMAT_R8_UNORM:
	case VK_FORMAT_R8_SINT:
	case VK_FORMAT_R8_UINT:
	case VK_FORMAT_S8_UINT:
		return 1;
	case VK_FORMAT_R8G8_UNORM:
	case VK_FORMAT_R8G8_SINT:
	case VK_FORMAT_R8G8_UINT:
	case VK_FORMAT_R16_UNORM:
	case VK_FORMAT_R16_SFLOAT:
	case VK_FORMAT_R16_SINT:
	case VK_FORMAT_R16_UINT:
	case VK_FORMAT_D16_UNORM:
		return 2;
	case VK_FORMAT_R8G8B8_UNORM:
	case VK_FORMAT_R8G8B8_SINT:
	case VK_FORMAT_R8G8B8_UINT:
		return 3;
	case VK_FORMAT_R8G8B8A8_UNORM:
	case VK_FORMAT_R8G8B8A8_SINT:
	case VK_FORMAT_R8G8B8A8_UINT:
	case VK_FORMAT_R8G8B8A8_SRGB:
	case VK_FORMAT_B8G8R8A8_UNORM:
	case VK_FORMAT_B8G8R8A8_SRGB:
	case VK_FORMAT_R16G16_UNORM:
	case VK_FORMAT_R16G16_SFLOAT:
	case VK_FORMAT_R16G16_SINT:
	case VK_FORMAT_R16G16_UINT:
	case VK_FORMAT_R32_SFLOAT:
	case VK_FORMAT_R32_SINT:
	case VK_FORMAT_R32_UINT:
	case VK_FORMAT_D32_SFLOAT:
	case VK_FORMAT_D24_UNORM_S8_UINT:
		return 4;
	case VK_FORMAT_R16G16B16_UNORM:
	case VK_FORMAT_R16G16B16_SFLOAT:
	case VK_FORMAT_R16G16B16_SINT:
	case VK_FORMAT_R16G16B16_UINT:
		return 6;
	case VK_FORMAT_R16G16B16A16_UNORM:
	case VK_FORMAT_R16G16B16A16_SFLOAT:
	case VK_FORMAT_R16G16B16A16_SINT:
	case VK_FORMAT_R16G16B16A16_UINT:
	case VK_FORMAT_R32G32_SFLOAT:
	case VK_FORMAT_R32G32_SINT:
	case VK_FORMAT_R32G32_UINT:
	case VK_FORMAT_D32_SFLOAT_S8_UINT:
		return 8;
	case VK_FORMAT_R32G32B32_SFLOAT:
	case VK_FORMAT_R32G32B32_SINT:
	case VK_FORMAT_R32G32B32_UINT:
		return 12;
	case VK_FORMAT_R32G32B32A32_SFLOAT:
	case VK_FORMAT_R32G32B32A32_SINT:
	case VK_FORMAT_R32G32B32A32_UINT:
		return 16;
	default:
		return 0;
	}
}

static void rtvk_texture_copy_region(struct rtvk_context* ctx, struct rtvk_queue* queue, struct rtvk_texture* src_texture, u32 src_x, u32 src_y, u32 src_z, u32 src_mip, struct rtvk_texture* dst_texture, u32 dst_x, u32 dst_y, u32 dst_z, u32 dst_mip, u32 width, u32 height, u32 depth);
static void rtvk_texture_copy_buffer_command(struct rtvk_context* ctx, struct rtvk_queue* queue);
struct rtvk_texture* rtvk_texture_node_create(struct rtvk_context* ctx);

static bool rtvk_texture_view_needs_bgra_swizzle(VkFormat format) {
	return format == VK_FORMAT_B8G8R8A8_UNORM || format == VK_FORMAT_B8G8R8A8_SRGB;
}

static u32 rtvk_texture_mip_level_count(u32 width, u32 height, u32 depth) {
	u32 max_extent = width > height ? width : height;
	max_extent = max_extent > depth ? max_extent : depth;
	max_extent = max_extent > 1u ? max_extent : 1u;
	u32 mip_levels = 1;
	while (max_extent > 1) {
		max_extent /= 2;
		++mip_levels;
	}
	return mip_levels;
}

static VkAccessFlags rtvk_texture_layout_access(VkImageLayout layout) {
	switch (layout) {
	case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL: /**********/
		return VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
	case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL: /**/
		return VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
	case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL: /**************/
		return VK_ACCESS_TRANSFER_READ_BIT;
	case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL: /**************/
		return VK_ACCESS_TRANSFER_WRITE_BIT;
	case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL: /**********/
		return VK_ACCESS_SHADER_READ_BIT;
	default:
		return 0;
	}
}

static VkPipelineStageFlags rtvk_texture_layout_stage(VkImageLayout layout) {
	switch (layout) {
	case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL: /**********/
		return VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL: /**/
		return VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
	case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL: /**************/
		return VK_PIPELINE_STAGE_TRANSFER_BIT;
	case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL: /**************/
		return VK_PIPELINE_STAGE_TRANSFER_BIT;
	case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL: /**********/
		return VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
	case VK_IMAGE_LAYOUT_PRESENT_SRC_KHR: /********************/
		return VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
	default: /************************************************/
		return VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
	}
}

void rtvk_image_transition_layout(VkCommandBuffer command_buffer, struct rtvk_image_base* image, VkImageLayout layout) {
	if (!image || !image->vk_image || image->vk_layout == layout) {
		return;
	}

	VkImageMemoryBarrier barrier = { VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER };
	barrier.srcAccessMask = rtvk_texture_layout_access(image->vk_layout);
	barrier.dstAccessMask = rtvk_texture_layout_access(layout);
	barrier.oldLayout = image->vk_layout;
	barrier.newLayout = layout;
	barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.image = image->vk_image;
	barrier.subresourceRange.aspectMask = rtvk_texture_format_aspect(image->vk_format);
	barrier.subresourceRange.baseMipLevel = 0;
	barrier.subresourceRange.levelCount = image->mip_levels;
	barrier.subresourceRange.baseArrayLayer = 0;
	barrier.subresourceRange.layerCount = VK_REMAINING_ARRAY_LAYERS;
	vkCmdPipelineBarrier(
		command_buffer,
		rtvk_texture_layout_stage(image->vk_layout),
		rtvk_texture_layout_stage(layout),
		0,
		0,
		NULL,
		0,
		NULL,
		1,
		&barrier
	);
	image->vk_layout = layout;
}

static struct rtvk_queue* rtvk_texture_graphics_queue(struct rtvk_context* ctx, struct rtvk_queue* queue) {
	if (queue && queue->capability == RT_QUEUE_GRAPHICS) {
		return queue;
	}
	return rtvk_queue_query(ctx, RT_QUEUE_GRAPHICS);
}

void rtvk_texture_init(struct rtvk_context* ctx, struct rtvk_texture* texture) {
	rtvk_init_resource_base(ctx, RTVK_RESOURCE_BASE(texture), texture, rtvk_texture_finalize_resource);
}

struct rtvk_texture* rtvk_texture_active_node(struct rtvk_texture* texture) {
	if (!texture) {
		return NULL;
	}
	return texture->active;
}

struct rtvk_texture* rtvk_texture_node_clone(struct rtvk_context* ctx, const struct rtvk_texture* source) {
	if (!source) {
		return NULL;
	}

	VkImageCreateInfo image_info = { VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO };
	image_info.format = source->base.vk_format;
	image_info.extent.width = source->base.width;
	image_info.extent.height = source->base.height;
	image_info.extent.depth = source->base.depth;
	image_info.mipLevels = source->base.mip_levels;
	image_info.arrayLayers = 1;
	image_info.samples = VK_SAMPLE_COUNT_1_BIT;
	image_info.tiling = VK_IMAGE_TILING_OPTIMAL;
	image_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	image_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

	switch (source->base.type) {
	case RT_TEXTURE_1D:
		image_info.imageType = VK_IMAGE_TYPE_1D;
		image_info.extent.height = 1;
		image_info.extent.depth = 1;
		break;
	case RT_TEXTURE_2D:
		image_info.imageType = VK_IMAGE_TYPE_2D;
		image_info.extent.depth = 1;
		break;
	case RT_TEXTURE_3D:
		image_info.imageType = VK_IMAGE_TYPE_3D;
		break;
	case RT_TEXTURE_1D_ARRAY:
		image_info.imageType = VK_IMAGE_TYPE_1D;
		image_info.extent.height = 1;
		image_info.extent.depth = 1;
		image_info.arrayLayers = source->base.depth;
		break;
	case RT_TEXTURE_2D_ARRAY:
		image_info.imageType = VK_IMAGE_TYPE_2D;
		image_info.extent.depth = 1;
		image_info.arrayLayers = source->base.depth;
		break;
	default:
		return NULL;
	}

	VkFormatProperties properties;
	vkGetPhysicalDeviceFormatProperties(ctx->vk_physical_device, image_info.format, &properties);
	if (properties.optimalTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT) {
		image_info.usage |= VK_IMAGE_USAGE_SAMPLED_BIT;
	}
	if (properties.optimalTilingFeatures & VK_FORMAT_FEATURE_STORAGE_IMAGE_BIT) {
		image_info.usage |= VK_IMAGE_USAGE_STORAGE_BIT;
	}
	if (properties.optimalTilingFeatures & VK_FORMAT_FEATURE_TRANSFER_SRC_BIT) {
		image_info.usage |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
	}
	if (properties.optimalTilingFeatures & VK_FORMAT_FEATURE_TRANSFER_DST_BIT) {
		image_info.usage |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
	}
	if (properties.optimalTilingFeatures & VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT) {
		image_info.usage |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
	}
	if (properties.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT) {
		image_info.usage |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
	}
	if (!image_info.usage) {
		rtvk_throwf(RT_UNSUPPORTED_FEATURE, "texture format has no Vulkan image usage");
		return NULL;
	}

	struct rtvk_texture* target = rtvk_texture_node_create(ctx);
	if (!target) {
		return NULL;
	}

	VmaAllocationCreateInfo allocation_info = { 0 };
	allocation_info.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;
	VkResult result = vmaCreateImage(ctx->vma_allocator, &image_info, &allocation_info, &target->base.vk_image, &target->vma_allocation, NULL);
	if (result != VK_SUCCESS) {
		rtvk_resource_retire(RTVK_RESOURCE_BASE(target));
		rtvk_throwf(rtvk_error_from_vk(result), "Vulkan call returned %s", rtvk_vk_result_name(result));
		return NULL;
	}

	target->base.vk_format = source->base.vk_format;
	target->base.vk_layout = VK_IMAGE_LAYOUT_UNDEFINED;
	target->base.type = source->base.type;
	target->base.width = image_info.extent.width;
	target->base.height = image_info.extent.height;
	target->base.depth = source->base.type == RT_TEXTURE_3D ? image_info.extent.depth : image_info.arrayLayers;
	target->base.mip_levels = image_info.mipLevels;
	return target;
}

struct rtvk_texture_write rtvk_texture_write_begin(struct rtvk_context* ctx, struct rtvk_texture* texture) {
	struct rtvk_texture_write write = { 0 };
	if (!texture) {
		return write;
	}

	write.target = texture->active;
	if (!write.target || (rtvk_atomic_load(&write.target->base.base.ref_count) == 1 && rtvk_atomic_load(&write.target->base.base.job_count) == 0)) {
		return write;
	}

	write.source = write.target;
	write.target = rtvk_texture_node_clone(ctx, write.source);
	if (!write.target) {
		write.source = NULL;
	}
	return write;
}

void rtvk_texture_write_commit(struct rtvk_texture* texture, struct rtvk_texture_write* write) {
	if (!texture || !write || !write->source || !write->target) {
		return;
	}

	rtvk_texture_recycle_node(texture, write->source);
	texture->active = write->target;
	write->source = NULL;
	write->target = NULL;
}

void rtvk_texture_write_cancel(struct rtvk_texture* texture, struct rtvk_texture_write* write) {
	if (!texture || !write || !write->source || !write->target) {
		return;
	}

	rtvk_texture_recycle_node(texture, write->target);
	write->source = NULL;
	write->target = NULL;
}

void rtvk_texture_resize(struct rtvk_context* ctx, struct rtvk_texture* texture, enum rt_texture_type type, enum rt_format format, rt_extent_3d extent, usize mip_count) {
	if (!texture) {
		return;
	}
	rtvk_texture_collect_nodes(texture);

	VkFormat vk_format = rtvk_texture_format(format);
	if (vk_format == VK_FORMAT_UNDEFINED) {
		rtvk_throwf(RT_UNSUPPORTED_FEATURE, "unsupported texture format");
		return;
	}

	VkImageCreateInfo image_info = { VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO };
	image_info.format = vk_format;
	image_info.extent.width = (u32)extent.width;
	image_info.extent.height = (u32)extent.height;
	image_info.extent.depth = (u32)extent.depth;
	image_info.mipLevels = (u32)mip_count;
	image_info.arrayLayers = 1;
	image_info.samples = VK_SAMPLE_COUNT_1_BIT;
	image_info.tiling = VK_IMAGE_TILING_OPTIMAL;
	image_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	image_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

	switch (type) {
	case RT_TEXTURE_1D:
		image_info.imageType = VK_IMAGE_TYPE_1D;
		image_info.extent.height = 1;
		image_info.extent.depth = 1;
		break;
	case RT_TEXTURE_2D:
		image_info.imageType = VK_IMAGE_TYPE_2D;
		image_info.extent.depth = 1;
		break;
	case RT_TEXTURE_3D:
		image_info.imageType = VK_IMAGE_TYPE_3D;
		break;
	case RT_TEXTURE_1D_ARRAY:
		image_info.imageType = VK_IMAGE_TYPE_1D;
		image_info.extent.height = 1;
		image_info.extent.depth = 1;
		image_info.arrayLayers = (u32)extent.depth;
		break;
	case RT_TEXTURE_2D_ARRAY:
		image_info.imageType = VK_IMAGE_TYPE_2D;
		image_info.extent.depth = 1;
		image_info.arrayLayers = (u32)extent.depth;
		break;
	default:
		return;
	}

	VkFormatProperties properties;
	vkGetPhysicalDeviceFormatProperties(ctx->vk_physical_device, vk_format, &properties);
	if (properties.optimalTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT) {
		image_info.usage |= VK_IMAGE_USAGE_SAMPLED_BIT;
	}
	if (properties.optimalTilingFeatures & VK_FORMAT_FEATURE_STORAGE_IMAGE_BIT) {
		image_info.usage |= VK_IMAGE_USAGE_STORAGE_BIT;
	}
	if (properties.optimalTilingFeatures & VK_FORMAT_FEATURE_TRANSFER_SRC_BIT) {
		image_info.usage |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
	}
	if (properties.optimalTilingFeatures & VK_FORMAT_FEATURE_TRANSFER_DST_BIT) {
		image_info.usage |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
	}
	if (properties.optimalTilingFeatures & VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT) {
		image_info.usage |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
	}
	if (properties.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT) {
		image_info.usage |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
	}
	if (!image_info.usage) {
		rtvk_throwf(RT_UNSUPPORTED_FEATURE, "texture format has no Vulkan image usage");
		return;
	}

	struct rtvk_texture* node = rtvk_texture_node_create(ctx);
	if (!node) {
		return;
	}

	VmaAllocationCreateInfo allocation_info = { 0 };
	allocation_info.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;
	VkResult result = vmaCreateImage(ctx->vma_allocator, &image_info, &allocation_info, &node->base.vk_image, &node->vma_allocation, NULL);
	if (result != VK_SUCCESS) {
		rtvk_resource_retire(RTVK_RESOURCE_BASE(node));
		rtvk_throwf(rtvk_error_from_vk(result), "Vulkan call returned %s", rtvk_vk_result_name(result));
		return;
	}

	node->base.vk_format = vk_format;
	node->base.vk_layout = VK_IMAGE_LAYOUT_UNDEFINED;
	node->base.type = type;
	node->base.width = image_info.extent.width;
	node->base.height = image_info.extent.height;
	node->base.depth = type == RT_TEXTURE_3D ? image_info.extent.depth : image_info.arrayLayers;
	node->base.mip_levels = image_info.mipLevels;

	if (texture->active) {
		rtvk_texture_recycle_node(texture, texture->active);
	}
	texture->active = node;
	rtvk_texture_collect_nodes(texture);
}

void rtvk_texture_view_init(struct rtvk_context* ctx, struct rtvk_texture_view* view) {
	rtvk_init_resource_base(ctx, RTVK_RESOURCE_BASE(view), view, rtvk_texture_view_finalize_resource);
	view->mag_filter = RT_FILTER_LINEAR;
	view->min_filter = RT_FILTER_LINEAR;
	view->mip_filter = RT_MIP_FILTER_NONE;
	view->address_u = RT_ADDRESS_REPEAT;
	view->address_v = RT_ADDRESS_REPEAT;
	view->address_w = RT_ADDRESS_REPEAT;
	view->max_anisotropy = 1;
	view->vk_sampler = rtvk_texture_view_sampler_create(ctx, view);
}

void rtvk_texture_finish(struct rtvk_texture* texture) {
	struct rtvk_context* ctx = texture->base.base.ctx;
	if (texture->active) {
		rtvk_resource_retire(RTVK_RESOURCE_BASE(texture->active));
		texture->active = NULL;
	}
	struct rtvk_texture* node = texture->next;
	while (node) {
		struct rtvk_texture* next = node->next;
		node->next = NULL;
		rtvk_resource_retire(RTVK_RESOURCE_BASE(node));
		node = next;
	}
	texture->next = NULL;
	if (texture->vma_allocation) {
		vmaDestroyImage(ctx->vma_allocator, texture->base.vk_image, texture->vma_allocation);
	}
	texture->base.vk_image = VK_NULL_HANDLE;
	texture->vma_allocation = VK_NULL_HANDLE;
}

struct rtvk_texture* rtvk_texture_node_create(struct rtvk_context* ctx) {
	struct rtvk_texture* node = calloc(1, sizeof(*node));
	if (!node) {
		rtvk_throwf(RT_OUT_OF_HOST_MEMORY, "failed to allocate texture metadata");
		return NULL;
	}

	rtvk_init_resource_base(ctx, RTVK_RESOURCE_BASE(node), node, rtvk_texture_finalize_resource);
	node->base.vk_layout = VK_IMAGE_LAYOUT_UNDEFINED;
	node->base.vk_format = VK_FORMAT_UNDEFINED;
	node->base.mip_levels = 1;
	return node;
}

void rtvk_texture_recycle_node(struct rtvk_texture* texture, struct rtvk_texture* node) {
	assert(texture);
	if (!node) {
		return;
	}
	node->next = texture->next;
	texture->next = node;
}

void rtvk_texture_collect_nodes(struct rtvk_texture* texture) {
	assert(texture);
	struct rtvk_texture** link = &texture->next;
	while (*link) {
		struct rtvk_texture* node = *link;
		if (rtvk_atomic_load(&node->base.base.ref_count) == 1) {
			*link = node->next;
			node->next = NULL;
			rtvk_resource_retire(RTVK_RESOURCE_BASE(node));
			continue;
		}
		link = &node->next;
	}
}

void rtvk_texture_view_finish(struct rtvk_texture_view* view) {
	struct rtvk_context* ctx = view->base.ctx;
	vkDestroySampler(ctx->vk_device, view->vk_sampler, VK_ALLOCATOR);
	vkDestroyImageView(ctx->vk_device, view->vk_image_view, VK_ALLOCATOR);
	view->vk_sampler = VK_NULL_HANDLE;
	view->vk_image_view = VK_NULL_HANDLE;
	if (view->image) {
		rtvk_resource_release(RTVK_RESOURCE_BASE(view->image));
		view->image = NULL;
	}
	if (view->texture) {
		rtvk_resource_release(RTVK_RESOURCE_BASE(view->texture));
		view->texture = NULL;
	}
}

VkImageView rtvk_image_view_create(struct rtvk_context* ctx, const struct rtvk_image_base* image) {
	assert(image && image->vk_image);
	VkImageViewCreateInfo view_info = { VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO };
	u32 layer_count = 1;
	if ((image->type == RT_TEXTURE_1D_ARRAY || image->type == RT_TEXTURE_2D_ARRAY) && image->depth) {
		layer_count = image->depth;
	}
	view_info.pNext = NULL;
	view_info.flags = 0;
	view_info.image = image->vk_image;
	switch (image->type) {
	case RT_TEXTURE_1D:
		view_info.viewType = VK_IMAGE_VIEW_TYPE_1D;
		break;
	case RT_TEXTURE_2D:
		view_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
		break;
	case RT_TEXTURE_3D:
		view_info.viewType = VK_IMAGE_VIEW_TYPE_3D;
		break;
	case RT_TEXTURE_1D_ARRAY:
		view_info.viewType = VK_IMAGE_VIEW_TYPE_1D_ARRAY;
		break;
	case RT_TEXTURE_2D_ARRAY:
		view_info.viewType = VK_IMAGE_VIEW_TYPE_2D_ARRAY;
		break;
	default:
		view_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
		break;
	}
	view_info.format = image->vk_format;
	view_info.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
	view_info.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
	view_info.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
	view_info.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
	view_info.subresourceRange.aspectMask = rtvk_texture_format_aspect(image->vk_format);
	view_info.subresourceRange.baseMipLevel = 0;
	view_info.subresourceRange.levelCount = image->mip_levels ? image->mip_levels : 1;
	view_info.subresourceRange.baseArrayLayer = 0;
	view_info.subresourceRange.layerCount = layer_count;

	VkImageView image_view = VK_NULL_HANDLE;
	VkResult result = vkCreateImageView(ctx->vk_device, &view_info, VK_ALLOCATOR, &image_view);
	if (result != VK_SUCCESS) {
		rtvk_throwf(rtvk_error_from_vk(result), "Vulkan call returned %s", rtvk_vk_result_name(result));
		return VK_NULL_HANDLE;
	}
	return image_view;
}

void rtvk_texture_view_bind_image(struct rtvk_context* ctx, struct rtvk_texture_view* view, struct rtvk_image_base* image) {
	assert(view);
	assert(image);

	if (view->vk_image_view || view->image || view->texture) {
		rtvk_texture_view_finish(view);
		rtvk_init_resource_base(ctx, RTVK_RESOURCE_BASE(view), view, rtvk_texture_view_finalize_resource);
	}

	view->vk_image_view = rtvk_image_view_create(ctx, image);
	if (!view->vk_image_view) {
		return;
	}

	rtvk_resource_retain(RTVK_RESOURCE_BASE(image));
	view->image = image;
}

void rtvk_texture_view_bind(struct rtvk_context* ctx, struct rtvk_texture_view* view, struct rtvk_texture* texture) {
	assert(view);
	assert(texture);
	assert(texture->active);
	assert(texture->active->base.vk_image);
	rtvk_texture_view_bind_image(ctx, view, &texture->active->base);
	if (view->vk_image_view) {
		rtvk_resource_retain(RTVK_RESOURCE_BASE(texture));
		view->texture = texture;
	}
}

struct rtvk_texture_view* rtvk_texture_view_create_for_texture(struct rtvk_context* ctx, struct rtvk_texture* texture) {
	assert(texture);
	assert(texture->active);
	assert(texture->active->base.vk_image);
	struct rtvk_texture_view* view = rtvk_texture_view_create(ctx);
	if (!view) {
		return NULL;
	}
	rtvk_texture_view_bind(ctx, view, texture);
	if (rtvk_error() != RT_SUCCESS) {
		rtvk_texture_view_destroy(ctx, view);
		return NULL;
	}
	return view;
}

static void rtvk_texture_view_recreate_sampler(struct rtvk_texture_view* texture_view) {
	VkSampler vk_sampler = rtvk_texture_view_sampler_create(texture_view->base.ctx, texture_view);
	if (!vk_sampler) {
		return;
	}
	vkDestroySampler(texture_view->base.ctx->vk_device, texture_view->vk_sampler, VK_ALLOCATOR);
	texture_view->vk_sampler = vk_sampler;
}

void rtvk_texture_view_filter(struct rtvk_texture_view* texture_view, enum rt_filter mag_filter, enum rt_filter min_filter, enum rt_mip_filter mip_filter) {
	if (texture_view->mag_filter == mag_filter &&
		texture_view->min_filter == min_filter &&
		texture_view->mip_filter == mip_filter) {
		return;
	}
	texture_view->mag_filter = mag_filter;
	texture_view->min_filter = min_filter;
	texture_view->mip_filter = mip_filter;
	rtvk_texture_view_recreate_sampler(texture_view);
}

void rtvk_texture_view_address(struct rtvk_texture_view* texture_view, enum rt_address_mode address_u, enum rt_address_mode address_v, enum rt_address_mode address_w) {
	if (texture_view->address_u == address_u &&
		texture_view->address_v == address_v &&
		texture_view->address_w == address_w) {
		return;
	}
	texture_view->address_u = address_u;
	texture_view->address_v = address_v;
	texture_view->address_w = address_w;
	rtvk_texture_view_recreate_sampler(texture_view);
}

void rtvk_texture_view_anisotropy(struct rtvk_texture_view* texture_view, usize max_anisotropy) {
	if (texture_view->max_anisotropy == max_anisotropy) {
		return;
	}
	texture_view->max_anisotropy = max_anisotropy;
	rtvk_texture_view_recreate_sampler(texture_view);
}

void rtvk_texture_view_lod(struct rtvk_texture_view* texture_view, f32 min_lod, f32 max_lod, f32 lod_bias) {
	if (texture_view->min_lod == min_lod &&
		texture_view->max_lod == max_lod &&
		texture_view->lod_bias == lod_bias) {
		return;
	}
	texture_view->min_lod = min_lod;
	texture_view->max_lod = max_lod;
	texture_view->lod_bias = lod_bias;
	rtvk_texture_view_recreate_sampler(texture_view);
}

rt_timepoint rtvk_texture_copy(struct rtvk_context* ctx, struct rtvk_texture* src_texture, u32 src_mip, struct rtvk_texture* dst_texture, u32 dst_mip) {
	struct rtvk_queue* queue = rtvk_queue_query(ctx, RT_QUEUE_GRAPHICS);
	if (!queue) {
		queue = rtvk_queue_query(ctx, RT_QUEUE_TRANSFER);
	}
	rt_timepoint timepoint = { 0 };
	if (!queue) {
		rtvk_throwf(RT_IMPROPER_USAGE, "texture copy requires a graphics or transfer queue");
		return timepoint;
	}
	if (!src_texture || !dst_texture || !src_texture->active || !dst_texture->active ||
		!src_texture->active->base.vk_image || !dst_texture->active->base.vk_image) {
		rtvk_throwf(RT_IMPROPER_USAGE, "texture copy source or destination is NULL");
		return timepoint;
	}
	const u32 mip_width = src_texture->active->base.width >> src_mip;
	const u32 mip_height = src_texture->active->base.height >> src_mip;
	rtvk_texture_copy_region(ctx, queue, src_texture, 0, 0, 0, src_mip, dst_texture, 0, 0, 0, dst_mip, mip_width ? mip_width : 1, mip_height ? mip_height : 1, 1);
	if (rtvk_error() != RT_SUCCESS) {
		return timepoint;
	}
	timepoint = queue->copy_command_timepoint;
	return timepoint;
}

static void rtvk_texture_upload_command(struct rtvk_context* ctx, struct rtvk_queue* queue) {
	if (queue->upload_command_pool && queue->upload_command_buffer) {
		rtvk_queue_collect_to_value(ctx, queue, rtvk_queue_completed_value(ctx, queue));
		if (rtvk_timepoint_value(queue->upload_command_timepoint) > queue->completed_value) {
			rtvk_queue_retire_upload_resources(ctx, queue, true, false);
		}
	}
	if (queue->upload_command_pool && queue->upload_command_buffer) {
		queue->upload_command_timepoint = (rt_timepoint){ 0 };
		vkResetCommandPool(ctx->vk_device, queue->upload_command_pool, 0);
		return;
	}

	VkCommandPoolCreateInfo pool_info = { VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO };
	pool_info.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT | VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
	pool_info.queueFamilyIndex = queue->family_index;
	VkResult result = vkCreateCommandPool(ctx->vk_device, &pool_info, VK_ALLOCATOR, &queue->upload_command_pool);
	if (result != VK_SUCCESS) {
		rtvk_throwf(rtvk_error_from_vk(result), "Vulkan call returned %s", rtvk_vk_result_name(result));
		return;
	}

	VkCommandBufferAllocateInfo alloc_info = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO };
	alloc_info.commandPool = queue->upload_command_pool;
	alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	alloc_info.commandBufferCount = 1;
	result = vkAllocateCommandBuffers(ctx->vk_device, &alloc_info, &queue->upload_command_buffer);
	if (result != VK_SUCCESS) {
		vkDestroyCommandPool(ctx->vk_device, queue->upload_command_pool, VK_ALLOCATOR);
		queue->upload_command_pool = VK_NULL_HANDLE;
		rtvk_throwf(rtvk_error_from_vk(result), "Vulkan call returned %s", rtvk_vk_result_name(result));
		return;
	}
	return;
}

static void rtvk_texture_upload_staging(struct rtvk_context* ctx, struct rtvk_queue* queue, u64 size) {
	if (queue->upload_staging_buffer && queue->upload_staging_size >= size) {
		rtvk_queue_collect_to_value(ctx, queue, rtvk_queue_completed_value(ctx, queue));
		if (rtvk_timepoint_value(queue->upload_command_timepoint) > queue->completed_value) {
			rtvk_queue_retire_upload_resources(ctx, queue, false, true);
		}
	}
	if (queue->upload_staging_buffer && queue->upload_staging_size >= size) {
		return;
	}

	rtvk_queue_collect_to_value(ctx, queue, rtvk_queue_completed_value(ctx, queue));
	if (queue->upload_staging_buffer) {
		if (rtvk_timepoint_value(queue->upload_command_timepoint) > queue->completed_value) {
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
		NULL
	);
	if (result != VK_SUCCESS) {
		rtvk_throwf(rtvk_error_from_vk(result), "Vulkan call returned %s", rtvk_vk_result_name(result));
		return;
	}
	queue->upload_staging_size = size;
}

static void rtvk_texture_copy_region(struct rtvk_context* ctx, struct rtvk_queue* queue, struct rtvk_texture* src_texture, u32 src_x, u32 src_y, u32 src_z, u32 src_mip, struct rtvk_texture* dst_texture, u32 dst_x, u32 dst_y, u32 dst_z, u32 dst_mip, u32 width, u32 height, u32 depth) {
	queue = rtvk_texture_graphics_queue(ctx, queue);
	const u32 src_w = src_texture->active->base.width >> src_mip;
	const u32 src_mip_w = src_w ? src_w : 1;
	const u32 src_h = src_texture->active->base.height >> src_mip;
	const u32 src_mip_h = src_h ? src_h : 1;
	const u32 dst_w = dst_texture->active->base.width >> dst_mip;
	const u32 dst_mip_w = dst_w ? dst_w : 1;
	const u32 dst_h = dst_texture->active->base.height >> dst_mip;
	const u32 dst_mip_h = dst_h ? dst_h : 1;

	rtvk_mutex_lock(&queue->lock);
	rtvk_queue_flush_locked(ctx, queue);
	rtvk_texture_copy_buffer_command(ctx, queue);
	if (rtvk_error() != RT_SUCCESS) {
		rtvk_mutex_unlock(&queue->lock);
		return;
	}
	VkCommandBuffer command_buffer = queue->copy_command_buffer;

	VkCommandBufferBeginInfo begin_info = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
	begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
	VkResult result = vkBeginCommandBuffer(command_buffer, &begin_info);
	if (result != VK_SUCCESS) {
		rtvk_throwf(rtvk_error_from_vk(result), "Vulkan call returned %s", rtvk_vk_result_name(result));
		rtvk_mutex_unlock(&queue->lock);
		return;
	}

	struct rtvk_texture* src_node = src_texture->active;
	struct rtvk_texture* dst_node = dst_texture->active;
	VkImageLayout src_restore_layout = src_node->base.vk_layout == VK_IMAGE_LAYOUT_UNDEFINED ? VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL : src_node->base.vk_layout;
	VkImageLayout dst_restore_layout = dst_node->base.vk_layout == VK_IMAGE_LAYOUT_UNDEFINED ? VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL : dst_node->base.vk_layout;
	VkImageAspectFlags aspect = rtvk_texture_format_aspect(src_node->base.vk_format);

	VkImageMemoryBarrier barriers[2] = { { VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER }, { VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER } };
	barriers[0].srcAccessMask = rtvk_texture_layout_access(src_node->base.vk_layout);
	barriers[0].dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
	barriers[0].oldLayout = src_node->base.vk_layout;
	barriers[0].newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
	barriers[0].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barriers[0].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barriers[0].image = src_node->base.vk_image;
	barriers[0].subresourceRange.aspectMask = aspect;
	barriers[0].subresourceRange.baseMipLevel = src_mip;
	barriers[0].subresourceRange.levelCount = 1;
	barriers[0].subresourceRange.baseArrayLayer = 0;
	barriers[0].subresourceRange.layerCount = 1;
	barriers[1].srcAccessMask = rtvk_texture_layout_access(dst_node->base.vk_layout);
	barriers[1].dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
	barriers[1].oldLayout = dst_node->base.vk_layout;
	barriers[1].newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
	barriers[1].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barriers[1].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barriers[1].image = dst_node->base.vk_image;
	barriers[1].subresourceRange.aspectMask = aspect;
	barriers[1].subresourceRange.baseMipLevel = dst_mip;
	barriers[1].subresourceRange.levelCount = 1;
	barriers[1].subresourceRange.baseArrayLayer = 0;
	barriers[1].subresourceRange.layerCount = 1;
	vkCmdPipelineBarrier(command_buffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, NULL, 0, NULL, 2, barriers);

	VkImageCopy copy = { 0 };
	copy.srcSubresource.aspectMask = aspect;
	copy.srcSubresource.mipLevel = src_mip;
	copy.srcSubresource.baseArrayLayer = 0;
	copy.srcSubresource.layerCount = 1;
	copy.srcOffset.x = (i32)src_x;
	copy.srcOffset.y = (i32)src_y;
	copy.srcOffset.z = 0;
	copy.dstSubresource.aspectMask = aspect;
	copy.dstSubresource.mipLevel = dst_mip;
	copy.dstSubresource.baseArrayLayer = 0;
	copy.dstSubresource.layerCount = 1;
	copy.dstOffset.x = (i32)dst_x;
	copy.dstOffset.y = (i32)dst_y;
	copy.dstOffset.z = 0;
	copy.extent.width = width;
	copy.extent.height = height;
	copy.extent.depth = depth;
	vkCmdCopyImage(command_buffer, src_node->base.vk_image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, dst_node->base.vk_image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copy);

	barriers[0].srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
	barriers[0].dstAccessMask = rtvk_texture_layout_access(src_restore_layout);
	barriers[0].oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
	barriers[0].newLayout = src_restore_layout;
	barriers[0].subresourceRange.baseMipLevel = src_mip;
	barriers[1].srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
	barriers[1].dstAccessMask = rtvk_texture_layout_access(dst_restore_layout);
	barriers[1].oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
	barriers[1].newLayout = dst_restore_layout;
	barriers[1].subresourceRange.baseMipLevel = dst_mip;
	vkCmdPipelineBarrier(command_buffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, NULL, 0, NULL, 2, barriers);

	result = vkEndCommandBuffer(command_buffer);
	if (result != VK_SUCCESS) {
		rtvk_throwf(rtvk_error_from_vk(result), "Vulkan call returned %s", rtvk_vk_result_name(result));
		rtvk_mutex_unlock(&queue->lock);
		return;
	}

	queue->copy_command_timepoint = rtvk_queue_submit_immediate_locked(ctx, queue, command_buffer);
	if (!queue->copy_command_timepoint.value) {
		rtvk_mutex_unlock(&queue->lock);
		return;
	}
	src_node->base.vk_layout = src_restore_layout;
	dst_node->base.vk_layout = dst_restore_layout;
	rtvk_mutex_unlock(&queue->lock);
}

rt_timepoint rtvk_texture_data(struct rtvk_context* ctx, struct rtvk_texture* texture, enum rt_texture_type type, u32 mip, u32 width, u32 height, u32 depth, enum rt_format format, const void* data) {
	struct rtvk_queue* queue = rtvk_queue_query(ctx, RT_QUEUE_GRAPHICS);
	if (!queue) {
		queue = rtvk_queue_query(ctx, RT_QUEUE_TRANSFER);
	}
	rt_timepoint timepoint = { 0 };
	if (!queue) {
		rtvk_throwf(RT_IMPROPER_USAGE, "texture data upload requires a graphics or transfer queue");
		return timepoint;
	}
	if (!texture) {
		rtvk_throwf(RT_IMPROPER_USAGE, "texture is NULL");
		return timepoint;
	}
	if (type != RT_TEXTURE_2D || mip != 0 || depth > 1) {
		rtvk_throwf(RT_UNSUPPORTED_FEATURE, "texture data currently supports only 2D mip 0 textures");
		return timepoint;
	}
	if (width == 0 || height == 0) {
		rtvk_throwf(RT_IMPROPER_USAGE, "texture dimensions must be nonzero");
		return timepoint;
	}

	VkFormat vk_format = rtvk_texture_format(format);
	u32 bytes_per_pixel = rtvk_texture_format_bytes_per_pixel(format);
	if (vk_format == VK_FORMAT_UNDEFINED || bytes_per_pixel == 0) {
		rtvk_throwf(RT_UNSUPPORTED_FEATURE, "unsupported texture format");
		return timepoint;
	}

	rtvk_queue_collect_to_value(ctx, queue, rtvk_queue_completed_value(ctx, queue));
	rtvk_texture_collect_nodes(texture);

	struct rtvk_texture* node = rtvk_texture_node_create(ctx);
	if (!node) {
		return timepoint;
	}

	struct rtvk_texture* old_node = texture->active;
	texture->active = NULL;
	if (old_node) {
		rtvk_texture_recycle_node(texture, old_node);
	}

	VkImageCreateInfo image_info = { VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO };
	image_info.imageType = VK_IMAGE_TYPE_2D;
	image_info.format = vk_format;
	image_info.extent.width = width;
	image_info.extent.height = height;
	image_info.extent.depth = 1;
	image_info.mipLevels = (data && !rtvk_format_has_depth(vk_format) && !rtvk_format_has_stencil(vk_format))
							   ? rtvk_texture_mip_level_count(width, height, 1)
							   : 1;
	image_info.arrayLayers = 1;
	image_info.samples = VK_SAMPLE_COUNT_1_BIT;
	image_info.tiling = VK_IMAGE_TILING_OPTIMAL;
	image_info.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
	if (rtvk_format_has_depth(vk_format) || rtvk_format_has_stencil(vk_format)) {
		// Depth-only / depth-stencil formats almost never advertise
		// STORAGE_IMAGE support; asking for it makes vkCreateImage fail on
		// most desktop GPUs.
		image_info.usage |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
	} else {
		image_info.usage |= VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
	}
	image_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	image_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

	VmaAllocationCreateInfo allocation_info = { 0 };
	allocation_info.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;

	VkResult result = vmaCreateImage(ctx->vma_allocator, &image_info, &allocation_info, &node->base.vk_image, &node->vma_allocation, NULL);
	if (result != VK_SUCCESS) {
		rtvk_resource_retire(RTVK_RESOURCE_BASE(node));
		rtvk_throwf(rtvk_error_from_vk(result), "Vulkan call returned %s", rtvk_vk_result_name(result));
		return timepoint;
	}

	node->base.vk_format = vk_format;
	node->base.vk_layout = VK_IMAGE_LAYOUT_UNDEFINED;
	node->base.type = type;
	node->base.width = width;
	node->base.height = height;
	node->base.depth = 1;
	node->base.mip_levels = image_info.mipLevels;
	const VkImageAspectFlags aspect = rtvk_texture_format_aspect(vk_format);
	if ((rtvk_format_has_depth(vk_format) || rtvk_format_has_stencil(vk_format)) && !data) {
		texture->active = node;
		return timepoint;
	}

	u64 upload_size = (u64)width * height * bytes_per_pixel;
	if (data && upload_size) {
		rtvk_texture_upload_staging(ctx, queue, upload_size);
		if (rtvk_error() != RT_SUCCESS) {
			rtvk_resource_retire(RTVK_RESOURCE_BASE(node));
			return timepoint;
		}
	}

	if (data && upload_size) {
		VmaAllocationInfo staging_info;
		vmaGetAllocationInfo(ctx->vma_allocator, queue->upload_staging_allocation, &staging_info);
		memcpy(staging_info.pMappedData, data, (usize)upload_size);
		vmaFlushAllocation(ctx->vma_allocator, queue->upload_staging_allocation, 0, upload_size);
	}

	rtvk_texture_upload_command(ctx, queue);
	if (rtvk_error() != RT_SUCCESS) {
		rtvk_resource_retire(RTVK_RESOURCE_BASE(node));
		return timepoint;
	}
	VkCommandBuffer command_buffer = queue->upload_command_buffer;

	VkCommandBufferBeginInfo begin_info = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
	begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
	result = vkBeginCommandBuffer(command_buffer, &begin_info);
	if (result != VK_SUCCESS) {
		rtvk_resource_retire(RTVK_RESOURCE_BASE(node));
		rtvk_throwf(rtvk_error_from_vk(result), "Vulkan call returned %s", rtvk_vk_result_name(result));
		return timepoint;
	}

	VkImageMemoryBarrier barrier = { VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER };
	barrier.srcAccessMask = 0;
	barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	barrier.dstAccessMask = data ? VK_ACCESS_TRANSFER_WRITE_BIT : VK_ACCESS_SHADER_READ_BIT;
	barrier.newLayout = data ? VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL : VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.image = node->base.vk_image;
	barrier.subresourceRange.aspectMask = aspect;
	barrier.subresourceRange.baseMipLevel = 0;
	barrier.subresourceRange.levelCount = data ? node->base.mip_levels : 1;
	barrier.subresourceRange.baseArrayLayer = 0;
	barrier.subresourceRange.layerCount = 1;
	vkCmdPipelineBarrier(command_buffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, data ? VK_PIPELINE_STAGE_TRANSFER_BIT : VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, NULL, 0, NULL, 1, &barrier);

	if (data && upload_size) {
		VkBufferImageCopy copy = {};
		copy.bufferOffset = 0;
		copy.imageSubresource.aspectMask = aspect;
		copy.imageSubresource.mipLevel = 0;
		copy.imageSubresource.baseArrayLayer = 0;
		copy.imageSubresource.layerCount = 1;
		copy.imageExtent.width = width;
		copy.imageExtent.height = height;
		copy.imageExtent.depth = 1;
		vkCmdCopyBufferToImage(
			command_buffer,
			queue->upload_staging_buffer,
			node->base.vk_image,
			VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			1,
			&copy
		);

		if (node->base.mip_levels > 1) {
			u32 mip_width = width;
			u32 mip_height = height;
			for (u32 mip_level = 1; mip_level < node->base.mip_levels; ++mip_level) {
				VkImageMemoryBarrier src_barrier = { VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER };
				src_barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
				src_barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
				src_barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
				src_barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
				src_barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
				src_barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
				src_barrier.image = node->base.vk_image;
				src_barrier.subresourceRange.aspectMask = aspect;
				src_barrier.subresourceRange.baseMipLevel = mip_level - 1;
				src_barrier.subresourceRange.levelCount = 1;
				src_barrier.subresourceRange.baseArrayLayer = 0;
				src_barrier.subresourceRange.layerCount = 1;
				vkCmdPipelineBarrier(command_buffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, NULL, 0, NULL, 1, &src_barrier);

				VkImageBlit blit = { 0 };
				blit.srcSubresource.aspectMask = aspect;
				blit.srcSubresource.mipLevel = mip_level - 1;
				blit.srcSubresource.baseArrayLayer = 0;
				blit.srcSubresource.layerCount = 1;
				blit.srcOffsets[0].x = 0;
				blit.srcOffsets[0].y = 0;
				blit.srcOffsets[0].z = 0;
				blit.srcOffsets[1].x = (i32)mip_width;
				blit.srcOffsets[1].y = (i32)mip_height;
				blit.srcOffsets[1].z = 1;
				blit.dstSubresource.aspectMask = aspect;
				blit.dstSubresource.mipLevel = mip_level;
				blit.dstSubresource.baseArrayLayer = 0;
				blit.dstSubresource.layerCount = 1;
				blit.dstOffsets[0].x = 0;
				blit.dstOffsets[0].y = 0;
				blit.dstOffsets[0].z = 0;
				blit.dstOffsets[1].x = (i32)((mip_width >> 1) > 1u ? (mip_width >> 1) : 1u);
				blit.dstOffsets[1].y = (i32)((mip_height >> 1) > 1u ? (mip_height >> 1) : 1u);
				blit.dstOffsets[1].z = 1;
				vkCmdBlitImage(command_buffer, node->base.vk_image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, node->base.vk_image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &blit, VK_FILTER_LINEAR);

				VkImageMemoryBarrier dst_barrier = { VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER };
				dst_barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
				dst_barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
				dst_barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
				dst_barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
				dst_barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
				dst_barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
				dst_barrier.image = node->base.vk_image;
				dst_barrier.subresourceRange.aspectMask = aspect;
				dst_barrier.subresourceRange.baseMipLevel = mip_level - 1;
				dst_barrier.subresourceRange.levelCount = 1;
				dst_barrier.subresourceRange.baseArrayLayer = 0;
				dst_barrier.subresourceRange.layerCount = 1;
				vkCmdPipelineBarrier(command_buffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, NULL, 0, NULL, 1, &dst_barrier);

				mip_width = (mip_width >> 1) > 1u ? (mip_width >> 1) : 1u;
				mip_height = (mip_height >> 1) > 1u ? (mip_height >> 1) : 1u;
			}

			VkImageMemoryBarrier final_barrier = { VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER };
			final_barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
			final_barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
			final_barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
			final_barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
			final_barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			final_barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			final_barrier.image = node->base.vk_image;
			final_barrier.subresourceRange.aspectMask = aspect;
			final_barrier.subresourceRange.baseMipLevel = node->base.mip_levels - 1;
			final_barrier.subresourceRange.levelCount = 1;
			final_barrier.subresourceRange.baseArrayLayer = 0;
			final_barrier.subresourceRange.layerCount = 1;
			vkCmdPipelineBarrier(command_buffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, NULL, 0, NULL, 1, &final_barrier);
		} else {
			barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
			barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
			barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
			barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
			barrier.subresourceRange.levelCount = 1;
			vkCmdPipelineBarrier(command_buffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, NULL, 0, NULL, 1, &barrier);
		}
	}

	result = vkEndCommandBuffer(command_buffer);
	if (result == VK_SUCCESS) {
		timepoint = rtvk_queue_submit_immediate(ctx, queue, command_buffer);
		if (timepoint.value) {
			queue->upload_command_timepoint = timepoint;
		} else {
			return timepoint;
		}
	}

	if (result != VK_SUCCESS) {
		rtvk_resource_retire(RTVK_RESOURCE_BASE(node));
		rtvk_throwf(rtvk_error_from_vk(result), "Vulkan call returned %s", rtvk_vk_result_name(result));
		return timepoint;
	}

	node->base.vk_layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	rtvk_texture_recycle_node(texture, texture->active);
	texture->active = node;
	return timepoint;
}

rt_timepoint rtvk_texture_subcopy(struct rtvk_context* ctx, struct rtvk_texture* src_texture, u32 src_mip, u32 src_x, u32 src_y, u32 src_z, struct rtvk_texture* dst_texture, u32 dst_mip, u32 dst_x, u32 dst_y, u32 dst_z, u32 width, u32 height, u32 depth) {
	struct rtvk_queue* queue = rtvk_queue_query(ctx, RT_QUEUE_GRAPHICS);
	if (!queue) {
		queue = rtvk_queue_query(ctx, RT_QUEUE_TRANSFER);
	}
	rt_timepoint timepoint = { 0 };
	if (!queue) {
		rtvk_throwf(RT_IMPROPER_USAGE, "texture subcopy requires a graphics or transfer queue");
		return timepoint;
	}
	rtvk_texture_copy_region(ctx, queue, src_texture, src_x, src_y, src_z, src_mip, dst_texture, dst_x, dst_y, dst_z, dst_mip, width, height, depth);
	if (rtvk_error() != RT_SUCCESS) {
		return timepoint;
	}
	timepoint = queue->copy_command_timepoint;
	return timepoint;
}

rt_timepoint rtvk_texture_subdata(struct rtvk_context* ctx, struct rtvk_texture* texture, u32 mip, u32 offset_x, u32 offset_y, u32 offset_z, u32 width, u32 height, u32 depth, const void* data) {
	struct rtvk_queue* queue = rtvk_queue_query(ctx, RT_QUEUE_GRAPHICS);
	if (!queue) {
		queue = rtvk_queue_query(ctx, RT_QUEUE_TRANSFER);
	}
	rt_timepoint timepoint = { 0 };
	if (!queue) {
		rtvk_throwf(RT_IMPROPER_USAGE, "texture subdata upload requires a graphics or transfer queue");
		return timepoint;
	}
	if (!texture || !texture->active || !texture->active->base.vk_image) {
		rtvk_throwf(RT_IMPROPER_USAGE, "texture subdata target is NULL");
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
	if (node->base.type != RT_TEXTURE_2D || mip != 0 || offset_z != 0 || depth != 1) {
		rtvk_throwf(RT_UNSUPPORTED_FEATURE, "texture subdata currently supports only 2D mip 0 textures");
		return timepoint;
	}
	if (offset_x > node->base.width || offset_y > node->base.height ||
		width > node->base.width - offset_x || height > node->base.height - offset_y) {
		rtvk_throwf(RT_IMPROPER_USAGE, "texture subdata region is out of bounds");
		return timepoint;
	}

	u32 bytes_per_pixel = rtvk_texture_view_bytes_per_pixel(node->base.vk_format);
	if (bytes_per_pixel == 0) {
		rtvk_throwf(RT_UNSUPPORTED_FEATURE, "texture subdata does not support this Vulkan format");
		return timepoint;
	}

	u64 upload_size = (u64)width * height * depth * bytes_per_pixel;
	rtvk_queue_collect_to_value(ctx, queue, rtvk_queue_completed_value(ctx, queue));
	rtvk_queue_flush(ctx, queue);
	rtvk_texture_upload_staging(ctx, queue, upload_size);
	if (rtvk_error() != RT_SUCCESS) {
		return timepoint;
	}

	VmaAllocationInfo staging_info;
	vmaGetAllocationInfo(ctx->vma_allocator, queue->upload_staging_allocation, &staging_info);
	memcpy(staging_info.pMappedData, data, (usize)upload_size);
	vmaFlushAllocation(ctx->vma_allocator, queue->upload_staging_allocation, 0, upload_size);

	rtvk_texture_upload_command(ctx, queue);
	if (rtvk_error() != RT_SUCCESS) {
		return timepoint;
	}
	VkCommandBuffer command_buffer = queue->upload_command_buffer;

	VkCommandBufferBeginInfo begin_info = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
	begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
	VkResult result = vkBeginCommandBuffer(command_buffer, &begin_info);
	if (result != VK_SUCCESS) {
		rtvk_throwf(rtvk_error_from_vk(result), "Vulkan call returned %s", rtvk_vk_result_name(result));
		return timepoint;
	}

	VkImageLayout original_layout = node->base.vk_layout;
	VkImageLayout restore_layout = original_layout == VK_IMAGE_LAYOUT_UNDEFINED ? VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL : original_layout;
	const VkImageAspectFlags aspect = rtvk_texture_format_aspect(node->base.vk_format);

	VkImageMemoryBarrier barrier = { VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER };
	barrier.srcAccessMask = rtvk_texture_layout_access(original_layout);
	barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
	barrier.oldLayout = original_layout;
	barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
	barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.image = node->base.vk_image;
	barrier.subresourceRange.aspectMask = aspect;
	barrier.subresourceRange.baseMipLevel = 0;
	barrier.subresourceRange.levelCount = 1;
	barrier.subresourceRange.baseArrayLayer = 0;
	barrier.subresourceRange.layerCount = 1;
	vkCmdPipelineBarrier(command_buffer, rtvk_texture_layout_stage(original_layout), VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, NULL, 0, NULL, 1, &barrier);

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
		node->base.vk_image,
		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
		1,
		&copy
	);

	barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
	barrier.dstAccessMask = rtvk_texture_layout_access(restore_layout);
	barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
	barrier.newLayout = restore_layout;
	VkPipelineStageFlags restore_stage = rtvk_texture_layout_stage(restore_layout);
	if (restore_stage == VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT) {
		restore_stage = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
	}
	vkCmdPipelineBarrier(command_buffer, VK_PIPELINE_STAGE_TRANSFER_BIT, restore_stage, 0, 0, NULL, 0, NULL, 1, &barrier);

	result = vkEndCommandBuffer(command_buffer);
	if (result == VK_SUCCESS) {
		timepoint = rtvk_queue_submit_immediate(ctx, queue, command_buffer);
		if (timepoint.value) {
			queue->upload_command_timepoint = timepoint;
			node->base.vk_layout = restore_layout;
		} else {
			return timepoint;
		}
	}

	if (result != VK_SUCCESS) {
		rtvk_throwf(rtvk_error_from_vk(result), "Vulkan call returned %s", rtvk_vk_result_name(result));
		return timepoint;
	}

	return timepoint;
}

static void rtvk_texture_copy_buffer_command(struct rtvk_context* ctx, struct rtvk_queue* queue) {
	if (queue->copy_command_pool && queue->copy_command_buffer) {
		rtvk_queue_collect_to_value(ctx, queue, rtvk_queue_completed_value(ctx, queue));
		queue->copy_command_timepoint = (rt_timepoint){ 0 };
		vkResetCommandPool(ctx->vk_device, queue->copy_command_pool, 0);
		return;
	}

	VkCommandPoolCreateInfo pool_info = { VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO };
	pool_info.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT | VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
	pool_info.queueFamilyIndex = queue->family_index;

	VkResult result = vkCreateCommandPool(ctx->vk_device, &pool_info, VK_ALLOCATOR, &queue->copy_command_pool);
	if (result != VK_SUCCESS) {
		rtvk_throwf(rtvk_error_from_vk(result), "Vulkan call returned %s", rtvk_vk_result_name(result));
		return;
	}

	VkCommandBufferAllocateInfo alloc_info = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO };
	alloc_info.commandPool = queue->copy_command_pool;
	alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	alloc_info.commandBufferCount = 1;
	result = vkAllocateCommandBuffers(ctx->vk_device, &alloc_info, &queue->copy_command_buffer);
	if (result != VK_SUCCESS) {
		vkDestroyCommandPool(ctx->vk_device, queue->copy_command_pool, VK_ALLOCATOR);
		queue->copy_command_pool = VK_NULL_HANDLE;
		rtvk_throwf(rtvk_error_from_vk(result), "Vulkan call returned %s", rtvk_vk_result_name(result));
		return;
	}
}

rt_timepoint rtvk_texture_view_copy_to_buffer(struct rtvk_context* ctx, struct rtvk_texture_view* texture_view, struct rtvk_buffer* buffer) {
	struct rtvk_queue* queue = rtvk_queue_query(ctx, RT_QUEUE_TRANSFER);
	rt_timepoint timepoint = { 0 };
	if (!queue) {
		rtvk_throwf(RT_IMPROPER_USAGE, "texture view copy requires a transfer queue");
		return timepoint;
	}
	if (!texture_view || !texture_view->image || !texture_view->vk_image_view) {
		rtvk_throwf(RT_IMPROPER_USAGE, "texture view copy backing is NULL");
		return timepoint;
	}
	struct rtvk_image_base* image = texture_view->image;
	if (!buffer) {
		rtvk_throwf(RT_IMPROPER_USAGE, "texture view copy requires a valid destination buffer");
		return timepoint;
	}
	if (buffer->active && buffer->active->memory_type != RT_HOST_MEMORY) {
		rtvk_throwf(RT_IMPROPER_USAGE, "texture view copy requires a host-visible destination buffer");
		return timepoint;
	}

	u32 bytes_per_pixel = rtvk_texture_view_bytes_per_pixel(image->vk_format);
	if (bytes_per_pixel == 0) {
		rtvk_throwf(RT_UNSUPPORTED_FEATURE, "texture view copy does not support this Vulkan format");
		return timepoint;
	}

	u64 total_size = (u64)image->width * image->height * bytes_per_pixel;
	if (!buffer->active || buffer->active->size < total_size) {
		rtvk_buffer_resize(ctx, buffer, RT_HOST_MEMORY, total_size);
	}
	if (!buffer->active) {
		return timepoint;
	}

	rtvk_queue_flush(ctx, queue);
	rtvk_texture_copy_buffer_command(ctx, queue);
	if (rtvk_error() != RT_SUCCESS) {
		return timepoint;
	}
	VkCommandBuffer command_buffer = queue->copy_command_buffer;

	VkCommandBufferBeginInfo begin_info = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
	begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
	VkResult result = vkBeginCommandBuffer(command_buffer, &begin_info);
	if (result != VK_SUCCESS) {
		rtvk_throwf(rtvk_error_from_vk(result), "Vulkan call returned %s", rtvk_vk_result_name(result));
		return timepoint;
	}

	VkImageLayout original_layout = image->vk_layout;
	VkImageMemoryBarrier barrier = { VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER };
	barrier.srcAccessMask = rtvk_texture_layout_access(original_layout);
	barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
	barrier.oldLayout = original_layout;
	barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
	barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.image = image->vk_image;
	barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	barrier.subresourceRange.baseMipLevel = 0;
	barrier.subresourceRange.levelCount = 1;
	barrier.subresourceRange.baseArrayLayer = 0;
	barrier.subresourceRange.layerCount = 1;
	vkCmdPipelineBarrier(command_buffer, rtvk_texture_layout_stage(original_layout), VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, NULL, 0, NULL, 1, &barrier);

	VkBufferImageCopy copy = { 0 };
	copy.bufferOffset = 0;
	copy.bufferRowLength = 0;
	copy.bufferImageHeight = 0;
	copy.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	copy.imageSubresource.mipLevel = 0;
	copy.imageSubresource.baseArrayLayer = 0;
	copy.imageSubresource.layerCount = 1;
	copy.imageExtent.width = image->width;
	copy.imageExtent.height = image->height;
	copy.imageExtent.depth = 1;
	vkCmdCopyImageToBuffer(
		command_buffer,
		image->vk_image,
		VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
		buffer->active->vk_buffer,
		1,
		&copy
	);

	barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
	barrier.dstAccessMask = rtvk_texture_layout_access(original_layout);
	barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
	barrier.newLayout = original_layout;
	VkPipelineStageFlags restore_stage = rtvk_texture_layout_stage(original_layout);
	if (restore_stage == VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT) {
		restore_stage = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
	}
	vkCmdPipelineBarrier(command_buffer, VK_PIPELINE_STAGE_TRANSFER_BIT, restore_stage, 0, 0, NULL, 0, NULL, 1, &barrier);

	result = vkEndCommandBuffer(command_buffer);
	if (result == VK_SUCCESS) {
		timepoint = rtvk_queue_submit_immediate(ctx, queue, command_buffer);
		if (timepoint.value) {
			queue->copy_command_timepoint = timepoint;
		} else {
			return timepoint;
		}
	}

	rtvk_queue_collect_to_value(ctx, queue, rtvk_queue_completed_value(ctx, queue));
	if (result != VK_SUCCESS) {
		rtvk_throwf(rtvk_error_from_vk(result), "Vulkan call returned %s", rtvk_vk_result_name(result));
		return timepoint;
	}

	return timepoint;
}
