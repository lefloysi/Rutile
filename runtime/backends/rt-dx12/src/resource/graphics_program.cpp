#include "graphics_program.hpp"
#include "context.hpp"
#include "error.hpp"
#include "rtsl-hlsl.hpp"

#include <cassert>
#include <cstddef>
#include <cstdio>
#include <cstring>
#include <span>

rt_graphics_program rtGraphicsProgramCreate() {
	return rtdx_graphics_program_to_handle(rtdx_graphics_program_create(rtdx_get_current_context()));
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

void rtGraphicsProgramRasterState(rt_graphics_program program, rt_cull_mode cull_mode, rt_front_face front_face, rt_fill_mode fill_mode) {
	rtdx_graphics_program_raster_state(rtdx_get_current_context(), rtdx_graphics_program_from_handle(program), cull_mode, front_face, fill_mode);
}

void rtGraphicsProgramBlendState(
	rt_graphics_program program,
	bool enabled,
	rt_blend_factor src_color,
	rt_blend_factor dst_color,
	rt_blend_op color_op,
	rt_blend_factor src_alpha,
	rt_blend_factor dst_alpha,
	rt_blend_op alpha_op
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

RTDX_DEFINE_RESOURCE_PRIVATE(graphics_program)

void rtdx_graphics_program_init(rtdx_context* ctx, rtdx_graphics_program* program) {
	rtdx_init_resource_base(ctx, RTDX_RESOURCE_BASE(program), rtdx_resource_type::graphics_program);
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

static void rtdx_graphics_program_destroy_pipeline(rtdx_graphics_program* program) {
	rtdx_release(&program->d3d_pipeline);
	program->d3d_pipeline_format = DXGI_FORMAT_UNKNOWN;
	program->d3d_pipeline_depth_format = DXGI_FORMAT_UNKNOWN;
}

static void rtdx_graphics_program_destroy_root_signature(rtdx_graphics_program* program) {
	rtdx_graphics_program_destroy_pipeline(program);
	rtdx_release(&program->d3d_root_signature);
}

void rtdx_graphics_program_finish(rtdx_context* ctx, rtdx_graphics_program* program) {
	rtdx_graphics_program_destroy_root_signature(program);
	program->uniform_locations.clear();
	program->program_source.clear();
	rtdx_finish_resource_base(ctx, RTDX_RESOURCE_BASE(program));
}

static bool rtdx_graphics_program_create_root_signature(rtdx_context* ctx, rtdx_graphics_program* program) {
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
	desc.pStaticSamplers = nullptr;
	desc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

	ID3DBlob* signature = nullptr;
	ID3DBlob* errors = nullptr;
	HRESULT result = D3D12SerializeRootSignature(&desc, D3D_ROOT_SIGNATURE_VERSION_1, &signature, &errors);
	if (FAILED(result)) {
		const char* message = errors ? static_cast<const char*>(errors->GetBufferPointer()) : "root signature serialization failed";
		rtdx_throwf(RT_INITIALIZATION_FAILED, "%s", message);
		rtdx_release(&errors);
		return false;
	}

	result = ctx->d3d_device->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(&program->d3d_root_signature));
	rtdx_release(&signature);
	rtdx_release(&errors);
	if (FAILED(result)) {
		rtdx_throwf(rtdx_error_from_hresult(result), "CreateRootSignature failed: 0x%08x", static_cast<u32>(result));
		return false;
	}
	return true;
}

bool rtdx_graphics_program_prepare(
	rtdx_context* /*ctx*/,
	rtdx_graphics_program* program,
	DXGI_FORMAT color_format,
	DXGI_FORMAT depth_format
) {
	if (!program || !program->d3d_root_signature) {
		rtdx_throwf(RT_IMPROPER_USAGE, "graphics program must be finalized before use");
		return false;
	}

	// TODO: real pipeline creation lives behind the upcoming HLSL -> DXBC step.
	// For now the placeholder transpiler returns dummy HLSL and we skip pipeline
	// creation entirely, so draws using this program will be no-ops.
	program->d3d_pipeline_format = color_format;
	program->d3d_pipeline_depth_format = depth_format;
	return true;
}

void rtdx_graphics_program_layout(rtdx_context* /*ctx*/, rtdx_graphics_program* program, const rt_vertex_layout* layout) {
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

	std::memcpy(program->vertex_attributes, layout->attributes, sizeof(layout->attributes[0]) * layout->attribute_count);
	program->vertex_layout.stride = layout->stride;
	program->vertex_layout.attributes = program->vertex_attributes;
	program->vertex_layout.attribute_count = layout->attribute_count;
	rtdx_graphics_program_destroy_pipeline(program);
}

void rtdx_graphics_program_source(rtdx_context* /*ctx*/, rtdx_graphics_program* program, u64 size, const void* data) {
	if (!program) {
		rtdx_throwf(RT_IMPROPER_USAGE, "graphics program is NULL");
		return;
	}
	const auto* bytes = static_cast<const unsigned char*>(data);
	program->program_source.assign(bytes, bytes + size);
	program->uniform_locations.clear();
	rtdx_graphics_program_destroy_root_signature(program);
}

void rtdx_graphics_program_raster_state(
	rtdx_context* /*ctx*/,
	rtdx_graphics_program* program,
	rt_cull_mode cull_mode,
	rt_front_face front_face,
	rt_fill_mode fill_mode
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
	rtdx_context* /*ctx*/,
	rtdx_graphics_program* program,
	bool enabled,
	rt_blend_factor src_color,
	rt_blend_factor dst_color,
	rt_blend_op color_op,
	rt_blend_factor src_alpha,
	rt_blend_factor dst_alpha,
	rt_blend_op alpha_op
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

static void rtdx_graphics_program_translate_rtslp(rtdx_graphics_program* program) {
	const std::span source{
		reinterpret_cast<const std::byte*>(program->program_source.data()),
		program->program_source.size(),
	};
	rtdx_rtsl_program translated = rtdx_rtsl_program_load(source);
	if (!translated.module) {
		rtdx_throwf(RT_SHADER_COMPILATION_FAILED, "failed to load RTSL program");
		return;
	}
	const rtdx_rtsl_translation reflection = rtdx_rtsl_program_translate(translated);

	program->uniform_locations.clear();
	const size_t uniform_count = reflection.uniforms.size();
	program->uniform_locations.reserve(uniform_count);
	for (size_t i = 0; i < uniform_count; ++i) {
		const rtsl_uniform_info& info = reflection.uniforms[i];
		if (info.binding >= RTDX_MAX_SHADER_BINDINGS) {
			rtdx_throwf(RT_SHADER_COMPILATION_FAILED, "uniform binding exceeds dx12 backend limit");
			program->uniform_locations.clear();
			rtdx_rtsl_program_destroy(translated);
			return;
		}
		rtdx_uniform_location loc{};
		loc.program = program;
		std::snprintf(loc.name, sizeof(loc.name), "%s", info.qualified_name ? info.qualified_name : "");
		loc.slot = info.binding;
		if (info.kind == RTSL_UNIFORM_KIND_UNIFORM_BUFFER) {
			loc.kind = rtdx_uniform_location_kind::buffer;
			loc.root_parameter = info.binding;
			loc.sampler_root_parameter = 0;
		} else if (info.kind == RTSL_UNIFORM_KIND_STORAGE_BUFFER) {
			loc.kind = rtdx_uniform_location_kind::storage_buffer;
			loc.root_parameter = RTDX_MAX_SHADER_BINDINGS + info.binding;
			loc.sampler_root_parameter = 0;
		} else if (info.kind == RTSL_UNIFORM_KIND_SAMPLED_IMAGE) {
			loc.kind = rtdx_uniform_location_kind::texture;
			loc.root_parameter = RTDX_MAX_SHADER_BINDINGS + info.binding;
			loc.sampler_root_parameter = RTDX_MAX_SHADER_BINDINGS * 2 + info.binding;
		} else {
			rtdx_throwf(RT_SHADER_COMPILATION_FAILED, "unsupported RTSL uniform kind in DX12 backend");
			program->uniform_locations.clear();
			rtdx_rtsl_program_destroy(translated);
			return;
		}
		program->uniform_locations.push_back(loc);
	}
	rtdx_rtsl_program_destroy(translated);
}

void rtdx_graphics_program_reset(rtdx_context* /*ctx*/, rtdx_graphics_program* program) {
	if (!program) {
		rtdx_throwf(RT_IMPROPER_USAGE, "graphics program is NULL");
		return;
	}
	rtdx_graphics_program_destroy_root_signature(program);
	program->uniform_locations.clear();
}

void rtdx_graphics_program_finalize(rtdx_context* ctx, rtdx_graphics_program* program) {
	assert(ctx);
	assert(program);
	if (program->program_source.empty()) {
		rtdx_throwf(RT_SHADER_LINK_FAILED, "graphics program finalize requires an RTSLP source set via rtGraphicsProgramSource");
		return;
	}
	rtdx_graphics_program_translate_rtslp(program);
	if (rtError() != RT_SUCCESS) {
		return;
	}
	if (!program->d3d_root_signature && !rtdx_graphics_program_create_root_signature(ctx, program)) {
		return;
	}
}

rt_uniform_location rtdx_graphics_program_uniform_location(rtdx_context* /*ctx*/, rtdx_graphics_program* program, const char* name) {
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
	for (rtdx_uniform_location& loc : program->uniform_locations) {
		if (std::strcmp(name, loc.name) == 0) {
			return reinterpret_cast<rt_uniform_location>(&loc);
		}
	}
	return RT_NULL_HANDLE;
}
