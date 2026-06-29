#include "graphics_program.h"
#include "context.h"
#include "error.h"
#include "shader_compiler.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <d3dcompiler.h>
#include <exception>
#include <new>
#include <stdexcept>
#include <string.h>

/*===============================================================================================*/
/*                                                                                               */
/*===============================================================================================*/

extern "C" {

rt_graphics_program rtGraphicsProgramCreate(void) {
	struct rtdx_graphics_program* program = rtdx_graphics_program_create(rtdx_get_current_context());
	return rtdx_graphics_program_to_handle(program);
}

void rtGraphicsProgramDestroy(rt_graphics_program program) {
	rtdx_graphics_program_destroy(rtdx_get_current_context(), rtdx_graphics_program_from_handle(program));
}

void rtGraphicsProgramLayout(rt_graphics_program program, const rt_vertex_layout* layout) {
	rtdx_graphics_program_layout(rtdx_get_current_context(), rtdx_graphics_program_from_handle(program), layout);
}

void rtGraphicsProgramSource(rt_graphics_program program, u64 size, const void* data) {
	rtdx_graphics_program_source(rtdx_get_current_context(), rtdx_graphics_program_from_handle(program), size, data);
}

void rtGraphicsProgramRasterState(rt_graphics_program program, enum rt_cull_mode cull_mode, enum rt_front_face front_face, enum rt_fill_mode fill_mode) {
	rtdx_graphics_program_raster_state(rtdx_get_current_context(), rtdx_graphics_program_from_handle(program), cull_mode, front_face, fill_mode);
}

void rtGraphicsProgramBlendState(
	rt_graphics_program program,
	bool enabled,
	enum rt_blend_factor src_color,
	enum rt_blend_factor dst_color,
	enum rt_blend_op color_op,
	enum rt_blend_factor src_alpha,
	enum rt_blend_factor dst_alpha,
	enum rt_blend_op alpha_op
) {
	rtdx_graphics_program_blend_state(rtdx_get_current_context(), rtdx_graphics_program_from_handle(program), enabled, src_color, dst_color, color_op, src_alpha, dst_alpha, alpha_op);
}

void rtGraphicsProgramFinalize(rt_graphics_program program) {
	rtdx_graphics_program_finalize(rtdx_get_current_context(), rtdx_graphics_program_from_handle(program));
}

void rtGraphicsProgramReset(rt_graphics_program program) {
	rtdx_graphics_program_reset(rtdx_get_current_context(), rtdx_graphics_program_from_handle(program));
}

rt_uniform_location rtGraphicsProgramUniformLocation(rt_graphics_program program, const char* name) {
	return rtdx_graphics_program_uniform_location(rtdx_get_current_context(), rtdx_graphics_program_from_handle(program), name);
}
}

/*===============================================================================================*/
/*                                                                                               */
/*===============================================================================================*/

RTDX_DEFINE_RESOURCE_PRIVATE(graphics_program)

void rtdx_graphics_program_init(struct rtdx_context* ctx, struct rtdx_graphics_program* program) {
	rtdx_init_resource_base(ctx, RTDX_RESOURCE_BASE(program), RT_RESOURCE_GRAPHICS_PROGRAM);
	program->cull_mode = RT_CULL_NONE;
	program->front_face = RT_FRONT_FACE_CCW;
	program->fill_mode = RT_FILL_SOLID;
	program->blend_enabled = false;
	program->src_color_blend = RT_BLEND_ONE;
	program->dst_color_blend = RT_BLEND_ZERO;
	program->color_blend_op = RT_BLEND_OP_ADD;
	program->src_alpha_blend = RT_BLEND_ONE;
	program->dst_alpha_blend = RT_BLEND_ZERO;
	program->alpha_blend_op = RT_BLEND_OP_ADD;
}

static void rtdx_graphics_program_destroy_pipeline(struct rtdx_graphics_program* program) {
	if (program->d3d_pipeline) {
		rtdx_release(&program->d3d_pipeline);
		program->d3d_pipeline = NULL;
	}
	program->d3d_pipeline_format = DXGI_FORMAT_UNKNOWN;
	program->d3d_pipeline_depth_format = DXGI_FORMAT_UNKNOWN;
}

static void rtdx_graphics_program_destroy_root_signature(struct rtdx_graphics_program* program) {
	rtdx_graphics_program_destroy_pipeline(program);
	if (program->d3d_root_signature) {
		rtdx_release(&program->d3d_root_signature);
		program->d3d_root_signature = NULL;
	}
}

