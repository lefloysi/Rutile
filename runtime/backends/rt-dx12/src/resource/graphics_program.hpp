#pragma once

#include "config.hpp"
#include "resource.hpp"

#include <d3d12.h>
#include <dxgi1_6.h>
#include <vector>

inline constexpr u32 RTDX_MAX_VERTEX_ATTRIBUTES = 16;
inline constexpr u32 RTDX_MAX_SHADER_BINDINGS = 8;
inline constexpr u32 RTDX_MAX_SHADER_RESOURCE_NAME = 64;

struct rtdx_graphics_program;

RTDX_API rt_graphics_program rtGraphicsProgramCreate();
RTDX_API void rtGraphicsProgramDestroy(rt_graphics_program program);
RTDX_API void rtGraphicsProgramLayout(rt_graphics_program program, const rt_vertex_layout* layout);
RTDX_API void rtGraphicsProgramSource(rt_graphics_program program, u64 size, const void* data);
RTDX_API void rtGraphicsProgramRasterState(rt_graphics_program program, rt_cull_mode cull_mode, rt_front_face front_face, rt_fill_mode fill_mode);
RTDX_API void rtGraphicsProgramBlendState(rt_graphics_program program, bool enabled, rt_blend_factor src_color, rt_blend_factor dst_color, rt_blend_op color_op, rt_blend_factor src_alpha, rt_blend_factor dst_alpha, rt_blend_op alpha_op);
RTDX_API void rtGraphicsProgramFinalize(rt_graphics_program program);
RTDX_API void rtGraphicsProgramReset(rt_graphics_program program);
RTDX_API rt_uniform_location rtGraphicsProgramUniformLocation(rt_graphics_program program, const char* name);

enum class rtdx_uniform_location_kind {
	buffer,
	storage_buffer,
	texture,
};

struct rtdx_uniform_location {
	rtdx_graphics_program* program;
	char name[RTDX_MAX_SHADER_RESOURCE_NAME];
	rtdx_uniform_location_kind kind;
	u32 slot;
	u32 root_parameter;
	u32 sampler_root_parameter;
};

struct rtdx_graphics_program {
	rtdx_resource_base base;
	ID3D12RootSignature* d3d_root_signature;
	ID3D12PipelineState* d3d_pipeline;

	rt_vertex_layout vertex_layout;
	rt_vertex_attribute vertex_attributes[RTDX_MAX_VERTEX_ATTRIBUTES];
	rt_cull_mode cull_mode;
	rt_front_face front_face;
	rt_fill_mode fill_mode;
	bool blend_enabled;
	rt_blend_factor src_color_blend;
	rt_blend_factor dst_color_blend;
	rt_blend_op color_blend_op;
	rt_blend_factor src_alpha_blend;
	rt_blend_factor dst_alpha_blend;
	rt_blend_op alpha_blend_op;
	DXGI_FORMAT d3d_pipeline_format;
	DXGI_FORMAT d3d_pipeline_depth_format;

	std::vector<rtdx_uniform_location> uniform_locations;
	std::vector<unsigned char> program_source;
};
RTDX_DECLARE_NEW_RESOURCE(graphics_program)

bool rtdx_graphics_program_prepare(rtdx_context* ctx, rtdx_graphics_program* program, DXGI_FORMAT color_format, DXGI_FORMAT depth_format);
void rtdx_graphics_program_layout(rtdx_context* ctx, rtdx_graphics_program* program, const rt_vertex_layout* layout);
void rtdx_graphics_program_source(rtdx_context* ctx, rtdx_graphics_program* program, u64 size, const void* data);
void rtdx_graphics_program_raster_state(rtdx_context* ctx, rtdx_graphics_program* program, rt_cull_mode cull_mode, rt_front_face front_face, rt_fill_mode fill_mode);
void rtdx_graphics_program_blend_state(rtdx_context* ctx, rtdx_graphics_program* program, bool enabled, rt_blend_factor src_color, rt_blend_factor dst_color, rt_blend_op color_op, rt_blend_factor src_alpha, rt_blend_factor dst_alpha, rt_blend_op alpha_op);
void rtdx_graphics_program_finalize(rtdx_context* ctx, rtdx_graphics_program* program);
void rtdx_graphics_program_reset(rtdx_context* ctx, rtdx_graphics_program* program);
rt_uniform_location rtdx_graphics_program_uniform_location(rtdx_context* ctx, rtdx_graphics_program* program, const char* name);
