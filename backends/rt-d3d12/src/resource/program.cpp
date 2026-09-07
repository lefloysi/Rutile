#include "program.hpp"
#include "context.hpp"
#include "error.hpp"
#include "transpiler/hlsl.hpp"

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <climits>
#include <cstring>
#include <dxcapi.h>
#include <iterator>
#include <optional>
#include <rtsl/IR/Verifier.hpp>
#include <rtsl/Serialization/Artifact.hpp>
#include <span>
#include <string>
#include <string_view>

/*===============================================================================================*/
/*                                                                                               */
/*===============================================================================================*/

rt_program_t* rtProgramCreate() {
	rtd3d12_begin_errorable_operation();
	return rtd3d12::create_resource<rt_program_t>(rtd3d12_get_current_context());
}

void rtProgramDestroy(rt_program_t* program) {
	if (program) {
		(program)->retire();
	}
}

void rtProgramSetLayout(rt_program_t* program, const rt::vertex_layout* layout) {
	rtd3d12_begin_errorable_operation();
	program->layout(layout);
}

void rtProgramSource(rt_program_t* program, const char* entry_point, const u08* data, usize size) {
	rtd3d12_begin_errorable_operation();
	program->source(entry_point, data, size);
}

void rtProgramSetRasterState(rt_program_t* program, rt::cull_mode cull_mode, rt::front_face front_face, rt::fill_mode fill_mode) {
	rtd3d12_begin_errorable_operation();
	program->raster_state(cull_mode, front_face, fill_mode);
}

void rtProgramSetBlendState(rt_program_t* program, bool enabled, rt::blend_factor src_color, rt::blend_factor dst_color, rt::blend_op color_op, rt::blend_factor src_alpha, rt::blend_factor dst_alpha, rt::blend_op alpha_op) {
	rtd3d12_begin_errorable_operation();
	program->blend_state(enabled, src_color, dst_color, color_op, src_alpha, dst_alpha, alpha_op);
}

void rtProgramFinalize(rt_program_t* program) {
	rtd3d12_begin_errorable_operation();
	program->finalize();
}

rt::location* rtProgramUniformLocation(rt_program_t* program, const char* name) {
	rtd3d12_begin_errorable_operation();
	return program->uniform_location(name);
}

rt::location* rtProgramInputLocation(rt_program_t* program, const rt::vertex_attribute* attributes, usize attribute_count) {
	rtd3d12_begin_errorable_operation();
	return program->input_location(attributes, attribute_count);
}

rt::location* rtProgramOutputLocation(rt_program_t* program, const char* name) {
	rtd3d12_begin_errorable_operation();
	return program->output_location(name);
}

rt_program_t* rtd3d12_location_program(const rt::location* location) {
	if (!location) return nullptr;
	const rt::location* locations = location - location->address;
	return reinterpret_cast<rt_program_t*>(reinterpret_cast<std::byte*>(const_cast<rt::location*>(locations)) - offsetof(rt_program_t, locations));
}

/*===============================================================================================*/
/*                                                                                               */
/*===============================================================================================*/

rt_program_t::rt_program_t(rtd3d12_context* context)
	: rtd3d12_resource(context),
	  d3d_root_signature(nullptr),
	  d3d_pipeline(nullptr),
	  vertex_layout{},
	  vertex_inputs{},
	  vertex_attributes{},
	  vertex_attribute_inputs{},
	  cull_mode(rt::cull_mode::none),
	  front_face(rt::front_face::counter_clockwise),
	  fill_mode(rt::fill_mode::solid),
	  blend_enabled(false),
	  src_color_blend(rt::blend_factor::one),
	  dst_color_blend(rt::blend_factor::zero),
	  color_blend_op(rt::blend_op::add),
	  src_alpha_blend(rt::blend_factor::one),
	  dst_alpha_blend(rt::blend_factor::zero),
	  alpha_blend_op(rt::blend_op::add),
	  d3d_pipeline_format(DXGI_FORMAT_UNKNOWN),
	  d3d_pipeline_depth_format(DXGI_FORMAT_UNKNOWN),
	  d3d_primitive_topology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST),
	  d3d_compute_pipeline(false),
	  locations{},
	  input_mappings{},
	  output_mappings{},
	  descriptor_mappings{},
	  uniform_data_mappings{},
	  storage_data_mappings{},
	  location_allocated{},
	  location_count(0) {}

void rt_program_t::destroy_pipeline() {
	if (d3d_pipeline) {
		d3d_pipeline->Release();
		d3d_pipeline = nullptr;
	}
	d3d_pipeline_format = DXGI_FORMAT_UNKNOWN;
	d3d_pipeline_depth_format = DXGI_FORMAT_UNKNOWN;
	d3d_compute_pipeline = false;
}

void rt_program_t::destroy_root_signature() {
	destroy_pipeline();
	if (d3d_root_signature) {
		d3d_root_signature->Release();
		d3d_root_signature = nullptr;
	}
}

rt_program_t::~rt_program_t() {
	destroy_root_signature();
	entry_point.clear();
	rtsl_artifact.reset();
	shaders.clear();
}

void rt_program_t::clear_mappings() {
	std::memset(locations, 0, sizeof(locations));
	input_mappings.fill(std::nullopt);
	output_mappings.fill(std::nullopt);
	descriptor_mappings.fill(std::nullopt);
	uniform_data_mappings.fill(std::nullopt);
	storage_data_mappings.fill(std::nullopt);
	location_allocated.fill(false);
	location_count = 0;
}