static void rtdx_graphics_program_clear_translation(struct rtdx_graphics_program* program) {
	free(program->vertex_hlsl);
	free(program->fragment_hlsl);
	free(program->uniform_locations);
	program->vertex_hlsl = NULL;
	program->fragment_hlsl = NULL;
	program->uniform_locations = NULL;
	program->vertex_hlsl_size = 0;
	program->fragment_hlsl_size = 0;
	program->uniform_location_count = 0;
}

static void rtdx_graphics_program_clear_sources(struct rtdx_graphics_program* program) {
	free(program->program_source);
	program->program_source = NULL;
	program->program_source_size = 0;
}

void rtdx_graphics_program_finish(struct rtdx_context* ctx, struct rtdx_graphics_program* program) {
	rtdx_graphics_program_destroy_root_signature(program);
	rtdx_graphics_program_clear_translation(program);
	rtdx_graphics_program_clear_sources(program);
	rtdx_finish_resource_base(ctx, RTDX_RESOURCE_BASE(program));
}

static D3D12_CULL_MODE rtdx_cull_mode(enum rt_cull_mode mode);
static D3D12_FILL_MODE rtdx_fill_mode(enum rt_fill_mode mode);
static D3D12_BLEND rtdx_blend_factor(enum rt_blend_factor factor);
static D3D12_BLEND_OP rtdx_blend_op(enum rt_blend_op op);

static DXGI_FORMAT rtdx_vertex_format(enum rt_format format) {
	switch (format) {
	case RT_R32_SFLOAT:
		return DXGI_FORMAT_R32_FLOAT;
	case RT_RG32_SFLOAT:
		return DXGI_FORMAT_R32G32_FLOAT;
	case RT_RGB32_SFLOAT:
		return DXGI_FORMAT_R32G32B32_FLOAT;
	case RT_RGBA32_SFLOAT:
		return DXGI_FORMAT_R32G32B32A32_FLOAT;
	case RT_R32_SINT:
		return DXGI_FORMAT_R32_SINT;
	case RT_RG32_SINT:
		return DXGI_FORMAT_R32G32_SINT;
	case RT_RGB32_SINT:
		return DXGI_FORMAT_R32G32B32_SINT;
	case RT_RGBA32_SINT:
		return DXGI_FORMAT_R32G32B32A32_SINT;
	case RT_R32_UINT:
		return DXGI_FORMAT_R32_UINT;
	case RT_RG32_UINT:
		return DXGI_FORMAT_R32G32_UINT;
	case RT_RGB32_UINT:
		return DXGI_FORMAT_R32G32B32_UINT;
	case RT_RGBA32_UINT:
		return DXGI_FORMAT_R32G32B32A32_UINT;
	default:
		return DXGI_FORMAT_UNKNOWN;
	}
}

static bool rtdx_compile_shader(
	struct rtdx_graphics_program* program,
	bool vertex_stage,
	const char* entry,
	const char* target,
	ID3DBlob** out_blob
) {
	UINT flags = D3DCOMPILE_ENABLE_STRICTNESS;
#if defined(_DEBUG)
	flags |= D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#endif

	const char* shader_source = vertex_stage ? program->vertex_hlsl : program->fragment_hlsl;
	u64 shader_size = vertex_stage ? program->vertex_hlsl_size : program->fragment_hlsl_size;
	if (!shader_source || shader_size == 0) {
		rtdx_throwf(RT_SHADER_COMPILATION_FAILED, "graphics program has no translated %s shader", vertex_stage ? "vertex" : "fragment");
		return false;
	}
	ID3DBlob* errors = NULL;
	HRESULT result = D3DCompile(
		shader_source,
		shader_size,
		"rt-dx12-shader",
		NULL,
		NULL,
		entry,
		target,
		flags,
		0,
		out_blob,
		&errors
	);
	if (FAILED(result)) {
		const char* message = errors ? (const char*)errors->GetBufferPointer() : "unknown shader compiler error";
		rtdx_throwf(RT_SHADER_COMPILATION_FAILED, "%s", message);
		rtdx_release(&errors);
		return false;
	}

	rtdx_release(&errors);
	return true;
}

