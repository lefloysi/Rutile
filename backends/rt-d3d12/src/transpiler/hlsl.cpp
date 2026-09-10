#include "hlsl.hpp"

#include <rtsl/IR/Verifier.hpp>

#include <algorithm>
#include <bit>
#include <cctype>
#include <iomanip>
#include <limits>
#include <sstream>
#include <unordered_map>
#include <unordered_set>

namespace rtd3d12::hlsl {
namespace {

std::string hlslIdentifier(std::string_view spelling);

std::string typeName(const rtsl::ir::Module& module, rtsl::ir::TypeId id, Error& error) {
	const rtsl::ir::Type* type = module.findType(id);
	if (!type) { error = {"type", "references an unknown RTIR type"}; return {}; }
	switch (type->kind) {
	case rtsl::ir::TypeKind::type_void: return "void";
	case rtsl::ir::TypeKind::type_boolean: return "bool";
	case rtsl::ir::TypeKind::type_signed_integer: return "int";
	case rtsl::ir::TypeKind::type_unsigned_integer: return "uint";
	case rtsl::ir::TypeKind::type_floating: return "float";
	case rtsl::ir::TypeKind::type_vector: {
		std::string element = typeName(module, type->element_type, error);
		return element.empty() ? std::string{} : element + std::to_string(type->element_count);
	}
	case rtsl::ir::TypeKind::type_matrix: {
		std::string element = typeName(module, type->element_type, error);
		return element.empty() ? std::string{} : element + std::to_string(type->element_count) + "x" + std::to_string(type->element_count);
	}
	case rtsl::ir::TypeKind::type_structure: {
		std::string name = hlslIdentifier(module.strings.get(type->name));
		if (name.empty()) { error = {"type", "anonymous structure cannot be emitted to HLSL"}; return {}; }
		return name;
	}
	case rtsl::ir::TypeKind::type_patch: return "rtsl_patch";
	default: error = {"type", "RTIR type is not supported by the D3D12 HLSL translator"}; return {};
	}
}

std::string hlslIdentifier(std::string_view spelling) {
	std::string result;
	result.reserve(spelling.size() + 1);
	for (const char character : spelling) {
		const unsigned char value = static_cast<unsigned char>(character);
		result.push_back(std::isalnum(value) || character == '_' ? character : '_');
	}
	if (result.empty() || std::isdigit(static_cast<unsigned char>(result.front()))) result.insert(result.begin(), '_');
	return result;
}

std::string symbolName(const rtsl::ir::Module& module, rtsl::ir::SymbolId symbol) {
	const auto* value = module.findSymbol(symbol);
	return value ? hlslIdentifier(module.strings.get(value->fully_qualified_name)) : std::string{};
}

std::string valueName(rtsl::ir::ValueId id) { return "v" + std::to_string(id.value()); }

std::uint32_t roundUp(std::uint32_t value, std::uint32_t alignment) {
	return (value + alignment - 1u) / alignment * alignment;
}

struct StorageLayout {
	std::uint32_t alignment{};
	std::uint32_t size{};
};

std::optional<StorageLayout> storageLayout(const rtsl::ir::Module& module, rtsl::ir::TypeId type_id) {
	const rtsl::ir::Type* type = module.findType(type_id);
	if (!type) return std::nullopt;
	switch (type->kind) {
	case rtsl::ir::TypeKind::type_boolean:
	case rtsl::ir::TypeKind::type_signed_integer:
	case rtsl::ir::TypeKind::type_unsigned_integer:
	case rtsl::ir::TypeKind::type_floating:
		return StorageLayout{4, 4};
	case rtsl::ir::TypeKind::type_vector: {
		auto element = storageLayout(module, type->element_type);
		if (!element) return std::nullopt;
		if (type->element_count == 2) return StorageLayout{8, element->size * 2};
		if (type->element_count == 3 || type->element_count == 4) return StorageLayout{16, 16};
		return std::nullopt;
	}
	case rtsl::ir::TypeKind::type_matrix:
		return StorageLayout{16, 16 * type->element_count};
	case rtsl::ir::TypeKind::type_array: {
		auto element = storageLayout(module, type->element_type);
		if (!element) return std::nullopt;
		return StorageLayout{16, roundUp(element->size, 16) * type->element_count};
	}
	case rtsl::ir::TypeKind::type_structure: {
		std::uint32_t alignment = 16;
		std::uint32_t offset{};
		for (const rtsl::ir::StructMember& member : type->members) {
			auto member_layout = storageLayout(module, member.type);
			if (!member_layout) return std::nullopt;
			alignment = (std::max)(alignment, member_layout->alignment);
			offset = roundUp(offset, member_layout->alignment);
			offset += member_layout->size;
		}
		return StorageLayout{alignment, roundUp(offset, alignment)};
	}
	default: return std::nullopt;
	}
}

class Emitter {
public:
	Emitter(const rtsl::ir::Module& module, Error& error) : module(module), error(error) {}