rt::location* rt_program_t::allocate_location(bool zero_address) {
	if (location_count == std::size(locations)) {
		rtd3d12_fail(rt::error::shader_link_failed, "program exposes more than 256 locations");
		return nullptr;
	}
	for (u32 address = zero_address ? 0u : 1u; address < std::size(locations); ++address) {
		if (location_allocated[address]) {
			continue;
		}
		locations[address].address = static_cast<u08>(address);
		location_allocated[address] = true;
		++location_count;
		return &locations[address];
	}
	if (zero_address) {
		rtd3d12_fail(rt::error::shader_link_failed, "program location zero is already mapped");
	} else {
		rtd3d12_fail(rt::error::shader_link_failed, "program has no free nonzero locations");
	}
	return nullptr;
}

static std::optional<u32> rtd3d12_type_byte_size(const rtsl::ir::Module& module, rtsl::ir::TypeId type_id) {
	const rtsl::ir::Type* type = module.findType(type_id);
	if (!type) {
		return std::nullopt;
	}
	switch (type->kind) {
	case rtsl::ir::TypeKind::type_boolean:
	case rtsl::ir::TypeKind::type_signed_integer:
	case rtsl::ir::TypeKind::type_unsigned_integer:
	case rtsl::ir::TypeKind::type_floating:
		return type->bit_width / 8u;
	case rtsl::ir::TypeKind::type_vector: {
		const std::optional<u32> element = rtd3d12_type_byte_size(module, type->element_type);
		return element ? std::optional<u32>{ *element * type->element_count } : std::nullopt;
	}
	case rtsl::ir::TypeKind::type_matrix: {
		const std::optional<u32> column = rtd3d12_type_byte_size(module, type->element_type);
		return column ? std::optional<u32>{ *column * type->element_count } : std::nullopt;
	}
	case rtsl::ir::TypeKind::type_structure: {
		u32 size = 0;
		for (const rtsl::ir::StructMember& member : type->members) {
			const std::optional<u32> member_size = rtd3d12_type_byte_size(module, member.type);
			if (!member_size) {
				return std::nullopt;
			}
			size += *member_size;
		}
		return size;
	}
	default:
		return std::nullopt;
	}
}

struct rtd3d12_storage_layout { u32 alignment; u32 size; };

static std::optional<rtd3d12_storage_layout> rtd3d12_storage_type_layout(const rtsl::ir::Module& module, rtsl::ir::TypeId type_id) {
	const rtsl::ir::Type* type = module.findType(type_id);
	if (!type) return std::nullopt;
	switch (type->kind) {
	case rtsl::ir::TypeKind::type_boolean:
	case rtsl::ir::TypeKind::type_signed_integer:
	case rtsl::ir::TypeKind::type_unsigned_integer:
	case rtsl::ir::TypeKind::type_floating: return rtd3d12_storage_layout{4, 4};
	case rtsl::ir::TypeKind::type_vector: {
		auto element = rtd3d12_storage_type_layout(module, type->element_type);
		if (!element) return std::nullopt;
		if (type->element_count == 2) return rtd3d12_storage_layout{8, element->size * 2};
		if (type->element_count == 3 || type->element_count == 4) return rtd3d12_storage_layout{16, 16};
		return std::nullopt;
	}
	case rtsl::ir::TypeKind::type_matrix: return rtd3d12_storage_layout{16, 16 * type->element_count};
	case rtsl::ir::TypeKind::type_array: {
		auto element = rtd3d12_storage_type_layout(module, type->element_type);
		if (!element) return std::nullopt;
		const u32 stride = (element->size + 15u) & ~u32(15u);
		return rtd3d12_storage_layout{16, stride * type->element_count};
	}
	case rtsl::ir::TypeKind::type_structure: {
		u32 alignment = 16;
		u32 offset{};
		for (const rtsl::ir::StructMember& member : type->members) {
			auto layout = rtd3d12_storage_type_layout(module, member.type);
			if (!layout) return std::nullopt;
			alignment = (std::max)(alignment, layout->alignment);
			offset = (offset + layout->alignment - 1u) / layout->alignment * layout->alignment;
			offset += layout->size;
		}
		offset = (offset + alignment - 1u) / alignment * alignment;
		return rtd3d12_storage_layout{alignment, offset};
	}
	default: return std::nullopt;
	}
}