static bool rtdx_graphics_program_create_root_signature(struct rtdx_context* ctx, struct rtdx_graphics_program* program) {
	D3D12_DESCRIPTOR_RANGE srv_ranges[RTDX_MAX_SHADER_BINDINGS] = {};
	D3D12_DESCRIPTOR_RANGE sampler_ranges[RTDX_MAX_SHADER_BINDINGS] = {};
	for (u32 i = 0; i < RTDX_MAX_SHADER_BINDINGS; ++i) {
		srv_ranges[i].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
		srv_ranges[i].NumDescriptors = 1;
		srv_ranges[i].BaseShaderRegister = i;
		srv_ranges[i].RegisterSpace = 0;
		srv_ranges[i].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

		sampler_ranges[i].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER;
		sampler_ranges[i].NumDescriptors = 1;
		sampler_ranges[i].BaseShaderRegister = i;
		sampler_ranges[i].RegisterSpace = 0;
		sampler_ranges[i].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;
	}

	D3D12_ROOT_PARAMETER parameters[RTDX_MAX_SHADER_BINDINGS * 3] = {};
	for (u32 i = 0; i < RTDX_MAX_SHADER_BINDINGS; ++i) {
		parameters[i].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
		parameters[i].Descriptor.ShaderRegister = i;
		parameters[i].Descriptor.RegisterSpace = 0;
		parameters[i].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

		u32 srv_parameter = RTDX_MAX_SHADER_BINDINGS + i;
		parameters[srv_parameter].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
		parameters[srv_parameter].DescriptorTable.NumDescriptorRanges = 1;
		parameters[srv_parameter].DescriptorTable.pDescriptorRanges = &srv_ranges[i];
		parameters[srv_parameter].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

		u32 sampler_parameter = RTDX_MAX_SHADER_BINDINGS * 2 + i;
		parameters[sampler_parameter].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
		parameters[sampler_parameter].DescriptorTable.NumDescriptorRanges = 1;
		parameters[sampler_parameter].DescriptorTable.pDescriptorRanges = &sampler_ranges[i];
		parameters[sampler_parameter].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	}

	D3D12_ROOT_SIGNATURE_DESC desc = {};
	desc.NumParameters = RTDX_MAX_SHADER_BINDINGS * 3;
	desc.pParameters = parameters;
	desc.NumStaticSamplers = 0;
	desc.pStaticSamplers = NULL;
	desc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

	ID3DBlob* signature = NULL;
	ID3DBlob* errors = NULL;
	HRESULT result = D3D12SerializeRootSignature(&desc, D3D_ROOT_SIGNATURE_VERSION_1, &signature, &errors);
	if (FAILED(result)) {
		const char* message = errors ? (const char*)errors->GetBufferPointer() : "root signature serialization failed";
		rtdx_throwf(RT_INITIALIZATION_FAILED, "%s", message);
		rtdx_release(&errors);
		return false;
	}

	result = ctx->d3d_device->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(&program->d3d_root_signature));
	rtdx_release(&signature);
	rtdx_release(&errors);
	if (FAILED(result)) {
		rtdx_throwf(rtdx_error_from_hresult(result), "CreateRootSignature failed: 0x%08x", (u32)result);
		return false;
	}
	return true;
}

