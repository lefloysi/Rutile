#pragma once

#include "config.hpp"
#include "resource.hpp"

#include <d3d12.h>
#include <dxgi1_6.h>

RTDX_API rt_texture rtTextureCreate();
RTDX_API void rtTextureDestroy(rt_texture texture);
RTDX_API rt_texture_view rtTextureViewCreate();
RTDX_API void rtTextureViewBind(rt_texture_view texture_view, rt_texture texture);
RTDX_API void rtTextureViewDestroy(rt_texture_view texture_view);
RTDX_API void rtTextureViewFilter(rt_texture_view texture_view, rt_filter mag_filter, rt_filter min_filter, rt_mip_filter mip_filter);
RTDX_API void rtTextureViewAddress(rt_texture_view texture_view, rt_address_mode address_u, rt_address_mode address_v, rt_address_mode address_w);
RTDX_API void rtTextureViewAnisotropy(rt_texture_view texture_view, u32 max_anisotropy);
RTDX_API void rtTextureViewLod(rt_texture_view texture_view, f32 min_lod, f32 max_lod, f32 lod_bias);

RTDX_API rt_timepoint rtTextureCopy(rt_texture src_texture, u32 src_mip, rt_texture dst_texture, u32 dst_mip);
RTDX_API rt_timepoint rtTextureData(rt_texture texture, rt_texture_type type, u32 mip, u32 width, u32 height, u32 depth, rt_format format, const void* data);
RTDX_API rt_timepoint rtTextureSubcopy(rt_texture src_texture, u32 src_mip, u32 src_x, u32 src_y, u32 src_z, rt_texture dst_texture, u32 dst_mip, u32 dst_x, u32 dst_y, u32 dst_z, u32 width, u32 height, u32 depth);
RTDX_API rt_timepoint rtTextureSubdata(rt_texture texture, u32 mip, u32 offset_x, u32 offset_y, u32 offset_z, u32 width, u32 height, u32 depth, const void* data);
RTDX_API rt_timepoint rtTextureViewCopyToBuffer(rt_texture_view texture_view, rt_buffer buffer);
RTDX_API rt_extent_3d rtTextureViewExtent(rt_texture_view texture_view);

struct rtdx_buffer;

struct rtdx_texture {
	rtdx_resource_base base;
	rtdx_texture* active;
	rtdx_texture* next;

	ID3D12Resource* d3d_resource;

	u32 width;
	u32 height;
	u32 depth;
	DXGI_FORMAT dxgi_format;
	D3D12_RESOURCE_STATES state;
	rt_texture_type type;
};
RTDX_DECLARE_NEW_RESOURCE(texture)

struct rtdx_texture_view {
	rtdx_resource_base base;

	rtdx_texture* texture;
	ID3D12Resource* d3d_resource;
	ID3D12DescriptorHeap* d3d_sampler_heap;
	ID3D12DescriptorHeap* d3d_rtv_heap;
	ID3D12DescriptorHeap* d3d_dsv_heap;
	D3D12_CPU_DESCRIPTOR_HANDLE rtv;
	D3D12_CPU_DESCRIPTOR_HANDLE dsv;
	D3D12_CPU_DESCRIPTOR_HANDLE sampler_cpu;
	D3D12_GPU_DESCRIPTOR_HANDLE sampler_gpu;

	u32 width;
	u32 height;
	u32 depth;
	DXGI_FORMAT dxgi_format;
	D3D12_RESOURCE_STATES state;
	rt_filter mag_filter;
	rt_filter min_filter;
	rt_mip_filter mip_filter;
	rt_address_mode address_u;
	rt_address_mode address_v;
	rt_address_mode address_w;
	u32 max_anisotropy;
	f32 min_lod;
	f32 max_lod;
	f32 lod_bias;
};
RTDX_DECLARE_NEW_RESOURCE(texture_view)

rtdx_texture* rtdx_texture_create_for_swapchain_image(rtdx_context* ctx, ID3D12Resource* image, DXGI_FORMAT format, u32 width, u32 height);
rtdx_texture_view* rtdx_texture_view_create_for_texture(rtdx_context* ctx, rtdx_texture* texture, D3D12_CPU_DESCRIPTOR_HANDLE rtv);
void rtdx_texture_view_bind(rtdx_context* ctx, rtdx_texture_view* view, rtdx_texture* texture);
rtdx_texture_view* rtdx_texture_view_create_for_swapchain(rtdx_context* ctx, rtdx_texture* texture, D3D12_CPU_DESCRIPTOR_HANDLE rtv);
void rtdx_texture_node_retain(rtdx_texture* texture);
void rtdx_texture_node_release(rtdx_texture* texture);
void rtdx_texture_view_filter(rtdx_texture_view* texture_view, rt_filter mag_filter, rt_filter min_filter, rt_mip_filter mip_filter);
void rtdx_texture_view_address(rtdx_texture_view* texture_view, rt_address_mode address_u, rt_address_mode address_v, rt_address_mode address_w);
void rtdx_texture_view_anisotropy(rtdx_texture_view* texture_view, u32 max_anisotropy);
void rtdx_texture_view_lod(rtdx_texture_view* texture_view, f32 min_lod, f32 max_lod, f32 lod_bias);
bool rtdx_texture_view_prepare_sampler(rtdx_context* ctx, rtdx_texture_view* texture_view);
bool rtdx_texture_format_is_depth(DXGI_FORMAT format);

rtdx_timepoint rtdx_texture_copy(rtdx_context* ctx, rtdx_texture* src_texture, u32 src_mip, rtdx_texture* dst_texture, u32 dst_mip);
rtdx_timepoint rtdx_texture_data(rtdx_context* ctx, rtdx_texture* texture, rt_texture_type type, u32 width, u32 height, u32 depth, u32 mip, rt_format format, const void* data);
rtdx_timepoint rtdx_texture_subcopy(rtdx_context* ctx, rtdx_texture* src_texture, u32 src_mip, u32 src_x, u32 src_y, u32 src_z, rtdx_texture* dst_texture, u32 dst_mip, u32 dst_x, u32 dst_y, u32 dst_z, u32 width, u32 height, u32 depth);
rtdx_timepoint rtdx_texture_subdata(rtdx_context* ctx, rtdx_texture* texture, u32 mip, u32 offset_x, u32 offset_y, u32 offset_z, u32 width, u32 height, u32 depth, const void* data);
rtdx_timepoint rtdx_texture_view_copy_to_buffer(rtdx_context* ctx, rtdx_texture_view* texture_view, rtdx_buffer* buffer);