static bool rtd3d12_program_create_root_signature(rtd3d12_context* ctx, rt_program_t* program) {
	const rtsl::ir::Module& module = program->rtsl_artifact->module;
	std::vector<D3D12_DESCRIPTOR_RANGE> ranges;
	std::vector<D3D12_ROOT_PARAMETER> parameters;
	/* A storage texture contributes UAV, SRV, and sampler ranges. Root
	 * parameters retain pointers into this vector until serialization, so it
	 * must not reallocate while the signature is assembled. */
	ranges.reserve(module.resources.size() * 3 + module.uniforms.size() + 1);
	parameters.reserve(module.resources.size() * 3 + module.uniforms.size() + 1);
	program->clear_mappings();

	u32 next_resource_binding = 0;
	for (const rtsl::ir::Resource& resource : module.resources) {
		rtd3d12_program_descriptor_mapping mapping = {};
		const rtsl::ir::Symbol* symbol = module.findSymbol(resource.symbol);
		mapping.name = symbol ? std::string(module.strings.get(symbol->fully_qualified_name)) : std::string{};
		mapping.binding = resource.binding ? resource.binding->binding : next_resource_binding++;
		const u32 space = resource.binding ? resource.binding->set : 0;
		const D3D12_SHADER_VISIBILITY visibility = D3D12_SHADER_VISIBILITY_ALL;

		if (resource.kind == rtsl::ir::ResourceKind::resource_uniform_buffer) {
			mapping.type = rtd3d12_descriptor_type::constant_buffer;
			mapping.root_parameter = static_cast<u32>(parameters.size());
			D3D12_DESCRIPTOR_RANGE range = {};
			range.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_CBV;
			range.NumDescriptors = 1;
			range.BaseShaderRegister = mapping.binding;
			range.RegisterSpace = space;
			range.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;
			ranges.push_back(range);
			D3D12_ROOT_PARAMETER parameter = {};
			parameter.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
			parameter.DescriptorTable.NumDescriptorRanges = 1;
			parameter.DescriptorTable.pDescriptorRanges = &ranges.back();
			parameter.ShaderVisibility = visibility;
			parameters.push_back(parameter);
		} else if (resource.kind == rtsl::ir::ResourceKind::resource_sampled_texture || resource.kind == rtsl::ir::ResourceKind::resource_sampler) {
			mapping.type = rtd3d12_descriptor_type::texture;
			mapping.root_parameter = static_cast<u32>(parameters.size());
			for (const D3D12_DESCRIPTOR_RANGE_TYPE range_type : { D3D12_DESCRIPTOR_RANGE_TYPE_SRV, D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER }) {
				D3D12_DESCRIPTOR_RANGE range = {};
				range.RangeType = range_type;
				range.NumDescriptors = 1;
				range.BaseShaderRegister = mapping.binding;
				range.RegisterSpace = space;
				range.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;
				ranges.push_back(range);

				D3D12_ROOT_PARAMETER parameter = {};
				parameter.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
				parameter.DescriptorTable.NumDescriptorRanges = 1;
				parameter.DescriptorTable.pDescriptorRanges = &ranges.back();
				parameter.ShaderVisibility = visibility;
				parameters.push_back(parameter);
			}
			mapping.sampler_root_parameter = mapping.root_parameter + 1;
		} else if (resource.kind == rtsl::ir::ResourceKind::resource_storage_buffer) {
			mapping.type = rtd3d12_descriptor_type::storage_buffer;
			mapping.storage_stride = 0;
			mapping.root_parameter = static_cast<u32>(parameters.size());
			D3D12_DESCRIPTOR_RANGE range = {};
			range.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
			range.NumDescriptors = 1;
			range.BaseShaderRegister = mapping.binding;
			range.RegisterSpace = space;
			range.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;
			ranges.push_back(range);

			D3D12_ROOT_PARAMETER parameter = {};
			parameter.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
			parameter.DescriptorTable.NumDescriptorRanges = 1;
			parameter.DescriptorTable.pDescriptorRanges = &ranges.back();
			parameter.ShaderVisibility = visibility;
			parameters.push_back(parameter);
		} else if (resource.kind == rtsl::ir::ResourceKind::resource_storage_texture) {
			mapping.type = rtd3d12_descriptor_type::storage_texture;
			mapping.root_parameter = static_cast<u32>(parameters.size());
			for (const D3D12_DESCRIPTOR_RANGE_TYPE range_type : { D3D12_DESCRIPTOR_RANGE_TYPE_UAV, D3D12_DESCRIPTOR_RANGE_TYPE_SRV, D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER }) {
				D3D12_DESCRIPTOR_RANGE range = {};
				range.RangeType = range_type;
				range.NumDescriptors = 1;
				range.BaseShaderRegister = mapping.binding;
				range.RegisterSpace = space;
				range.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;
				ranges.push_back(range);
				D3D12_ROOT_PARAMETER parameter = {};
				parameter.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
				parameter.DescriptorTable.NumDescriptorRanges = 1;
				parameter.DescriptorTable.pDescriptorRanges = &ranges.back();
				parameter.ShaderVisibility = visibility;
				parameters.push_back(parameter);
			}
			mapping.sampled_root_parameter = mapping.root_parameter + 1;
			mapping.sampler_root_parameter = mapping.root_parameter + 2;
		} else {
			rtd3d12_fail(rt::error::unsupported_feature, "DirectX 12 does not support RTIR resource '{}' yet", mapping.name.c_str());
			return false;
		}
		rt::location* location = program->allocate_location();
		if (!location) return false;
		program->descriptor_mappings[location->address] = std::move(mapping);
	}
	// RTSL plain uniforms are independently bindable Rutile constant buffers.
	// Keep their register assignment in sync with the HLSL emitter.
	u32 uniform_binding = 0;
	for (const rtsl::ir::Resource& resource : module.resources) {
		if (resource.binding && resource.binding->set == 0)
			uniform_binding = (std::max)(uniform_binding, resource.binding->binding + 1);
	}
	for (const rtsl::ir::Uniform& uniform : module.uniforms) {
		const rtsl::ir::Symbol* symbol = module.findSymbol(uniform.symbol);
		const std::optional<u32> byte_size = uniform.size ? uniform.size : rtd3d12_type_byte_size(module, uniform.type);
		if (!symbol || !byte_size || !*byte_size) {
			rtd3d12_fail(rt::error::shader_link_failed, "cannot reflect RTSL uniform into a D3D12 constant buffer");
			return false;
		}
		const rtsl::ir::Binding binding = uniform.binding.value_or(rtsl::ir::Binding{.set = 0, .binding = uniform_binding++});
		rtd3d12_program_descriptor_mapping mapping = {};
		mapping.name = std::string(module.strings.get(symbol->fully_qualified_name));
		mapping.type = rtd3d12_descriptor_type::constant_buffer;
		mapping.binding = binding.binding;
		mapping.root_parameter = static_cast<u32>(parameters.size());
		D3D12_DESCRIPTOR_RANGE range = {};
		range.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_CBV;
		range.NumDescriptors = 1;
		range.BaseShaderRegister = binding.binding;
		range.RegisterSpace = binding.set;
		range.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;
		ranges.push_back(range);
		D3D12_ROOT_PARAMETER parameter = {};
		parameter.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
		parameter.DescriptorTable.NumDescriptorRanges = 1;
		parameter.DescriptorTable.pDescriptorRanges = &ranges.back();
		parameter.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
		parameters.push_back(parameter);
		rt::location* location = program->allocate_location();
		if (!location) return false;
		program->uniform_data_mappings[location->address] = rtd3d12_program_data_mapping{
			.name = mapping.name,
			.binding = mapping.binding,
			.root_parameter = mapping.root_parameter,
			.byte_offset = 0,
			.byte_size = *byte_size,
			.block_size = (*byte_size + 15u) & ~usize(15u),
		};
		program->descriptor_mappings[location->address] = std::move(mapping);
	}
	/* Plain RTSL `storage` declarations share one raw UAV.  Their locations
	 * address members of that block and are written with rtCmdStorageData. */
	std::vector<const rtsl::ir::StorageObject*> storage_objects;
	for (const rtsl::ir::StorageObject& object : module.storage_objects)
		if (object.address_space == rtsl::ir::AddressSpace::address_space_storage) storage_objects.push_back(&object);
	if (!storage_objects.empty()) {
		D3D12_DESCRIPTOR_RANGE range = {};
		range.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
		range.NumDescriptors = 1;
		range.BaseShaderRegister = 0;
		range.RegisterSpace = 0;
		range.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;
		ranges.push_back(range);
		D3D12_ROOT_PARAMETER parameter = {};
		parameter.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
		parameter.DescriptorTable.NumDescriptorRanges = 1;
		parameter.DescriptorTable.pDescriptorRanges = &ranges.back();
		parameter.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
		const u32 root_parameter = static_cast<u32>(parameters.size());
		parameters.push_back(parameter);

		usize block_size{};
		std::vector<usize> offsets;
		offsets.reserve(storage_objects.size());
		for (const rtsl::ir::StorageObject* object : storage_objects) {
			const auto layout = rtd3d12_storage_type_layout(module, object->type);
			if (!layout || !layout->size) {
				rtd3d12_fail(rt::error::shader_link_failed, "cannot reflect RTSL storage object into D3D12 storage data");
				return false;
			}
			block_size = (block_size + layout->alignment - 1u) / layout->alignment * layout->alignment;
			offsets.push_back(block_size);
			block_size += layout->size;
		}
		block_size = (block_size + 15u) & ~usize(15u);
		for (usize index = 0; index < storage_objects.size(); ++index) {
			const rtsl::ir::StorageObject& object = *storage_objects[index];
			const rtsl::ir::Symbol* symbol = module.findSymbol(object.symbol);
			const auto layout = rtd3d12_storage_type_layout(module, object.type);
			if (!symbol || !layout) return false;
			rt::location* location = program->allocate_location();
			if (!location) return false;
			program->storage_data_mappings[location->address] = rtd3d12_program_data_mapping{
				.name = std::string(module.strings.get(symbol->fully_qualified_name)), .binding = 0,
				.root_parameter = root_parameter, .byte_offset = offsets[index], .byte_size = layout->size, .block_size = block_size};
		}
	}
	for (usize index = 0; index < program->vertex_layout.input_count; index++) {
		rt::location* location = program->allocate_location();
		if (!location) return false;
		program->input_mappings[location->address] = rtd3d12_program_input_mapping{.vertex_input = index};
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
		rtd3d12_fail(rt::error::initialization_failed, "{}", message);
		if (errors) {
			errors->Release();
			errors = nullptr;
		}
		return false;
	}

	result = ctx->d3d_device->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(&program->d3d_root_signature));
	if (signature) {
		signature->Release();
		signature = nullptr;
	}
	if (errors) {
		errors->Release();
		errors = nullptr;
	}
	if (FAILED(result)) {
		rtd3d12_fail(rtd3d12_error_from_hresult(result), "CreateRootSignature failed: 0x{:08x}", static_cast<u32>(result));
		return false;
	}
	return true;
}