static bool rtdx_graphics_program_create_pipeline(
	struct rtdx_context* ctx,
	struct rtdx_graphics_program* program,
	DXGI_FORMAT color_format,
	DXGI_FORMAT depth_format
) {
	ID3DBlob* vertex_shader = NULL;
	if (!rtdx_compile_shader(program, true, "main", "vs_5_0", &vertex_shader)) {
		return false;
	}

	ID3DBlob* pixel_shader = NULL;
	if (!rtdx_compile_shader(program, false, "main", "ps_5_0", &pixel_shader)) {
		rtdx_release(&vertex_shader);
		return false;
	}

	D3D12_INPUT_ELEMENT_DESC elements[RTDX_MAX_VERTEX_ATTRIBUTES] = {};
	for (u32 i = 0; i < program->vertex_layout.attribute_count; i++) {
		DXGI_FORMAT format = rtdx_vertex_format(program->vertex_attributes[i].format);
		if (format == DXGI_FORMAT_UNKNOWN) {
			rtdx_throwf(RT_UNSUPPORTED_FEATURE, "unsupported vertex attribute format");
			rtdx_release(&vertex_shader);
			rtdx_release(&pixel_shader);
			return false;
		}
		elements[i].SemanticName = program->vertex_attributes[i].name ? program->vertex_attributes[i].name : "ATTR";
		elements[i].SemanticIndex = 0;
		elements[i].Format = format;
		elements[i].InputSlot = 0;
		elements[i].AlignedByteOffset = program->vertex_attributes[i].offset;
		elements[i].InputSlotClass = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA;
		elements[i].InstanceDataStepRate = 0;
	}

	D3D12_GRAPHICS_PIPELINE_STATE_DESC pipeline_info = {};
	pipeline_info.pRootSignature = program->d3d_root_signature;
	pipeline_info.VS = { vertex_shader->GetBufferPointer(), vertex_shader->GetBufferSize() };
	pipeline_info.PS = { pixel_shader->GetBufferPointer(), pixel_shader->GetBufferSize() };
	pipeline_info.BlendState.RenderTarget[0].BlendEnable = program->blend_enabled;
	pipeline_info.BlendState.RenderTarget[0].SrcBlend = rtdx_blend_factor(program->src_color_blend);
	pipeline_info.BlendState.RenderTarget[0].DestBlend = rtdx_blend_factor(program->dst_color_blend);
	pipeline_info.BlendState.RenderTarget[0].BlendOp = rtdx_blend_op(program->color_blend_op);
	pipeline_info.BlendState.RenderTarget[0].SrcBlendAlpha = rtdx_blend_factor(program->src_alpha_blend);
	pipeline_info.BlendState.RenderTarget[0].DestBlendAlpha = rtdx_blend_factor(program->dst_alpha_blend);
	pipeline_info.BlendState.RenderTarget[0].BlendOpAlpha = rtdx_blend_op(program->alpha_blend_op);
	pipeline_info.BlendState.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
	pipeline_info.SampleMask = UINT_MAX;
	pipeline_info.RasterizerState.FillMode = rtdx_fill_mode(program->fill_mode);
	pipeline_info.RasterizerState.CullMode = rtdx_cull_mode(program->cull_mode);
	pipeline_info.RasterizerState.FrontCounterClockwise = program->front_face == RT_FRONT_FACE_CCW;
	pipeline_info.RasterizerState.DepthBias = D3D12_DEFAULT_DEPTH_BIAS;
	pipeline_info.RasterizerState.DepthBiasClamp = D3D12_DEFAULT_DEPTH_BIAS_CLAMP;
	pipeline_info.RasterizerState.SlopeScaledDepthBias = D3D12_DEFAULT_SLOPE_SCALED_DEPTH_BIAS;
	pipeline_info.RasterizerState.DepthClipEnable = TRUE;
	pipeline_info.DepthStencilState.DepthEnable = depth_format != DXGI_FORMAT_UNKNOWN;
	pipeline_info.DepthStencilState.DepthWriteMask = pipeline_info.DepthStencilState.DepthEnable
														 ? D3D12_DEPTH_WRITE_MASK_ALL
														 : D3D12_DEPTH_WRITE_MASK_ZERO;
	pipeline_info.DepthStencilState.DepthFunc = pipeline_info.DepthStencilState.DepthEnable
													? D3D12_COMPARISON_FUNC_LESS
													: D3D12_COMPARISON_FUNC_ALWAYS;
	pipeline_info.DepthStencilState.StencilEnable = FALSE;
	pipeline_info.DepthStencilState.StencilReadMask = D3D12_DEFAULT_STENCIL_READ_MASK;
	pipeline_info.DepthStencilState.StencilWriteMask = D3D12_DEFAULT_STENCIL_WRITE_MASK;
	pipeline_info.DepthStencilState.FrontFace.StencilFailOp = D3D12_STENCIL_OP_KEEP;
	pipeline_info.DepthStencilState.FrontFace.StencilDepthFailOp = D3D12_STENCIL_OP_KEEP;
	pipeline_info.DepthStencilState.FrontFace.StencilPassOp = D3D12_STENCIL_OP_KEEP;
	pipeline_info.DepthStencilState.FrontFace.StencilFunc = D3D12_COMPARISON_FUNC_ALWAYS;
	pipeline_info.DepthStencilState.BackFace = pipeline_info.DepthStencilState.FrontFace;
	pipeline_info.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	pipeline_info.NumRenderTargets = 1;
	pipeline_info.RTVFormats[0] = color_format;
	pipeline_info.DSVFormat = depth_format;
	pipeline_info.SampleDesc.Count = 1;
	pipeline_info.SampleDesc.Quality = 0;
	if (program->vertex_layout.attribute_count > 0) {
		pipeline_info.InputLayout = { elements, program->vertex_layout.attribute_count };
	} else {
		pipeline_info.InputLayout = { NULL, 0 };
	}

	HRESULT result = ctx->d3d_device->CreateGraphicsPipelineState(&pipeline_info, IID_PPV_ARGS(&program->d3d_pipeline));
	rtdx_release(&vertex_shader);
	rtdx_release(&pixel_shader);
	if (FAILED(result)) {
		rtdx_throwf(rtdx_error_from_hresult(result), "CreateGraphicsPipelineState failed: 0x%08x", (u32)result);
		return false;
	}

	program->d3d_pipeline_format = color_format;
	program->d3d_pipeline_depth_format = depth_format;
	return true;
}

