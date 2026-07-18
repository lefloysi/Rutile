#include "texture.hpp"
#include "context.hpp"
#include "error.hpp"
#include "resource/buffer.hpp"
#include "resource/queue.hpp"

#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <vector>

/*===============================================================================================*/
/*                                                                                               */
/*===============================================================================================*/

rt_texture rtTextureCreate(void) {
	struct rtdx_texture* texture = rtdx_texture_create(rtdx_get_current_context());
	return rtdx_texture_to_handle(texture);
}

void rtTextureDestroy(rt_texture texture) {
	rtdx_texture_destroy(rtdx_get_current_context(), rtdx_texture_from_handle(texture));
}

rt_texture_view rtTextureViewCreate(void) {
	return rtdx_texture_view_to_handle(rtdx_texture_view_create(rtdx_get_current_context()));
}

void rtTextureViewBind(rt_texture_view texture_view, rt_texture texture) {
	rtdx_texture_view_bind(
		rtdx_get_current_context(),
		rtdx_texture_view_from_handle(texture_view),
		rtdx_texture_from_handle(texture)
	);
}

void rtTextureViewDestroy(rt_texture_view texture_view) {
	rtdx_texture_view_destroy(rtdx_get_current_context(), rtdx_texture_view_from_handle(texture_view));
}

void rtTextureViewFilter(rt_texture_view texture_view, enum rt_filter mag_filter, enum rt_filter min_filter, enum rt_mip_filter mip_filter) {
	rtdx_texture_view_filter(rtdx_texture_view_from_handle(texture_view), mag_filter, min_filter, mip_filter);
}

void rtTextureViewAddress(rt_texture_view texture_view, enum rt_address_mode address_u, enum rt_address_mode address_v, enum rt_address_mode address_w) {
	rtdx_texture_view_address(rtdx_texture_view_from_handle(texture_view), address_u, address_v, address_w);
}

void rtTextureViewAnisotropy(rt_texture_view texture_view, u32 max_anisotropy) {
	rtdx_texture_view_anisotropy(rtdx_texture_view_from_handle(texture_view), max_anisotropy);
}

void rtTextureViewLod(rt_texture_view texture_view, f32 min_lod, f32 max_lod, f32 lod_bias) {
	rtdx_texture_view_lod(rtdx_texture_view_from_handle(texture_view), min_lod, max_lod, lod_bias);
}

static D3D12_TEXTURE_ADDRESS_MODE rtdx_address_mode(enum rt_address_mode mode) {
	switch (mode) {
	case RT_ADDRESS_CLAMP:
		return D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
	case RT_ADDRESS_MIRROR:
		return D3D12_TEXTURE_ADDRESS_MODE_MIRROR;
	default:
		return D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	}
}