static DXGI_FORMAT rtd3d12_vertex_format(rt::format format) {
	switch (format) {
	case rt::format::r32_sfloat:
		return DXGI_FORMAT_R32_FLOAT;
	case rt::format::rg32_sfloat:
		return DXGI_FORMAT_R32G32_FLOAT;
	case rt::format::rgb32_sfloat:
		return DXGI_FORMAT_R32G32B32_FLOAT;
	case rt::format::rgba32_sfloat:
		return DXGI_FORMAT_R32G32B32A32_FLOAT;
	case rt::format::r32_sint:
		return DXGI_FORMAT_R32_SINT;
	case rt::format::rg32_sint:
		return DXGI_FORMAT_R32G32_SINT;
	case rt::format::rgb32_sint:
		return DXGI_FORMAT_R32G32B32_SINT;
	case rt::format::rgba32_sint:
		return DXGI_FORMAT_R32G32B32A32_SINT;
	case rt::format::r32_uint:
		return DXGI_FORMAT_R32_UINT;
	case rt::format::rg32_uint:
		return DXGI_FORMAT_R32G32_UINT;
	case rt::format::rgb32_uint:
		return DXGI_FORMAT_R32G32B32_UINT;
	case rt::format::rgba32_uint:
		return DXGI_FORMAT_R32G32B32A32_UINT;
	default:
		return DXGI_FORMAT_UNKNOWN;
	}
}

