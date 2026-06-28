#ifndef RTDX_GRAPHICS_PROGRAM_H
#define RTDX_GRAPHICS_PROGRAM_H

#include "config.h"
#include "resource.h"

#include <d3d12.h>
#include <dxgi1_6.h>

#define RTDX_MAX_VERTEX_ATTRIBUTES 16
#define RTDX_MAX_SHADER_BINDINGS 8
#define RTDX_MAX_SHADER_RESOURCE_NAME 64

/*===============================================================================================*/
/*                                                                                               */
/*===============================================================================================*/

RTDX_EXTERN_C_ENTER
RTDX_API rt_graphics_program rtGraphicsProgramCreate(void);
RTDX_API void rtGraphicsProgramDestroy(rt_graphics_program program);
RTDX_API void rtGraphicsProgramLayout(rt_graphics_program program, const rt_vertex_layout* layout);
RTDX_API void rtGraphicsProgramSource(rt_graphics_program program, u64 size, const void* data);
RTDX_API void rtGraphicsProgramRasterState(rt_graphics_program program, enum rt_cull_mode cull_mode, enum rt_front_face front_face, enum rt_fill_mode fill_mode);
RTDX_API void rtGraphicsProgramBlendState(rt_graphics_program program, bool enabled, enum rt_blend_factor src_color, enum rt_blend_factor dst_color, enum rt_blend_op color_op, enum rt_blend_factor src_alpha, enum rt_blend_factor dst_alpha, enum rt_blend_op alpha_op);
RTDX_API void rtGraphicsProgramLink(rt_graphics_program program);
RTDX_API rt_uniform_location rtGraphicsProgramUniformLocation(rt_graphics_program program, const char* name);
RTDX_EXTERN_C_EXIT

/*===============================================================================================*/
/*                                                                                               */
/*===============================================================================================*/

typedef enum rtdx_uniform_location_kind {
	RTDX_UNIFORM_LOCATION_BUFFER,
	RTDX_UNIFORM_LOCATION_STORAGE_BUFFER,
	RTDX_UNIFORM_LOCATION_TEXTURE,
} rtdx_uniform_location_kind;

struct rtdx_uniform_location {
	struct rtdx_graphics_program* program;
	char name[RTDX_MAX_SHADER_RESOURCE_NAME];
	rtdx_uniform_location_kind kind;
	u32 slot;
	u32 root_parameter;
	u32 sampler_root_parameter;
};

struct rtdx_graphics_program {
	struct rtdx_resource_base base;
	ID3D12RootSignature* d3d_root_signature;
	ID3D12PipelineState* d3d_pipeline;

	rt_vertex_layout vertex_layout;
	rt_vertex_attribute vertex_attributes[RTDX_MAX_VERTEX_ATTRIBUTES];
	enum rt_cull_mode cull_mode;
	enum rt_front_face front_face;
	enum rt_fill_mode fill_mode;
	bool blend_enabled;
	enum rt_blend_factor src_color_blend;
	enum rt_blend_factor dst_color_blend;
	enum rt_blend_op color_blend_op;
	enum rt_blend_factor src_alpha_blend;
	enum rt_blend_factor dst_alpha_blend;
	enum rt_blend_op alpha_blend_op;
	DXGI_FORMAT d3d_pipeline_format;
	DXGI_FORMAT d3d_pipeline_depth_format;

	rtdx_uniform_location* uniform_locations;
	u32 uniform_location_count;

	char* program_source;
	u64 program_source_size;
	char* vertex_hlsl;
	u64 vertex_hlsl_size;
	char* fragment_hlsl;
	u64 fragment_hlsl_size;
};
RTDX_DECLARE_NEW_RESOURCE(graphics_program)

bool rtdx_graphics_program_prepare(struct rtdx_context* ctx, struct rtdx_graphics_program* program, DXGI_FORMAT color_format, DXGI_FORMAT depth_format);
void rtdx_graphics_program_layout(struct rtdx_context* ctx, struct rtdx_graphics_program* program, const rt_vertex_layout* layout);
void rtdx_graphics_program_source(struct rtdx_context* ctx, struct rtdx_graphics_program* program, u64 size, const void* data);
void rtdx_graphics_program_raster_state(struct rtdx_context* ctx, struct rtdx_graphics_program* program, enum rt_cull_mode cull_mode, enum rt_front_face front_face, enum rt_fill_mode fill_mode);
void rtdx_graphics_program_blend_state(struct rtdx_context* ctx, struct rtdx_graphics_program* program, bool enabled, enum rt_blend_factor src_color, enum rt_blend_factor dst_color, enum rt_blend_op color_op, enum rt_blend_factor src_alpha, enum rt_blend_factor dst_alpha, enum rt_blend_op alpha_op);
void rtdx_graphics_program_link(struct rtdx_context* ctx, struct rtdx_graphics_program* program);
rt_uniform_location rtdx_graphics_program_uniform_location(struct rtdx_context* ctx, struct rtdx_graphics_program* program, const char* name);

#endif /* RTDX_GRAPHICS_PROGRAM_H */