static void rtdx_texture_view_normalize_sampler(struct rtdx_texture_view* view) {
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

static D3D12_SAMPLER_DESC rtdx_sampler_desc(struct rtdx_texture_view* view) {
	rtdx_texture_view_normalize_sampler(view);

	bool min_linear = view->min_filter == RT_FILTER_LINEAR;
	bool mag_linear = view->mag_filter == RT_FILTER_LINEAR;
	bool mip_linear = view->mip_filter == RT_MIP_FILTER_LINEAR;
	D3D12_SAMPLER_DESC result = {};
	result.Filter = min_linear ? (mag_linear ? (mip_linear ? D3D12_FILTER_MIN_MAG_MIP_LINEAR : D3D12_FILTER_MIN_MAG_LINEAR_MIP_POINT) : D3D12_FILTER_MIN_LINEAR_MAG_MIP_POINT) : (mag_linear ? D3D12_FILTER_MIN_POINT_MAG_LINEAR_MIP_POINT : D3D12_FILTER_MIN_MAG_MIP_POINT);
	result.AddressU = rtdx_address_mode(view->address_u);
	result.AddressV = rtdx_address_mode(view->address_v);
	result.AddressW = rtdx_address_mode(view->address_w);
	result.MipLODBias = view->lod_bias;
	result.MaxAnisotropy = view->max_anisotropy;
	result.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
	result.BorderColor[0] = 0.0f;
	result.BorderColor[1] = 0.0f;
	result.BorderColor[2] = 0.0f;
	result.BorderColor[3] = 0.0f;
	result.MinLOD = view->min_lod;
	result.MaxLOD = view->max_lod;
	return result;
}

rt_timepoint rtTextureCopy(rt_texture src_texture, u32 src_mip, rt_texture dst_texture, u32 dst_mip) {
	struct rtdx_timepoint timepoint = rtdx_texture_copy(
		rtdx_get_current_context(),
		rtdx_texture_from_handle(src_texture),
		src_mip,
		rtdx_texture_from_handle(dst_texture),
		dst_mip
	);
	return rtdx_timepoint_to_public(timepoint);
}

rt_timepoint rtTextureData(rt_texture texture, enum rt_texture_type type, u32 mip, u32 width, u32 height, u32 depth, enum rt_format format, const void* data) {
	struct rtdx_timepoint timepoint = rtdx_texture_data(
		rtdx_get_current_context(),
		rtdx_texture_from_handle(texture),
		type,
		width,
		height,
		depth,
		mip,
		format,
		data
	);
	return rtdx_timepoint_to_public(timepoint);
}

rt_timepoint rtTextureSubcopy(rt_texture src_texture, u32 src_mip, u32 src_x, u32 src_y, u32 src_z, rt_texture dst_texture, u32 dst_mip, u32 dst_x, u32 dst_y, u32 dst_z, u32 width, u32 height, u32 depth) {
	struct rtdx_timepoint timepoint = rtdx_texture_subcopy(
		rtdx_get_current_context(),
		rtdx_texture_from_handle(src_texture),
		src_mip,
		src_x,
		src_y,
		src_z,
		rtdx_texture_from_handle(dst_texture),
		dst_mip,
		dst_x,
		dst_y,
		dst_z,
		width,
		height,
		depth
	);
	return rtdx_timepoint_to_public(timepoint);
}

rt_timepoint rtTextureSubdata(rt_texture texture, u32 mip, u32 offset_x, u32 offset_y, u32 offset_z, u32 width, u32 height, u32 depth, const void* data) {
	struct rtdx_timepoint timepoint = rtdx_texture_subdata(
		rtdx_get_current_context(),
		rtdx_texture_from_handle(texture),
		mip,
		offset_x,
		offset_y,
		offset_z,
		width,
		height,
		depth,
		data
	);
	return rtdx_timepoint_to_public(timepoint);
}

rt_timepoint rtTextureViewCopyToBuffer(rt_texture_view texture_view, rt_buffer buffer) {
	struct rtdx_timepoint timepoint = rtdx_texture_view_copy_to_buffer(
		rtdx_get_current_context(),
		rtdx_texture_view_from_handle(texture_view),
		rtdx_buffer_from_handle(buffer)
	);
	return rtdx_timepoint_to_public(timepoint);
}

rt_extent_3d rtTextureViewExtent(rt_texture_view texture_view) {
	rt_extent_3d extent = { 0, 0, 0 };
	struct rtdx_texture_view* view = rtdx_texture_view_from_handle(texture_view);
	if (!view || !view->d3d_resource) {
		rtdx_throwf(RT_IMPROPER_USAGE, "texture view extent query source is invalid");
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

RTDX_DEFINE_RESOURCE_PRIVATE(texture)
RTDX_DEFINE_RESOURCE_PRIVATE(texture_view)

static u32 rtdx_texture_view_bytes_per_pixel(DXGI_FORMAT format) {
	switch (format) {
	case DXGI_FORMAT_R8G8B8A8_UNORM:
	case DXGI_FORMAT_R8G8B8A8_UNORM_SRGB:
	case DXGI_FORMAT_B8G8R8A8_UNORM:
	case DXGI_FORMAT_B8G8R8A8_UNORM_SRGB:
		return 4;
	default:
		return 0;
	}
}

static bool rtdx_texture_upload_staging(struct rtdx_context* ctx, struct rtdx_queue* queue, u64 size);

static DXGI_FORMAT rtdx_texture_format(enum rt_format format) {
	switch (format) {
	case RT_RGBA8_UNORM:
		return DXGI_FORMAT_R8G8B8A8_UNORM;
	case RT_D16_UNORM:
		return DXGI_FORMAT_D16_UNORM;
	case RT_D32_SFLOAT:
		return DXGI_FORMAT_D32_FLOAT;
	case RT_S8_UINT:
		return DXGI_FORMAT_UNKNOWN;
	case RT_D24_UNORM_S8_UINT:
		return DXGI_FORMAT_D24_UNORM_S8_UINT;
	case RT_D32_SFLOAT_S8_UINT:
		return DXGI_FORMAT_D32_FLOAT_S8X24_UINT;
	default:
		return DXGI_FORMAT_UNKNOWN;
	}
}

static u32 rtdx_texture_format_bytes_per_pixel(enum rt_format format) {
	switch (format) {
	case RT_RGBA8_UNORM:
		return 4;
	case RT_D16_UNORM:
		return 2;
	case RT_D32_SFLOAT:
		return 4;
	case RT_S8_UINT:
		return 1;
	case RT_D24_UNORM_S8_UINT:
		return 4;
	case RT_D32_SFLOAT_S8_UINT:
		return 8;
	default:
		return 0;
	}
}

static struct rtdx_queue* rtdx_texture_upload_queue(struct rtdx_context* ctx) {
	struct rtdx_queue* queue = rtdx_queue_query(ctx, RT_QUEUE_TRANSFER);
	if (queue) {
		return queue;
	}
	return rtdx_queue_query(ctx, RT_QUEUE_GRAPHICS);
}

bool rtdx_texture_format_is_depth(DXGI_FORMAT format) {
	return format == DXGI_FORMAT_D16_UNORM || format == DXGI_FORMAT_D32_FLOAT ||
		   format == DXGI_FORMAT_D24_UNORM_S8_UINT || format == DXGI_FORMAT_D32_FLOAT_S8X24_UINT;
}

static bool rtdx_texture_view_needs_bgra_swizzle(DXGI_FORMAT format) {
	return format == DXGI_FORMAT_B8G8R8A8_UNORM || format == DXGI_FORMAT_B8G8R8A8_UNORM_SRGB;
}

static bool rtdx_texture_copy_region(
	struct rtdx_context* ctx,
	struct rtdx_queue* queue,
	struct rtdx_texture* src_node,
	u32 src_x,
	u32 src_y,
	u32 src_z,
	struct rtdx_texture* dst_node,
	u32 dst_x,
	u32 dst_y,
	u32 dst_z,
	u32 width,
	u32 height,
	u32 depth
) {
	(void)src_z;
	(void)dst_z;
	if (!src_node || !src_node->d3d_resource || !dst_node || !dst_node->d3d_resource) {
		rtdx_throwf(RT_IMPROPER_USAGE, "texture copy source or destination is invalid");
		return false;
	}
	if (src_node->dxgi_format != dst_node->dxgi_format) {
		rtdx_throwf(RT_UNSUPPORTED_FEATURE, "texture copy requires matching texture formats");
		return false;
	}
	if (src_node->type != RT_TEXTURE_2D || dst_node->type != RT_TEXTURE_2D || depth != 1) {
		rtdx_throwf(RT_UNSUPPORTED_FEATURE, "texture copy currently supports only 2D single-layer textures");
		return false;
	}
	if (src_x > src_node->width || src_y > src_node->height ||
		dst_x > dst_node->width || dst_y > dst_node->height ||
		width == 0 || height == 0 ||
		width > src_node->width - src_x || height > src_node->height - src_y ||
		width > dst_node->width - dst_x || height > dst_node->height - dst_y) {
		rtdx_throwf(RT_IMPROPER_USAGE, "texture copy region is out of bounds");
		return false;
	}

	rtdx_queue_collect(ctx, queue);
	if (!rtdx_queue_acquire_upload_command(ctx, queue)) {
		return false;
	}
	ID3D12GraphicsCommandList* command_list = queue->upload_command_list;
	D3D12_RESOURCE_STATES src_original_state = src_node->state;
	D3D12_RESOURCE_STATES dst_original_state = dst_node->state;

	if (src_original_state != D3D12_RESOURCE_STATE_COPY_SOURCE) {
		D3D12_RESOURCE_BARRIER barrier = {};
		barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
		barrier.Transition.pResource = src_node->d3d_resource;
		barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
		barrier.Transition.StateBefore = src_original_state;
		barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_SOURCE;
		command_list->ResourceBarrier(1, &barrier);
	}
	if (dst_original_state != D3D12_RESOURCE_STATE_COPY_DEST) {
		D3D12_RESOURCE_BARRIER barrier = {};
		barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
		barrier.Transition.pResource = dst_node->d3d_resource;
		barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
		barrier.Transition.StateBefore = dst_original_state;
		barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_DEST;
		command_list->ResourceBarrier(1, &barrier);
	}

	D3D12_TEXTURE_COPY_LOCATION src_location = {};
	src_location.pResource = src_node->d3d_resource;
	src_location.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
	src_location.SubresourceIndex = 0;

	D3D12_TEXTURE_COPY_LOCATION dst_location = {};
	dst_location.pResource = dst_node->d3d_resource;
	dst_location.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
	dst_location.SubresourceIndex = 0;
	command_list->CopyTextureRegion(&dst_location, dst_x, dst_y, 0, &src_location, NULL);

	if (dst_original_state != D3D12_RESOURCE_STATE_COPY_DEST) {
		D3D12_RESOURCE_BARRIER barrier = {};
		barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
		barrier.Transition.pResource = dst_node->d3d_resource;
		barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
		barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
		barrier.Transition.StateAfter = dst_original_state;
		command_list->ResourceBarrier(1, &barrier);
	}
	if (src_original_state != D3D12_RESOURCE_STATE_COPY_SOURCE) {
		D3D12_RESOURCE_BARRIER barrier = {};
		barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
		barrier.Transition.pResource = src_node->d3d_resource;
		barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
		barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_SOURCE;
		barrier.Transition.StateAfter = src_original_state;
		command_list->ResourceBarrier(1, &barrier);
	}

	HRESULT result = command_list->Close();
	if (FAILED(result)) {
		rtdx_throwf(rtdx_error_from_hresult(result), "ID3D12GraphicsCommandList::Close failed: 0x%08x", (u32)result);
		return false;
	}

	ID3D12CommandList* lists[] = { command_list };
	queue->d3d_queue->ExecuteCommandLists(1, lists);
	u64 fence_value = queue->fence_value + 1;
	result = queue->d3d_queue->Signal(queue->d3d_fence, fence_value);
	if (FAILED(result)) {
		rtdx_throwf(rtdx_error_from_hresult(result), "ID3D12CommandQueue::Signal failed: 0x%08x", (u32)result);
		return false;
	}

	queue->fence_value = fence_value;
	queue->upload_fence_value = fence_value;
	return true;
}

void rtdx_texture_init(struct rtdx_context* ctx, struct rtdx_texture* texture) {
	rtdx_init_resource_base(ctx, RTDX_RESOURCE_BASE(texture), rtdx_resource_type::texture);
}

void rtdx_texture_view_init(struct rtdx_context* ctx, struct rtdx_texture_view* view) {
	rtdx_init_resource_base(ctx, RTDX_RESOURCE_BASE(view), rtdx_resource_type::texture_view);
	rtdx_texture_view_normalize_sampler(view);
}

void rtdx_texture_finish(struct rtdx_context* ctx, struct rtdx_texture* texture) {
	rtdx_texture_node_release(texture->active);
	texture->active = NULL;

	struct rtdx_texture* node = texture->next;
	while (node) {
		struct rtdx_texture* next = node->next;
		node->next = NULL;
		rtdx_texture_node_release(node);
		node = next;
	}
	texture->next = NULL;
	rtdx_finish_resource_base(ctx, RTDX_RESOURCE_BASE(texture));
}

void rtdx_texture_view_finish(struct rtdx_context* ctx, struct rtdx_texture_view* view) {
	rtdx_release(&view->d3d_sampler_heap);
	rtdx_release(&view->d3d_rtv_heap);
	rtdx_release(&view->d3d_dsv_heap);
	rtdx_texture_node_release(view->texture);
	view->texture = NULL;
	view->rtv.ptr = 0;
	view->dsv.ptr = 0;
	view->sampler_cpu.ptr = 0;
	view->sampler_gpu.ptr = 0;
	view->d3d_resource = NULL;
	view->dxgi_format = DXGI_FORMAT_UNKNOWN;
	view->state = D3D12_RESOURCE_STATE_COMMON;
	rtdx_finish_resource_base(ctx, RTDX_RESOURCE_BASE(view));
}

static struct rtdx_texture* rtdx_texture_node_create(struct rtdx_context* ctx) {
	struct rtdx_texture* node = RTDX_ALLOC_RESOURCE(struct rtdx_texture);
	if (!node) {
		rtdx_throwf(RT_OUT_OF_HOST_MEMORY, "failed to allocate texture metadata");
		return NULL;
	}

	rtdx_init_resource_base(ctx, RTDX_RESOURCE_BASE(node), rtdx_resource_type::texture);
	node->state = D3D12_RESOURCE_STATE_COMMON;
	return node;
}

void rtdx_texture_node_retain(struct rtdx_texture* texture) {
	if (!texture) {
		return;
	}
	rtdx_resource_retain(RTDX_RESOURCE_BASE(texture));
}

void rtdx_texture_node_release(struct rtdx_texture* texture) {
	if (!texture) {
		return;
	}
	if (rtdx_atomic_dec(&texture->base.ref_count) != 0) {
		return;
	}
	struct rtdx_context* ctx = texture->base.ctx;
	rtdx_release(&texture->d3d_resource);
	rtdx_finish_resource_base(ctx, RTDX_RESOURCE_BASE(texture));
	rtdx_atomic_bool_finish(&texture->base.zombie);
	rtdx_atomic_u32_finish(&texture->base.job_count);
	rtdx_atomic_u32_finish(&texture->base.ref_count);
	RTDX_FREE_RESOURCE(texture);
}

static bool rtdx_texture_view_rebuild_descriptors(struct rtdx_context* ctx, struct rtdx_texture_view* view) {
	if (!view || !view->d3d_resource || view->dxgi_format == DXGI_FORMAT_UNKNOWN) {
		rtdx_throwf(RT_IMPROPER_USAGE, "texture view is invalid");
		return false;
	}

	rtdx_release(&view->d3d_rtv_heap);
	rtdx_release(&view->d3d_dsv_heap);
	view->rtv.ptr = 0;
	view->dsv.ptr = 0;

	if (rtdx_texture_format_is_depth(view->dxgi_format)) {
		D3D12_DESCRIPTOR_HEAP_DESC heap_desc = {};
		heap_desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
		heap_desc.NumDescriptors = 1;
		HRESULT result = ctx->d3d_device->CreateDescriptorHeap(&heap_desc, IID_PPV_ARGS(&view->d3d_dsv_heap));
		if (FAILED(result)) {
			rtdx_throwf(rtdx_error_from_hresult(result), "CreateDescriptorHeap(DSV) failed: 0x%08x", (u32)result);
			return false;
		}
		view->dsv = view->d3d_dsv_heap->GetCPUDescriptorHandleForHeapStart();
		D3D12_DEPTH_STENCIL_VIEW_DESC dsv_desc = {};
		dsv_desc.Format = view->dxgi_format;
		dsv_desc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
		ctx->d3d_device->CreateDepthStencilView(view->d3d_resource, &dsv_desc, view->dsv);
		return true;
	}

	D3D12_DESCRIPTOR_HEAP_DESC heap_desc = {};
	heap_desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
	heap_desc.NumDescriptors = 1;
	HRESULT result = ctx->d3d_device->CreateDescriptorHeap(&heap_desc, IID_PPV_ARGS(&view->d3d_rtv_heap));
	if (FAILED(result)) {
		rtdx_throwf(rtdx_error_from_hresult(result), "CreateDescriptorHeap(RTV) failed: 0x%08x", (u32)result);
		return false;
	}
	view->rtv = view->d3d_rtv_heap->GetCPUDescriptorHandleForHeapStart();
	D3D12_RENDER_TARGET_VIEW_DESC rtv_desc = {};
	rtv_desc.Format = view->dxgi_format;
	rtv_desc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
	ctx->d3d_device->CreateRenderTargetView(view->d3d_resource, &rtv_desc, view->rtv);
	return true;
}

static void rtdx_texture_recycle_node(struct rtdx_texture* texture, struct rtdx_texture* node) {
	if (!node) {
		return;
	}
	node->next = texture->next;
	texture->next = node;
}

static void rtdx_texture_collect_nodes(struct rtdx_texture* texture) {
	struct rtdx_texture** link = &texture->next;
	while (*link) {
		struct rtdx_texture* node = *link;
		if (rtdx_atomic_load(&node->base.ref_count) == 1) {
			*link = node->next;
			node->next = NULL;
			rtdx_texture_node_release(node);
			continue;
		}
		link = &node->next;
	}
}

struct rtdx_texture* rtdx_texture_create_for_swapchain_image(struct rtdx_context* ctx, ID3D12Resource* image, DXGI_FORMAT format, u32 width, u32 height) {
	struct rtdx_texture* texture = rtdx_texture_create(ctx);
	if (!texture) {
		return NULL;
	}

	struct rtdx_texture* node = rtdx_texture_node_create(ctx);
	if (!node) {
		rtdx_texture_destroy(ctx, texture);
		return NULL;
	}

	node->d3d_resource = image;
	node->d3d_resource->AddRef();
	node->dxgi_format = format;
	node->state = D3D12_RESOURCE_STATE_PRESENT;
	node->type = RT_TEXTURE_2D;
	node->width = width;
	node->height = height;
	node->depth = 1;
	texture->active = node;
	return texture;
}

struct rtdx_texture_view* rtdx_texture_view_create_for_texture(struct rtdx_context* ctx, struct rtdx_texture* texture, D3D12_CPU_DESCRIPTOR_HANDLE rtv) {
	struct rtdx_texture* node = texture ? texture->active : NULL;
	if (!node || !node->d3d_resource) {
		rtdx_throwf(RT_IMPROPER_USAGE, "texture view source texture is invalid");
		return NULL;
	}

	struct rtdx_texture_view* view = rtdx_texture_view_create(ctx);
	if (!view) {
		return NULL;
	}

	rtdx_texture_node_retain(node);
	view->texture = node;
	view->d3d_resource = node->d3d_resource;
	view->dxgi_format = node->dxgi_format;
	view->state = node->state;
	view->width = node->width;
	view->height = node->height;
	view->depth = node->depth;
	view->rtv = rtv;
	if (!rtdx_texture_view_rebuild_descriptors(ctx, view)) {
		rtdx_texture_view_destroy(ctx, view);
		return NULL;
	}
	return view;
}

void rtdx_texture_view_bind(struct rtdx_context* ctx, struct rtdx_texture_view* view, struct rtdx_texture* texture) {
	struct rtdx_texture* node = texture ? texture->active : NULL;
	if (!view || !node || !node->d3d_resource) {
		rtdx_throwf(RT_IMPROPER_USAGE, "texture view bind source texture is invalid");
		return;
	}
	if (view->texture == node && view->d3d_resource == node->d3d_resource) {
		return;
	}
	if (view->texture || view->d3d_resource || view->d3d_rtv_heap || view->d3d_dsv_heap || view->d3d_sampler_heap) {
		rtdx_texture_view_destroy(ctx, view);
		rtdx_texture_view_init(ctx, view);
	}
	rtdx_texture_node_retain(node);
	view->texture = node;
	view->d3d_resource = node->d3d_resource;
	view->dxgi_format = node->dxgi_format;
	view->state = node->state;
	view->width = node->width;
	view->height = node->height;
	view->depth = node->depth;
	if (!rtdx_texture_view_rebuild_descriptors(ctx, view)) {
		rtdx_texture_view_destroy(ctx, view);
		rtdx_texture_view_init(ctx, view);
		return;
	}
}

struct rtdx_texture_view* rtdx_texture_view_create_for_swapchain(struct rtdx_context* ctx, struct rtdx_texture* texture, D3D12_CPU_DESCRIPTOR_HANDLE rtv) {
	return rtdx_texture_view_create_for_texture(ctx, texture, rtv);
}

static bool rtdx_texture_view_sampler_valid(struct rtdx_texture_view* texture_view) {
	if (!texture_view) {
		rtdx_throwf(RT_IMPROPER_USAGE, "texture view is NULL");
		return false;
	}
	return true;
}

static bool rtdx_texture_view_prepare_sampler_heap(struct rtdx_context* ctx, struct rtdx_texture_view* texture_view) {
	if (!rtdx_texture_view_sampler_valid(texture_view)) {
		return false;
	}
	if (texture_view->d3d_sampler_heap) {
		return true;
	}

	D3D12_DESCRIPTOR_HEAP_DESC heap_desc = {};
	heap_desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER;
	heap_desc.NumDescriptors = 1;
	heap_desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	HRESULT result = ctx->d3d_device->CreateDescriptorHeap(&heap_desc, IID_PPV_ARGS(&texture_view->d3d_sampler_heap));
	if (FAILED(result)) {
		rtdx_throwf(rtdx_error_from_hresult(result), "CreateDescriptorHeap(sampler) failed: 0x%08x", (u32)result);
		return false;
	}

	texture_view->sampler_cpu = texture_view->d3d_sampler_heap->GetCPUDescriptorHandleForHeapStart();
	texture_view->sampler_gpu.ptr = 0;
	return true;
}

bool rtdx_texture_view_prepare_sampler(struct rtdx_context* ctx, struct rtdx_texture_view* texture_view) {
	bool has_sampler = texture_view && texture_view->d3d_sampler_heap;
	if (!rtdx_texture_view_prepare_sampler_heap(ctx, texture_view)) {
		return false;
	}
	if (has_sampler) {
		return true;
	}
	D3D12_SAMPLER_DESC sampler_desc = rtdx_sampler_desc(texture_view);
	ctx->d3d_device->CreateSampler(&sampler_desc, texture_view->sampler_cpu);
	return true;
}

static bool rtdx_texture_view_recreate_sampler(struct rtdx_texture_view* texture_view) {
	struct rtdx_context* ctx = texture_view->base.ctx;
	if (!rtdx_texture_view_prepare_sampler_heap(ctx, texture_view)) {
		return false;
	}
	D3D12_SAMPLER_DESC sampler_desc = rtdx_sampler_desc(texture_view);
	ctx->d3d_device->CreateSampler(&sampler_desc, texture_view->sampler_cpu);
	return true;
}

void rtdx_texture_view_filter(
	struct rtdx_texture_view* texture_view,
	enum rt_filter mag_filter,
	enum rt_filter min_filter,
	enum rt_mip_filter mip_filter
) {
	if (!rtdx_texture_view_sampler_valid(texture_view)) {
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
	rtdx_texture_view_normalize_sampler(texture_view);
	rtdx_texture_view_recreate_sampler(texture_view);
}

void rtdx_texture_view_address(
	struct rtdx_texture_view* texture_view,
	enum rt_address_mode address_u,
	enum rt_address_mode address_v,
	enum rt_address_mode address_w
) {
	if (!rtdx_texture_view_sampler_valid(texture_view)) {
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
	rtdx_texture_view_normalize_sampler(texture_view);
	rtdx_texture_view_recreate_sampler(texture_view);
}

void rtdx_texture_view_anisotropy(struct rtdx_texture_view* texture_view, u32 max_anisotropy) {
	if (!rtdx_texture_view_sampler_valid(texture_view)) {
		return;
	}
	if (texture_view->max_anisotropy == max_anisotropy) {
		return;
	}
	texture_view->max_anisotropy = max_anisotropy;
	rtdx_texture_view_normalize_sampler(texture_view);
	rtdx_texture_view_recreate_sampler(texture_view);
}

void rtdx_texture_view_lod(
	struct rtdx_texture_view* texture_view,
	f32 min_lod,
	f32 max_lod,
	f32 lod_bias
) {
	if (!rtdx_texture_view_sampler_valid(texture_view)) {
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
	rtdx_texture_view_normalize_sampler(texture_view);
	rtdx_texture_view_recreate_sampler(texture_view);
}

struct rtdx_timepoint rtdx_texture_copy(struct rtdx_context* ctx, struct rtdx_texture* src_texture, u32 src_mip, struct rtdx_texture* dst_texture, u32 dst_mip) {
	struct rtdx_queue* queue = rtdx_texture_upload_queue(ctx);
	struct rtdx_timepoint timepoint = { queue, 0 };
	if (!queue) {
		rtdx_throwf(RT_IMPROPER_USAGE, "texture copy requires a valid queue");
		return timepoint;
	}
	struct rtdx_texture* src_node = src_texture ? src_texture->active : NULL;
	struct rtdx_texture* dst_node = dst_texture ? dst_texture->active : NULL;
	if (!src_node || !dst_node) {
		rtdx_throwf(RT_IMPROPER_USAGE, "texture copy source or destination is invalid");
		return timepoint;
	}
	if (src_mip != 0 || dst_mip != 0) {
		rtdx_throwf(RT_UNSUPPORTED_FEATURE, "texture copy currently supports only mip 0");
		return timepoint;
	}
	if (!rtdx_texture_copy_region(ctx, queue, src_node, 0, 0, 0, dst_node, 0, 0, 0, src_node->width, src_node->height, 1)) {
		return timepoint;
	}
	timepoint.value = queue->fence_value;
	return timepoint;
}

static bool rtdx_texture_upload_staging(struct rtdx_context* ctx, struct rtdx_queue* queue, u64 size) {
	if (queue->upload_buffer && queue->upload_buffer_size >= size) {
		rtdx_timepoint_wait(ctx, { queue, queue->upload_fence_value });
		queue->upload_fence_value = 0;
		return true;
	}

	rtdx_timepoint_wait(ctx, { queue, queue->upload_fence_value });
	queue->upload_fence_value = 0;
	rtdx_release(&queue->upload_buffer);
	queue->upload_buffer_size = 0;

	D3D12_HEAP_PROPERTIES upload_heap = {};
	upload_heap.Type = D3D12_HEAP_TYPE_UPLOAD;
	upload_heap.CreationNodeMask = 1;
	upload_heap.VisibleNodeMask = 1;

	D3D12_RESOURCE_DESC upload_desc = {};
	upload_desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	upload_desc.Width = size;
	upload_desc.Height = 1;
	upload_desc.DepthOrArraySize = 1;
	upload_desc.MipLevels = 1;
	upload_desc.SampleDesc.Count = 1;
	upload_desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

	HRESULT result = ctx->d3d_device->CreateCommittedResource(
		&upload_heap,
		D3D12_HEAP_FLAG_NONE,
		&upload_desc,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		NULL,
		IID_PPV_ARGS(&queue->upload_buffer)
	);
	if (FAILED(result)) {
		rtdx_throwf(rtdx_error_from_hresult(result), "CreateCommittedResource(texture upload) failed: 0x%08x", (u32)result);
		return false;
	}
	queue->upload_buffer_size = size;
	return true;
}

struct rtdx_timepoint rtdx_texture_data(struct rtdx_context* ctx, struct rtdx_texture* texture, enum rt_texture_type type, u32 width, u32 height, u32 depth, u32 mip, enum rt_format format, const void* data) {
	struct rtdx_queue* queue = rtdx_texture_upload_queue(ctx);
	struct rtdx_timepoint timepoint = { queue, 0 };
	assert(queue);
	assert(texture);
	assert(type == RT_TEXTURE_2D);
	assert(mip == 0);
	assert(depth <= 1);
	assert(width != 0);
	assert(height != 0);

	DXGI_FORMAT dxgi_format = rtdx_texture_format(format);
	u32 bytes_per_pixel = rtdx_texture_format_bytes_per_pixel(format);
	if (dxgi_format == DXGI_FORMAT_UNKNOWN || bytes_per_pixel == 0) {
		rtdx_throwf(RT_UNSUPPORTED_FEATURE, "unsupported texture format");
		return timepoint;
	}

	rtdx_queue_collect(ctx, queue);
	rtdx_texture_collect_nodes(texture);
	struct rtdx_texture* node = rtdx_texture_node_create(ctx);
	if (!node) {
		return timepoint;
	}

	D3D12_HEAP_PROPERTIES texture_heap = {};
	texture_heap.Type = D3D12_HEAP_TYPE_DEFAULT;
	texture_heap.CreationNodeMask = 1;
	texture_heap.VisibleNodeMask = 1;

	D3D12_RESOURCE_DESC texture_desc = {};
	texture_desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	texture_desc.Width = width;
	texture_desc.Height = height;
	texture_desc.DepthOrArraySize = 1;
	texture_desc.MipLevels = 1;
	texture_desc.Format = dxgi_format;
	texture_desc.SampleDesc.Count = 1;
	texture_desc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
	if (rtdx_texture_format_is_depth(dxgi_format)) {
		texture_desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
	} else {
		texture_desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
	}

	D3D12_CLEAR_VALUE clear_value = {};
	clear_value.Format = dxgi_format;
	clear_value.DepthStencil.Depth = 1.0f;
	const bool depth_format = rtdx_texture_format_is_depth(dxgi_format);
	const bool has_initial_data = data != NULL;
	D3D12_RESOURCE_STATES initial_state = depth_format ? D3D12_RESOURCE_STATE_DEPTH_WRITE : (has_initial_data ? D3D12_RESOURCE_STATE_COPY_DEST : D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

	HRESULT result = ctx->d3d_device->CreateCommittedResource(
		&texture_heap,
		D3D12_HEAP_FLAG_NONE,
		&texture_desc,
		initial_state,
		depth_format ? &clear_value : NULL,
		IID_PPV_ARGS(&node->d3d_resource)
	);
	if (FAILED(result)) {
		rtdx_texture_node_release(node);
		rtdx_throwf(rtdx_error_from_hresult(result), "CreateCommittedResource(texture) failed: 0x%08x", (u32)result);
		return timepoint;
	}

	node->dxgi_format = dxgi_format;
	node->state = initial_state;
	node->type = type;
	node->width = width;
	node->height = height;
	node->depth = 1;

	if (depth_format && !data) {
		rtdx_texture_recycle_node(texture, texture->active);
		texture->active = node;
		return timepoint;
	}
	if (!data) {
		rtdx_texture_recycle_node(texture, texture->active);
		texture->active = node;
		return timepoint;
	}

	D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint = {};
	u32 row_count = 0;
	u64 row_size = 0;
	u64 upload_size = 0;
	ctx->d3d_device->GetCopyableFootprints(&texture_desc, 0, 1, 0, &footprint, &row_count, &row_size, &upload_size);

	if (data && !rtdx_texture_upload_staging(ctx, queue, upload_size)) {
		rtdx_texture_node_release(node);
		return timepoint;
	}

	if (data) {
		void* mapped = NULL;
		result = queue->upload_buffer->Map(0, NULL, &mapped);
		if (FAILED(result)) {
			rtdx_texture_node_release(node);
			rtdx_throwf(rtdx_error_from_hresult(result), "ID3D12Resource::Map failed: 0x%08x", (u32)result);
			return timepoint;
		}

		const u08* src = (const u08*)data;
		u08* dst = (u08*)mapped;
		u64 packed_pitch = (u64)width * bytes_per_pixel;
		for (u32 y = 0; y < height; y++) {
			memcpy(dst + (usize)y * footprint.Footprint.RowPitch, src + (usize)y * packed_pitch, (usize)packed_pitch);
		}
		queue->upload_buffer->Unmap(0, NULL);
	}

	if (!rtdx_queue_acquire_upload_command(ctx, queue)) {
		rtdx_texture_node_release(node);
		return timepoint;
	}
	ID3D12GraphicsCommandList* command_list = queue->upload_command_list;

	if (data) {
		D3D12_TEXTURE_COPY_LOCATION src = {};
		src.pResource = queue->upload_buffer;
		src.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
		src.PlacedFootprint = footprint;

		D3D12_TEXTURE_COPY_LOCATION dst = {};
		dst.pResource = node->d3d_resource;
		dst.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
		dst.SubresourceIndex = 0;
		command_list->CopyTextureRegion(&dst, 0, 0, 0, &src, NULL);
	}

	D3D12_RESOURCE_BARRIER barrier = {};
	barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	barrier.Transition.pResource = node->d3d_resource;
	barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
	barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
	barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
	command_list->ResourceBarrier(1, &barrier);

	result = command_list->Close();
	if (FAILED(result)) {
		rtdx_texture_node_release(node);
		rtdx_throwf(rtdx_error_from_hresult(result), "ID3D12GraphicsCommandList::Close failed: 0x%08x", (u32)result);
		return timepoint;
	}

	ID3D12CommandList* lists[] = { command_list };
	queue->d3d_queue->ExecuteCommandLists(1, lists);
	u64 fence_value = queue->fence_value + 1;
	result = queue->d3d_queue->Signal(queue->d3d_fence, fence_value);
	if (FAILED(result)) {
		rtdx_texture_node_release(node);
		rtdx_throwf(rtdx_error_from_hresult(result), "ID3D12CommandQueue::Signal failed: 0x%08x", (u32)result);
		return timepoint;
	}

	queue->fence_value = fence_value;
	queue->upload_fence_value = fence_value;
	timepoint.value = fence_value;
	node->state = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;

	rtdx_texture_recycle_node(texture, texture->active);
	texture->active = node;
	return timepoint;
}

struct rtdx_timepoint rtdx_texture_subcopy(struct rtdx_context* ctx, struct rtdx_texture* src_texture, u32 src_mip, u32 src_x, u32 src_y, u32 src_z, struct rtdx_texture* dst_texture, u32 dst_mip, u32 dst_x, u32 dst_y, u32 dst_z, u32 width, u32 height, u32 depth) {
	struct rtdx_queue* queue = rtdx_texture_upload_queue(ctx);
	struct rtdx_timepoint timepoint = { queue, 0 };
	assert(queue);
	struct rtdx_texture* src_node = src_texture ? src_texture->active : NULL;
	struct rtdx_texture* dst_node = dst_texture ? dst_texture->active : NULL;
	assert(src_node);
	assert(dst_node);
	assert(src_mip == 0);
	assert(dst_mip == 0);
	assert(src_z == 0);
	assert(dst_z == 0);
	assert(depth == 1);
	if (!rtdx_texture_copy_region(ctx, queue, src_node, src_x, src_y, src_z, dst_node, dst_x, dst_y, dst_z, width, height, depth)) {
		return timepoint;
	}
	timepoint.value = queue->fence_value;
	return timepoint;
}

struct rtdx_timepoint rtdx_texture_subdata(struct rtdx_context* ctx, struct rtdx_texture* texture, u32 mip, u32 offset_x, u32 offset_y, u32 offset_z, u32 width, u32 height, u32 depth, const void* data) {
	struct rtdx_queue* queue = rtdx_texture_upload_queue(ctx);
	struct rtdx_timepoint timepoint = { queue, 0 };
	assert(queue);
	struct rtdx_texture* node = texture ? texture->active : NULL;
	assert(node && node->d3d_resource);
	assert(mip == 0);
	assert(offset_z == 0);
	assert(depth == 1);
	assert(data);
	assert(width != 0);
	assert(height != 0);
	assert(offset_x <= node->width);
	assert(offset_y <= node->height);
	assert(width <= node->width - offset_x);
	assert(height <= node->height - offset_y);

	u32 bytes_per_pixel = rtdx_texture_view_bytes_per_pixel(node->dxgi_format);
	if (bytes_per_pixel == 0) {
		rtdx_throwf(RT_UNSUPPORTED_FEATURE, "texture subdata does not support this DX12 format");
		return timepoint;
	}

	D3D12_RESOURCE_DESC footprint_desc = {};
	footprint_desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	footprint_desc.Width = width;
	footprint_desc.Height = height;
	footprint_desc.DepthOrArraySize = 1;
	footprint_desc.MipLevels = 1;
	footprint_desc.Format = node->dxgi_format;
	footprint_desc.SampleDesc.Count = 1;
	footprint_desc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;

	D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint = {};
	u32 row_count = 0;
	u64 row_size = 0;
	u64 upload_size = 0;
	ctx->d3d_device->GetCopyableFootprints(&footprint_desc, 0, 1, 0, &footprint, &row_count, &row_size, &upload_size);
	if (!rtdx_texture_upload_staging(ctx, queue, upload_size)) {
		return timepoint;
	}

	void* mapped = NULL;
	HRESULT result = queue->upload_buffer->Map(0, NULL, &mapped);
	if (FAILED(result)) {
		rtdx_throwf(rtdx_error_from_hresult(result), "ID3D12Resource::Map failed: 0x%08x", (u32)result);
		return timepoint;
	}
	const u08* src = (const u08*)data;
	u08* dst = (u08*)mapped;
	u64 packed_pitch = (u64)width * bytes_per_pixel;
	for (u32 y = 0; y < height; ++y) {
		memcpy(dst + (usize)y * footprint.Footprint.RowPitch, src + (usize)y * packed_pitch, (usize)packed_pitch);
	}
	queue->upload_buffer->Unmap(0, NULL);

	if (!rtdx_queue_acquire_upload_command(ctx, queue)) {
		return timepoint;
	}
	ID3D12GraphicsCommandList* command_list = queue->upload_command_list;
	D3D12_RESOURCE_STATES original_state = node->state;
	if (original_state != D3D12_RESOURCE_STATE_COPY_DEST) {
		D3D12_RESOURCE_BARRIER barrier = {};
		barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
		barrier.Transition.pResource = node->d3d_resource;
		barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
		barrier.Transition.StateBefore = original_state;
		barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_DEST;
		command_list->ResourceBarrier(1, &barrier);
	}

	D3D12_TEXTURE_COPY_LOCATION src_location = {};
	src_location.pResource = queue->upload_buffer;
	src_location.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
	src_location.PlacedFootprint = footprint;

	D3D12_TEXTURE_COPY_LOCATION dst_location = {};
	dst_location.pResource = node->d3d_resource;
	dst_location.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
	dst_location.SubresourceIndex = 0;
	command_list->CopyTextureRegion(&dst_location, offset_x, offset_y, 0, &src_location, NULL);

	if (original_state != D3D12_RESOURCE_STATE_COPY_DEST) {
		D3D12_RESOURCE_BARRIER barrier = {};
		barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
		barrier.Transition.pResource = node->d3d_resource;
		barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
		barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
		barrier.Transition.StateAfter = original_state;
		command_list->ResourceBarrier(1, &barrier);
	}

	result = command_list->Close();
	if (FAILED(result)) {
		rtdx_throwf(rtdx_error_from_hresult(result), "ID3D12GraphicsCommandList::Close failed: 0x%08x", (u32)result);
		return timepoint;
	}

	ID3D12CommandList* lists[] = { command_list };
	queue->d3d_queue->ExecuteCommandLists(1, lists);
	u64 fence_value = queue->fence_value + 1;
	result = queue->d3d_queue->Signal(queue->d3d_fence, fence_value);
	if (FAILED(result)) {
		rtdx_throwf(rtdx_error_from_hresult(result), "ID3D12CommandQueue::Signal failed: 0x%08x", (u32)result);
		return timepoint;
	}

	queue->fence_value = fence_value;
	queue->upload_fence_value = fence_value;
	timepoint.value = fence_value;
	node->state = original_state;
	return timepoint;
}

struct rtdx_timepoint rtdx_texture_view_copy_to_buffer(struct rtdx_context* ctx, struct rtdx_texture_view* texture_view, struct rtdx_buffer* buffer) {
	struct rtdx_queue* queue = rtdx_texture_upload_queue(ctx);
	struct rtdx_timepoint timepoint = { queue, 0 };
	assert(queue);
	assert(texture_view && texture_view->d3d_resource);
	assert(buffer);
	assert((buffer->usage & RT_BUFFER_USAGE_TRANSFER_DST) || (buffer->usage & RT_BUFFER_USAGE_STAGING));

	u32 bytes_per_pixel = rtdx_texture_view_bytes_per_pixel(texture_view->dxgi_format);
	if (bytes_per_pixel == 0) {
		rtdx_throwf(RT_UNSUPPORTED_FEATURE, "texture view copy does not support this DX12 format");
		return timepoint;
	}

	u64 packed_size = (u64)texture_view->width * texture_view->height * bytes_per_pixel;
	if (!buffer->storage || buffer->storage->size < packed_size) {
		rtdx_buffer_data(ctx, buffer, buffer->mode, buffer->usage, packed_size, NULL);
	}
	if (!buffer->storage) {
		return timepoint;
	}

	D3D12_RESOURCE_DESC texture_desc = texture_view->d3d_resource->GetDesc();
	D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint = {};
	u32 row_count = 0;
	u64 row_size = 0;
	u64 total_size = 0;
	ctx->d3d_device->GetCopyableFootprints(&texture_desc, 0, 1, 0, &footprint, &row_count, &row_size, &total_size);

	D3D12_HEAP_PROPERTIES heap = {};
	heap.Type = D3D12_HEAP_TYPE_READBACK;
	heap.CreationNodeMask = 1;
	heap.VisibleNodeMask = 1;

	D3D12_RESOURCE_DESC buffer_desc = {};
	buffer_desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	buffer_desc.Width = total_size;
	buffer_desc.Height = 1;
	buffer_desc.DepthOrArraySize = 1;
	buffer_desc.MipLevels = 1;
	buffer_desc.SampleDesc.Count = 1;
	buffer_desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

	ID3D12Resource* readback = NULL;
	HRESULT result = ctx->d3d_device->CreateCommittedResource(
		&heap,
		D3D12_HEAP_FLAG_NONE,
		&buffer_desc,
		D3D12_RESOURCE_STATE_COPY_DEST,
		NULL,
		IID_PPV_ARGS(&readback)
	);
	if (FAILED(result)) {
		rtdx_throwf(rtdx_error_from_hresult(result), "CreateCommittedResource(readback) failed: 0x%08x", (u32)result);
		return timepoint;
	}

	ID3D12CommandAllocator* allocator = NULL;
	result = ctx->d3d_device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&allocator));
	if (FAILED(result)) {
		rtdx_release(&readback);
		rtdx_throwf(rtdx_error_from_hresult(result), "CreateCommandAllocator failed: 0x%08x", (u32)result);
		return timepoint;
	}

	ID3D12GraphicsCommandList* command_list = NULL;
	result = ctx->d3d_device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, allocator, NULL, IID_PPV_ARGS(&command_list));
	if (FAILED(result)) {
		rtdx_release(&allocator);
		rtdx_release(&readback);
		rtdx_throwf(rtdx_error_from_hresult(result), "CreateCommandList failed: 0x%08x", (u32)result);
		return timepoint;
	}

	D3D12_RESOURCE_STATES original_state = texture_view->state;
	if (original_state != D3D12_RESOURCE_STATE_COPY_SOURCE) {
		D3D12_RESOURCE_BARRIER barrier = {};
		barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
		barrier.Transition.pResource = texture_view->d3d_resource;
		barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
		barrier.Transition.StateBefore = original_state;
		barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_SOURCE;
		command_list->ResourceBarrier(1, &barrier);
	}

	D3D12_TEXTURE_COPY_LOCATION dst = {};
	dst.pResource = readback;
	dst.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
	dst.PlacedFootprint = footprint;

	D3D12_TEXTURE_COPY_LOCATION src = {};
	src.pResource = texture_view->d3d_resource;
	src.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
	src.SubresourceIndex = 0;
	command_list->CopyTextureRegion(&dst, 0, 0, 0, &src, NULL);

	if (original_state != D3D12_RESOURCE_STATE_COPY_SOURCE) {
		D3D12_RESOURCE_BARRIER barrier = {};
		barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
		barrier.Transition.pResource = texture_view->d3d_resource;
		barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
		barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_SOURCE;
		barrier.Transition.StateAfter = original_state;
		command_list->ResourceBarrier(1, &barrier);
	}

	result = command_list->Close();
	if (FAILED(result)) {
		rtdx_release(&command_list);
		rtdx_release(&allocator);
		rtdx_release(&readback);
		rtdx_throwf(rtdx_error_from_hresult(result), "ID3D12GraphicsCommandList::Close failed: 0x%08x", (u32)result);
		return timepoint;
	}

	ID3D12CommandList* lists[] = { command_list };
	queue->d3d_queue->ExecuteCommandLists(1, lists);
	u64 fence_value = queue->fence_value + 1;
	result = queue->d3d_queue->Signal(queue->d3d_fence, fence_value);
	if (FAILED(result)) {
		rtdx_release(&command_list);
		rtdx_release(&allocator);
		rtdx_release(&readback);
		rtdx_throwf(rtdx_error_from_hresult(result), "ID3D12CommandQueue::Signal failed: 0x%08x", (u32)result);
		return timepoint;
	}
	queue->fence_value = fence_value;
	rtdx_timepoint_wait(ctx, { queue, fence_value });

	D3D12_RANGE read_range = { 0, (SIZE_T)total_size };
	void* mapped = NULL;
	result = readback->Map(0, &read_range, &mapped);
	if (FAILED(result)) {
		rtdx_release(&command_list);
		rtdx_release(&allocator);
		rtdx_release(&readback);
		rtdx_throwf(rtdx_error_from_hresult(result), "ID3D12Resource::Map failed: 0x%08x", (u32)result);
		return timepoint;
	}

	std::vector<u08> packed((usize)packed_size);
	const u08* src_bytes = (const u08*)mapped;
	for (u32 y = 0; y < texture_view->height; y++) {
		const u08* src_row = src_bytes + (usize)y * footprint.Footprint.RowPitch;
		u08* dst_row = packed.data() + (usize)y * texture_view->width * bytes_per_pixel;
		memcpy(dst_row, src_row, (usize)texture_view->width * bytes_per_pixel);
	}
	D3D12_RANGE write_range = { 0, 0 };
	readback->Unmap(0, &write_range);

	if (rtdx_texture_view_needs_bgra_swizzle(texture_view->dxgi_format)) {
		for (u64 i = 0; i < packed_size; i += 4) {
			u08 tmp = packed[(usize)i + 0];
			packed[(usize)i + 0] = packed[(usize)i + 2];
			packed[(usize)i + 2] = tmp;
		}
	}

	rtdx_buffer_subdata(ctx, buffer, 0, packed_size, packed.data());
	rtdx_release(&command_list);
	rtdx_release(&allocator);
	rtdx_release(&readback);
	return timepoint;
}