static D3D12_BLEND rtd3d12_blend_factor(rt::blend_factor factor) {
	switch (factor) {
	case rt::blend_factor::zero:
		return D3D12_BLEND_ZERO;
	case rt::blend_factor::one:
		return D3D12_BLEND_ONE;
	case rt::blend_factor::source_color:
		return D3D12_BLEND_SRC_COLOR;
	case rt::blend_factor::one_minus_source_color:
		return D3D12_BLEND_INV_SRC_COLOR;
	case rt::blend_factor::destination_color:
		return D3D12_BLEND_DEST_COLOR;
	case rt::blend_factor::one_minus_destination_color:
		return D3D12_BLEND_INV_DEST_COLOR;
	case rt::blend_factor::source_alpha:
		return D3D12_BLEND_SRC_ALPHA;
	case rt::blend_factor::one_minus_source_alpha:
		return D3D12_BLEND_INV_SRC_ALPHA;
	case rt::blend_factor::destination_alpha:
		return D3D12_BLEND_DEST_ALPHA;
	case rt::blend_factor::one_minus_destination_alpha:
		return D3D12_BLEND_INV_DEST_ALPHA;
	default:
		return D3D12_BLEND_ZERO;
	}
}

static D3D12_BLEND_OP rtd3d12_blend_op(rt::blend_op operation) {
	switch (operation) {
	case rt::blend_op::add:
		return D3D12_BLEND_OP_ADD;
	case rt::blend_op::subtract:
		return D3D12_BLEND_OP_SUBTRACT;
	case rt::blend_op::reverse_subtract:
		return D3D12_BLEND_OP_REV_SUBTRACT;
	case rt::blend_op::minimum:
		return D3D12_BLEND_OP_MIN;
	case rt::blend_op::maximum:
		return D3D12_BLEND_OP_MAX;
	default:
		return D3D12_BLEND_OP_ADD;
	}
}

static std::optional<D3D_PRIMITIVE_TOPOLOGY> rtd3d12_tessellation_primitive_topology(
	const rtsl::ir::EntryPoint& entry
) {
	const auto* configuration = std::get_if<rtsl::ir::TessellationControlConfiguration>(&entry.configuration);
	if (!configuration || configuration->output_control_points == 0 || configuration->output_control_points > 32) {
		rtd3d12_fail(rt::error::shader_link_failed, "tessellation-control RTIR output_control_points must be in the D3D12 range [1, 32]");
		return std::nullopt;
	}
	return static_cast<D3D_PRIMITIVE_TOPOLOGY>(
		static_cast<unsigned>(D3D_PRIMITIVE_TOPOLOGY_1_CONTROL_POINT_PATCHLIST) + configuration->output_control_points - 1);
}

bool rt_program_t::prepare(
	DXGI_FORMAT color_format,
	DXGI_FORMAT depth_format
) {
	if (!d3d_root_signature) {
		rtd3d12_fail(rt::error::improper_usage, "program must be finalized before use");
		return false;
	}
	if (d3d_compute_pipeline) {
		rtd3d12_fail(rt::error::improper_usage, "compute program cannot be used in a rendering scope");
		return false;
	}
	if (d3d_pipeline && d3d_pipeline_format == color_format && d3d_pipeline_depth_format == depth_format) {
		return true;
	}
	destroy_pipeline();

	std::vector<D3D12_INPUT_ELEMENT_DESC> elements;
	elements.reserve(RTD3D12_MAX_VERTEX_ATTRIBUTES);
	for (usize input_index = 0; input_index < vertex_layout.input_count; ++input_index) {
		const rt::vertex_input& input = vertex_layout.inputs[input_index];
		for (usize attribute_index = 0; attribute_index < input.attribute_count; ++attribute_index) {
			const rt::vertex_attribute& attribute = input.attributes[attribute_index];
			const DXGI_FORMAT format = rtd3d12_vertex_format(attribute.format);
			if (format == DXGI_FORMAT_UNKNOWN) {
				rtd3d12_fail(rt::error::unsupported_feature, "unsupported vertex attribute format");
				return false;
			}
			elements.push_back(D3D12_INPUT_ELEMENT_DESC{
				.SemanticName = "TEXCOORD",
				.SemanticIndex = static_cast<UINT>(attribute_index),
				.Format = format,
				.InputSlot = static_cast<UINT>(input_index),
				.AlignedByteOffset = static_cast<UINT>(attribute.offset),
				.InputSlotClass = input.rate == rt::vertex_rate::instance ? D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA : D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,
				.InstanceDataStepRate = input.rate == rt::vertex_rate::instance ? 1u : 0u,
			});
		}
	}

	D3D12_GRAPHICS_PIPELINE_STATE_DESC desc = {};
	desc.pRootSignature = d3d_root_signature;
	for (const rtd3d12_program_shader& shader : shaders) {
		const D3D12_SHADER_BYTECODE bytecode = { shader.bytecode.data(), shader.bytecode.size() };
		if (shader.stage == rtsl::ir::Stage::stage_vertex) {
			desc.VS = bytecode;
		} else if (shader.stage == rtsl::ir::Stage::stage_fragment) {
			desc.PS = bytecode;
		} else if (shader.stage == rtsl::ir::Stage::stage_tessellation_control) {
			desc.HS = bytecode;
		} else if (shader.stage == rtsl::ir::Stage::stage_tessellation_evaluation) {
			desc.DS = bytecode;
		} else if (shader.stage == rtsl::ir::Stage::stage_geometry) {
			desc.GS = bytecode;
		}
	}
	desc.BlendState.AlphaToCoverageEnable = FALSE;
	desc.BlendState.IndependentBlendEnable = FALSE;
	D3D12_RENDER_TARGET_BLEND_DESC& blend = desc.BlendState.RenderTarget[0];
	blend.BlendEnable = blend_enabled;
	blend.LogicOpEnable = FALSE;
	blend.SrcBlend = rtd3d12_blend_factor(src_color_blend);
	blend.DestBlend = rtd3d12_blend_factor(dst_color_blend);
	blend.BlendOp = rtd3d12_blend_op(color_blend_op);
	blend.SrcBlendAlpha = rtd3d12_blend_factor(src_alpha_blend);
	blend.DestBlendAlpha = rtd3d12_blend_factor(dst_alpha_blend);
	blend.BlendOpAlpha = rtd3d12_blend_op(alpha_blend_op);
	blend.LogicOp = D3D12_LOGIC_OP_NOOP;
	blend.RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
	desc.SampleMask = UINT_MAX;
	desc.RasterizerState.FillMode = fill_mode == rt::fill_mode::wireframe ? D3D12_FILL_MODE_WIREFRAME : D3D12_FILL_MODE_SOLID;
	desc.RasterizerState.CullMode = cull_mode == rt::cull_mode::front ? D3D12_CULL_MODE_FRONT : cull_mode == rt::cull_mode::back ? D3D12_CULL_MODE_BACK
																																	 : D3D12_CULL_MODE_NONE;
	desc.RasterizerState.FrontCounterClockwise = front_face == rt::front_face::counter_clockwise;
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
	desc.PrimitiveTopologyType = d3d_primitive_topology >= D3D_PRIMITIVE_TOPOLOGY_1_CONTROL_POINT_PATCHLIST
		? D3D12_PRIMITIVE_TOPOLOGY_TYPE_PATCH
		: D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	desc.NumRenderTargets = 1;
	desc.RTVFormats[0] = color_format;
	desc.DSVFormat = depth_format;
	desc.SampleDesc.Count = 1;

	const HRESULT result = ctx->d3d_device->CreateGraphicsPipelineState(&desc, IID_PPV_ARGS(&d3d_pipeline));
	if (FAILED(result)) {
		rtd3d12_fail(rtd3d12_error_from_hresult(result), "CreateGraphicsPipelineState failed: 0x{:08x}", static_cast<u32>(result));
		return false;
	}

	d3d_pipeline_format = color_format;
	d3d_pipeline_depth_format = depth_format;
	return true;
}