bool rtdx_graphics_program_prepare(
	struct rtdx_context* ctx,
	struct rtdx_graphics_program* program,
	DXGI_FORMAT color_format,
	DXGI_FORMAT depth_format
) {
	if (!program || !program->d3d_root_signature) {
		rtdx_throwf(RT_IMPROPER_USAGE, "graphics program must be finalized before use");
		return false;
	}

	if (program->d3d_pipeline && program->d3d_pipeline_format == color_format &&
		program->d3d_pipeline_depth_format == depth_format) {
		return true;
	}

	rtdx_graphics_program_destroy_pipeline(program);

	return rtdx_graphics_program_create_pipeline(ctx, program, color_format, depth_format);
}

void rtdx_graphics_program_layout(struct rtdx_context* ctx, struct rtdx_graphics_program* program, const rt_vertex_layout* layout) {
	if (!program) {
		rtdx_throwf(RT_IMPROPER_USAGE, "graphics program is NULL");
		return;
	}
	if (!layout || !layout->attributes || layout->attribute_count == 0) {
		program->vertex_layout = {};
		rtdx_graphics_program_destroy_pipeline(program);
		return;
	}
	if (layout->attribute_count > RTDX_MAX_VERTEX_ATTRIBUTES) {
		rtdx_throwf(RT_IMPROPER_USAGE, "too many vertex attributes");
		return;
	}
	if (layout->stride == 0) {
		rtdx_throwf(RT_IMPROPER_USAGE, "vertex layout stride is zero");
		return;
	}

	memcpy(program->vertex_attributes, layout->attributes, sizeof(layout->attributes[0]) * layout->attribute_count);
	program->vertex_layout.stride = layout->stride;
	program->vertex_layout.attributes = program->vertex_attributes;
	program->vertex_layout.attribute_count = layout->attribute_count;
	rtdx_graphics_program_destroy_pipeline(program);
}

static bool rtdx_graphics_program_set_source(char** dst, u64* dst_size, u64 size, const void* data) {
	free(*dst);
	*dst = NULL;
	*dst_size = 0;
	if (!data || size == 0) {
		return true;
	}
	char* copy = (char*)malloc((size_t)size);
	if (!copy) {
		rtdx_throwf(RT_OUT_OF_HOST_MEMORY, "failed to allocate shader source copy");
		return false;
	}
	memcpy(copy, data, (size_t)size);
	*dst = copy;
	*dst_size = size;
	return true;
}

void rtdx_graphics_program_source(struct rtdx_context* ctx, struct rtdx_graphics_program* program, u64 size, const void* data) {
	(void)ctx;
	if (!program) {
		rtdx_throwf(RT_IMPROPER_USAGE, "graphics program is NULL");
		return;
	}
	if (!rtdx_graphics_program_set_source(&program->program_source, &program->program_source_size, size, data)) {
		return;
	}
	rtdx_graphics_program_clear_translation(program);
	rtdx_graphics_program_destroy_root_signature(program);
}

void rtdx_graphics_program_raster_state(
	struct rtdx_context* ctx,
	struct rtdx_graphics_program* program,
	enum rt_cull_mode cull_mode,
	enum rt_front_face front_face,
	enum rt_fill_mode fill_mode
) {
	if (!program) {
		rtdx_throwf(RT_IMPROPER_USAGE, "graphics program is NULL");
		return;
	}

	program->cull_mode = cull_mode;
	program->front_face = front_face;
	program->fill_mode = fill_mode;
	rtdx_graphics_program_destroy_pipeline(program);
}

