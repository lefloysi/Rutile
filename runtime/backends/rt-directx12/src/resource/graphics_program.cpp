#include "graphics_program.hpp"
#include "context.hpp"
#include "error.hpp"

#include <dxcapi.h>
#include <rtsl/hlsl.hpp>
#include <cassert>
#include <climits>
#include <cstring>
#include <iterator>
#include <span>
#include <string>
#include <string_view>

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
	program->rtsl_program.reset();
	program->vertex_dxil.clear();
	program->fragment_dxil.clear();
	rtdx_finish_resource_base(ctx, RTDX_RESOURCE_BASE(program));
}

static D3D12_SHADER_VISIBILITY rtdx_shader_visibility(rtsl::StageMask stages) {
	if (stages == rtsl::StageMask::vertex) {
		return D3D12_SHADER_VISIBILITY_VERTEX;
	}
	if (stages == rtsl::StageMask::fragment) {
		return D3D12_SHADER_VISIBILITY_PIXEL;
	}
	return D3D12_SHADER_VISIBILITY_ALL;
}

static bool rtdx_graphics_program_create_root_signature(rtdx_context* ctx, rtdx_graphics_program* program) {
	std::vector<D3D12_DESCRIPTOR_RANGE> ranges;
	std::vector<D3D12_ROOT_PARAMETER> parameters;
	ranges.reserve(program->rtsl_program->resources().size() * 2);
	parameters.reserve(program->rtsl_program->resources().size() * 2);
	program->uniform_locations.clear();
	program->uniform_locations.reserve(program->rtsl_program->resources().size());

	for (const rtsl::Resource& resource : program->rtsl_program->resources()) {
		rtdx_uniform_location location = {};
		location.program = program;
		strncpy_s(location.name, resource.name.c_str(), _TRUNCATE);
		location.slot = static_cast<u32>(program->uniform_locations.size());
		location.binding = resource.descriptor.binding;
		const D3D12_SHADER_VISIBILITY visibility = rtdx_shader_visibility(resource.stages);

		if (resource.kind == rtsl::ResourceKind::uniform_buffer) {
			location.kind = rtdx_uniform_location_kind::buffer;
			location.root_parameter = static_cast<u32>(parameters.size());
			D3D12_ROOT_PARAMETER parameter = {};
			parameter.ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
			parameter.Descriptor.ShaderRegister = resource.descriptor.binding;
			parameter.Descriptor.RegisterSpace = resource.descriptor.set;
			parameter.ShaderVisibility = visibility;
			parameters.push_back(parameter);
		} else if (resource.kind == rtsl::ResourceKind::sampled_texture || resource.kind == rtsl::ResourceKind::sampler) {
			location.kind = rtdx_uniform_location_kind::texture;
			location.root_parameter = static_cast<u32>(parameters.size());
			for (const D3D12_DESCRIPTOR_RANGE_TYPE range_type : { D3D12_DESCRIPTOR_RANGE_TYPE_SRV, D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER }) {
				D3D12_DESCRIPTOR_RANGE range = {};
				range.RangeType = range_type;
				range.NumDescriptors = 1;
				range.BaseShaderRegister = resource.descriptor.binding;
				range.RegisterSpace = resource.descriptor.set;
				range.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;
				ranges.push_back(range);

				D3D12_ROOT_PARAMETER parameter = {};
				parameter.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
				parameter.DescriptorTable.NumDescriptorRanges = 1;
				parameter.DescriptorTable.pDescriptorRanges = &ranges.back();
				parameter.ShaderVisibility = visibility;
				parameters.push_back(parameter);
			}
			location.sampler_root_parameter = location.root_parameter + 1;
		} else {
			rtdx_throwf(RT_UNSUPPORTED_FEATURE, "DirectX 12 does not support RTSL resource '%s' yet", resource.name.c_str());
			return false;
		}
		program->uniform_locations.push_back(location);
	}

	D3D12_ROOT_SIGNATURE_DESC desc = {};
	desc.NumParameters = static_cast<UINT>(parameters.size());
	desc.pParameters = parameters.data();
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

static DXGI_FORMAT rtdx_vertex_format(rt_format format) {
	switch (format) {
	case RT_R32_SFLOAT: return DXGI_FORMAT_R32_FLOAT;
	case RT_RG32_SFLOAT: return DXGI_FORMAT_R32G32_FLOAT;
	case RT_RGB32_SFLOAT: return DXGI_FORMAT_R32G32B32_FLOAT;
	case RT_RGBA32_SFLOAT: return DXGI_FORMAT_R32G32B32A32_FLOAT;
	case RT_R32_SINT: return DXGI_FORMAT_R32_SINT;
	case RT_RG32_SINT: return DXGI_FORMAT_R32G32_SINT;
	case RT_RGB32_SINT: return DXGI_FORMAT_R32G32B32_SINT;
	case RT_RGBA32_SINT: return DXGI_FORMAT_R32G32B32A32_SINT;
	case RT_R32_UINT: return DXGI_FORMAT_R32_UINT;
	case RT_RG32_UINT: return DXGI_FORMAT_R32G32_UINT;
	case RT_RGB32_UINT: return DXGI_FORMAT_R32G32B32_UINT;
	case RT_RGBA32_UINT: return DXGI_FORMAT_R32G32B32A32_UINT;
	default: return DXGI_FORMAT_UNKNOWN;
	}
}

static D3D12_BLEND rtdx_blend_factor(rt_blend_factor factor) {
	switch (factor) {
	case RT_BLEND_ZERO: return D3D12_BLEND_ZERO;
	case RT_BLEND_ONE: return D3D12_BLEND_ONE;
	case RT_BLEND_SRC_COLOR: return D3D12_BLEND_SRC_COLOR;
	case RT_BLEND_ONE_MINUS_SRC_COLOR: return D3D12_BLEND_INV_SRC_COLOR;
	case RT_BLEND_DST_COLOR: return D3D12_BLEND_DEST_COLOR;
	case RT_BLEND_ONE_MINUS_DST_COLOR: return D3D12_BLEND_INV_DEST_COLOR;
	case RT_BLEND_SRC_ALPHA: return D3D12_BLEND_SRC_ALPHA;
	case RT_BLEND_ONE_MINUS_SRC_ALPHA: return D3D12_BLEND_INV_SRC_ALPHA;
	case RT_BLEND_DST_ALPHA: return D3D12_BLEND_DEST_ALPHA;
	case RT_BLEND_ONE_MINUS_DST_ALPHA: return D3D12_BLEND_INV_DEST_ALPHA;
	default: return D3D12_BLEND_ZERO;
	}
}

static D3D12_BLEND_OP rtdx_blend_op(rt_blend_op operation) {
	switch (operation) {
	case RT_BLEND_OP_ADD: return D3D12_BLEND_OP_ADD;
	case RT_BLEND_OP_SUBTRACT: return D3D12_BLEND_OP_SUBTRACT;
	case RT_BLEND_OP_REVERSE_SUBTRACT: return D3D12_BLEND_OP_REV_SUBTRACT;
	case RT_BLEND_OP_MIN: return D3D12_BLEND_OP_MIN;
	case RT_BLEND_OP_MAX: return D3D12_BLEND_OP_MAX;
	default: return D3D12_BLEND_OP_ADD;
	}
}

bool rtdx_graphics_program_prepare(
	rtdx_context* ctx,
	rtdx_graphics_program* program,
	DXGI_FORMAT color_format,
	DXGI_FORMAT depth_format
) {
	if (!program || !program->d3d_root_signature) {
		rtdx_throwf(RT_IMPROPER_USAGE, "graphics program must be finalized before use");
		return false;
	}
	if (program->d3d_pipeline && program->d3d_pipeline_format == color_format && program->d3d_pipeline_depth_format == depth_format) {
		return true;
	}
	rtdx_graphics_program_destroy_pipeline(program);

	std::vector<D3D12_INPUT_ELEMENT_DESC> elements;
	elements.reserve(program->vertex_layout.attribute_count);
	const rtsl::EntryPoint* vertex = program->rtsl_program->entry(rtsl::Stage::vertex);
	if (!vertex || (program->vertex_layout.attribute_count && !vertex->input)) {
		rtdx_throwf(RT_SHADER_LINK_FAILED, "RTSL vertex entry does not match the configured vertex layout");
		return false;
	}
	for (u32 index = 0; index < program->vertex_layout.attribute_count; ++index) {
		const rt_vertex_attribute& attribute = program->vertex_attributes[index];
		const DXGI_FORMAT format = rtdx_vertex_format(attribute.format);
		if (format == DXGI_FORMAT_UNKNOWN) {
			rtdx_throwf(RT_UNSUPPORTED_FEATURE, "unsupported vertex attribute format");
			return false;
		}
		auto reflected = vertex->input->elements.end();
		for (auto candidate = vertex->input->elements.begin(); candidate != vertex->input->elements.end(); ++candidate) {
			if (candidate->name == attribute.name) {
				reflected = candidate;
				break;
			}
		}
		if (reflected == vertex->input->elements.end() || !reflected->location) {
			rtdx_throwf(RT_SHADER_LINK_FAILED, "vertex attribute '%s' is not declared by the RTSL entry", attribute.name);
			return false;
		}
		elements.push_back(D3D12_INPUT_ELEMENT_DESC{
			.SemanticName = "TEXCOORD",
			.SemanticIndex = *reflected->location,
			.Format = format,
			.InputSlot = 0,
			.AlignedByteOffset = attribute.offset,
			.InputSlotClass = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,
			.InstanceDataStepRate = 0,
		});
	}

	D3D12_GRAPHICS_PIPELINE_STATE_DESC desc = {};
	desc.pRootSignature = program->d3d_root_signature;
	desc.VS = { program->vertex_dxil.data(), program->vertex_dxil.size() };
	desc.PS = { program->fragment_dxil.data(), program->fragment_dxil.size() };
	desc.BlendState.AlphaToCoverageEnable = FALSE;
	desc.BlendState.IndependentBlendEnable = FALSE;
	D3D12_RENDER_TARGET_BLEND_DESC& blend = desc.BlendState.RenderTarget[0];
	blend.BlendEnable = program->blend_enabled;
	blend.LogicOpEnable = FALSE;
	blend.SrcBlend = rtdx_blend_factor(program->src_color_blend);
	blend.DestBlend = rtdx_blend_factor(program->dst_color_blend);
	blend.BlendOp = rtdx_blend_op(program->color_blend_op);
	blend.SrcBlendAlpha = rtdx_blend_factor(program->src_alpha_blend);
	blend.DestBlendAlpha = rtdx_blend_factor(program->dst_alpha_blend);
	blend.BlendOpAlpha = rtdx_blend_op(program->alpha_blend_op);
	blend.LogicOp = D3D12_LOGIC_OP_NOOP;
	blend.RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
	desc.SampleMask = UINT_MAX;
	desc.RasterizerState.FillMode = program->fill_mode == RT_FILL_WIREFRAME ? D3D12_FILL_MODE_WIREFRAME : D3D12_FILL_MODE_SOLID;
	desc.RasterizerState.CullMode = program->cull_mode == RT_CULL_FRONT ? D3D12_CULL_MODE_FRONT :
		program->cull_mode == RT_CULL_BACK ? D3D12_CULL_MODE_BACK : D3D12_CULL_MODE_NONE;
	desc.RasterizerState.FrontCounterClockwise = program->front_face == RT_FRONT_FACE_CCW;
	desc.RasterizerState.DepthBias = D3D12_DEFAULT_DEPTH_BIAS;
	desc.RasterizerState.DepthBiasClamp = D3D12_DEFAULT_DEPTH_BIAS_CLAMP;
	desc.RasterizerState.SlopeScaledDepthBias = D3D12_DEFAULT_SLOPE_SCALED_DEPTH_BIAS;
	desc.RasterizerState.DepthClipEnable = TRUE;
	desc.DepthStencilState.DepthEnable = depth_format != DXGI_FORMAT_UNKNOWN;
	desc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
	desc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS;
	desc.DepthStencilState.StencilEnable = FALSE;
	desc.DepthStencilState.StencilReadMask = D3D12_DEFAULT_STENCIL_READ_MASK;
	desc.DepthStencilState.StencilWriteMask = D3D12_DEFAULT_STENCIL_WRITE_MASK;
	desc.InputLayout = { elements.data(), static_cast<UINT>(elements.size()) };
	desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	desc.NumRenderTargets = 1;
	desc.RTVFormats[0] = color_format;
	desc.DSVFormat = depth_format;
	desc.SampleDesc.Count = 1;

	const HRESULT result = ctx->d3d_device->CreateGraphicsPipelineState(&desc, IID_PPV_ARGS(&program->d3d_pipeline));
	if (FAILED(result)) {
		rtdx_throwf(rtdx_error_from_hresult(result), "CreateGraphicsPipelineState failed: 0x%08x", static_cast<u32>(result));
		return false;
	}

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
	program->rtsl_program.reset();
	program->vertex_dxil.clear();
	program->fragment_dxil.clear();
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

void rtdx_graphics_program_reset(rtdx_context* /*ctx*/, rtdx_graphics_program* program) {
	if (!program) {
		rtdx_throwf(RT_IMPROPER_USAGE, "graphics program is NULL");
		return;
	}
	rtdx_graphics_program_destroy_root_signature(program);
	program->uniform_locations.clear();
	program->rtsl_program.reset();
	program->vertex_dxil.clear();
	program->fragment_dxil.clear();
}

static bool rtdx_compile_hlsl(std::string_view source, const wchar_t* profile, std::vector<unsigned char>& bytecode) {
	IDxcCompiler3* compiler = nullptr;
	IDxcResult* result = nullptr;
	IDxcBlobUtf8* errors = nullptr;
	IDxcBlob* object = nullptr;

	HRESULT status = DxcCreateInstance(CLSID_DxcCompiler, IID_PPV_ARGS(&compiler));
	if (SUCCEEDED(status)) {
		const DxcBuffer buffer = {
			.Ptr = source.data(),
			.Size = source.size(),
			.Encoding = DXC_CP_UTF8,
		};
		const wchar_t* arguments[] = { L"-E", L"main", L"-T", profile, L"-HV", L"2021", L"-Ges" };
		status = compiler->Compile(&buffer, arguments, static_cast<UINT32>(std::size(arguments)), nullptr, IID_PPV_ARGS(&result));
	}
	HRESULT compile_status = status;
	if (SUCCEEDED(status)) {
		result->GetStatus(&compile_status);
		result->GetOutput(DXC_OUT_ERRORS, IID_PPV_ARGS(&errors), nullptr);
	}
	if (FAILED(status) || FAILED(compile_status)) {
		const char* message = errors && errors->GetStringLength() ? errors->GetStringPointer() : "DXC failed without diagnostics";
		rtdx_throwf(RT_SHADER_LINK_FAILED, "%s", message);
		rtdx_release(&object);
		rtdx_release(&errors);
		rtdx_release(&result);
		rtdx_release(&compiler);
		return false;
	}
	status = result->GetOutput(DXC_OUT_OBJECT, IID_PPV_ARGS(&object), nullptr);
	if (FAILED(status) || !object) {
		rtdx_throwf(RT_SHADER_LINK_FAILED, "DXC did not produce shader bytecode");
		rtdx_release(&errors);
		rtdx_release(&result);
		rtdx_release(&compiler);
		return false;
	}
	const auto* begin = static_cast<const unsigned char*>(object->GetBufferPointer());
	bytecode.assign(begin, begin + object->GetBufferSize());
	rtdx_release(&object);
	rtdx_release(&errors);
	rtdx_release(&result);
	rtdx_release(&compiler);
	return true;
}

void rtdx_graphics_program_finalize(rtdx_context* ctx, rtdx_graphics_program* program) {
	assert(ctx);
	assert(program);
	if (program->program_source.empty()) {
		rtdx_throwf(RT_SHADER_LINK_FAILED, "graphics program finalize requires an RTSLP source set via rtGraphicsProgramSource");
		return;
	}
	auto loaded = rtsl::load_program(std::as_bytes(std::span{ program->program_source }));
	if (!loaded) {
		rtdx_throwf(RT_SHADER_LINK_FAILED, "invalid RTSLP source: %s", loaded.error().message.c_str());
		return;
	}
	auto vertex = rtsl::hlsl::transpile(*loaded, rtsl::Stage::vertex);
	auto fragment = rtsl::hlsl::transpile(*loaded, rtsl::Stage::fragment);
	if (!vertex || !fragment) {
		const rtsl::hlsl::Error& error = !vertex ? vertex.error() : fragment.error();
		rtdx_throwf(RT_SHADER_LINK_FAILED, "RTSL to HLSL failed in %s: %s", error.context.c_str(), error.message.c_str());
		return;
	}
	std::vector<unsigned char> vertex_dxil;
	std::vector<unsigned char> fragment_dxil;
	if (!rtdx_compile_hlsl(vertex->source, L"vs_6_0", vertex_dxil) ||
		!rtdx_compile_hlsl(fragment->source, L"ps_6_0", fragment_dxil)) {
		return;
	}

	rtdx_graphics_program_destroy_root_signature(program);
	program->rtsl_program = std::make_unique<rtsl::Program>(std::move(*loaded));
	program->vertex_dxil = std::move(vertex_dxil);
	program->fragment_dxil = std::move(fragment_dxil);
	if (!rtdx_graphics_program_create_root_signature(ctx, program)) {
		program->rtsl_program.reset();
		program->vertex_dxil.clear();
		program->fragment_dxil.clear();
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