void rt_program_t::layout(const rt::vertex_layout* layout) {
	if (!layout || !layout->inputs || layout->input_count == 0) {
		vertex_layout = {};
		destroy_pipeline();
		return;
	}
	if (layout->input_count > RTD3D12_MAX_VERTEX_STREAMS) {
		rtd3d12_fail(rt::error::improper_usage, "too many vertex inputs");
		return;
	}
	usize attribute_count = 0;
	for (usize input_index = 0; input_index < layout->input_count; ++input_index) {
		const rt::vertex_input& input = layout->inputs[input_index];
		if (!input.attributes || input.attribute_count == 0 || attribute_count + input.attribute_count > RTD3D12_MAX_VERTEX_ATTRIBUTES) {
			rtd3d12_fail(rt::error::improper_usage, "invalid vertex input attributes");
			return;
		}
		std::memcpy(vertex_attributes + attribute_count, input.attributes, sizeof(input.attributes[0]) * input.attribute_count);
		vertex_inputs[input_index] = input;
		vertex_inputs[input_index].attributes = vertex_attributes + attribute_count;
		attribute_count += input.attribute_count;
	}
	vertex_layout.inputs = vertex_inputs;
	vertex_layout.input_count = layout->input_count;
	destroy_pipeline();
}

void rt_program_t::source(const char* entry_point, const void* data, usize size) {
	if (!entry_point || !entry_point[0]) {
		rtd3d12_fail(rt::error::improper_usage, "program entry point is empty");
		return;
	}
	if (!data || size == 0) {
		rtd3d12_fail(rt::error::improper_usage, "program source data is empty");
		return;
	}
	rtsl::ArtifactReader reader;
	auto loaded = reader.read(std::span<const std::byte>{ static_cast<const std::byte*>(data), size });
	if (!loaded || !loaded.artifact || loaded.artifact->kind != rtsl::ArtifactKind::artifact_program) {
		const char* message = loaded.error ? loaded.error->message.c_str() : "artifact is not a linked RTSL program";
		rtd3d12_fail(rt::error::shader_link_failed, "invalid RTSL program source: {}", message);
		return;
	}
	destroy_root_signature();
	this->entry_point = entry_point;
	clear_mappings();
	rtsl_artifact.emplace(std::move(*loaded.artifact));
	shaders.clear();
}

void rt_program_t::raster_state(
	rt::cull_mode cull_mode,
	rt::front_face front_face,
	rt::fill_mode fill_mode
) {
	this->cull_mode = cull_mode;
	this->front_face = front_face;
	this->fill_mode = fill_mode;
	destroy_pipeline();
}

void rt_program_t::blend_state(
	bool enabled,
	rt::blend_factor src_color,
	rt::blend_factor dst_color,
	rt::blend_op color_op,
	rt::blend_factor src_alpha,
	rt::blend_factor dst_alpha,
	rt::blend_op alpha_op
) {
	blend_enabled = enabled;
	src_color_blend = src_color;
	dst_color_blend = dst_color;
	color_blend_op = color_op;
	src_alpha_blend = src_alpha;
	dst_alpha_blend = dst_alpha;
	alpha_blend_op = alpha_op;
	destroy_pipeline();
}

