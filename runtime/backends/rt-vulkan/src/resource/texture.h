#ifndef RTVK_TEXTURE_H
#define RTVK_TEXTURE_H

#include "config.h"
#include "resource.h"

#include <vk_mem_alloc.h>
#include <volk.h>

/*===============================================================================================*/
/*                                                                                               */
/*===============================================================================================*/

struct rtvk_swapchain_frame;
struct rtvk_buffer;

RTVK_API rt_texture rtTextureCreate(void);
RTVK_API void rtTextureDestroy(rt_texture texture);
RTVK_API rt_texture_view rtTextureViewCreate(void);
RTVK_API void rtTextureViewBind(rt_texture_view texture_view, rt_texture texture);
RTVK_API void rtTextureViewDestroy(rt_texture_view texture_view);
RTVK_API void rtTextureViewFilter(rt_texture_view texture_view, enum rt_filter mag_filter, enum rt_filter min_filter, enum rt_mip_filter mip_filter);
RTVK_API void rtTextureViewAddress(rt_texture_view texture_view, enum rt_address_mode address_u, enum rt_address_mode address_v, enum rt_address_mode address_w);
RTVK_API void rtTextureViewAnisotropy(rt_texture_view texture_view, u32 max_anisotropy);
RTVK_API void rtTextureViewLod(rt_texture_view texture_view, f32 min_lod, f32 max_lod, f32 lod_bias);

RTVK_API rt_timepoint rtTextureCopy(rt_texture src_texture, u32 src_mip, rt_texture dst_texture, u32 dst_mip);
RTVK_API rt_timepoint rtTextureData(rt_texture texture, enum rt_texture_type type, u32 mip, u32 width, u32 height, u32 depth, enum rt_format format, const void* data);
RTVK_API rt_timepoint rtTextureSubcopy(rt_texture src_texture, u32 src_mip, u32 src_x, u32 src_y, u32 src_z, rt_texture dst_texture, u32 dst_mip, u32 dst_x, u32 dst_y, u32 dst_z, u32 width, u32 height, u32 depth);
RTVK_API rt_timepoint rtTextureSubdata(rt_texture texture, u32 mip, u32 offset_x, u32 offset_y, u32 offset_z, u32 width, u32 height, u32 depth, const void* data);
RTVK_API rt_timepoint rtTextureViewCopyToBuffer(rt_texture_view texture_view, rt_buffer buffer);
RTVK_API rt_extent_3d rtTextureViewExtent(rt_texture_view texture_view);

/*===============================================================================================*/
/*                                                                                               */
/*===============================================================================================*/


struct rtvk_texture {
	struct rtvk_resource_base base;
	struct rtvk_texture* active;
	struct rtvk_texture* next;

	VkImage vk_image;
	VkFormat vk_format;
	VkImageLayout vk_layout;
	VmaAllocation vma_allocation;
	enum rt_texture_type type;
	u32 width;
	u32 height;
	u32 depth;
	u32 mip_levels;
};

RTVK_DECLARE_NEW_RESOURCE(texture)

struct rtvk_texture_view {
	struct rtvk_resource_base base;
	struct rtvk_resource_base* backing;
	VkImageView vk_image_view;
	VkSampler vk_sampler;

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

struct rtvk_texture_view* rtvk_texture_view_create_for_texture(struct rtvk_context* ctx, struct rtvk_texture* texture);
void rtvk_texture_view_bind(struct rtvk_context* ctx, struct rtvk_texture_view* view, struct rtvk_texture* texture);
// Bind a view against any image-owning resource by its shared base header.
// The backing must be an image-owning resource type (currently
// RT_RESOURCE_TEXTURE or RT_RESOURCE_SWAPCHAIN_FRAME) — the concrete struct
// is reached via container_of on base->type when descriptor state is needed.
void rtvk_texture_view_bind_backing(struct rtvk_context* ctx, struct rtvk_texture_view* view, struct rtvk_resource_base* backing);

// Resolve an image-owning resource base to the fields needed for barrier
// insertion, VkImageView creation, extent queries. Reads live from the
// backing struct — no caching. `out_layout` receives a pointer into the
// backing so callers can update it after transitions.
void rtvk_image_backing_read(struct rtvk_resource_base* backing, VkImage* out_image, VkFormat* out_format, VkImageLayout** out_layout, enum rt_texture_type* out_type, u32* out_w, u32* out_h, u32* out_d, u32* out_mips);

// Convenience wrappers over rtvk_image_backing_read for single-field queries.
// A NULL backing is treated as an all-zero image (width/height 0,
// format UNDEFINED, layout UNDEFINED) so caller-side null guards can be
// replaced by using the return value directly.
u32 rtvk_view_width(const struct rtvk_texture_view* view);
u32 rtvk_view_height(const struct rtvk_texture_view* view);
VkFormat rtvk_view_format(const struct rtvk_texture_view* view);
VkImageLayout rtvk_view_layout(const struct rtvk_texture_view* view);

void rtvk_texture_view_filter(struct rtvk_texture_view* texture_view, enum rt_filter mag_filter, enum rt_filter min_filter, enum rt_mip_filter mip_filter);
void rtvk_texture_view_address(struct rtvk_texture_view* texture_view, enum rt_address_mode address_u, enum rt_address_mode address_v, enum rt_address_mode address_w);
void rtvk_texture_view_anisotropy(struct rtvk_texture_view* texture_view, u32 max_anisotropy);
void rtvk_texture_view_lod(struct rtvk_texture_view* texture_view, f32 min_lod, f32 max_lod, f32 lod_bias);
VkImageAspectFlags rtvk_texture_format_aspect(VkFormat format);
void rtvk_texture_recycle_node(struct rtvk_texture* texture, struct rtvk_texture* node);
void rtvk_texture_collect_nodes(struct rtvk_texture* texture);

struct rtvk_timepoint rtvk_texture_copy(struct rtvk_context* ctx, struct rtvk_texture* src_texture, u32 src_mip, struct rtvk_texture* dst_texture, u32 dst_mip);
struct rtvk_timepoint rtvk_texture_data(struct rtvk_context* ctx, struct rtvk_texture* texture, enum rt_texture_type type, u32 mip, u32 width, u32 height, u32 depth, enum rt_format format, const void* data);
struct rtvk_timepoint rtvk_texture_subcopy(struct rtvk_context* ctx, struct rtvk_texture* src_texture, u32 src_mip, u32 src_x, u32 src_y, u32 src_z, struct rtvk_texture* dst_texture, u32 dst_mip, u32 dst_x, u32 dst_y, u32 dst_z, u32 width, u32 height, u32 depth);
struct rtvk_timepoint rtvk_texture_subdata(struct rtvk_context* ctx, struct rtvk_texture* texture, u32 mip, u32 offset_x, u32 offset_y, u32 offset_z, u32 width, u32 height, u32 depth, const void* data);
struct rtvk_timepoint rtvk_texture_view_copy_to_buffer(struct rtvk_context* ctx, struct rtvk_texture_view* texture_view, struct rtvk_buffer* buffer);

#endif
