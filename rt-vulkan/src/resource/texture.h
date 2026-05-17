#ifndef RTVK_TEXTURE_H
#define RTVK_TEXTURE_H

#include "config.h"
#include "resource.h"

#include <vulkan/vulkan.h>
#include <vk_mem_alloc.h>

/*===============================================================================================*/
/*                                                                                               */
/*===============================================================================================*/

struct rtvk_swapchain_frame;
struct rtvk_buffer;

RTVK_API rt_texture rtTextureCreate(void);
RTVK_API void rtTextureDestroy(rt_texture texture);
RTVK_API rt_texture_view rtTextureViewCreate(rt_texture texture);
RTVK_API void rtTextureViewDestroy(rt_texture_view texture_view);
RTVK_API void rtTextureViewFilter(rt_texture_view texture_view, enum rt_filter mag_filter, enum rt_filter min_filter, enum rt_mip_filter mip_filter);
RTVK_API void rtTextureViewAddress(rt_texture_view texture_view, enum rt_address_mode address_u, enum rt_address_mode address_v, enum rt_address_mode address_w);
RTVK_API void rtTextureViewAnisotropy(rt_texture_view texture_view, u32 max_anisotropy);
RTVK_API void rtTextureViewLod(rt_texture_view texture_view, f32 min_lod, f32 max_lod, f32 lod_bias);

RTVK_API rt_timepoint rtTextureCopy(rt_queue queue, rt_texture src_texture, u32 src_mip, rt_texture dst_texture, u32 dst_mip);
RTVK_API rt_timepoint rtTextureData(rt_queue queue, rt_texture texture, enum rt_texture_type type, u32 mip, u32 offset_x, u32 offset_y, u32 offset_z, enum rt_format format, const void* data);
RTVK_API rt_timepoint rtTextureSubcopy(rt_queue queue, rt_texture src_texture, u32 src_mip, u32 src_x, u32 src_y, u32 src_z, rt_texture dst_texture, u32 dst_mip, u32 dst_x, u32 dst_y, u32 dst_z, u32 width, u32 height, u32 depth);
RTVK_API rt_timepoint rtTextureSubdata(rt_queue queue, rt_texture texture, u32 mip, u32 offset_x, u32 offset_y, u32 offset_z, u32 width, u32 height, u32 depth, const void* data);
RTVK_API rt_timepoint rtTextureViewCopyToBuffer(rt_queue queue, rt_texture_view texture_view, rt_buffer buffer);
RTVK_API rt_extent_3d rtTextureViewExtent(rt_texture_view texture_view);

/*===============================================================================================*/
/*                                                                                               */
/*===============================================================================================*/

struct rtvk_texture {
	struct rtvk_resource_base base;
	struct rtvk_texture* active;
	struct rtvk_texture* next;

	VkImage vk_image;
	VmaAllocation vma_allocation;

	u32 width;
	u32 height;
	u32 depth;
	VkFormat vk_format;
	VkImageLayout vk_layout;
	enum rt_texture_type type;
};

RTVK_DECLARE_NEW_RESOURCE(texture)

typedef struct rtvk_retired_sampler {
	struct rtvk_retired_sampler* next;
	VkSampler vk_sampler;
} rtvk_retired_sampler;

struct rtvk_texture_view {
	struct rtvk_resource_base base;
	struct rtvk_texture* texture;
	rtvk_retired_sampler* retired_samplers;

	VkImageView vk_image_view;
	VkSampler vk_sampler;

	u32 width;
	u32 height;
	u32 depth;
	VkFormat vk_format;
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

RTVK_DECLARE_NEW_RESOURCE(texture_view)

struct rtvk_texture* rtvk_texture_create_for_swapchain_image(struct rtvk_context* ctx, VkImage image, VkFormat format, u32 width, u32 height);
struct rtvk_texture_view* rtvk_texture_view_create_for_texture(struct rtvk_context* ctx, struct rtvk_texture* texture);
struct rtvk_texture_view* rtvk_texture_view_create_for_swapchain(struct rtvk_context* ctx, struct rtvk_texture* texture);
void rtvk_texture_node_retain(struct rtvk_texture* texture);
void rtvk_texture_node_release(struct rtvk_texture* texture);
void rtvk_texture_view_filter(struct rtvk_texture_view* texture_view, enum rt_filter mag_filter, enum rt_filter min_filter, enum rt_mip_filter mip_filter);
void rtvk_texture_view_address(struct rtvk_texture_view* texture_view, enum rt_address_mode address_u, enum rt_address_mode address_v, enum rt_address_mode address_w);
void rtvk_texture_view_anisotropy(struct rtvk_texture_view* texture_view, u32 max_anisotropy);
void rtvk_texture_view_lod(struct rtvk_texture_view* texture_view, f32 min_lod, f32 max_lod, f32 lod_bias);
bool rtvk_texture_view_prepare_sampler(struct rtvk_context* ctx, struct rtvk_texture_view* texture_view);
VkImageAspectFlags rtvk_texture_format_aspect(VkFormat format);

struct rtvk_timepoint rtvk_texture_copy(struct rtvk_context* ctx, struct rtvk_queue* queue, struct rtvk_texture* src_texture, u32 src_mip, struct rtvk_texture* dst_texture, u32 dst_mip);
struct rtvk_timepoint rtvk_texture_data(struct rtvk_context* ctx, struct rtvk_queue* queue, struct rtvk_texture* texture, enum rt_texture_type type, u32 mip, u32 offset_x, u32 offset_y, u32 offset_z, enum rt_format format, const void* data);
struct rtvk_timepoint rtvk_texture_subcopy(struct rtvk_context* ctx, struct rtvk_queue* queue, struct rtvk_texture* src_texture, u32 src_mip, u32 src_x, u32 src_y, u32 src_z, struct rtvk_texture* dst_texture, u32 dst_mip, u32 dst_x, u32 dst_y, u32 dst_z, u32 width, u32 height, u32 depth);
struct rtvk_timepoint rtvk_texture_subdata(struct rtvk_context* ctx, struct rtvk_queue* queue, struct rtvk_texture* texture, u32 mip, u32 offset_x, u32 offset_y, u32 offset_z, u32 width, u32 height, u32 depth, const void* data);
struct rtvk_timepoint rtvk_texture_view_copy_to_buffer(struct rtvk_context* ctx, struct rtvk_queue* queue, struct rtvk_texture_view* texture_view, struct rtvk_buffer* buffer);

#endif