namespace rtd3d12_program_detail {

bool compile_hlsl(std::string_view source, std::string_view entry_point, const wchar_t* profile, std::vector<std::byte>& bytecode) {
	IDxcCompiler3* compiler = nullptr;
	IDxcResult* result = nullptr;
	IDxcBlob* object = nullptr;

	HRESULT status = DxcCreateInstance(CLSID_DxcCompiler, IID_PPV_ARGS(&compiler));
	if (SUCCEEDED(status)) {
		const DxcBuffer buffer = {
			.Ptr = source.data(),
			.Size = source.size(),
			.Encoding = DXC_CP_UTF8,
		};
		const std::wstring wide_entry_point(entry_point.begin(), entry_point.end());
		const wchar_t* arguments[] = { L"-E", wide_entry_point.c_str(), L"-T", profile, L"-HV", L"2021", L"-Ges" };
		status = compiler->Compile(&buffer, arguments, static_cast<UINT32>(std::size(arguments)), nullptr, IID_PPV_ARGS(&result));
	}
	if (FAILED(status) || !result) {
		rtd3d12_fail(rt::error::shader_link_failed, "DXC could not compile the generated HLSL");
		if (compiler) compiler->Release();
		return false;
	}
	HRESULT compile_status = E_FAIL;
	status = result->GetStatus(&compile_status);
	if (FAILED(status) || FAILED(compile_status)) {
		IDxcBlobUtf8* diagnostics = nullptr;
		std::string message = "DXC rejected the generated HLSL";
		if (SUCCEEDED(result->GetOutput(DXC_OUT_ERRORS, IID_PPV_ARGS(&diagnostics), nullptr)) && diagnostics) {
			const std::string_view text{ diagnostics->GetStringPointer(), diagnostics->GetStringLength() };
			if (!text.empty()) message.assign(text);
			diagnostics->Release();
		}
		rtd3d12_fail(rt::error::shader_link_failed, "{}", message);
		result->Release();
		compiler->Release();
		return false;
	}
	status = result->GetOutput(DXC_OUT_OBJECT, IID_PPV_ARGS(&object), nullptr);
	if (FAILED(status) || !object || object->GetBufferSize() == 0) {
		rtd3d12_fail(rt::error::shader_link_failed, "DXC rejected the generated HLSL");
		if (object) object->Release();
		if (result) result->Release();
		if (compiler) compiler->Release();
		return false;
	}
	const auto* begin = static_cast<const std::byte*>(object->GetBufferPointer());
	bytecode.assign(begin, begin + object->GetBufferSize());
	if (object) {
		object->Release();
		object = nullptr;
	}
	if (result) {
		result->Release();
		result = nullptr;
	}
	if (compiler) {
		compiler->Release();
		compiler = nullptr;
	}
	return true;
}

}