void rtdx_graphics_program_blend_state(
	struct rtdx_context* ctx,
	struct rtdx_graphics_program* program,
	bool enabled,
	enum rt_blend_factor src_color,
	enum rt_blend_factor dst_color,
	enum rt_blend_op color_op,
	enum rt_blend_factor src_alpha,
	enum rt_blend_factor dst_alpha,
	enum rt_blend_op alpha_op
) {
	if (!program) {
		rtdx_throwf(RT_IMPROPER_USAGE, "graphics program is NULL");
		return;
	}

	program->blend_enabled = enabled;
	program->src_color_blend = src_color;
	program->dst_color_blend = dst_color;
	program->color_blend_op = color_op;
	program->src_alpha_blend = src_alpha;
	program->dst_alpha_blend = dst_alpha;
	program->alpha_blend_op = alpha_op;
	rtdx_graphics_program_destroy_pipeline(program);
}

static bool rtdx_graphics_program_translate_rtslp(struct rtdx_graphics_program* program) {
	try {
		rtdx_graphics_program_clear_translation(program);
		rtdx_graphics_hlsl_compile_result translated =
			rtdx_shader_compile_graphics_rtslp(program->program_source_size, program->program_source);

		program->vertex_hlsl = translated.vertex_hlsl;
		program->vertex_hlsl_size = translated.vertex_hlsl_size;
		program->fragment_hlsl = translated.fragment_hlsl;
		program->fragment_hlsl_size = translated.fragment_hlsl_size;
		// Ownership transferred; the destructor will free anything still set.
		translated.vertex_hlsl = NULL;
		translated.fragment_hlsl = NULL;

		if (translated.uniform_count > 0) {
			program->uniform_locations = static_cast<rtdx_uniform_location*>(
				std::calloc(translated.uniform_count, sizeof(rtdx_uniform_location))
			);
			if (!program->uniform_locations) {
				rtdx_graphics_hlsl_result_clear(&translated);
				throw std::bad_alloc();
			}
			for (u32 i = 0; i < translated.uniform_count; ++i) {
				const auto& info = translated.uniforms[i];
				if (info.binding >= RTDX_MAX_SHADER_BINDINGS) {
					rtdx_graphics_hlsl_result_clear(&translated);
					throw std::runtime_error("uniform binding exceeds dx12 backend limit");
				}
				auto& loc = program->uniform_locations[i];
				loc.program = program;
				std::snprintf(loc.name, sizeof(loc.name), "%s", info.name);
				loc.slot = info.binding;
				if (info.kind == RTSL_HLSL_UNIFORM_TEXTURE_SAMPLED) {
					loc.kind = RTDX_UNIFORM_LOCATION_TEXTURE;
					loc.root_parameter = RTDX_MAX_SHADER_BINDINGS + info.binding;
					loc.sampler_root_parameter = RTDX_MAX_SHADER_BINDINGS * 2 + info.binding;
				} else {
					loc.kind = RTDX_UNIFORM_LOCATION_BUFFER;
					loc.root_parameter = info.binding;
					loc.sampler_root_parameter = 0;
				}
			}
			program->uniform_location_count = translated.uniform_count;
		}
		rtdx_graphics_hlsl_result_clear(&translated);
		return true;
	} catch (const std::bad_alloc&) {
		rtdx_throwf(RT_OUT_OF_HOST_MEMORY, "shader translation ran out of memory");
		return false;
	} catch (const std::exception& error) {
		rtdx_throwf(RT_SHADER_COMPILATION_FAILED, "%s", error.what());
		return false;
	} catch (...) {
		rtdx_throwf(RT_SHADER_COMPILATION_FAILED, "unknown shader translation failure");
		return false;
	}
}

void rtdx_graphics_program_reset(struct rtdx_context* ctx, struct rtdx_graphics_program* program) {
	(void)ctx;
	if (!program) {
		rtdx_throwf(RT_IMPROPER_USAGE, "graphics program is NULL");
		return;
	}
	rtdx_graphics_program_destroy_root_signature(program);
	rtdx_graphics_program_clear_translation(program);
}