	bool emit(std::ostringstream& out, const rtsl::ir::EntryPoint& entry) {
		for (const auto& type : module.types)
			if (type.kind == rtsl::ir::TypeKind::type_structure && !type.members.empty() && !emitStruct(out, type)) return false;
		if (!prepareStorageObjects()) return false;
		std::uint32_t next_resource_binding = 0;
		std::uint32_t next_uniform_binding = 0;
		for (const auto& resource : module.resources) if (resource.binding && resource.binding->set == 0) next_uniform_binding = (std::max)(next_uniform_binding, resource.binding->binding + 1);
		for (const auto& resource : module.resources) {
			const std::string name = symbolName(module, resource.symbol);
			if (name.empty()) return fail("resource", "resource has no symbol name");
			const auto binding = resource.binding.value_or(rtsl::ir::Binding{.set = 0, .binding = next_resource_binding++});
			switch (resource.kind) {
			case rtsl::ir::ResourceKind::resource_uniform_buffer: out << "ConstantBuffer<float4> " << name << " : register(b" << binding.binding << ", space" << binding.set << ");\n"; break;
			case rtsl::ir::ResourceKind::resource_storage_buffer: out << "RWByteAddressBuffer " << name << " : register(u" << binding.binding << ", space" << binding.set << ");\n"; break;
			case rtsl::ir::ResourceKind::resource_sampled_texture:
				out << "Texture2D<float4> " << name << " : register(t" << binding.binding << ", space" << binding.set << ");\n";
				out << "SamplerState " << name << "_sampler : register(s" << binding.binding << ", space" << binding.set << ");\n";
				break;
			case rtsl::ir::ResourceKind::resource_sampler: out << "SamplerState " << name << " : register(s" << binding.binding << ", space" << binding.set << ");\n"; break;
			case rtsl::ir::ResourceKind::resource_storage_texture: {
				const rtsl::ir::Type* type = module.findType(resource.type);
				const std::string_view image = type ? module.strings.get(type->name) : std::string_view{};
				if (!type || type->parameter_types.size() != 1 || (image != "image_1d" && image != "image_2d" && image != "image_3d")) return fail("resource", "storage image must preserve its RTIR dimension and texel type");
				const std::string texel = typeName(module, type->parameter_types[0], error);
				if (texel.empty()) return false;
				const char* dimension = image == "image_1d" ? "1D" : image == "image_2d" ? "2D" : "3D";
				out << "RWTexture" << dimension << "<" << texel << "> " << name << " : register(u" << binding.binding << ", space" << binding.set << ");\n";
				out << "Texture" << dimension << "<" << texel << "> " << name << "_sampled : register(t" << binding.binding << ", space" << binding.set << ");\n";
				out << "SamplerState " << name << "_sampler : register(s" << binding.binding << ", space" << binding.set << ");\n";
				break;
			}
			default: return fail("resource", "resource kind is not supported by the D3D12 HLSL translator");
			}
		}
		for (const auto& uniform : module.uniforms) {
			const std::string name = symbolName(module, uniform.symbol);
			const std::string type = typeName(module, uniform.type, error);
			if (name.empty() || type.empty()) return fail("uniform", "uniform has no emittable name or type");
			const auto binding = uniform.binding.value_or(rtsl::ir::Binding{.set = 0, .binding = next_uniform_binding++});
			out << "cbuffer cb_" << name << " : register(b" << binding.binding << ", space" << binding.set << ") { " << type << " " << name << "; };\n";
		}
		if (!storage_offsets.empty()) out << "RWByteAddressBuffer rtsl_storage : register(u0, space0);\n";
		for (const auto& function : module.functions) {
			const bool is_entry = std::ranges::any_of(module.entry_points,
				[&](const rtsl::ir::EntryPoint& candidate) { return candidate.function == function.id; });
			const bool is_direct_entry = function.id == entry.function &&
				(entry.stage == rtsl::ir::Stage::stage_vertex ||
					entry.stage == rtsl::ir::Stage::stage_fragment ||
					entry.stage == rtsl::ir::Stage::stage_compute);
			if (!function.declaration && (!is_entry || is_direct_entry) && !emitFunction(out, function)) return false;
		}
		const auto* function = module.findFunction(entry.function);
		if (!function) return fail("entry", "entry point references an unknown function");
		out << "\n";
		if (entry.stage == rtsl::ir::Stage::stage_tessellation_control)
			return emitTessellationControl(out, entry, *function);
		if (entry.stage == rtsl::ir::Stage::stage_tessellation_evaluation)
			return emitTessellationEvaluation(out, entry, *function);
		if (entry.stage == rtsl::ir::Stage::stage_geometry)
			return emitGeometry(out, entry, *function);
		if (entry.stage == rtsl::ir::Stage::stage_compute)
			return emitCompute(out, entry, *function);
		if (entry.stage != rtsl::ir::Stage::stage_vertex && entry.stage != rtsl::ir::Stage::stage_fragment)
			return fail("entry", "D3D12 backend does not support this entry stage");
		const std::string entry_return_type = typeName(module, function->return_type, error);
		if (entry_return_type.empty()) return false;
		const rtsl::ir::Type* return_type = module.findType(function->return_type);
		out << entry_return_type << " main(";
		for (std::size_t i = 0; i < function->parameters.size(); ++i) {
			if (i) out << ", ";
			const std::string parameter_type = typeName(module, function->parameters[i].type, error);
			if (parameter_type.empty()) return false;
			out << parameter_type << " " << valueName(function->parameters[i].value);
			const rtsl::ir::Type* type = module.findType(function->parameters[i].type);
			if (!type || type->kind != rtsl::ir::TypeKind::type_structure) out << " : TEXCOORD" << i;
		}
		out << ")";
		if (!return_type || return_type->kind != rtsl::ir::TypeKind::type_structure) {
			if (entry.stage == rtsl::ir::Stage::stage_vertex) out << " : SV_Position";
			else out << " : SV_Target0";
		}
		out << " { return " << symbolName(module, function->symbol) << "(";
		for (std::size_t i = 0; i < function->parameters.size(); ++i) { if (i) out << ", "; out << valueName(function->parameters[i].value); }
		out << "); }\n";
		return !error.message.size();
	}

private:
	bool fail(std::string context, std::string message) { error = {std::move(context), std::move(message)}; return false; }
	bool prepareStorageObjects() {
		if (!storage_offsets.empty()) return true;
		std::uint32_t offset{};
		for (const rtsl::ir::StorageObject& object : module.storage_objects) {
			if (object.address_space != rtsl::ir::AddressSpace::address_space_storage) continue;
			auto layout = storageLayout(module, object.type);
			if (!layout) return fail("storage", "storage object has an unsupported layout");
			offset = roundUp(offset, layout->alignment);
			storage_offsets.emplace(object.symbol.value(), offset);
			offset += layout->size;
		}
		storage_block_size = roundUp(offset, 16);
		return true;
	}
	std::optional<std::string> storageLoad(rtsl::ir::TypeId type_id, std::uint32_t offset) {
		return byteAddressLoad("rtsl_storage", type_id, std::to_string(offset));
	}
	std::optional<std::string> byteAddressLoad(std::string_view buffer, rtsl::ir::TypeId type_id, std::string offset) {
		const rtsl::ir::Type* value_type = module.findType(type_id);
		if (!value_type) return std::nullopt;
		auto load = [&](std::uint32_t count = 1) { return std::string(buffer) + ".Load" + (count == 1 ? "" : std::to_string(count)) + "(" + offset + ")"; };
		auto scalar = [&](std::string_view operation) { return std::string(operation) + "(" + load() + ")"; };
		switch (value_type->kind) {
		case rtsl::ir::TypeKind::type_unsigned_integer: return load();
		case rtsl::ir::TypeKind::type_signed_integer: return scalar("asint");
		case rtsl::ir::TypeKind::type_floating: return scalar("asfloat");
		case rtsl::ir::TypeKind::type_boolean: return load() + " != 0";
		case rtsl::ir::TypeKind::type_vector: {
			if (value_type->element_count < 2 || value_type->element_count > 4) return std::nullopt;
			const rtsl::ir::Type* element = module.findType(value_type->element_type);
			if (!element) return std::nullopt;
			const std::string vector_load = load(value_type->element_count);
			switch (element->kind) {
			case rtsl::ir::TypeKind::type_unsigned_integer: return vector_load;
			case rtsl::ir::TypeKind::type_signed_integer: return "asint(" + vector_load + ")";
			case rtsl::ir::TypeKind::type_floating: return "asfloat(" + vector_load + ")";
			default: return std::nullopt;
			}
		}
		default: return std::nullopt;
		}
	}
	const rtsl::ir::Type* patchElement(const rtsl::ir::Function& function, std::size_t parameter = 0) {
		if (function.parameters.size() <= parameter) { fail("entry", "entry point has no patch parameter"); return nullptr; }
		const auto* patch = module.findType(function.parameters[parameter].type);
		if (patch && patch->kind == rtsl::ir::TypeKind::type_pointer) patch = module.findType(patch->element_type);
		if (!patch || patch->kind != rtsl::ir::TypeKind::type_patch) { fail("entry", "entry parameter is not an RTIR patch type"); return nullptr; }
		const auto* element = module.findType(patch->element_type);
		if (!element) fail("entry", "patch element type is unknown");
		return element;
	}
	std::optional<std::uint32_t> controlPointCount(const rtsl::ir::EntryPoint& entry) {
		const auto* configuration = std::get_if<rtsl::ir::TessellationControlConfiguration>(&entry.configuration);
		if (!configuration || configuration->output_control_points == 0) {
			fail("entry", "tessellation-control entry requires a non-zero RTIR output_control_points configuration");
			return std::nullopt;
		}
		return configuration->output_control_points;
	}
	const rtsl::ir::TessellationEvaluationConfiguration* tessellationEvaluation(const rtsl::ir::EntryPoint& entry) {
		for (const auto& candidate : module.entry_points) {
			if (candidate.stage != rtsl::ir::Stage::stage_tessellation_evaluation || module.strings.get(candidate.source_name) != module.strings.get(entry.source_name)) continue;
			return std::get_if<rtsl::ir::TessellationEvaluationConfiguration>(&candidate.configuration);
		}
		fail("entry", "tessellation-control entry has no matching tessellation-evaluation entry");
		return nullptr;
	}
	std::optional<std::uint32_t> matchingControlPointCount(const rtsl::ir::EntryPoint& entry) {
		for (const auto& candidate : module.entry_points)
			if (candidate.stage == rtsl::ir::Stage::stage_tessellation_control && module.strings.get(candidate.source_name) == module.strings.get(entry.source_name))
				return controlPointCount(candidate);
		fail("entry", "tessellation-evaluation entry has no matching tessellation-control entry");
		return std::nullopt;
	}
	static std::string tessellationDomain(const rtsl::ir::TessellationEvaluationConfiguration& configuration) {
		switch (configuration.domain) {
		case rtsl::ir::TessellationDomain::tessellation_domain_triangles: return "tri";
		case rtsl::ir::TessellationDomain::tessellation_domain_quads: return "quad";
		case rtsl::ir::TessellationDomain::tessellation_domain_isolines: return "isoline";
		}
		return {};
	}
	static std::string tessellationPartitioning(const rtsl::ir::TessellationEvaluationConfiguration& configuration) {
		switch (configuration.spacing) {
		case rtsl::ir::TessellationSpacing::tessellation_spacing_equal: return "integer";
		case rtsl::ir::TessellationSpacing::tessellation_spacing_fractional_even: return "fractional_even";
		case rtsl::ir::TessellationSpacing::tessellation_spacing_fractional_odd: return "fractional_odd";
		}
		return {};
	}
	static std::string tessellationOutputTopology(const rtsl::ir::TessellationEvaluationConfiguration& configuration) {
		if (configuration.domain == rtsl::ir::TessellationDomain::tessellation_domain_isolines) return "line";
		return configuration.winding == rtsl::ir::Winding::winding_clockwise ? "triangle_cw" : "triangle_ccw";
	}
	bool emitTessellationFactorType(std::ostringstream& out, const rtsl::ir::TessellationEvaluationConfiguration& configuration) {
		out << "struct rtsl_tessellation_factors {\n";
		switch (configuration.domain) {
		case rtsl::ir::TessellationDomain::tessellation_domain_triangles: out << "  float outer[3] : SV_TessFactor;\n  float inner[1] : SV_InsideTessFactor;\n"; break;
		case rtsl::ir::TessellationDomain::tessellation_domain_quads: out << "  float outer[4] : SV_TessFactor;\n  float inner[2] : SV_InsideTessFactor;\n"; break;
		case rtsl::ir::TessellationDomain::tessellation_domain_isolines: out << "  float outer[2] : SV_TessFactor;\n"; break;
		default: return fail("entry", "unknown tessellation domain");
		}
		out << "};\n";
		return true;
	}
	bool emitTessellationControl(std::ostringstream& out, const rtsl::ir::EntryPoint& entry, const rtsl::ir::Function& function) {
		const auto count = controlPointCount(entry); if (!count) return false;
		const auto* configuration = tessellationEvaluation(entry); if (!configuration) return false;
		const auto* element = patchElement(function); if (!element) return false;
		const std::string element_name = typeName(module, element->id, error); if (element_name.empty()) return false;
		if (!emitTessellationFactorType(out, *configuration)) return false;
		const std::string input = valueName(function.parameters[0].value);
		const std::string patch_function = "rtsl_patch_constants_" + std::to_string(entry.function.value());
		out << "rtsl_tessellation_factors " << patch_function << "(InputPatch<" << element_name << ", " << *count << "> " << input << ") {\n";
		entry_patch_parameter = function.parameters[0].value; entry_patch_name = input; entry_current_index = "0"; entry_factor_name = "result"; suppress_outer_stores = false; suppress_returns = true;
		out << "  rtsl_tessellation_factors result = (rtsl_tessellation_factors)0;\n";
		if (!emitEntryFunctionBody(out, function, 1)) return false;
		out << "  return result;\n}\n";
		out << "[domain(\"" << tessellationDomain(*configuration) << "\")]\n[partitioning(\"" << tessellationPartitioning(*configuration) << "\")]\n[outputtopology(\"" << tessellationOutputTopology(*configuration) << "\")]\n[outputcontrolpoints(" << *count << ")]\n[patchconstantfunc(\"" << patch_function << "\")]\n";
		out << element_name << " main(InputPatch<" << element_name << ", " << *count << "> " << input << ", uint rtsl_control_point : SV_OutputControlPointID) {\n";
		entry_patch_parameter = function.parameters[0].value; entry_patch_name = input; entry_current_index = "rtsl_control_point"; entry_factor_name = "rtsl_ignored_outer"; suppress_outer_stores = true; suppress_returns = false;
		if (!emitEntryFunctionBody(out, function, 1)) return false;
		out << "}\n";
		clearEntryLowering();
		return true;
	}
	bool emitTessellationEvaluation(std::ostringstream& out, const rtsl::ir::EntryPoint& entry, const rtsl::ir::Function& function) {
		const auto count = matchingControlPointCount(entry); if (!count) return false;
		const auto* configuration = std::get_if<rtsl::ir::TessellationEvaluationConfiguration>(&entry.configuration);
		if (!configuration) return fail("entry", "tessellation-evaluation configuration is missing");
		const auto* element = patchElement(function); if (!element) return false;
		const std::string element_name = typeName(module, element->id, error); if (element_name.empty()) return false;
		const std::string result_name = typeName(module, function.return_type, error); if (result_name.empty()) return false;
		if (!emitTessellationFactorType(out, *configuration)) return false;
		const std::string input = valueName(function.parameters[0].value);
		out << "[domain(\"" << tessellationDomain(*configuration) << "\")]\n" << result_name << " main(rtsl_tessellation_factors rtsl_factors, float2 rtsl_domain_location : SV_DomainLocation, OutputPatch<" << element_name << ", " << *count << "> " << input << ") {\n";
		entry_patch_parameter = function.parameters[0].value; entry_patch_name = input; entry_coordinate_name = configuration->domain == rtsl::ir::TessellationDomain::tessellation_domain_isolines ? "rtsl_domain_location.x" : "rtsl_domain_location";
		suppress_returns = false;
		if (!emitEntryFunctionBody(out, function, 1)) return false;
		out << "}\n";
		clearEntryLowering();
		return true;
	}
	bool emitGeometry(std::ostringstream& out, const rtsl::ir::EntryPoint& entry, const rtsl::ir::Function& function) {
		const auto* configuration = std::get_if<rtsl::ir::GeometryConfiguration>(&entry.configuration);
		if (!configuration || configuration->input != rtsl::ir::PrimitiveTopology::primitive_triangles || configuration->output != rtsl::ir::PrimitiveTopology::primitive_triangle_strip || configuration->maximum_vertices == 0)
			return fail("entry", "D3D12 geometry lowering requires triangle input, triangle-strip output, and a non-zero maximum_vertices RTIR configuration");
		if (function.parameters.size() != 1) return fail("entry", "geometry entry must have one primitive parameter");
		const auto* primitive = module.findType(function.parameters[0].type);
		if (!primitive || primitive->kind != rtsl::ir::TypeKind::type_primitive) return fail("entry", "geometry entry parameter is not an RTIR primitive type");
		const std::string element = typeName(module, primitive->element_type, error); if (element.empty()) return false;
		const std::string input = valueName(function.parameters[0].value);
		out << "[instance(" << configuration->invocations << ")]\n[maxvertexcount(" << configuration->maximum_vertices << ")]\nvoid main(triangle " << element << " " << input << "[3], inout TriangleStream<" << element << "> rtsl_stream) {\n";
		entry_geometry_stream = "rtsl_stream"; suppress_returns = true;
		if (!emitEntryFunctionBody(out, function, 1)) return false;
		out << "}\n";
		clearEntryLowering();
		return true;
	}
	bool emitCompute(std::ostringstream& out, const rtsl::ir::EntryPoint& entry, const rtsl::ir::Function& function) {
		const auto* configuration = std::get_if<rtsl::ir::ComputeConfiguration>(&entry.configuration);
		if (!configuration || configuration->workgroup_size[0] == 0 || configuration->workgroup_size[1] == 0 || configuration->workgroup_size[2] == 0)
			return fail("entry", "compute entry requires a non-zero RTIR workgroup_size configuration");
		if (function.parameters.size() != 3)
			return fail("entry", "compute entry requires three global-invocation RTIR parameters");
		const auto* return_type = module.findType(function.return_type);
		if (!return_type || return_type->kind != rtsl::ir::TypeKind::type_void)
			return fail("entry", "D3D12 compute entry must return void");
		out << "[numthreads(" << configuration->workgroup_size[0] << ", " << configuration->workgroup_size[1] << ", " << configuration->workgroup_size[2] << ")]\n";
		out << "void main(uint3 rtsl_global_invocation : SV_DispatchThreadID) { " << symbolName(module, function.symbol) << "(";
		bool seen[3]{};
		for (std::size_t index = 0; index < function.parameters.size(); ++index) {
			if (index) out << ", ";
			const rtsl::ir::Parameter& parameter = function.parameters[index];
			const rtsl::ir::Type* type = module.findType(parameter.type);
			if (!type || type->kind != rtsl::ir::TypeKind::type_unsigned_integer || type->bit_width != 32 || !parameter.builtin)
				return fail("entry", "compute entry parameter must be a usize global-invocation builtin");
			std::uint32_t component{};
			switch (*parameter.builtin) {
			case rtsl::ir::Builtin::builtin_global_invocation_x: component = 0; break;
			case rtsl::ir::Builtin::builtin_global_invocation_y: component = 1; break;
			case rtsl::ir::Builtin::builtin_global_invocation_z: component = 2; break;
			default: return fail("entry", "compute entry parameter has an unsupported builtin");
			}
			if (seen[component]) return fail("entry", "compute entry repeats a global-invocation builtin");
			seen[component] = true;
			out << "rtsl_global_invocation." << "xyz"[component];
		}
		if (!seen[0] || !seen[1] || !seen[2]) return fail("entry", "compute entry must provide global invocation x, y, and z");
		out << "); }\n";
		return true;
	}
	bool emitEntryFunctionBody(std::ostringstream& out, const rtsl::ir::Function& function, int indent) {
		geometry_end_markers.clear();
		if (!entry_geometry_stream.empty()) {
			for (const auto& block : function.blocks)
				for (const auto& instruction : block.instructions)
					if (isEndPrimitiveCall(instruction)) geometry_end_markers.insert(instruction.operands[1].value());
		}
		std::unordered_map<std::uint32_t, rtsl::ir::TypeId> value_types;
		for (const auto& parameter : function.parameters) value_types.emplace(parameter.value.value(), parameter.type);
		for (const auto& block : function.blocks) for (const auto& argument : block.arguments) value_types.emplace(argument.value.value(), argument.type);
		for (const auto& block : function.blocks) for (const auto& instruction : block.instructions) if (instruction.result) value_types[instruction.result.value()] = instruction.type;
		std::unordered_map<std::uint32_t, const rtsl::ir::Block*> blocks;
		for (const auto& block : function.blocks) blocks.emplace(block.id.value(), &block);
		return !function.blocks.empty() && emitBlock(out, function.blocks.front(), blocks, value_types, indent);
	}
	void clearEntryLowering() { entry_patch_parameter = {}; entry_patch_name.clear(); entry_current_index.clear(); entry_coordinate_name.clear(); entry_factor_name.clear(); entry_geometry_stream.clear(); geometry_end_markers.clear(); outer_accesses.clear(); suppress_outer_stores = false; suppress_returns = false; }
	bool emitStruct(std::ostringstream& out, const rtsl::ir::Type& type) {
		const std::string name(module.strings.get(type.name)); if (name.empty()) return fail("type", "anonymous structure cannot be emitted to HLSL");
		out << "struct " << name << " {\n";
		std::uint32_t texcoord = 0;
		for (std::size_t index = 0; index < type.members.size(); ++index) {
			const auto& member = type.members[index];
			const std::string member_type = typeName(module, member.type, error);
			const std::string member_name = hlslIdentifier(module.strings.get(member.name));
			if (member_type.empty()) return false;
			out << "  " << member_type << " " << member_name;
			const bool is_position = std::ranges::any_of(type.builtin_members, [index](const rtsl::ir::BuiltinMember& builtin) {
				return builtin.builtin == rtsl::ir::Builtin::builtin_position && !builtin.member_path.empty() && builtin.member_path.front() == index;
			});
			if (is_position) out << " : SV_Position";
			else out << " : TEXCOORD" << texcoord++;
			out << ";\n";
		}
		out << "};\n"; return true;
	}
	bool emitFunction(std::ostringstream& out, const rtsl::ir::Function& function) {
		const std::string name = symbolName(module, function.symbol); if (name.empty()) return fail("function", "function has no symbol name");
		const std::string result_type = typeName(module, function.return_type, error); if (result_type.empty()) return false;
		out << result_type << " " << name << "(";
		for (std::size_t i = 0; i < function.parameters.size(); ++i) { if (i) out << ", "; const std::string type = typeName(module, function.parameters[i].type, error); if (type.empty()) return false; out << type << " " << valueName(function.parameters[i].value); }
		out << ") {\n";
		std::unordered_map<std::uint32_t, rtsl::ir::TypeId> value_types;
		for (const auto& parameter : function.parameters) value_types.emplace(parameter.value.value(), parameter.type);
		for (const auto& block : function.blocks) for (const auto& argument : block.arguments) value_types.emplace(argument.value.value(), argument.type);
		for (const auto& block : function.blocks) for (const auto& instruction : block.instructions) if (instruction.result) value_types[instruction.result.value()] = instruction.type;
		std::unordered_map<std::uint32_t, const rtsl::ir::Block*> blocks;
		for (const auto& block : function.blocks) blocks.emplace(block.id.value(), &block);
		if (function.blocks.empty() || !emitBlock(out, function.blocks.front(), blocks, value_types, 1)) return false;
		out << "}\n"; return true;
	}
	bool emitBlock(std::ostringstream& out, const rtsl::ir::Block& block, const std::unordered_map<std::uint32_t, const rtsl::ir::Block*>& blocks, const std::unordered_map<std::uint32_t, rtsl::ir::TypeId>& value_types, int indent) {
		const std::string pad(static_cast<std::size_t>(indent) * 2, ' ');
		for (const auto& instruction : block.instructions) if (!emitInstruction(out, instruction, value_types, pad)) return false;
		if (!block.terminator) return fail("function", "function has no terminator");
		const auto& term = *block.terminator;
		if (term.kind == rtsl::ir::TerminatorKind::terminator_return) { if (!suppress_returns) out << pad << "return;\n"; return true; }
		if (term.kind == rtsl::ir::TerminatorKind::terminator_return_value && term.operands.size() == 1) { if (!suppress_returns) out << pad << "return " << valueName(term.operands[0]) << ";\n"; return true; }
		if (term.kind != rtsl::ir::TerminatorKind::terminator_conditional_branch || term.operands.size() != 1 || term.successors.size() != 2 || block.merge.kind != rtsl::ir::MergeKind::merge_selection) return fail("terminator", "RTIR control flow is not supported by the D3D12 HLSL translator");
		auto then_it = blocks.find(term.successors[0].block.value()), else_it = blocks.find(term.successors[1].block.value()), merge_it = blocks.find(block.merge.merge_block.value());
		if (then_it == blocks.end() || else_it == blocks.end() || merge_it == blocks.end()) return fail("terminator", "selection references an unknown block");
		const auto& merge = *merge_it->second;
		const bool else_is_merge = else_it->second->id == merge.id;
		if (else_is_merge && term.successors[1].arguments.size() != merge.arguments.size()) return fail("terminator", "selection merge arguments are malformed");
		for (std::size_t index = 0; index < merge.arguments.size(); ++index) {
			const std::string type = typeName(module, merge.arguments[index].type, error);
			if (type.empty()) return false;
			out << pad << type << " " << valueName(merge.arguments[index].value);
			if (else_is_merge) out << " = " << valueName(term.successors[1].arguments[index]);
			out << ";\n";
		}
		out << pad << "if (" << valueName(term.operands[0]) << ") {\n";
		if (!emitSelectionArm(out, *then_it->second, merge, blocks, value_types, indent + 1)) return false;
		if (else_is_merge) out << pad << "}\n";
		else {
			out << pad << "} else {\n";
			if (!emitSelectionArm(out, *else_it->second, merge, blocks, value_types, indent + 1)) return false;
			out << pad << "}\n";
		}
		return emitBlock(out, merge, blocks, value_types, indent);
	}
	bool emitSelectionArm(std::ostringstream& out, const rtsl::ir::Block& arm, const rtsl::ir::Block& merge, const std::unordered_map<std::uint32_t, const rtsl::ir::Block*>& blocks, const std::unordered_map<std::uint32_t, rtsl::ir::TypeId>& value_types, int indent) {
		const std::string pad(static_cast<std::size_t>(indent) * 2, ' ');
		for (const auto& instruction : arm.instructions) if (!emitInstruction(out, instruction, value_types, pad)) return false;
		if (!arm.terminator) return fail("terminator", "selection arm has no terminator");
		const auto& term = *arm.terminator;
		if (term.kind == rtsl::ir::TerminatorKind::terminator_return) {
			if (!suppress_returns) out << pad << "return;\n";
			return true;
		}
		if (term.kind == rtsl::ir::TerminatorKind::terminator_return_value && term.operands.size() == 1) {
			if (!suppress_returns) out << pad << "return " << valueName(term.operands[0]) << ";\n";
			return true;
		}
		if (term.kind == rtsl::ir::TerminatorKind::terminator_branch && term.successors.size() == 1 && term.successors[0].block == merge.id) {
			if (term.successors[0].arguments.size() != merge.arguments.size()) return fail("terminator", "selection merge arguments are malformed");
			for (std::size_t index = 0; index < merge.arguments.size(); ++index) out << pad << valueName(merge.arguments[index].value) << " = " << valueName(term.successors[0].arguments[index]) << ";\n";
			return true;
		}
		if (term.kind == rtsl::ir::TerminatorKind::terminator_conditional_branch && term.operands.size() == 1 && term.successors.size() == 2 && arm.merge.kind == rtsl::ir::MergeKind::merge_selection) {
			auto then_it = blocks.find(term.successors[0].block.value()), else_it = blocks.find(term.successors[1].block.value()), inner_merge_it = blocks.find(arm.merge.merge_block.value());
			if (then_it == blocks.end() || else_it == blocks.end() || inner_merge_it == blocks.end()) return fail("terminator", "nested selection references an unknown block");
			const auto& inner_merge = *inner_merge_it->second;
			for (const auto& argument : inner_merge.arguments) {
				const std::string type = typeName(module, argument.type, error);
				if (type.empty()) return false;
				out << pad << type << " " << valueName(argument.value) << ";\n";
			}
			out << pad << "if (" << valueName(term.operands[0]) << ") {\n";
			if (!emitSelectionArm(out, *then_it->second, inner_merge, blocks, value_types, indent + 1)) return false;
			out << pad << "} else {\n";
			if (!emitSelectionArm(out, *else_it->second, inner_merge, blocks, value_types, indent + 1)) return false;
			out << pad << "}\n";
			return emitSelectionArm(out, inner_merge, merge, blocks, value_types, indent);
		}
		return fail("terminator", "nested or non-structured selection arm is not supported by the D3D12 HLSL translator");
	}
	bool isEndPrimitiveCall(const rtsl::ir::Instruction& instruction) const {
		if (entry_geometry_stream.empty() || instruction.opcode != rtsl::ir::Opcode::opcode_call || instruction.operands.size() != 2) return false;
		const rtsl::ir::Function* function = module.findFunction(instruction.callee);
		if (!function || !function->declaration || !function->implicit || function->parameters.size() != 2 || function->return_type != function->parameters[0].type) return false;
		const rtsl::ir::Type* primitive = module.findType(function->parameters[0].type);
		const rtsl::ir::Type* marker = module.findType(function->parameters[1].type);
		return primitive && primitive->kind == rtsl::ir::TypeKind::type_primitive && marker &&
			marker->kind == rtsl::ir::TypeKind::type_structure && marker->members.empty();
	}
	bool isGeometryEmitCall(const rtsl::ir::Instruction& instruction) const {
		if (entry_geometry_stream.empty() || instruction.opcode != rtsl::ir::Opcode::opcode_call || instruction.operands.size() != 2) return false;
		const rtsl::ir::Function* function = module.findFunction(instruction.callee);
		if (!function || !function->declaration || !function->implicit || function->parameters.size() != 2 || function->return_type != function->parameters[0].type) return false;
		const rtsl::ir::Type* primitive = module.findType(function->parameters[0].type);
		return primitive && primitive->kind == rtsl::ir::TypeKind::type_primitive &&
			function->parameters[1].type == primitive->element_type;
	}
	bool isPositionXYCall(const rtsl::ir::Instruction& instruction) const {
		if (instruction.opcode != rtsl::ir::Opcode::opcode_call || instruction.operands.size() != 1) return false;
		const rtsl::ir::Function* function = module.findFunction(instruction.callee);
		if (!function || !function->declaration || !function->implicit || function->parameters.size() != 1) return false;
		const rtsl::ir::Type* result = module.findType(function->return_type);
		const rtsl::ir::Type* argument = module.findType(function->parameters[0].type);
		const rtsl::ir::Type* scalar = result ? module.findType(result->element_type) : nullptr;
		return result && argument && scalar && result->kind == rtsl::ir::TypeKind::type_vector &&
			argument->kind == rtsl::ir::TypeKind::type_vector && result->element_count == 2 &&
			argument->element_count == 4 && result->element_type == argument->element_type &&
			scalar->kind == rtsl::ir::TypeKind::type_floating;
	}
	bool emitInstruction(std::ostringstream& out, const rtsl::ir::Instruction& ins, const std::unordered_map<std::uint32_t, rtsl::ir::TypeId>& value_types, std::string_view pad) {
		const rtsl::ir::Type* instruction_type = module.findType(ins.type);
		const bool geometry_primitive = !entry_geometry_stream.empty() && instruction_type &&
			instruction_type->kind == rtsl::ir::TypeKind::type_primitive;
		const bool geometry_end_marker = geometry_end_markers.contains(ins.result.value());
		const bool resultless = ins.opcode == rtsl::ir::Opcode::opcode_store ||
			ins.opcode == rtsl::ir::Opcode::opcode_resource_store || geometry_primitive || geometry_end_marker;
		const std::string type = resultless ? std::string{} : typeName(module, ins.type, error);
		if (type.empty() && !resultless) return false;
		auto binary = [&](const char* op) { if (ins.operands.size() != 2) return fail("instruction", "binary instruction has invalid operand count"); out << pad << type << " " << valueName(ins.result) << " = " << valueName(ins.operands[0]) << " " << op << " " << valueName(ins.operands[1]) << ";\n"; return true; };
		auto valueType = [&](rtsl::ir::ValueId value) {
			auto found = value_types.find(value.value());
			if (found == value_types.end()) return static_cast<const rtsl::ir::Type*>(nullptr);
			const rtsl::ir::Type* value_type = module.findType(found->second);
			while (value_type && value_type->kind == rtsl::ir::TypeKind::type_pointer) value_type = module.findType(value_type->element_type);
			return value_type;
		};
		switch (ins.opcode) {
		case rtsl::ir::Opcode::opcode_constant_boolean: out << pad << type << " " << valueName(ins.result) << " = " << (ins.immediates.empty() || !ins.immediates[0] ? "false" : "true") << ";\n"; return true;
		case rtsl::ir::Opcode::opcode_constant_integer: if (ins.immediates.empty()) return fail("instruction", "integer constant has no literal"); out << pad << type << " " << valueName(ins.result) << " = " << ins.immediates[0] << ";\n"; return true;
		case rtsl::ir::Opcode::opcode_constant_floating: {
			if (ins.immediates.empty()) return fail("instruction", "floating constant has no literal");
			std::ostringstream literal;
			literal << std::setprecision(std::numeric_limits<float>::max_digits10) << std::showpoint << std::bit_cast<float>(ins.immediates[0]);
			out << pad << type << " " << valueName(ins.result) << " = " << literal.str() << "f;\n";
			return true;
		}
		case rtsl::ir::Opcode::opcode_constant_composite:
		case rtsl::ir::Opcode::opcode_construct: {
			const rtsl::ir::Type* constructed = module.findType(ins.type);
			if (!constructed) return fail("instruction", "construction references an unknown type");
			if (geometry_end_marker) {
				if (constructed->kind != rtsl::ir::TypeKind::type_structure || !constructed->members.empty() || !ins.operands.empty())
					return fail("instruction", "geometry end marker construction is malformed");
				return true;
			}
			// Geometry primitives are RTIR's transient emission accumulator.  They do
			// not correspond to an HLSL value: constructing one emits its supplied
			// vertices into the stage stream.
			if (constructed->kind == rtsl::ir::TypeKind::type_primitive && !entry_geometry_stream.empty()) {
				if (ins.operands.size() > constructed->element_count)
					return fail("instruction", "geometry primitive construction exceeds its maximum vertex count");
				for (const rtsl::ir::ValueId vertex : ins.operands)
					out << pad << entry_geometry_stream << ".Append(" << valueName(vertex) << ");\n";
				return true;
			}
			if (constructed->kind == rtsl::ir::TypeKind::type_structure) {
				if (constructed->members.size() != ins.operands.size()) return fail("instruction", "structure construction has the wrong operand count");
				out << pad << type << " " << valueName(ins.result) << ";\n";
				for (std::size_t i = 0; i < constructed->members.size(); ++i)
					out << pad << valueName(ins.result) << "." << hlslIdentifier(module.strings.get(constructed->members[i].name)) << " = " << valueName(ins.operands[i]) << ";\n";
				return true;
			}
			out << pad << type << " " << valueName(ins.result) << " = " << type << "(";
			for (std::size_t i = 0; i < ins.operands.size(); ++i) { if (i) out << ", "; out << valueName(ins.operands[i]); }
			out << ");\n";
			return true;
		}
		case rtsl::ir::Opcode::opcode_insert: {
			if (!geometry_primitive || ins.operands.size() != 2)
				return fail("instruction", "insert is only supported for a geometry primitive accumulator");
			out << pad << entry_geometry_stream << ".Append(" << valueName(ins.operands[1]) << ");\n";
			return true;
		}
		case rtsl::ir::Opcode::opcode_load: {
			const auto* loaded = module.findType(ins.type);
			if (loaded && loaded->kind == rtsl::ir::TypeKind::type_patch && !entry_patch_name.empty()) return true;
			return fail("instruction", "D3D12 HLSL translator only supports loading the entry patch");
		}
		case rtsl::ir::Opcode::opcode_add: return binary("+"); case rtsl::ir::Opcode::opcode_subtract: return binary("-");
		case rtsl::ir::Opcode::opcode_multiply: {
			if (ins.operands.size() != 2) return fail("instruction", "multiply instruction has invalid operand count");
			const rtsl::ir::Type* left = valueType(ins.operands[0]);
			const rtsl::ir::Type* right = valueType(ins.operands[1]);
			const bool matrix_product = left && right &&
				((left->kind == rtsl::ir::TypeKind::type_matrix && (right->kind == rtsl::ir::TypeKind::type_matrix || right->kind == rtsl::ir::TypeKind::type_vector)) ||
				 (left->kind == rtsl::ir::TypeKind::type_vector && right->kind == rtsl::ir::TypeKind::type_matrix));
			if (!matrix_product) return binary("*");
			out << pad << type << " " << valueName(ins.result) << " = mul(" << valueName(ins.operands[0]) << ", " << valueName(ins.operands[1]) << ");\n";
			return true;
		}
		case rtsl::ir::Opcode::opcode_divide: return binary("/");
		case rtsl::ir::Opcode::opcode_remainder: return binary("%");
		case rtsl::ir::Opcode::opcode_compare_equal: return binary("==");
		case rtsl::ir::Opcode::opcode_compare_not_equal: return binary("!=");
		case rtsl::ir::Opcode::opcode_compare_less: return binary("<");
		case rtsl::ir::Opcode::opcode_compare_less_equal: return binary("<=");
		case rtsl::ir::Opcode::opcode_compare_greater: return binary(">");
		case rtsl::ir::Opcode::opcode_compare_greater_equal: return binary(">=");
		case rtsl::ir::Opcode::opcode_logical_and: return binary("&&");
		case rtsl::ir::Opcode::opcode_logical_or:
			return binary("||");
		case rtsl::ir::Opcode::opcode_logical_not:
			if (ins.operands.size() != 1)
				return fail("instruction", "logical not has invalid operand count");
			out << pad << type << " " << valueName(ins.result) << " = !" << valueName(ins.operands[0]) << ";\n";
			return true;
		case rtsl::ir::Opcode::opcode_negate:
			if (ins.operands.size() != 1)
				return fail("instruction", "negate has invalid operand count");
			out << pad << type << " " << valueName(ins.result) << " = -" << valueName(ins.operands[0]) << ";\n";
			return true;
		case rtsl::ir::Opcode::opcode_convert: {
			if (ins.operands.size() != 1)
				return fail("instruction", "convert has invalid operand count");
			const rtsl::ir::Type* source = valueType(ins.operands[0]);
			const rtsl::ir::Type* result = module.findType(ins.type);
			if (!source || !result)
				return fail("instruction", "convert has an unknown source or result type");
			const rtsl::ir::Type* source_scalar = source->kind == rtsl::ir::TypeKind::type_vector ? module.findType(source->element_type) : source;
			const rtsl::ir::Type* result_scalar = result->kind == rtsl::ir::TypeKind::type_vector ? module.findType(result->element_type) : result;
			const bool source_numeric = source_scalar && (source_scalar->kind == rtsl::ir::TypeKind::type_floating ||
														  source_scalar->kind == rtsl::ir::TypeKind::type_signed_integer || source_scalar->kind == rtsl::ir::TypeKind::type_unsigned_integer);
			const bool result_numeric = result_scalar && (result_scalar->kind == rtsl::ir::TypeKind::type_floating ||
														  result_scalar->kind == rtsl::ir::TypeKind::type_signed_integer || result_scalar->kind == rtsl::ir::TypeKind::type_unsigned_integer);
			const bool matching_shape = source->kind != rtsl::ir::TypeKind::type_vector && result->kind != rtsl::ir::TypeKind::type_vector ||
										source->kind == rtsl::ir::TypeKind::type_vector && result->kind == rtsl::ir::TypeKind::type_vector && source->element_count == result->element_count;
			if (!source_numeric || !result_numeric || !matching_shape)
				return fail("instruction", "convert requires matching scalar or vector numeric types");
			if (source_scalar->bit_width != 32 || result_scalar->bit_width != 32)
				return fail("instruction", "D3D12 HLSL conversion requires 32-bit numeric source and result types");
			if (source->id == result->id)
				return fail("instruction", "convert does not change the numeric type");
			out << pad << type << " " << valueName(ins.result) << " = " << type << "(" << valueName(ins.operands[0]) << ");\n";
			return true;
		}
		case rtsl::ir::Opcode::opcode_extract:
			if (ins.operands.size() != 1 || ins.immediates.empty())
				return fail("instruction", "extract is malformed");
			out << pad << type << " " << valueName(ins.result) << " = " << valueName(ins.operands[0]) << "[" << ins.immediates[0] << "];\n";
			return true;
		case rtsl::ir::Opcode::opcode_access: {
			if (ins.operands.empty())
				return fail("instruction", "access has no base operand");
			auto found = value_types.find(ins.operands[0].value());
			if (found == value_types.end())
				return fail("instruction", "access base type is unavailable");
			const rtsl::ir::Type* base = module.findType(found->second);
			if (!base)
				return fail("instruction", "access base type is unknown");
			if (base->kind == rtsl::ir::TypeKind::type_pointer)
				base = module.findType(base->element_type);
			if (base && base->kind == rtsl::ir::TypeKind::type_patch && ins.immediates.size() == 1) {
				const std::string_view member = module.strings.get(static_cast<rtsl::ir::StringId>(ins.immediates[0]));
				if (member == "current" && !entry_current_index.empty()) {
					out << pad << type << " " << valueName(ins.result) << " = " << entry_patch_name << "[" << entry_current_index << "];\n";
					return true;
				}
				if (member == "coordinate" && !entry_coordinate_name.empty()) {
					out << pad << type << " " << valueName(ins.result) << " = " << entry_coordinate_name << ";\n";
					return true;
				}
				if (member == "outer" && ins.operands.size() == 2 && !entry_factor_name.empty()) {
					outer_accesses.emplace(ins.result.value(), entry_factor_name + ".outer[" + valueName(ins.operands[1]) + "]");
					return true;
				}
				return fail("instruction", "unsupported patch member access for this D3D12 stage: " + std::string(member));
			}
			out << pad << type << " " << valueName(ins.result) << " = " << valueName(ins.operands[0]);
			if (base->kind == rtsl::ir::TypeKind::type_structure && !ins.immediates.empty()) {
				const auto member = static_cast<rtsl::ir::StringId>(ins.immediates[0]);
				out << "." << hlslIdentifier(module.strings.get(member));
			} else if (base->kind == rtsl::ir::TypeKind::type_vector && ins.immediates.size() == 1) {
				const std::string_view component = module.strings.get(static_cast<rtsl::ir::StringId>(ins.immediates[0]));
				if (component.size() != 1 || std::string_view{"xyzw"}.find(component[0]) == std::string_view::npos)
					return fail("instruction", "vector access has an invalid component '" + std::string(component) + "'");
				out << "." << component;
			} else if (ins.immediates.empty() && ins.operands.size() == 2) out << "[" << valueName(ins.operands[1]) << "]";
			else return fail("instruction", "access form is unsupported");
			out << ";\n"; return true;
		}
		case rtsl::ir::Opcode::opcode_store: {
			if (ins.operands.size() != 2) return fail("instruction", "store has invalid operand count");
			const auto outer = outer_accesses.find(ins.operands[0].value());
			if (outer != outer_accesses.end()) {
				if (!suppress_outer_stores) out << pad << outer->second << " = " << valueName(ins.operands[1]) << ";\n";
				return true;
			}
			return fail("instruction", "D3D12 HLSL translator only supports stores to indexed patch outer levels");
		}
		case rtsl::ir::Opcode::opcode_resource_load: {
			if (ins.immediates.size() != 1) return fail("instruction", "resource load is malformed");
			const auto storage = storage_offsets.find(ins.immediates[0]);
			if (storage != storage_offsets.end()) {
				auto expression = storageLoad(ins.type, storage->second);
				if (!expression) return fail("instruction", "storage load type is not supported by the D3D12 HLSL translator");
				out << pad << type << " " << valueName(ins.result) << " = " << *expression << ";\n";
				return true;
			}
			for (const rtsl::ir::Uniform& uniform : module.uniforms) {
				if (uniform.symbol.value() != ins.immediates[0]) continue;
				if (!ins.operands.empty()) return fail("instruction", "uniform resource load must not have dynamic operands");
				const std::string name = symbolName(module, uniform.symbol);
				if (name.empty()) return fail("instruction", "uniform resource load has no symbol name");
				out << pad << type << " " << valueName(ins.result) << " = " << name << ";\n";
				return true;
			}
			const rtsl::ir::Resource* resource{};
			for (const auto& candidate : module.resources) if (candidate.symbol.value() == ins.immediates[0]) { resource = &candidate; break; }
			if (!resource) return fail("instruction", "resource load references an unknown symbol");
			const std::string name = symbolName(module, resource->symbol);
			const rtsl::ir::Type* resource_type = module.findType(resource->type);
			if (!resource_type || name.empty()) return fail("instruction", "resource load has an unknown resource type");
			if (resource->kind == rtsl::ir::ResourceKind::resource_storage_buffer) {
				if (resource_type->parameter_types.size() != 2 || ins.operands.size() > 1) return fail("instruction", "buffer resource load has an invalid RTIR shape");
				auto layout = storageLayout(module, ins.operands.empty() ? resource_type->parameter_types[0] : resource_type->parameter_types[1]);
				const std::string offset = ins.operands.empty() ? "0" : std::to_string(storageLayout(module, resource_type->parameter_types[0])->size) + " + " + valueName(ins.operands[0]) + " * " + std::to_string(layout ? layout->size : 0);
				auto expression = byteAddressLoad(name, ins.type, offset);
				if (!expression) return fail("instruction", "buffer resource load type is unsupported");
				out << pad << type << " " << valueName(ins.result) << " = " << *expression << ";\n"; return true;
			}
			if (resource->kind == rtsl::ir::ResourceKind::resource_storage_texture) {
				if (ins.operands.empty() || ins.operands.size() > 3) return fail("instruction", "image resource load has an invalid coordinate count");
				out << pad << type << " " << valueName(ins.result) << " = " << name << "[";
				if (ins.operands.size() == 1) out << valueName(ins.operands[0]); else { out << "uint" << ins.operands.size() << "("; for (std::size_t i = 0; i < ins.operands.size(); ++i) { if (i) out << ", "; out << valueName(ins.operands[i]); } out << ")"; }
				out << "];\n"; return true;
			}
			return fail("instruction", "resource load uses an unsupported resource kind");
		}
		case rtsl::ir::Opcode::opcode_resource_store: {
			if (ins.immediates.size() != 1 || ins.operands.size() < 2) return fail("instruction", "resource store is malformed");
			const rtsl::ir::Resource* resource{}; for (const auto& candidate : module.resources) if (candidate.symbol.value() == ins.immediates[0]) { resource = &candidate; break; }
			if (!resource) return fail("instruction", "resource store references an unknown symbol");
			const std::string name = symbolName(module, resource->symbol);
			if (resource->kind == rtsl::ir::ResourceKind::resource_storage_texture) {
				const std::size_t extent = ins.operands.size() - 1; if (extent < 1 || extent > 3) return fail("instruction", "image resource store has an invalid coordinate count");
				out << pad << name << "["; if (extent == 1) out << valueName(ins.operands[0]); else { out << "uint" << extent << "("; for (std::size_t i = 0; i < extent; ++i) { if (i) out << ", "; out << valueName(ins.operands[i]); } out << ")"; } out << "] = " << valueName(ins.operands.back()) << ";\n"; return true;
			}
			if (resource->kind == rtsl::ir::ResourceKind::resource_storage_buffer && ins.operands.size() == 2) {
				const rtsl::ir::Type* resource_type = module.findType(resource->type);
				const auto value_type = value_types.find(ins.operands[1].value());
				if (!resource_type || resource_type->parameter_types.size() != 2 || value_type == value_types.end()) return fail("instruction", "buffer resource store has an invalid RTIR shape");
				auto header = storageLayout(module, resource_type->parameter_types[0]); auto element = storageLayout(module, resource_type->parameter_types[1]); const rtsl::ir::Type* stored = module.findType(value_type->second);
				if (!header || !element || !stored) return fail("instruction", "buffer resource store has an unsupported type");
				const std::string offset = std::to_string(header->size) + " + " + valueName(ins.operands[0]) + " * " + std::to_string(element->size);
				if (stored->kind == rtsl::ir::TypeKind::type_unsigned_integer) out << pad << name << ".Store(" << offset << ", " << valueName(ins.operands[1]) << ");\n";
				else if (stored->kind == rtsl::ir::TypeKind::type_signed_integer || stored->kind == rtsl::ir::TypeKind::type_floating) out << pad << name << ".Store(" << offset << ", asuint(" << valueName(ins.operands[1]) << "));\n";
				else if (stored->kind == rtsl::ir::TypeKind::type_vector && stored->element_count >= 2 && stored->element_count <= 4) out << pad << name << ".Store" << stored->element_count << "(" << offset << ", asuint(" << valueName(ins.operands[1]) << "));\n";
				else return fail("instruction", "buffer resource store type is unsupported");
				return true;
			}
			return fail("instruction", "resource store uses an unsupported resource kind");
		}
		case rtsl::ir::Opcode::opcode_resource_query: {
			if (ins.immediates.size() != 1 || !ins.operands.empty()) return fail("instruction", "resource query is malformed");
			const rtsl::ir::Resource* resource{}; for (const auto& candidate : module.resources) if (candidate.symbol.value() == ins.immediates[0]) { resource = &candidate; break; }
			const std::string name = resource ? symbolName(module, resource->symbol) : std::string{};
			const rtsl::ir::Type* result = module.findType(ins.type); const std::uint32_t extent = result && result->kind == rtsl::ir::TypeKind::type_vector ? result->element_count : 1;
			if (!resource || resource->kind != rtsl::ir::ResourceKind::resource_storage_texture || extent < 1 || extent > 3) return fail("instruction", "resource query requires a storage image extent type");
			out << pad << type << " " << valueName(ins.result) << "; ";
			if (extent == 1) out << name << ".GetDimensions(" << valueName(ins.result) << ");\n";
			else { out << name << ".GetDimensions("; for (std::uint32_t i = 0; i < extent; ++i) { if (i) out << ", "; out << valueName(ins.result) << "." << "xyz"[i]; } out << ");\n"; }
			return true;
		}
		case rtsl::ir::Opcode::opcode_resource_sample: {
			if (ins.immediates.size() != 1 || ins.operands.size() != 1) return fail("instruction", "resource sample is malformed");
			const rtsl::ir::Resource* resource{}; for (const auto& candidate : module.resources) if (candidate.symbol.value() == ins.immediates[0]) { resource = &candidate; break; }
			if (resource && resource->kind == rtsl::ir::ResourceKind::resource_sampled_texture) {
				const std::string name = symbolName(module, resource->symbol);
				out << pad << type << " " << valueName(ins.result) << " = " << name << ".Sample(" << name << "_sampler, " << valueName(ins.operands[0]) << ");\n";
				return true;
			}
			if (!resource || resource->kind != rtsl::ir::ResourceKind::resource_storage_texture) return fail("instruction", "resource sample requires a storage image");
			const rtsl::ir::Type* resource_type = module.findType(resource->type);
			if (!resource_type || module.strings.get(resource_type->name) != "image_2d" || resource_type->parameter_types.size() != 1) return fail("instruction", "resource sample requires image_2d<T>");
			const std::string name = symbolName(module, resource->symbol);
			out << pad << type << " " << valueName(ins.result) << " = " << name << "_sampled.Sample(" << name << "_sampler, " << valueName(ins.operands[0]) << ");\n";
			return true;
		}
		case rtsl::ir::Opcode::opcode_call: {
			if (isEndPrimitiveCall(ins)) {
				out << pad << entry_geometry_stream << ".RestartStrip();\n";
				return true;
			}
			if (isGeometryEmitCall(ins)) {
				out << pad << entry_geometry_stream << ".Append(" << valueName(ins.operands[1]) << ");\n";
				return true;
			}
			if (isPositionXYCall(ins)) {
				out << pad << type << " " << valueName(ins.result) << " = " << valueName(ins.operands[0]) << ".xy;\n";
				return true;
			}
			const auto* callee = module.findFunction(ins.callee); if (!callee) return fail("instruction", "call references unknown function"); out << pad << type << " " << valueName(ins.result) << " = " << symbolName(module, callee->symbol) << "("; for(std::size_t i=0;i<ins.operands.size();++i) { if(i) out << ", "; out << valueName(ins.operands[i]); } out << ");\n"; return true;
		}
		default: return fail("instruction", "RTIR opcode is not supported by the D3D12 HLSL translator");
		}
	}
	const rtsl::ir::Module& module; Error& error;
	rtsl::ir::ValueId entry_patch_parameter{};
	std::string entry_patch_name;
	std::string entry_current_index;
	std::string entry_coordinate_name;
	std::string entry_factor_name;
	std::string entry_geometry_stream;
	std::unordered_map<std::uint32_t, std::uint32_t> storage_offsets;
	std::uint32_t storage_block_size{};
	std::unordered_set<std::uint32_t> geometry_end_markers;
	std::unordered_map<std::uint32_t, std::string> outer_accesses;
	bool suppress_outer_stores{};
	bool suppress_returns{};
};
}

std::optional<Translation> transpile(const rtsl::ir::Module& module, const rtsl::ir::EntryPoint& entry, Error& error) {
	const auto verification = rtsl::ir::verify(module);
	if (!verification.valid()) { const auto& issue = verification.issues().front(); error = {issue.context, issue.message}; return std::nullopt; }
	std::ostringstream source; Emitter emitter(module, error);
	if (!emitter.emit(source, entry)) return std::nullopt;
	return Translation{source.str(), "main"};
}

} // namespace rtd3d12::hlsl