void rt_program_t::finalize() {
	if (!rtsl_artifact) {
		rtd3d12_fail(rt::error::shader_link_failed, "program finalize requires an RTSL artifact source set via rtProgramSource");
		return;
	}
	static constexpr struct {
		rtsl::ir::Stage stage;
		const wchar_t* profile;
	} stage_configs[] = {
		{ rtsl::ir::Stage::stage_vertex, L"vs_6_0" },
		{ rtsl::ir::Stage::stage_tessellation_control, L"hs_6_0" },
		{ rtsl::ir::Stage::stage_tessellation_evaluation, L"ds_6_0" },
		{ rtsl::ir::Stage::stage_geometry, L"gs_6_0" },
		{ rtsl::ir::Stage::stage_fragment, L"ps_6_0" },
		{ rtsl::ir::Stage::stage_compute, L"cs_6_0" },
	};

	std::vector<rtd3d12_program_shader> shaders;
	const rtsl::ir::EntryPoint* tessellation_control = nullptr;
	const rtsl::ir::EntryPoint* tessellation_evaluation = nullptr;
	bool compute = false;
	for (const auto& config : stage_configs) {
		const rtsl::ir::EntryPoint* entry = nullptr;
		for (const rtsl::ir::EntryPoint& candidate : rtsl_artifact->module.entry_points)
			if (candidate.stage == config.stage && rtsl_artifact->module.strings.get(candidate.source_name) == entry_point) { entry = &candidate; break; }
		if (!entry) {
			continue;
		}
		if (config.stage == rtsl::ir::Stage::stage_tessellation_control) {
			tessellation_control = entry;
		} else if (config.stage == rtsl::ir::Stage::stage_tessellation_evaluation) {
			tessellation_evaluation = entry;
		} else if (config.stage == rtsl::ir::Stage::stage_compute) {
			compute = true;
		}
		rtd3d12::hlsl::Error error;
		auto translation = rtd3d12::hlsl::transpile(rtsl_artifact->module, *entry, error);
		if (!translation) {
			rtd3d12_fail(rt::error::shader_link_failed, "RTSL to HLSL failed in {}: {}", error.context.c_str(), error.message.c_str());
			return;
		}
		rtd3d12_program_shader shader = { .stage = config.stage };
		if (!rtd3d12_program_detail::compile_hlsl(translation->source, translation->entry_point, config.profile, shader.bytecode)) {
			return;
		}
		shaders.push_back(std::move(shader));
	}
	if (shaders.empty()) {
		rtd3d12_fail(rt::error::shader_link_failed, "RTSL program does not expose entry point '{}' in a D3D12-supported stage", entry_point.c_str());
		return;
	}
	if (compute && shaders.size() != 1) {
		rtd3d12_fail(rt::error::shader_link_failed, "compute entry cannot be combined with graphics shader stages");
		return;
	}
	if (tessellation_control || tessellation_evaluation) {
		if (!tessellation_control || !tessellation_evaluation) {
			rtd3d12_fail(rt::error::shader_link_failed, "D3D12 tessellation requires both tessellation-control and tessellation-evaluation entries");
			return;
		}
		const bool has_vertex = std::any_of(shaders.begin(), shaders.end(), [](const rtd3d12_program_shader& shader) {
			return shader.stage == rtsl::ir::Stage::stage_vertex;
		});
		if (!has_vertex) {
			const rtsl::ir::EntryPoint* synthesized_vertex = nullptr;
			for (const rtsl::ir::EntryPoint& candidate : rtsl_artifact->module.entry_points) {
				if (candidate.stage != rtsl::ir::Stage::stage_vertex) continue;
				if (synthesized_vertex) {
					rtd3d12_fail(rt::error::shader_link_failed, "tessellation entry has no unambiguous linked vertex stage");
					return;
				}
				synthesized_vertex = &candidate;
			}
			if (!synthesized_vertex) {
				rtd3d12_fail(rt::error::shader_link_failed, "tessellation entry has no linked vertex stage");
				return;
			}
			rtd3d12::hlsl::Error error;
			auto translation = rtd3d12::hlsl::transpile(rtsl_artifact->module, *synthesized_vertex, error);
			if (!translation) {
				rtd3d12_fail(rt::error::shader_link_failed, "RTSL to HLSL failed in linked vertex stage: {}", error.message.c_str());
				return;
			}
			rtd3d12_program_shader shader = { .stage = rtsl::ir::Stage::stage_vertex };
			if (!rtd3d12_program_detail::compile_hlsl(translation->source, translation->entry_point, L"vs_6_0", shader.bytecode)) return;
			shaders.push_back(std::move(shader));
		}
		const std::optional<D3D_PRIMITIVE_TOPOLOGY> topology = rtd3d12_tessellation_primitive_topology(*tessellation_control);
		if (!topology) {
			return;
		}
		d3d_primitive_topology = *topology;
	} else {
		d3d_primitive_topology = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	}

	destroy_root_signature();
	this->shaders = std::move(shaders);
	if (!rtd3d12_program_create_root_signature(ctx, this)) {
		this->shaders.clear();
		return;
	}
	if (compute) {
		D3D12_COMPUTE_PIPELINE_STATE_DESC desc = {};
		desc.pRootSignature = d3d_root_signature;
		desc.CS = { this->shaders.front().bytecode.data(), this->shaders.front().bytecode.size() };
		const HRESULT result = ctx->d3d_device->CreateComputePipelineState(&desc, IID_PPV_ARGS(&d3d_pipeline));
		if (FAILED(result)) {
			rtd3d12_fail(rtd3d12_error_from_hresult(result), "CreateComputePipelineState failed: 0x{:08x}", static_cast<u32>(result));
			return;
		}
		d3d_compute_pipeline = true;
	}
}

rt::location* rt_program_t::uniform_location(const char* name) {
	if (!name) {
		rtd3d12_fail(rt::error::improper_usage, "uniform location name is nullptr");
		return nullptr;
	}
	if (!d3d_root_signature) {
		rtd3d12_fail(rt::error::improper_usage, "program must be finalized before querying uniforms");
		return nullptr;
	}
	for (u32 index = 0; index < std::size(locations); ++index) {
		if (!location_allocated[index]) continue;
		if ((uniform_data_mappings[index] && uniform_data_mappings[index]->name == name) ||
			(storage_data_mappings[index] && storage_data_mappings[index]->name == name) ||
			(descriptor_mappings[index] && descriptor_mappings[index]->name == name)) return &locations[index];
	}
	return nullptr;
}

rt::location* rt_program_t::input_location(const rt::vertex_attribute* attributes, usize attribute_count) {
	if (!attributes || !attribute_count) {
		rtd3d12_fail(rt::error::improper_usage, "vertex input attributes are invalid");
		return nullptr;
	}
	if (!d3d_root_signature) {
		rtd3d12_fail(rt::error::improper_usage, "program must be finalized before querying vertex inputs");
		return nullptr;
	}
	for (u32 index = 0; index < std::size(locations); ++index) {
		if (!location_allocated[index] || !input_mappings[index] || input_mappings[index]->vertex_input >= vertex_layout.input_count) continue;
		const rt::vertex_input& input = vertex_layout.inputs[input_mappings[index]->vertex_input];
		if (input.attribute_count != attribute_count) {
			continue;
		}
		bool matches = true;
		for (usize index = 0; index < attribute_count; ++index) {
			const rt::vertex_attribute& expected = input.attributes[index];
			const rt::vertex_attribute& supplied = attributes[index];
			if (expected.offset != supplied.offset || expected.format != supplied.format || std::strcmp(expected.name, supplied.name) != 0) {
				matches = false;
				break;
			}
		}
		if (matches) {
			return &locations[index];
		}
	}
	return nullptr;
}

rt::location* rt_program_t::output_location(const char* name) {
	if (!d3d_root_signature) {
		rtd3d12_fail(rt::error::improper_usage, "program must be finalized before querying fragment outputs");
		return nullptr;
	}
	for (u32 index = 0; index < std::size(locations); ++index) {
		if (!location_allocated[index] || !output_mappings[index]) continue;
		const rtd3d12_program_output_mapping& mapping = *output_mappings[index];
		if ((name && mapping.name == name) || (!name && mapping.name.empty())) return &locations[index];
	}
	return nullptr;
}