void rtdx_graphics_program_finalize(struct rtdx_context* ctx, struct rtdx_graphics_program* program) {
	if (!program) {
		rtdx_throwf(RT_IMPROPER_USAGE, "graphics program is NULL");
		return;
	}
	if (!program->program_source || program->program_source_size == 0) {
		rtdx_throwf(RT_SHADER_LINK_FAILED, "graphics program finalize requires an RTSLP source set via rtGraphicsProgramSource");
		return;
	}
	if (!rtdx_graphics_program_translate_rtslp(program)) {
		return;
	}
	if (!program->d3d_root_signature && !rtdx_graphics_program_create_root_signature(ctx, program)) {
		return;
	}
}

static D3D12_CULL_MODE rtdx_cull_mode(enum rt_cull_mode mode) {
	switch (mode) {
	case RT_CULL_NONE:
		return D3D12_CULL_MODE_NONE;
	case RT_CULL_FRONT:
		return D3D12_CULL_MODE_FRONT;
	case RT_CULL_BACK:
		return D3D12_CULL_MODE_BACK;
	default:
		return D3D12_CULL_MODE_NONE;
	}
}

static D3D12_FILL_MODE rtdx_fill_mode(enum rt_fill_mode mode) {
	switch (mode) {
	case RT_FILL_SOLID:
		return D3D12_FILL_MODE_SOLID;
	case RT_FILL_WIREFRAME:
		return D3D12_FILL_MODE_WIREFRAME;
	default:
		return D3D12_FILL_MODE_SOLID;
	}
}

static D3D12_BLEND rtdx_blend_factor(enum rt_blend_factor factor) {
	switch (factor) {
	case RT_BLEND_ZERO:
		return D3D12_BLEND_ZERO;
	case RT_BLEND_ONE:
		return D3D12_BLEND_ONE;
	case RT_BLEND_SRC_COLOR:
		return D3D12_BLEND_SRC_COLOR;
	case RT_BLEND_ONE_MINUS_SRC_COLOR:
		return D3D12_BLEND_INV_SRC_COLOR;
	case RT_BLEND_DST_COLOR:
		return D3D12_BLEND_DEST_COLOR;
	case RT_BLEND_ONE_MINUS_DST_COLOR:
		return D3D12_BLEND_INV_DEST_COLOR;
	case RT_BLEND_SRC_ALPHA:
		return D3D12_BLEND_SRC_ALPHA;
	case RT_BLEND_ONE_MINUS_SRC_ALPHA:
		return D3D12_BLEND_INV_SRC_ALPHA;
	case RT_BLEND_DST_ALPHA:
		return D3D12_BLEND_DEST_ALPHA;
	case RT_BLEND_ONE_MINUS_DST_ALPHA:
		return D3D12_BLEND_INV_DEST_ALPHA;
	default:
		return D3D12_BLEND_ONE;
	}
}

static D3D12_BLEND_OP rtdx_blend_op(enum rt_blend_op op) {
	switch (op) {
	case RT_BLEND_OP_ADD:
		return D3D12_BLEND_OP_ADD;
	case RT_BLEND_OP_SUBTRACT:
		return D3D12_BLEND_OP_SUBTRACT;
	case RT_BLEND_OP_REVERSE_SUBTRACT:
		return D3D12_BLEND_OP_REV_SUBTRACT;
	case RT_BLEND_OP_MIN:
		return D3D12_BLEND_OP_MIN;
	case RT_BLEND_OP_MAX:
		return D3D12_BLEND_OP_MAX;
	default:
		return D3D12_BLEND_OP_ADD;
	}
}

rt_uniform_location rtdx_graphics_program_uniform_location(struct rtdx_context* ctx, struct rtdx_graphics_program* program, const char* name) {
	if (!program) {
		rtdx_throwf(RT_IMPROPER_USAGE, "graphics program is NULL");
		return RT_NULL_HANDLE;
	}
	if (!name) {
		rtdx_throwf(RT_IMPROPER_USAGE, "uniform location name is NULL");
		return RT_NULL_HANDLE;
	}
	if (!program->d3d_root_signature) {
		rtdx_throwf(RT_IMPROPER_USAGE, "graphics program must be finalized before querying uniforms");
		return RT_NULL_HANDLE;
	}
	for (u32 i = 0; i < program->uniform_location_count; i++) {
		if (strcmp(name, program->uniform_locations[i].name) == 0) {
			return (rt_uniform_location)&program->uniform_locations[i];
		}
	}
	return RT_NULL_HANDLE;
}
