#include "transpiler.h"

#include <rtsl/IR/Verifier.hpp>
#include <rtsl/Serialization/Artifact.hpp>
#include <spirv-tools/libspirv.h>

#include <algorithm>
#include <bit>
#include <cstring>
#include <limits>
#include <new>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

struct rt_spirv_stage_binary {
	std::vector<std::uint32_t> words;
	std::string entry_point;
};

struct rt_spirv_owned_location {
	std::string name;
	rt_spirv_location_info info{};
};

struct rt_spirv_program {
	rt_spirv_stage_binary stages[RT_SPIRV_STAGE_COUNT];
	std::vector<rt_spirv_owned_location> locations;
};

namespace rutile::spirv {

std::string_view opcodeName(rtsl::ir::Opcode opcode) {
	switch (opcode) {
	case rtsl::ir::Opcode::opcode_undef: return "undef";
	case rtsl::ir::Opcode::opcode_constant_boolean: return "constant_boolean";
	case rtsl::ir::Opcode::opcode_constant_integer: return "constant_integer";
	case rtsl::ir::Opcode::opcode_constant_floating: return "constant_floating";
	case rtsl::ir::Opcode::opcode_constant_composite: return "constant_composite";
	case rtsl::ir::Opcode::opcode_variable: return "variable";
	case rtsl::ir::Opcode::opcode_load: return "load";
	case rtsl::ir::Opcode::opcode_store: return "store";
	case rtsl::ir::Opcode::opcode_access: return "access";
	case rtsl::ir::Opcode::opcode_add: return "add";
	case rtsl::ir::Opcode::opcode_subtract: return "subtract";
	case rtsl::ir::Opcode::opcode_multiply: return "multiply";
	case rtsl::ir::Opcode::opcode_divide: return "divide";
	case rtsl::ir::Opcode::opcode_remainder: return "remainder";
	case rtsl::ir::Opcode::opcode_negate: return "negate";
	case rtsl::ir::Opcode::opcode_compare_equal: return "compare_equal";
	case rtsl::ir::Opcode::opcode_compare_not_equal: return "compare_not_equal";
	case rtsl::ir::Opcode::opcode_compare_less: return "compare_less";
	case rtsl::ir::Opcode::opcode_compare_less_equal: return "compare_less_equal";
	case rtsl::ir::Opcode::opcode_compare_greater: return "compare_greater";
	case rtsl::ir::Opcode::opcode_compare_greater_equal: return "compare_greater_equal";
	case rtsl::ir::Opcode::opcode_logical_and: return "logical_and";
	case rtsl::ir::Opcode::opcode_logical_or: return "logical_or";
	case rtsl::ir::Opcode::opcode_logical_not: return "logical_not";
	case rtsl::ir::Opcode::opcode_convert: return "convert";
	case rtsl::ir::Opcode::opcode_bitcast: return "bitcast";
	case rtsl::ir::Opcode::opcode_construct: return "construct";
	case rtsl::ir::Opcode::opcode_extract: return "extract";
	case rtsl::ir::Opcode::opcode_insert: return "insert";
	case rtsl::ir::Opcode::opcode_call: return "call";
	case rtsl::ir::Opcode::opcode_barrier: return "barrier";
	case rtsl::ir::Opcode::opcode_memory_barrier: return "memory_barrier";
	case rtsl::ir::Opcode::opcode_resource_load: return "resource_load";
	case rtsl::ir::Opcode::opcode_resource_store: return "resource_store";
	case rtsl::ir::Opcode::opcode_resource_sample: return "resource_sample";
	case rtsl::ir::Opcode::opcode_resource_query: return "resource_query";
	case rtsl::ir::Opcode::opcode_derivative: return "derivative";
	case rtsl::ir::Opcode::opcode_discard: return "discard";
	}
	return "unknown";
}

std::uint32_t roundUp(std::uint32_t value, std::uint32_t alignment) {
	return (value + alignment - 1u) / alignment * alignment;
}

struct UniformLayout {
	std::uint32_t alignment{};
	std::uint32_t size{};
};

UniformLayout uniformLayout(const rtsl::ir::Module& module, rtsl::ir::TypeId type_id) {
	const rtsl::ir::Type* type = module.findType(type_id);
	if (!type) throw std::runtime_error("uniform layout references an unknown RTIR type");
	switch (type->kind) {
	case rtsl::ir::TypeKind::type_boolean:
	case rtsl::ir::TypeKind::type_signed_integer:
	case rtsl::ir::TypeKind::type_unsigned_integer:
	case rtsl::ir::TypeKind::type_floating:
		return {.alignment = 4, .size = 4};
	case rtsl::ir::TypeKind::type_vector: {
		const UniformLayout element = uniformLayout(module, type->element_type);
		if (type->element_count == 2) return {.alignment = 8, .size = element.size * 2};
		if (type->element_count == 3 || type->element_count == 4) return {.alignment = 16, .size = 16};
		throw std::runtime_error("uniform layout requires vectors with two to four components");
	}
	case rtsl::ir::TypeKind::type_matrix:
		return {.alignment = 16, .size = 16 * type->element_count};
	case rtsl::ir::TypeKind::type_array: {
		const UniformLayout element = uniformLayout(module, type->element_type);
		const std::uint32_t stride = roundUp(element.size, 16);
		return {.alignment = 16, .size = stride * type->element_count};
	}
	case rtsl::ir::TypeKind::type_structure: {
		std::uint32_t alignment = 16;
		std::uint32_t offset{};
		for (const rtsl::ir::StructMember& member : type->members) {
			const UniformLayout member_layout = uniformLayout(module, member.type);
			alignment = std::max(alignment, member_layout.alignment);
			offset = roundUp(offset, member_layout.alignment);
			offset += member_layout.size;
		}
		return {.alignment = alignment, .size = roundUp(offset, alignment)};
	}
	default:
		throw std::runtime_error("RTIR type cannot be placed in a uniform block");
	}
}

class ModuleBuilder {
public:
	ModuleBuilder(const rtsl::ir::Module& module, const rtsl::ir::EntryPoint& entry) : module(module), entry(entry) {}

	std::vector<std::uint32_t> build() {
		instruction(17, {1});
		if (std::ranges::any_of(module.resources, [](const rtsl::ir::Resource& resource) { return resource.kind == rtsl::ir::ResourceKind::resource_storage_texture; })) instruction(17, {50});
		if (std::ranges::any_of(module.resources, [](const rtsl::ir::Resource& resource) { return resource.kind == rtsl::ir::ResourceKind::resource_storage_texture; })) instruction(17, {55});
		if (std::ranges::any_of(module.resources, [](const rtsl::ir::Resource& resource) { return resource.kind == rtsl::ir::ResourceKind::resource_storage_texture; })) instruction(17, {56});
		if (entry.stage == rtsl::ir::Stage::stage_tessellation_control || entry.stage == rtsl::ir::Stage::stage_tessellation_evaluation)
			instruction(17, {3}); // Tessellation
		if (entry.stage == rtsl::ir::Stage::stage_geometry) instruction(17, {2}); // Geometry
		instruction(14, {0, 1});
		const rtsl::ir::Function* stage_function = module.findFunction(entry.function);
		if (!stage_function) throw std::runtime_error("entry point references an unknown RTIR function");
		if (entry.stage == rtsl::ir::Stage::stage_compute) {
			if (stage_function->parameters.size() != 3) throw std::runtime_error("compute entry requires exactly three global-invocation parameters");
			std::uint32_t components{};
			for (const auto& parameter : stage_function->parameters) {
				const rtsl::ir::Type* type = module.findType(parameter.type);
				if (!parameter.builtin || !type || type->kind != rtsl::ir::TypeKind::type_unsigned_integer || type->bit_width != 32) throw std::runtime_error("compute entry parameter must be a usize global-invocation builtin");
				const std::uint32_t component = *parameter.builtin == rtsl::ir::Builtin::builtin_global_invocation_x ? 0 : *parameter.builtin == rtsl::ir::Builtin::builtin_global_invocation_y ? 1 : *parameter.builtin == rtsl::ir::Builtin::builtin_global_invocation_z ? 2 : 3;
				if (component == 3 || components & (1u << component)) throw std::runtime_error("compute entry global-invocation builtins must be x, y, and z exactly once");
				components |= 1u << component;
			}
			if (components != 7) throw std::runtime_error("compute entry global-invocation builtins must be x, y, and z exactly once");
		}
		declareFunctions();
		buildInterfaces(*stage_function);
		if (entry.stage == rtsl::ir::Stage::stage_compute) declareComputeInvocationId();
		declareTessellationBuiltins();
		declareResources();
		declareUniforms();
		declareStorageObjects();
		const std::uint32_t wrapper_type = functionType(typeVoid(), {});
		wrapper_function = id();
		emitEntryPoint(wrapper_function, interface_ids);
		emitExecutionModes(wrapper_function);
		std::unordered_set<std::uint32_t> reachable_functions;
		const auto collect_reachable = [&](const auto& self, rtsl::ir::FunctionId function_id) -> void {
			if (!reachable_functions.insert(function_id.value()).second) return;
			const rtsl::ir::Function* function = module.findFunction(function_id);
			if (!function) throw std::runtime_error("RTIR call references an unknown function");
			for (const rtsl::ir::Block& block : function->blocks)
				for (const rtsl::ir::Instruction& instruction : block.instructions)
					if (instruction.opcode == rtsl::ir::Opcode::opcode_call) self(self, instruction.callee);
		};
		collect_reachable(collect_reachable, stage_function->id);
		for (const rtsl::ir::Function& function : module.functions)
			if (!function.declaration && reachable_functions.contains(function.id.value())) emitFunction(function);
		emitWrapper(*stage_function, wrapper_type);
		std::vector<std::uint32_t> result{0x07230203, 0x00010300, 0, next_id, 0};
		for (const std::vector<std::uint32_t>* section : {&preamble, &entry_points, &execution_modes, &debug, &annotations, &types_constants, &functions}) result.insert(result.end(), section->begin(), section->end());
		return result;
	}

private:
	std::uint32_t id() { return next_id++; }

	void instruction(std::uint16_t opcode, std::initializer_list<std::uint32_t> operands) {
		std::vector<std::uint32_t>& output = section(opcode);
		output.push_back((static_cast<std::uint32_t>(operands.size()) + 1u) << 16 | opcode);
		output.insert(output.end(), operands.begin(), operands.end());
	}

	void instruction(std::uint16_t opcode, const std::vector<std::uint32_t>& operands) {
		std::vector<std::uint32_t>& output = section(opcode);
		output.push_back((static_cast<std::uint32_t>(operands.size()) + 1u) << 16 | opcode);
		output.insert(output.end(), operands.begin(), operands.end());
	}

	std::vector<std::uint32_t>& section(std::uint16_t opcode) {
		if (opcode == 17 || opcode == 14) return preamble;
		if (opcode == 15) return entry_points;
		if (opcode == 16) return execution_modes;
		if (opcode == 5) return debug;
		if (opcode == 71 || opcode == 72) return annotations;
		if ((opcode >= 54 && opcode <= 125) ||
			(opcode >= 126 && opcode <= 190) || opcode == 218 || opcode == 219 || opcode == 224 || (opcode >= 245 && opcode <= 254))
			return functions;
		return types_constants;
	}

	void appendString(std::vector<std::uint32_t>& operands, std::string_view text) {
		const std::size_t words = (text.size() + 1u + 3u) / 4u;
		const std::size_t start = operands.size();
		operands.resize(start + words);
		std::memcpy(operands.data() + start, text.data(), text.size());
	}

	std::uint32_t typeVoid() {
		if (!void_type) { void_type = id(); instruction(19, {void_type}); }
		return void_type;
	}

	std::uint32_t typeFloat() {
		if (!float_type) { float_type = id(); instruction(22, {float_type, 32}); }
		return float_type;
	}

	std::uint32_t typeUnsignedInteger() {
		return typeInteger(32, false);
	}

	std::uint32_t typeInteger(std::uint32_t bit_width, bool is_signed) {
		const std::uint64_t key = static_cast<std::uint64_t>(bit_width) << 1 |
			static_cast<std::uint64_t>(is_signed);
		if (const auto found = integer_types.find(key); found != integer_types.end()) return found->second;
		const std::uint32_t result = id();
		instruction(21, {result, bit_width, is_signed ? 1u : 0u});
		integer_types.emplace(key, result);
		return result;
	}

	std::uint32_t typeFor(rtsl::ir::TypeId type_id) {
		if (const auto found = types.find(type_id.value()); found != types.end()) return found->second;
		const rtsl::ir::Type* type = module.findType(type_id);
		if (!type) return typeFloat();
		std::uint32_t result{};
		switch (type->kind) {
		case rtsl::ir::TypeKind::type_void:
			result = typeVoid(); break;
		case rtsl::ir::TypeKind::type_boolean:
			result = id(); instruction(20, {result}); break;
		case rtsl::ir::TypeKind::type_signed_integer:
		case rtsl::ir::TypeKind::type_unsigned_integer:
			result = typeInteger(type->bit_width ? type->bit_width : 32u,
				type->kind == rtsl::ir::TypeKind::type_signed_integer);
			break;
		case rtsl::ir::TypeKind::type_floating:
			if (!type->bit_width || type->bit_width == 32) result = typeFloat();
			else { result = id(); instruction(22, {result, type->bit_width}); }
			break;
		case rtsl::ir::TypeKind::type_vector:
			result = typeVector(typeFor(type->element_type), type->element_count); break;
		case rtsl::ir::TypeKind::type_matrix:
			result = typeMatrix(typeVector(typeFor(type->element_type), type->element_count), type->element_count); break;
		case rtsl::ir::TypeKind::type_structure: {
			result = id();
			std::vector<std::uint32_t> operands{result};
			for (const rtsl::ir::StructMember& member : type->members) operands.push_back(typeFor(member.type));
			instruction(30, operands);
			break;
		}
		case rtsl::ir::TypeKind::type_patch:
		case rtsl::ir::TypeKind::type_pointer:
			// Patches are stage interface handles in RTIR.  Their value in a function
			// signature is the control-point value; access operations select the
			// corresponding tessellation built-ins or per-vertex interface arrays.
			result = typeFor(type->element_type);
			break;
		case rtsl::ir::TypeKind::type_primitive:
			if (!type->element_type || !type->element_count) throw std::runtime_error("RTIR primitive type has no element type or maximum vertex count");
			result = typeArray(typeFor(type->element_type), type->element_count);
			break;
		default:
			throw std::runtime_error("RTIR type is not supported by the SPIR-V translator");
		}
		types.emplace(type_id.value(), result);
		return result;
	}

	std::uint32_t tessellationControlInvocations() const {
		if (entry.stage != rtsl::ir::Stage::stage_tessellation_control)
			throw std::runtime_error("tessellation-control invocation count requested for a non-control entry point");
		const auto* configuration = std::get_if<rtsl::ir::TessellationControlConfiguration>(&entry.configuration);
		if (!configuration || configuration->output_control_points == 0)
			throw std::runtime_error("tessellation-control entry requires a non-zero RTIR output_control_points configuration");
		return configuration->output_control_points;
	}

	std::uint32_t functionType(std::uint32_t result_type, std::span<const std::uint32_t> parameters) {
		for (const FunctionTypeRecord& record : function_type_records) {
			if (record.result == result_type && std::ranges::equal(record.parameters, parameters)) return record.id;
		}
		std::vector<std::uint32_t> operands{id(), result_type};
		operands.insert(operands.end(), parameters.begin(), parameters.end());
		instruction(33, operands);
		function_type_records.push_back(FunctionTypeRecord{
			result_type,
			std::vector<std::uint32_t>(parameters.begin(), parameters.end()),
			operands[0],
		});
		return operands[0];
	}

	std::uint32_t typeVector(std::uint32_t element, std::uint32_t count) {
		const std::uint64_t key = static_cast<std::uint64_t>(element) << 32 | count;
		if (const auto found = vectors.find(key); found != vectors.end()) return found->second;
		const std::uint32_t result = id();
		instruction(23, {result, element, count});
		vectors.emplace(key, result);
		return result;
	}

	std::uint32_t typeArray(std::uint32_t element, std::uint32_t count) {
		const std::uint64_t key = static_cast<std::uint64_t>(element) << 32 | count;
		if (const auto found = arrays.find(key); found != arrays.end()) return found->second;
		const std::uint32_t result = id();
		instruction(28, {result, element, constantUnsignedInteger(count)});
		arrays.emplace(key, result);
		return result;
	}

	std::uint32_t typeMatrix(std::uint32_t column, std::uint32_t count) {
		const std::uint64_t key = static_cast<std::uint64_t>(column) << 32 | count;
		if (const auto found = matrices.find(key); found != matrices.end()) return found->second;
		const std::uint32_t result = id();
		instruction(24, {result, column, count});
		matrices.emplace(key, result);
		return result;
	}

	std::uint32_t typeSampledImage2D() {
		if (sampled_image_type) return sampled_image_type;
		const std::uint32_t image = id();
		instruction(25, {image, typeFloat(), 1, 0, 0, 0, 1, 0});
		sampled_image_type = id();
		instruction(27, {sampled_image_type, image});
		return sampled_image_type;
	}

	std::uint32_t typeRuntimeArray(std::uint32_t element, std::uint32_t stride) {
		const std::uint64_t key = static_cast<std::uint64_t>(element) << 32 | stride;
		if (const auto found = runtime_arrays.find(key); found != runtime_arrays.end()) return found->second;
		const std::uint32_t result = id();
		instruction(29, {result, element});
		instruction(71, {result, 6, stride});
		runtime_arrays.emplace(key, result);
		return result;
	}

	std::uint32_t typeStorageImage(rtsl::ir::TypeId texel, std::uint32_t dimensions) {
		const std::uint64_t key = static_cast<std::uint64_t>(texel.value()) << 32 | dimensions;
		if (const auto found = storage_images.find(key); found != storage_images.end()) return found->second;
		const std::uint32_t result = id();
		instruction(25, {result, typeFor(texel), dimensions - 1, 0, 0, 0, 2, 0});
		storage_images.emplace(key, result);
		return result;
	}

	const rtsl::ir::Resource& requireResource(std::uint32_t symbol) const {
		const auto found = resources.find(symbol);
		if (found == resources.end()) throw std::runtime_error("RTIR resource operation references an unknown resource");
		return *found->second;
	}

	std::string_view resourceTypeName(const rtsl::ir::Resource& resource) const {
		const rtsl::ir::Type* type = module.findType(resource.type);
		return type ? module.strings.get(type->name) : std::string_view{};
	}

	std::uint32_t resourceStride(rtsl::ir::TypeId type) const {
		const rtsl::ir::Type* value = module.findType(type);
		if (!value) throw std::runtime_error("RTIR resource element type is unknown");
		if (value->kind == rtsl::ir::TypeKind::type_vector && value->element_count == 3) return 16;
		if (value->kind == rtsl::ir::TypeKind::type_vector) return resourceStride(value->element_type) * value->element_count;
		if (value->kind == rtsl::ir::TypeKind::type_structure) {
			std::uint32_t offset{};
			for (const auto& member : value->members) { const std::uint32_t size = resourceStride(member.type); offset = roundUp(offset, size > 8 ? 16 : size); offset += size; }
			return roundUp(offset, 16);
		}
		return value->bit_width ? value->bit_width / 8 : 4;
	}

	std::uint32_t pointerType(std::uint32_t storage_class, std::uint32_t type) {
		const std::uint64_t key = static_cast<std::uint64_t>(storage_class) << 32 | type;
		if (const auto found = pointers.find(key); found != pointers.end()) return found->second;
		const std::uint32_t result = id();
		instruction(32, {result, storage_class, type});
		pointers.emplace(key, result);
		return result;
	}

	std::uint32_t constantFloat(float value) {
		const std::uint32_t bits = std::bit_cast<std::uint32_t>(value);
		if (const auto found = float_constants.find(bits); found != float_constants.end()) return found->second;
		const std::uint32_t result = id();
		instruction(43, {typeFloat(), result, bits});
		float_constants.emplace(bits, result);
		return result;
	}

	std::uint32_t constantUnsignedInteger(std::uint32_t value) {
		if (const auto found = unsigned_integer_constants.find(value); found != unsigned_integer_constants.end()) return found->second;
		const std::uint32_t result = id();
		instruction(43, {typeUnsignedInteger(), result, value});
		unsigned_integer_constants.emplace(value, result);
		return result;
	}

	void name(std::uint32_t target, std::string_view value) {
		std::vector<std::uint32_t> operands{target};
		appendString(operands, value);
		instruction(5, operands);
	}

	struct InterfaceLeaf {
		std::uint32_t parameter_index{};
		std::vector<std::uint32_t> path;
		rtsl::ir::TypeId type;
		std::string name;
		std::string contract;
		std::uint32_t variable{};
		bool builtin_position{};
		bool omitted{};
		std::uint32_t input_array_count{};
	};

	std::string parameterContract(std::uint32_t parameter_index, std::span<const std::uint32_t> path) const {
		for (const rtsl::ir::InterfaceContract& contract : entry.parameter_contracts) {
			if (contract.parameter_index != parameter_index || contract.member_path.size() != path.size()) continue;
			const rtsl::ir::Function* function = module.findFunction(entry.function);
			if (!function || parameter_index >= function->parameters.size()) continue;
			rtsl::ir::TypeId type_id = function->parameters[parameter_index].type;
			bool matches = true;
			for (std::size_t index = 0; index < path.size(); ++index) {
				const rtsl::ir::Type* type = module.findType(type_id);
				if (!type || path[index] >= type->members.size() ||
					module.strings.get(type->members[path[index]].name) != module.strings.get(contract.member_path[index])) {
					matches = false;
					break;
				}
				type_id = type->members[path[index]].type;
			}
			if (matches) return std::string(module.strings.get(contract.contract));
		}
		return {};
	}

	void collectLeaves(std::uint32_t parameter_index, rtsl::ir::TypeId root_type, rtsl::ir::TypeId type_id, std::string name_value, std::vector<std::uint32_t>& path,
		std::vector<InterfaceLeaf>& leaves) {
		const rtsl::ir::Type* type = module.findType(type_id);
		if (type && type->kind == rtsl::ir::TypeKind::type_structure) {
			for (std::uint32_t index = 0; index < type->members.size(); ++index) {
				const rtsl::ir::StructMember& member = type->members[index];
				std::string member_name(module.strings.get(member.name));
				path.push_back(index);
				collectLeaves(parameter_index, root_type, member.type, member_name.empty() ? name_value : member_name, path, leaves);
				path.pop_back();
			}
			return;
		}
		InterfaceLeaf leaf{.parameter_index = parameter_index, .path = path, .type = type_id, .name = name_value};
		if (const rtsl::ir::Type* root = module.findType(root_type)) {
			for (const rtsl::ir::BuiltinMember& builtin : root->builtin_members) {
				if (builtin.builtin == rtsl::ir::Builtin::builtin_position && std::ranges::equal(builtin.member_path, path)) {
					leaf.builtin_position = true;
					break;
				}
			}
		}
		if (const std::string contract = parameterContract(parameter_index, path); !contract.empty()) leaf.contract = contract;
		leaves.push_back(leaf);
	}

	void createInterfaceVariables(std::vector<InterfaceLeaf>& leaves, bool output) {
		std::uint32_t location{};
		for (InterfaceLeaf& leaf : leaves) {
			const bool raster_position = leaf.builtin_position &&
				((output && (entry.stage == rtsl::ir::Stage::stage_vertex ||
					entry.stage == rtsl::ir::Stage::stage_tessellation_evaluation ||
					entry.stage == rtsl::ir::Stage::stage_geometry)) ||
				 (!output && entry.stage == rtsl::ir::Stage::stage_geometry));
			if (!output && entry.stage == rtsl::ir::Stage::stage_fragment && leaf.builtin_position) {
				leaf.omitted = true;
				continue;
			}
			leaf.variable = id();
			const std::uint32_t storage_class = output ? 3u : 1u;
			const std::uint32_t interface_type = !output && leaf.input_array_count ? typeArray(typeFor(leaf.type), leaf.input_array_count) : typeFor(leaf.type);
			instruction(59, {pointerType(storage_class, interface_type), leaf.variable, storage_class});
			name(leaf.variable, std::string(output ? "out_" : "in_") + leaf.name);
			if (raster_position)
				instruction(71, {leaf.variable, 11, 0});
			else instruction(71, {leaf.variable, 30, location++});
			if (leaf.contract == "flat") instruction(71, {leaf.variable, 14});
			interface_ids.push_back(leaf.variable);
		}
	}

	void createPatchInterfaceVariables(std::vector<InterfaceLeaf>& leaves, bool output, std::uint32_t count) {
		std::uint32_t location{};
		for (InterfaceLeaf& leaf : leaves) {
			leaf.variable = id();
			const std::uint32_t storage_class = output ? 3u : 1u;
			instruction(59, {pointerType(storage_class, typeArray(typeFor(leaf.type), count)), leaf.variable, storage_class});
			name(leaf.variable, std::string(output ? "out_patch_" : "in_patch_") + leaf.name);
			instruction(71, {leaf.variable, 30, location++});
			interface_ids.push_back(leaf.variable);
		}
	}

	const rtsl::ir::Type* patchType(rtsl::ir::TypeId type_id) const {
		const rtsl::ir::Type* type = module.findType(type_id);
		while (type && type->kind == rtsl::ir::TypeKind::type_pointer) type = module.findType(type->element_type);
		return type && type->kind == rtsl::ir::TypeKind::type_patch ? type : nullptr;
	}

	void buildInterfaces(const rtsl::ir::Function& function) {
		if (entry.stage == rtsl::ir::Stage::stage_compute) return;
		std::vector<std::uint32_t> path;
		for (std::uint32_t index = 0; index < function.parameters.size(); ++index) {
			if (const rtsl::ir::Type* patch = patchType(function.parameters[index].type))
				collectLeaves(index, patch->element_type, patch->element_type, "value", path, patch_input_leaves);
			else if (const rtsl::ir::Type* type = module.findType(function.parameters[index].type);
				entry.stage == rtsl::ir::Stage::stage_geometry && type && type->kind == rtsl::ir::TypeKind::type_primitive) {
				const std::size_t first = input_leaves.size();
				collectLeaves(index, type->element_type, type->element_type, "value", path, input_leaves);
				for (std::size_t leaf = first; leaf < input_leaves.size(); ++leaf) input_leaves[leaf].input_array_count = type->element_count;
			} else collectLeaves(index, function.parameters[index].type, function.parameters[index].type, "value", path, input_leaves);
		}
		// A scalar or vector fragment return is the unnamed color output.  It has
		// a physical SPIR-V location but deliberately no public Rutile location.
		// Structure members keep their member names as public output names.
		if (entry.stage == rtsl::ir::Stage::stage_tessellation_control)
			collectLeaves(0, function.return_type, function.return_type, "", path, patch_output_leaves);
		else if (const rtsl::ir::Type* result = module.findType(function.return_type);
			entry.stage == rtsl::ir::Stage::stage_geometry && result && result->kind == rtsl::ir::TypeKind::type_primitive)
			collectLeaves(0, result->element_type, result->element_type, "", path, output_leaves);
		else collectLeaves(0, function.return_type, function.return_type, "", path, output_leaves);
		createInterfaceVariables(input_leaves, false);
		if (!patch_input_leaves.empty()) createPatchInterfaceVariables(patch_input_leaves, false, 32);
		if (!patch_output_leaves.empty()) createPatchInterfaceVariables(patch_output_leaves, true, tessellationControlInvocations());
		createInterfaceVariables(output_leaves, true);
	}

	void declareTessellationBuiltins() {
		if (entry.stage == rtsl::ir::Stage::stage_tessellation_control) {
			invocation_id_variable = id();
			instruction(59, {pointerType(1, typeUnsignedInteger()), invocation_id_variable, 1});
			instruction(71, {invocation_id_variable, 11, 8}); // InvocationId
			interface_ids.push_back(invocation_id_variable);
			const std::uint32_t outer = typeArray(typeFloat(), 4);
			tessellation_outer_variable = id();
			instruction(59, {pointerType(3, outer), tessellation_outer_variable, 3});
			instruction(71, {tessellation_outer_variable, 11, 11}); // TessLevelOuter
			interface_ids.push_back(tessellation_outer_variable);
		}
		if (entry.stage == rtsl::ir::Stage::stage_tessellation_evaluation) {
			tessellation_coordinate_variable = id();
			instruction(59, {pointerType(1, typeVector(typeFloat(), 3)), tessellation_coordinate_variable, 1});
			instruction(71, {tessellation_coordinate_variable, 11, 13}); // TessCoord
			interface_ids.push_back(tessellation_coordinate_variable);
		}
	}

	void declareComputeInvocationId() {
		compute_invocation_id = id();
		instruction(59, {pointerType(1, typeVector(typeUnsignedInteger(), 3)), compute_invocation_id, 1});
		name(compute_invocation_id, "gl_GlobalInvocationID");
		instruction(71, {compute_invocation_id, 11, 28}); // GlobalInvocationId
		interface_ids.push_back(compute_invocation_id);
	}

	void declareResources() {
		for (const rtsl::ir::Resource& resource : module.resources) {
			resources.emplace(resource.symbol.value(), &resource);
			const std::uint32_t variable = id();
			if (resource.kind == rtsl::ir::ResourceKind::resource_sampled_texture) instruction(59, {pointerType(0, typeSampledImage2D()), variable, 0});
			else if (resource.kind == rtsl::ir::ResourceKind::resource_storage_buffer) {
				const rtsl::ir::Type* type = module.findType(resource.type);
				if (!type || type->parameter_types.size() != 2 || resourceTypeName(resource) != "buffer") throw std::runtime_error("RTIR storage buffer must preserve buffer<header, element> type parameters");
				const std::uint32_t block = id(); const std::uint32_t header = typeFor(type->parameter_types[0]); const std::uint32_t elements = typeRuntimeArray(typeFor(type->parameter_types[1]), resourceStride(type->parameter_types[1]));
				instruction(30, {block, header, elements}); instruction(71, {block, 2}); instruction(72, {block, 0, 35, 0}); instruction(72, {block, 1, 35, resourceStride(type->parameter_types[0])}); instruction(59, {pointerType(12, block), variable, 12});
			} else if (resource.kind == rtsl::ir::ResourceKind::resource_storage_texture) {
				const rtsl::ir::Type* type = module.findType(resource.type); const std::string_view name = resourceTypeName(resource); const std::uint32_t dimensions = name == "image_1d" ? 1 : name == "image_2d" ? 2 : name == "image_3d" ? 3 : 0;
				if (!type || type->parameter_types.size() != 1 || !dimensions) throw std::runtime_error("RTIR storage image must preserve image dimension and texel type");
				instruction(59, {pointerType(0, typeStorageImage(type->parameter_types[0], dimensions)), variable, 0});
				const rtsl::ir::Type* texel = module.findType(type->parameter_types[0]);
				if (name == "image_2d" && texel && texel->kind == rtsl::ir::TypeKind::type_floating && texel->bit_width == 32) {
					const std::uint32_t sampled = id();
					instruction(59, {pointerType(0, typeSampledImage2D()), sampled, 0});
					instruction(71, {sampled, 34, resource.binding ? resource.binding->set : 0});
					instruction(71, {sampled, 33, resource.binding ? resource.binding->binding : next_resource_binding});
					storage_sample_variables.emplace(resource.symbol.value(), sampled);
				}
			} else continue;
			instruction(71, {variable, 34, resource.binding ? resource.binding->set : 0});
			instruction(71, {variable, 33, resource.binding ? resource.binding->binding : next_resource_binding++});
			resource_variables.emplace(resource.symbol.value(), variable);
		}
	}

	void declareUniforms() {
		if (module.uniforms.empty()) return;
		std::vector<std::uint32_t> members;
		members.reserve(module.uniforms.size() + 1);
		const std::uint32_t block_type = id();
		members.push_back(block_type);
		for (const rtsl::ir::Uniform& uniform : module.uniforms) members.push_back(typeFor(uniform.type));
		instruction(30, members);
		instruction(71, {block_type, 2});
		std::uint32_t offset{};
		for (std::uint32_t index = 0; index < module.uniforms.size(); ++index) {
			const rtsl::ir::Uniform& uniform = module.uniforms[index];
			const UniformLayout layout = uniformLayout(module, uniform.type);
			offset = uniform.offset ? *uniform.offset : roundUp(offset, layout.alignment);
			instruction(72, {block_type, index, 35, offset});
			decorateUniformMember(block_type, index, uniform.type);
			uniform_members.emplace(uniform.symbol.value(), index);
			offset += uniform.size ? *uniform.size : layout.size;
		}
		uniform_block_variable = id();
		instruction(59, {pointerType(2, block_type), uniform_block_variable, 2});
		name(uniform_block_variable, "rutile_uniforms");
		instruction(71, {uniform_block_variable, 34, 0});
		instruction(71, {uniform_block_variable, 33, next_resource_binding++});
	}

	void declareStorageObjects() {
		std::vector<const rtsl::ir::StorageObject*> objects;
		for (const rtsl::ir::StorageObject& object : module.storage_objects)
			if (object.address_space == rtsl::ir::AddressSpace::address_space_storage) objects.push_back(&object);
		if (objects.empty()) return;
		std::vector<std::uint32_t> members;
		members.reserve(objects.size() + 1);
		const std::uint32_t block_type = id();
		members.push_back(block_type);
		for (const rtsl::ir::StorageObject* object : objects) members.push_back(typeFor(object->type));
		instruction(30, members);
		instruction(71, {block_type, 2});
		std::uint32_t offset{};
		for (std::uint32_t index = 0; index < objects.size(); ++index) {
			const rtsl::ir::StorageObject& object = *objects[index];
			const UniformLayout layout = uniformLayout(module, object.type);
			offset = roundUp(offset, layout.alignment);
			instruction(72, {block_type, index, 35, offset});
			decorateUniformMember(block_type, index, object.type);
			storage_members.emplace(object.symbol.value(), index);
			offset += layout.size;
		}
		storage_block_variable = id();
		instruction(59, {pointerType(12, block_type), storage_block_variable, 12});
		name(storage_block_variable, "rutile_storage");
		instruction(71, {storage_block_variable, 34, 0});
		instruction(71, {storage_block_variable, 33, next_resource_binding++});
	}

	void decorateUniformArray(rtsl::ir::TypeId type_id) {
		if (!uniform_arrays.insert(type_id.value()).second) return;
		const rtsl::ir::Type& type = requireType(type_id);
		const UniformLayout element = uniformLayout(module, type.element_type);
		instruction(71, {typeFor(type_id), 6, roundUp(element.size, 16)});
		decorateUniformType(type.element_type);
	}

	void decorateUniformStructure(rtsl::ir::TypeId type_id) {
		if (!uniform_structures.insert(type_id.value()).second) return;
		const rtsl::ir::Type& type = requireType(type_id);
		const std::uint32_t structure = typeFor(type_id);
		std::uint32_t offset{};
		for (std::uint32_t index = 0; index < type.members.size(); ++index) {
			const rtsl::ir::StructMember& member = type.members[index];
			const UniformLayout layout = uniformLayout(module, member.type);
			offset = roundUp(offset, layout.alignment);
			instruction(72, {structure, index, 35, offset});
			decorateUniformMember(structure, index, member.type);
			offset += layout.size;
		}
	}

	void decorateUniformType(rtsl::ir::TypeId type_id) {
		const rtsl::ir::Type& type = requireType(type_id);
		if (type.kind == rtsl::ir::TypeKind::type_array) decorateUniformArray(type_id);
		if (type.kind == rtsl::ir::TypeKind::type_structure) decorateUniformStructure(type_id);
	}

	void decorateUniformMember(std::uint32_t structure, std::uint32_t member, rtsl::ir::TypeId type_id) {
		const rtsl::ir::Type& type = requireType(type_id);
		if (type.kind == rtsl::ir::TypeKind::type_matrix) {
			instruction(72, {structure, member, 5});
			instruction(72, {structure, member, 7, 16});
		}
		decorateUniformType(type_id);
	}

	void declareFunctions() {
		for (const rtsl::ir::Function& function : module.functions) {
			std::vector<std::uint32_t> parameters;
			for (const rtsl::ir::Parameter& parameter : function.parameters) parameters.push_back(typeFor(parameter.type));
			function_types[function.id.value()] = functionType(typeFor(function.return_type), parameters);
			function_ids[function.id.value()] = id();
		}
	}

	std::uint32_t undef(rtsl::ir::TypeId type) {
		const std::uint32_t result = id();
		instruction(1, {typeFor(type), result});
		return result;
	}

	InterfaceLeaf* leafAt(std::vector<InterfaceLeaf>& leaves, std::span<const std::uint32_t> path) {
		for (InterfaceLeaf& leaf : leaves)
			if (std::ranges::equal(leaf.path, path)) return &leaf;
		return nullptr;
	}

	std::uint32_t loadInterfaceValue(rtsl::ir::TypeId type_id, std::vector<std::uint32_t>& path) {
		const rtsl::ir::Type* type = module.findType(type_id);
		if (entry.stage == rtsl::ir::Stage::stage_geometry && type && type->kind == rtsl::ir::TypeKind::type_primitive)
			return loadGeometryPrimitive(*type, path);
		if (type && type->kind == rtsl::ir::TypeKind::type_structure) {
			std::vector<std::uint32_t> operands{typeFor(type_id), id()};
			for (std::uint32_t index = 0; index < type->members.size(); ++index) {
				path.push_back(index);
				operands.push_back(loadInterfaceValue(type->members[index].type, path));
				path.pop_back();
			}
			instruction(80, operands);
			return operands[1];
		}
		InterfaceLeaf* leaf = leafAt(input_leaves, path);
		if (!leaf || leaf->omitted) return undef(type_id);
		const std::uint32_t result = id();
		instruction(61, {typeFor(type_id), result, leaf->variable});
		return result;
	}

	std::uint32_t loadGeometryPrimitive(const rtsl::ir::Type& primitive, std::vector<std::uint32_t>& path) {
		std::vector<std::uint32_t> elements{typeFor(primitive.id), id()};
		for (std::uint32_t index = 0; index < primitive.element_count; ++index)
			elements.push_back(loadGeometryInterfaceValue(primitive.element_type, index, path));
		instruction(80, elements);
		return elements[1];
	}

	std::uint32_t loadGeometryInterfaceValue(rtsl::ir::TypeId type_id, std::uint32_t index, std::vector<std::uint32_t>& path) {
		const rtsl::ir::Type* type = module.findType(type_id);
		if (type && type->kind == rtsl::ir::TypeKind::type_structure) {
			std::vector<std::uint32_t> operands{typeFor(type_id), id()};
			for (std::uint32_t member = 0; member < type->members.size(); ++member) {
				path.push_back(member);
				operands.push_back(loadGeometryInterfaceValue(type->members[member].type, index, path));
				path.pop_back();
			}
			instruction(80, operands);
			return operands[1];
		}
		InterfaceLeaf* leaf = leafAt(input_leaves, path);
		if (!leaf || leaf->omitted || !leaf->input_array_count) return undef(type_id);
		const std::uint32_t pointer = id();
		instruction(65, {pointerType(1, typeFor(type_id)), pointer, leaf->variable, constantUnsignedInteger(index)});
		const std::uint32_t result = id();
		instruction(61, {typeFor(type_id), result, pointer});
		return result;
	}

	void emitGeometryVertex(rtsl::ir::TypeId type_id, std::uint32_t value) {
		std::vector<std::uint32_t> path;
		storeInterfaceValue(type_id, value, path);
		instruction(218, {}); // OpEmitVertex
	}

	std::uint32_t loadPatchInterfaceValue(rtsl::ir::TypeId type_id, std::uint32_t index, std::vector<std::uint32_t>& path) {
		const rtsl::ir::Type* type = module.findType(type_id);
		if (type && type->kind == rtsl::ir::TypeKind::type_structure) {
			std::vector<std::uint32_t> operands{typeFor(type_id), id()};
			for (std::uint32_t member = 0; member < type->members.size(); ++member) {
				path.push_back(member);
				operands.push_back(loadPatchInterfaceValue(type->members[member].type, index, path));
				path.pop_back();
			}
			instruction(80, operands);
			return operands[1];
		}
		InterfaceLeaf* leaf = leafAt(patch_input_leaves, path);
		if (!leaf) throw std::runtime_error("RTIR patch access does not correspond to a tessellation input interface");
		const std::uint32_t pointer = id();
		instruction(65, {pointerType(1, typeFor(type_id)), pointer, leaf->variable, index});
		const std::uint32_t result = id();
		instruction(61, {typeFor(type_id), result, pointer});
		return result;
	}

	void storeInterfaceValue(rtsl::ir::TypeId type_id, std::uint32_t value, std::vector<std::uint32_t>& path) {
		const rtsl::ir::Type* type = module.findType(type_id);
		if (type && type->kind == rtsl::ir::TypeKind::type_structure) {
			for (std::uint32_t index = 0; index < type->members.size(); ++index) {
				const std::uint32_t extracted = id();
				instruction(81, {typeFor(type->members[index].type), extracted, value, index});
				path.push_back(index);
				storeInterfaceValue(type->members[index].type, extracted, path);
				path.pop_back();
			}
			return;
		}
		InterfaceLeaf* leaf = leafAt(output_leaves, path);
		if (leaf && !leaf->omitted) instruction(62, {leaf->variable, value});
	}

	void storePatchInterfaceValue(rtsl::ir::TypeId type_id, std::uint32_t value, std::uint32_t index, std::vector<std::uint32_t>& path) {
		const rtsl::ir::Type* type = module.findType(type_id);
		if (type && type->kind == rtsl::ir::TypeKind::type_structure) {
			for (std::uint32_t member = 0; member < type->members.size(); ++member) {
				const std::uint32_t extracted = id();
				instruction(81, {typeFor(type->members[member].type), extracted, value, member});
				path.push_back(member);
				storePatchInterfaceValue(type->members[member].type, extracted, index, path);
				path.pop_back();
			}
			return;
		}
		InterfaceLeaf* leaf = leafAt(patch_output_leaves, path);
		if (!leaf) throw std::runtime_error("RTIR tessellation-control output does not correspond to a patch interface");
		const std::uint32_t pointer = id();
		instruction(65, {pointerType(3, typeFor(type_id)), pointer, leaf->variable, index});
		instruction(62, {pointer, value});
	}

	std::uint32_t memberIndex(rtsl::ir::TypeId aggregate_type, rtsl::ir::StringId member_name) const {
		const rtsl::ir::Type* type = module.findType(aggregate_type);
		if (!type || type->kind != rtsl::ir::TypeKind::type_structure)
			throw std::runtime_error("RTIR member access does not target a structure");
		for (std::uint32_t index = 0; index < type->members.size(); ++index)
			if (type->members[index].name == member_name) return index;
		throw std::runtime_error("RTIR member access references an unknown member");
	}

	const rtsl::ir::Type& requireType(rtsl::ir::TypeId type_id) const {
		const rtsl::ir::Type* type = module.findType(type_id);
		if (!type) throw std::runtime_error("RTIR instruction references an unknown type");
		return *type;
	}

	rtsl::ir::TypeKind scalarKind(rtsl::ir::TypeId type_id) const {
		const rtsl::ir::Type& type = requireType(type_id);
		return type.kind == rtsl::ir::TypeKind::type_vector ? scalarKind(type.element_type) : type.kind;
	}

	std::uint32_t scalarBitWidth(rtsl::ir::TypeId type_id) const {
		const rtsl::ir::Type& type = requireType(type_id);
		return type.kind == rtsl::ir::TypeKind::type_vector ? scalarBitWidth(type.element_type) : type.bit_width;
	}

	bool matchingNumericShape(rtsl::ir::TypeId source_id, rtsl::ir::TypeId result_id) const {
		const rtsl::ir::Type& source = requireType(source_id);
		const rtsl::ir::Type& result = requireType(result_id);
		if (source.kind == rtsl::ir::TypeKind::type_vector || result.kind == rtsl::ir::TypeKind::type_vector)
			return source.kind == rtsl::ir::TypeKind::type_vector && result.kind == rtsl::ir::TypeKind::type_vector &&
				   source.element_count == result.element_count;
		return true;
	}

	std::uint32_t integerTypeForShape(rtsl::ir::TypeId shape_id, std::uint32_t bit_width, bool is_signed) {
		const rtsl::ir::Type& shape = requireType(shape_id);
		const std::uint32_t scalar = typeInteger(bit_width, is_signed);
		return shape.kind == rtsl::ir::TypeKind::type_vector ? typeVector(scalar, shape.element_count) : scalar;
	}

	void emitConvert(rtsl::ir::TypeId result_type, rtsl::ir::TypeId source_type, std::uint32_t result, std::uint32_t operand) {
		const rtsl::ir::TypeKind source_kind = scalarKind(source_type);
		const rtsl::ir::TypeKind result_kind = scalarKind(result_type);
		const bool source_numeric = source_kind == rtsl::ir::TypeKind::type_floating ||
									source_kind == rtsl::ir::TypeKind::type_signed_integer || source_kind == rtsl::ir::TypeKind::type_unsigned_integer;
		const bool result_numeric = result_kind == rtsl::ir::TypeKind::type_floating ||
									result_kind == rtsl::ir::TypeKind::type_signed_integer || result_kind == rtsl::ir::TypeKind::type_unsigned_integer;
		if (!source_numeric || !result_numeric || !matchingNumericShape(source_type, result_type))
			throw std::runtime_error("RTIR convert requires matching scalar or vector numeric types");

		const std::uint32_t source_width = scalarBitWidth(source_type);
		const std::uint32_t result_width = scalarBitWidth(result_type);
		if (!source_width || !result_width)
			throw std::runtime_error("RTIR convert requires numeric types with a non-zero bit width");

		if (source_kind == rtsl::ir::TypeKind::type_floating) {
			if (result_kind == rtsl::ir::TypeKind::type_floating) {
				if (source_width == result_width)
					throw std::runtime_error("RTIR convert does not change the floating-point type");
				instruction(115, { typeFor(result_type), result, operand }); // OpFConvert
			} else
				instruction(result_kind == rtsl::ir::TypeKind::type_signed_integer ? 110 : 109, { typeFor(result_type), result, operand }); // OpConvertFToS or OpConvertFToU
			return;
		}

		if (result_kind == rtsl::ir::TypeKind::type_floating) {
			instruction(source_kind == rtsl::ir::TypeKind::type_signed_integer ? 111 : 112, { typeFor(result_type), result, operand }); // OpConvertSToF or OpConvertUToF
			return;
		}

		const bool source_signed = source_kind == rtsl::ir::TypeKind::type_signed_integer;
		const bool result_signed = result_kind == rtsl::ir::TypeKind::type_signed_integer;
		if (source_signed == result_signed) {
			if (source_width == result_width)
				throw std::runtime_error("RTIR convert does not change the integer type");
			instruction(source_signed ? 114 : 113, { typeFor(result_type), result, operand }); // OpSConvert or OpUConvert
			return;
		}

		if (source_width == result_width) {
			instruction(124, { typeFor(result_type), result, operand }); // OpBitcast preserves modulo integer conversion.
			return;
		}

		const std::uint32_t converted = id();
		instruction(source_signed ? 114 : 113, { integerTypeForShape(result_type, result_width, source_signed), converted, operand }); // OpSConvert or OpUConvert
		instruction(124, { typeFor(result_type), result, converted });																   // OpBitcast preserves the converted bit pattern.
	}

	std::uint32_t componentIndex(char component) const {
		switch (component) {
		case 'x':
			return 0;
		case 'y':
			return 1;
		case 'z':
			return 2;
		case 'w':
			return 3;
		default:
			throw std::runtime_error("RTIR vector swizzle contains an invalid component");
		}
	}

	std::uint32_t valueFor(const std::unordered_map<std::uint32_t, std::uint32_t>& values, rtsl::ir::ValueId value) const {
		const auto found = values.find(value.value());
		if (found == values.end())
			throw std::runtime_error("RTIR instruction references a value not available in this SPIR-V block");
		return found->second;
	}

	std::uint16_t arithmeticOpcode(rtsl::ir::Opcode opcode, rtsl::ir::TypeId operand_type) const {
		const rtsl::ir::TypeKind kind = scalarKind(operand_type);
		const bool floating = kind == rtsl::ir::TypeKind::type_floating;
		const bool signed_integer = kind == rtsl::ir::TypeKind::type_signed_integer;
		const bool unsigned_integer = kind == rtsl::ir::TypeKind::type_unsigned_integer;
		switch (opcode) {
		case rtsl::ir::Opcode::opcode_add:
			if (floating) return 129;
			if (signed_integer || unsigned_integer) return 128;
			break;
		case rtsl::ir::Opcode::opcode_subtract:
			if (floating) return 131;
			if (signed_integer || unsigned_integer) return 130;
			break;
		case rtsl::ir::Opcode::opcode_multiply:
			if (floating) return 133;
			if (signed_integer || unsigned_integer) return 132;
			break;
		case rtsl::ir::Opcode::opcode_divide:
			if (floating) return 136;
			if (signed_integer) return 135;
			if (unsigned_integer) return 134;
			break;
		case rtsl::ir::Opcode::opcode_remainder:
			if (floating) return 140;
			if (signed_integer) return 138;
			if (unsigned_integer) return 137;
			break;
		default:
			break;
		}
		throw std::runtime_error("RTIR arithmetic instruction has unsupported operand types");
	}

	std::uint16_t comparisonOpcode(rtsl::ir::Opcode opcode, rtsl::ir::TypeId operand_type) const {
		const rtsl::ir::TypeKind kind = scalarKind(operand_type);
		const bool floating = kind == rtsl::ir::TypeKind::type_floating;
		const bool signed_integer = kind == rtsl::ir::TypeKind::type_signed_integer;
		const bool unsigned_integer = kind == rtsl::ir::TypeKind::type_unsigned_integer;
		const bool boolean = kind == rtsl::ir::TypeKind::type_boolean;
		switch (opcode) {
		case rtsl::ir::Opcode::opcode_compare_equal:
			if (floating) return 180;
			if (boolean) return 164;
			if (signed_integer || unsigned_integer) return 170;
			break;
		case rtsl::ir::Opcode::opcode_compare_not_equal:
			if (floating) return 182;
			if (boolean) return 165;
			if (signed_integer || unsigned_integer) return 171;
			break;
		case rtsl::ir::Opcode::opcode_compare_less:
			if (floating) return 184;
			if (signed_integer) return 177;
			if (unsigned_integer) return 176;
			break;
		case rtsl::ir::Opcode::opcode_compare_less_equal:
			if (floating) return 188;
			if (signed_integer) return 179;
			if (unsigned_integer) return 178;
			break;
		case rtsl::ir::Opcode::opcode_compare_greater:
			if (floating) return 186;
			if (signed_integer) return 173;
			if (unsigned_integer) return 172;
			break;
		case rtsl::ir::Opcode::opcode_compare_greater_equal:
			if (floating) return 190;
			if (signed_integer) return 175;
			if (unsigned_integer) return 174;
			break;
		default:
			break;
		}
		throw std::runtime_error("RTIR comparison instruction has unsupported operand types");
	}

	bool isEndPrimitiveCall(const rtsl::ir::Instruction& instruction) const {
		if (entry.stage != rtsl::ir::Stage::stage_geometry || instruction.opcode != rtsl::ir::Opcode::opcode_call || instruction.operands.size() != 2) return false;
		const rtsl::ir::Function* function = module.findFunction(instruction.callee);
		if (!function || !function->declaration || !function->implicit || function->parameters.size() != 2 || function->return_type != function->parameters[0].type) return false;
		const rtsl::ir::Type* primitive = module.findType(function->parameters[0].type);
		const rtsl::ir::Type* marker = module.findType(function->parameters[1].type);
		return primitive && primitive->kind == rtsl::ir::TypeKind::type_primitive && marker &&
			marker->kind == rtsl::ir::TypeKind::type_structure && marker->members.empty();
	}

	bool isGeometryEmitCall(const rtsl::ir::Instruction& instruction) const {
		if (entry.stage != rtsl::ir::Stage::stage_geometry || instruction.opcode != rtsl::ir::Opcode::opcode_call || instruction.operands.size() != 2) return false;
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

	void emitIRInstruction(const rtsl::ir::Instruction& source,
		std::unordered_map<std::uint32_t, std::uint32_t>& values,
		std::unordered_map<std::uint32_t, rtsl::ir::TypeId>& value_types,
		std::unordered_map<std::uint32_t, std::uint32_t>& constant_indices) {
		std::vector<std::uint32_t> operands;
		for (rtsl::ir::ValueId operand : source.operands) operands.push_back(valueFor(values, operand));
		std::uint32_t result{};
		switch (source.opcode) {
		case rtsl::ir::Opcode::opcode_constant_floating:
			result = id(); instruction(43, {typeFor(source.type), result, source.immediates.at(0)}); break;
		case rtsl::ir::Opcode::opcode_constant_integer: {
			const rtsl::ir::Type& type = requireType(source.type);
			if (type.kind != rtsl::ir::TypeKind::type_signed_integer && type.kind != rtsl::ir::TypeKind::type_unsigned_integer)
				throw std::runtime_error("RTIR integer constant does not have an integer type");
			const std::size_t word_count = (std::max<std::uint32_t>(type.bit_width, 1u) + 31u) / 32u;
			if (source.immediates.size() < word_count)
				throw std::runtime_error("RTIR integer constant does not contain enough literal words");
			result = id();
			std::vector<std::uint32_t> encoded{typeFor(source.type), result};
			encoded.insert(encoded.end(), source.immediates.begin(), source.immediates.begin() + word_count);
			instruction(43, encoded);
			if (source.result && word_count == 1) constant_indices.emplace(source.result.value(), source.immediates[0]);
			break;
		}
		case rtsl::ir::Opcode::opcode_constant_boolean:
			if (source.immediates.size() != 1) throw std::runtime_error("RTIR boolean constant is malformed");
			result = id(); instruction(source.immediates[0] ? 41 : 42, {typeFor(source.type), result}); break;
		case rtsl::ir::Opcode::opcode_construct: {
			const rtsl::ir::Type& constructed = requireType(source.type);
			if (entry.stage == rtsl::ir::Stage::stage_geometry && constructed.kind == rtsl::ir::TypeKind::type_primitive) {
				if (operands.size() > constructed.element_count) throw std::runtime_error("RTIR geometry primitive construction exceeds its maximum vertex count");
				for (const std::uint32_t vertex : operands) emitGeometryVertex(constructed.element_type, vertex);
				result = undef(source.type);
				break;
			}
			result = id();
			std::vector<std::uint32_t> encoded{typeFor(source.type), result};
			encoded.insert(encoded.end(), operands.begin(), operands.end());
			instruction(80, encoded);
			break;
		}
		case rtsl::ir::Opcode::opcode_access: {
			result = id();
			const rtsl::ir::TypeId base_type = value_types.at(source.operands.at(0).value());
			const rtsl::ir::Type& base = requireType(base_type);
			const rtsl::ir::Type* patch = base.kind == rtsl::ir::TypeKind::type_patch ? &base :
				(base.kind == rtsl::ir::TypeKind::type_pointer && requireType(base.element_type).kind == rtsl::ir::TypeKind::type_patch ?
					&requireType(base.element_type) : nullptr);
			if (patch) {
				const std::string_view member = source.immediates.empty() ? std::string_view{} :
					module.strings.get(rtsl::ir::StringId{source.immediates.front()});
				if (member == "current") {
					if (entry.stage != rtsl::ir::Stage::stage_tessellation_control || !invocation_id_variable)
						throw std::runtime_error("RTIR patch.current is only valid in tessellation control");
					const std::uint32_t invocation = id();
					instruction(61, {typeUnsignedInteger(), invocation, invocation_id_variable});
					std::vector<std::uint32_t> path;
					result = loadPatchInterfaceValue(patch->element_type, invocation, path);
					break;
				}
				if (member == "outer") {
					if (entry.stage != rtsl::ir::Stage::stage_tessellation_control || !tessellation_outer_variable)
						throw std::runtime_error("RTIR patch.outer access is only valid in tessellation control");
					if (operands.size() != 2) throw std::runtime_error("RTIR patch.outer must include its u32 index");
					instruction(65, {pointerType(3, typeFor(source.type)), result, tessellation_outer_variable, operands[1]});
					break;
				}
				if (member == "coordinate") {
					if (entry.stage != rtsl::ir::Stage::stage_tessellation_evaluation || !tessellation_coordinate_variable)
						throw std::runtime_error("RTIR patch.coordinate is only valid in tessellation evaluation");
					const std::uint32_t coordinate = id();
					instruction(61, {typeVector(typeFloat(), 3), coordinate, tessellation_coordinate_variable});
					const rtsl::ir::Type& result_type = requireType(source.type);
					if (result_type.kind == rtsl::ir::TypeKind::type_floating)
						instruction(81, {typeFor(source.type), result, coordinate, 0});
					else if (result_type.kind == rtsl::ir::TypeKind::type_vector)
						instruction(79, {typeFor(source.type), result, coordinate, coordinate, 0, 1});
					else throw std::runtime_error("RTIR patch.coordinate has an unsupported type");
					break;
				}
				if (!member.empty() || operands.size() != 2)
					throw std::runtime_error("RTIR patch access is malformed");
				std::vector<std::uint32_t> path;
				result = loadPatchInterfaceValue(patch->element_type, operands[1], path);
				break;
			}
			if (base.kind == rtsl::ir::TypeKind::type_primitive) {
				if (!source.immediates.empty() || operands.size() != 2) throw std::runtime_error("RTIR primitive access is malformed");
				const auto index = constant_indices.find(source.operands[1].value());
				if (index == constant_indices.end() || index->second >= base.element_count)
					throw std::runtime_error("RTIR geometry primitive access requires an in-range constant index");
				instruction(81, {typeFor(source.type), result, operands[0], index->second});
				break;
			}
			if (base.kind == rtsl::ir::TypeKind::type_vector) {
				if (source.immediates.empty()) {
					if (operands.size() != 2) throw std::runtime_error("RTIR vector subscript is malformed");
					instruction(77, {typeFor(source.type), result, operands[0], operands[1]});
					break;
				}
				const std::string_view swizzle = module.strings.get(rtsl::ir::StringId{source.immediates[0]});
				if (swizzle.empty()) throw std::runtime_error("RTIR vector member access is malformed");
				if (swizzle.size() == 1) instruction(81, {typeFor(source.type), result, operands.at(0), componentIndex(swizzle.front())});
				else {
					std::vector<std::uint32_t> encoded{typeFor(source.type), result, operands.at(0), operands.at(0)};
					for (char component : swizzle) encoded.push_back(componentIndex(component));
					instruction(79, encoded);
				}
				break;
			}
			if (source.immediates.empty()) throw std::runtime_error("RTIR aggregate subscript lowering is not implemented");
			const std::uint32_t index = memberIndex(base_type, rtsl::ir::StringId{source.immediates[0]});
			instruction(81, {typeFor(source.type), result, operands.at(0), index});
			break;
		}
		case rtsl::ir::Opcode::opcode_insert: {
			const rtsl::ir::Type& primitive = requireType(source.type);
			if (entry.stage != rtsl::ir::Stage::stage_geometry || primitive.kind != rtsl::ir::TypeKind::type_primitive || operands.size() != 2)
				throw std::runtime_error("RTIR insert is only supported for a geometry primitive accumulator");
			emitGeometryVertex(primitive.element_type, operands[1]);
			result = undef(source.type);
			break;
		}
		case rtsl::ir::Opcode::opcode_store:
			if (operands.size() != 2) throw std::runtime_error("RTIR store instruction is malformed");
			instruction(62, {operands[0], operands[1]});
			break;
		case rtsl::ir::Opcode::opcode_add:
		case rtsl::ir::Opcode::opcode_subtract:
		case rtsl::ir::Opcode::opcode_divide:
		case rtsl::ir::Opcode::opcode_remainder:
			if (operands.size() != 2) throw std::runtime_error("RTIR binary arithmetic instruction is malformed");
			result = id(); instruction(arithmeticOpcode(source.opcode, value_types.at(source.operands[0].value())),
				{typeFor(source.type), result, operands[0], operands[1]}); break;
		case rtsl::ir::Opcode::opcode_multiply: {
			if (operands.size() != 2) throw std::runtime_error("RTIR multiply instruction is malformed");
			const rtsl::ir::Type& left = requireType(value_types.at(source.operands[0].value()));
			const rtsl::ir::Type& right = requireType(value_types.at(source.operands[1].value()));
			result = id();
			if (left.kind == rtsl::ir::TypeKind::type_matrix && right.kind == rtsl::ir::TypeKind::type_vector)
				instruction(145, {typeFor(source.type), result, operands[0], operands[1]});
			else if (left.kind == rtsl::ir::TypeKind::type_vector && right.kind == rtsl::ir::TypeKind::type_matrix)
				instruction(144, {typeFor(source.type), result, operands[0], operands[1]});
			else if (left.kind == rtsl::ir::TypeKind::type_vector && right.kind == rtsl::ir::TypeKind::type_floating)
				instruction(142, {typeFor(source.type), result, operands[0], operands[1]});
			else if (right.kind == rtsl::ir::TypeKind::type_vector && left.kind == rtsl::ir::TypeKind::type_floating)
				instruction(142, {typeFor(source.type), result, operands[1], operands[0]});
			else if (left.kind == rtsl::ir::TypeKind::type_matrix && right.kind == rtsl::ir::TypeKind::type_floating)
				instruction(143, {typeFor(source.type), result, operands[0], operands[1]});
			else if (right.kind == rtsl::ir::TypeKind::type_matrix && left.kind == rtsl::ir::TypeKind::type_floating)
				instruction(143, {typeFor(source.type), result, operands[1], operands[0]});
			else instruction(arithmeticOpcode(source.opcode, value_types.at(source.operands[0].value())),
				{typeFor(source.type), result, operands[0], operands[1]});
			break;
		}
		case rtsl::ir::Opcode::opcode_negate: {
			if (operands.size() != 1) throw std::runtime_error("RTIR negate instruction is malformed");
			const rtsl::ir::TypeKind kind = scalarKind(value_types.at(source.operands[0].value()));
			if (kind != rtsl::ir::TypeKind::type_floating && kind != rtsl::ir::TypeKind::type_signed_integer)
				throw std::runtime_error("RTIR negate instruction requires a floating or signed integer operand");
			result = id(); instruction(kind == rtsl::ir::TypeKind::type_floating ? 127 : 126,
				{typeFor(source.type), result, operands[0]}); break;
		}
		case rtsl::ir::Opcode::opcode_compare_equal:
		case rtsl::ir::Opcode::opcode_compare_not_equal:
		case rtsl::ir::Opcode::opcode_compare_less:
		case rtsl::ir::Opcode::opcode_compare_less_equal:
		case rtsl::ir::Opcode::opcode_compare_greater:
		case rtsl::ir::Opcode::opcode_compare_greater_equal:
			if (operands.size() != 2) throw std::runtime_error("RTIR comparison instruction is malformed");
			result = id(); instruction(comparisonOpcode(source.opcode, value_types.at(source.operands[0].value())),
				{typeFor(source.type), result, operands[0], operands[1]}); break;
		case rtsl::ir::Opcode::opcode_logical_and:
		case rtsl::ir::Opcode::opcode_logical_or:
			if (operands.size() != 2) throw std::runtime_error("RTIR logical instruction is malformed");
			result = id(); instruction(source.opcode == rtsl::ir::Opcode::opcode_logical_and ? 167 : 166,
				{typeFor(source.type), result, operands[0], operands[1]}); break;
		case rtsl::ir::Opcode::opcode_logical_not:
			if (operands.size() != 1)
				throw std::runtime_error("RTIR logical not instruction is malformed");
			result = id();
			instruction(168, { typeFor(source.type), result, operands[0] });
			break;
		case rtsl::ir::Opcode::opcode_convert:
			if (operands.size() != 1)
				throw std::runtime_error("RTIR convert instruction is malformed");
			result = id();
			emitConvert(source.type, value_types.at(source.operands[0].value()), result, operands[0]);
			break;
		case rtsl::ir::Opcode::opcode_call: {
			if (isEndPrimitiveCall(source)) {
				instruction(219, {}); // OpEndPrimitive
				result = undef(source.type);
				break;
			}
			if (isGeometryEmitCall(source)) {
				const rtsl::ir::Type& primitive = requireType(source.type);
				emitGeometryVertex(primitive.element_type, operands[1]);
				result = undef(source.type);
				break;
			}
			if (isPositionXYCall(source)) {
				result = id();
				instruction(79, {typeFor(source.type), result, operands[0], operands[0], 0, 1}); // OpVectorShuffle
				break;
			}
			result = id();
			std::vector<std::uint32_t> encoded{typeFor(source.type), result, function_ids.at(source.callee.value())};
			encoded.insert(encoded.end(), operands.begin(), operands.end());
			instruction(57, encoded);
			break;
		}
		case rtsl::ir::Opcode::opcode_resource_sample: {
			if (source.immediates.empty() || operands.size() != 1) throw std::runtime_error("RTIR texture sample is malformed");
			auto resource = resource_variables.find(source.immediates[0]);
			if (resource == resource_variables.end()) throw std::runtime_error("RTIR texture sample references an unknown resource");
			if (requireResource(source.immediates[0]).kind == rtsl::ir::ResourceKind::resource_storage_texture) {
				const auto sampled = storage_sample_variables.find(source.immediates[0]);
				if (sampled == storage_sample_variables.end()) throw std::runtime_error("RTIR storage texture sample requires image_2d<f32>");
				const std::uint32_t image = id();
				instruction(61, {typeSampledImage2D(), image, sampled->second});
				const std::uint32_t sampled_value = id(); instruction(87, {typeVector(typeFloat(), 4), sampled_value, image, operands[0]});
				result = id(); instruction(81, {typeFor(source.type), result, sampled_value, 0}); break;
			}
			const std::uint32_t image = id();
			instruction(61, {typeSampledImage2D(), image, resource->second});
			result = id();
			instruction(87, {typeFor(source.type), result, image, operands[0]});
			break;
		}
		case rtsl::ir::Opcode::opcode_barrier:
			if (source.result || source.type || !source.operands.empty())
				throw std::runtime_error("RTIR barrier instruction is malformed");
			// The frontend admits barriers only in compute and tessellation-control
			// entry functions. Both synchronize the current workgroup's shared memory.
			instruction(224, {constantUnsignedInteger(2), constantUnsignedInteger(2), constantUnsignedInteger(0x108)});
			break;
		case rtsl::ir::Opcode::opcode_resource_load: {
			if (source.immediates.size() != 1 || !source.type)
				throw std::runtime_error("RTIR resource load is malformed");
			const auto resource_variable = resource_variables.find(source.immediates[0]);
			if (resource_variable != resource_variables.end()) {
				const rtsl::ir::Resource& resource = requireResource(source.immediates[0]); const rtsl::ir::Type* resource_type = module.findType(resource.type);
				if (!resource_type) throw std::runtime_error("RTIR resource load has an unknown resource type");
				if (resource.kind == rtsl::ir::ResourceKind::resource_storage_buffer) {
					if (source.operands.size() > 1) throw std::runtime_error("RTIR buffer resource load has too many coordinates");
					const std::uint32_t member = source.operands.empty() ? 0 : 1; std::vector<std::uint32_t> chain{pointerType(12, typeFor(source.type)), id(), resource_variable->second, constantUnsignedInteger(member)}; if (!source.operands.empty()) chain.push_back(valueFor(values, source.operands[0])); instruction(65, chain); result = id(); instruction(61, {typeFor(source.type), result, chain[1]}); break;
				}
				if (resource.kind == rtsl::ir::ResourceKind::resource_storage_texture) {
					const std::string_view name = resourceTypeName(resource); const std::size_t dimensions = name == "image_1d" ? 1 : name == "image_2d" ? 2 : name == "image_3d" ? 3 : 0;
					if (!dimensions || source.operands.size() != dimensions) throw std::runtime_error("RTIR image resource load has the wrong coordinate count");
					std::vector<std::uint32_t> coordinates; for (const auto operand : source.operands) { const std::uint32_t component = id(); instruction(124, {typeInteger(32, true), component, valueFor(values, operand)}); coordinates.push_back(component); }
					const std::uint32_t coordinate = dimensions == 1 ? coordinates[0] : [&] { const std::uint32_t value = id(); std::vector<std::uint32_t> construct{typeVector(typeInteger(32, true), (std::uint32_t)dimensions), value}; construct.insert(construct.end(), coordinates.begin(), coordinates.end()); instruction(80, construct); return value; }();
					const std::uint32_t image = id(); instruction(61, {typeStorageImage(resource_type->parameter_types[0], (std::uint32_t)dimensions), image, resource_variable->second}); const std::uint32_t physical = id(); instruction(98, {typeVector(typeFloat(), 4), physical, image, coordinate}); result = id(); instruction(81, {typeFor(source.type), result, physical, 0}); break;
				}
				throw std::runtime_error("RTIR resource load uses an unsupported resource kind");
			}
			const auto uniform = uniform_members.find(source.immediates[0]);
			const auto storage = storage_members.find(source.immediates[0]);
			if (uniform == uniform_members.end() && storage == storage_members.end())
				throw std::runtime_error("RTIR resource load does not reference a uniform or storage value");
			const bool is_storage = storage != storage_members.end();
			const std::uint32_t storage_class = is_storage ? 12 : 2;
			const std::uint32_t block = is_storage ? storage_block_variable : uniform_block_variable;
			const std::uint32_t member = is_storage ? storage->second : uniform->second;
			const std::uint32_t pointer = id();
			instruction(65, {pointerType(storage_class, typeFor(source.type)), pointer, block,
				constantUnsignedInteger(member)});
			result = id();
			instruction(61, {typeFor(source.type), result, pointer});
			break;
		}
		case rtsl::ir::Opcode::opcode_resource_store: {
			if (source.immediates.size() != 1) throw std::runtime_error("RTIR resource store is malformed"); const auto resource_variable = resource_variables.find(source.immediates[0]); if (resource_variable == resource_variables.end()) throw std::runtime_error("RTIR resource store references an unknown resource"); const rtsl::ir::Resource& resource = requireResource(source.immediates[0]);
			if (resource.kind == rtsl::ir::ResourceKind::resource_storage_buffer) { if (source.operands.size() != 2) throw std::runtime_error("RTIR buffer resource store requires coordinate and value"); const std::uint32_t pointer = id(); const auto type = value_types.at(source.operands[1].value()); instruction(65, {pointerType(12, typeFor(type)), pointer, resource_variable->second, constantUnsignedInteger(1), valueFor(values, source.operands[0])}); instruction(62, {pointer, valueFor(values, source.operands[1])}); break; }
			if (resource.kind == rtsl::ir::ResourceKind::resource_storage_texture) { const std::string_view name = resourceTypeName(resource); const std::size_t dimensions = name == "image_1d" ? 1 : name == "image_2d" ? 2 : name == "image_3d" ? 3 : 0; if (!dimensions || source.operands.size() != dimensions + 1) throw std::runtime_error("RTIR image resource store has the wrong coordinate count"); std::vector<std::uint32_t> coordinates; for (std::size_t index = 0; index < dimensions; ++index) { const std::uint32_t component = id(); instruction(124, {typeInteger(32, true), component, valueFor(values, source.operands[index])}); coordinates.push_back(component); } const std::uint32_t coordinate = dimensions == 1 ? coordinates[0] : [&] { const std::uint32_t value = id(); std::vector<std::uint32_t> construct{typeVector(typeInteger(32, true), (std::uint32_t)dimensions), value}; construct.insert(construct.end(), coordinates.begin(), coordinates.end()); instruction(80, construct); return value; }(); const rtsl::ir::Type* type = module.findType(resource.type); const std::uint32_t image = id(); instruction(61, {typeStorageImage(type->parameter_types[0], (std::uint32_t)dimensions), image, resource_variable->second}); const std::uint32_t value = valueFor(values, source.operands.back()); const std::uint32_t physical = id(); instruction(80, {typeVector(typeFloat(), 4), physical, value, constantFloat(0.0f), constantFloat(0.0f), constantFloat(1.0f)}); instruction(99, {image, coordinate, physical}); break; }
			throw std::runtime_error("RTIR resource store uses an unsupported resource kind");
		}
		case rtsl::ir::Opcode::opcode_resource_query: {
			if (source.immediates.size() != 1 || !source.type || !source.operands.empty()) throw std::runtime_error("RTIR resource query is malformed"); const auto resource_variable = resource_variables.find(source.immediates[0]); const rtsl::ir::Resource& resource = requireResource(source.immediates[0]); if (resource_variable == resource_variables.end() || resource.kind != rtsl::ir::ResourceKind::resource_storage_texture) throw std::runtime_error("RTIR resource query requires a storage image"); const rtsl::ir::Type* image_type = module.findType(resource.type); const rtsl::ir::Type* result_type = module.findType(source.type); const std::uint32_t dimensions = result_type && result_type->kind == rtsl::ir::TypeKind::type_vector ? result_type->element_count : 1; const std::uint32_t signed_type = dimensions == 1 ? typeInteger(32, true) : typeVector(typeInteger(32, true), dimensions); const std::string_view name = resourceTypeName(resource); const std::uint32_t image = id(); instruction(61, {typeStorageImage(image_type->parameter_types[0], name == "image_1d" ? 1 : name == "image_2d" ? 2 : 3), image, resource_variable->second}); const std::uint32_t queried = id(); instruction(104, {signed_type, queried, image}); result = id(); instruction(124, {typeFor(source.type), result, queried}); break;
		}
		default:
			throw std::runtime_error("RTIR instruction is not supported by the SPIR-V translator");
		}
		if (source.result) {
			values[source.result.value()] = result;
			value_types[source.result.value()] = source.type;
		}
	}

	void emitFunction(const rtsl::ir::Function& source) {
		instruction(54, {typeFor(source.return_type), function_ids.at(source.id.value()), 0,
			function_types.at(source.id.value())});
		std::unordered_map<std::uint32_t, std::uint32_t> values;
		std::unordered_map<std::uint32_t, rtsl::ir::TypeId> value_types;
		std::unordered_map<std::uint32_t, std::uint32_t> constant_indices;
		std::unordered_map<std::uint32_t, std::uint32_t> block_labels;
		for (const rtsl::ir::Block& block : source.blocks) block_labels.emplace(block.id.value(), id());
		for (const rtsl::ir::Parameter& parameter : source.parameters) {
			const std::uint32_t parameter_id = id();
			instruction(55, {typeFor(parameter.type), parameter_id});
			values[parameter.value.value()] = parameter_id;
			value_types[parameter.value.value()] = parameter.type;
		}
		const rtsl::ir::Symbol* symbol = module.findSymbol(source.symbol);
		const std::string_view function_name = symbol ? module.strings.get(symbol->fully_qualified_name) : "<unknown>";
		for (const rtsl::ir::Block& block : source.blocks) {
			instruction(248, {block_labels.at(block.id.value())});
			for (std::size_t argument_index = 0; argument_index < block.arguments.size(); ++argument_index) {
				const rtsl::ir::BlockArgument& argument = block.arguments[argument_index];
				std::vector<std::uint32_t> incoming{typeFor(argument.type), id()};
				for (const rtsl::ir::Block& predecessor : source.blocks) {
					if (!predecessor.terminator) continue;
					for (const rtsl::ir::Successor& successor : predecessor.terminator->successors) {
						if (successor.block != block.id) continue;
						if (argument_index >= successor.arguments.size())
							throw std::runtime_error("RTIR branch does not provide a block argument");
						incoming.push_back(valueFor(values, successor.arguments[argument_index]));
						incoming.push_back(block_labels.at(predecessor.id.value()));
					}
				}
				if (incoming.size() == 2) throw std::runtime_error("RTIR block argument has no predecessor");
				instruction(245, incoming);
				values[argument.value.value()] = incoming[1];
				value_types[argument.value.value()] = argument.type;
			}
			for (const rtsl::ir::Instruction& source_instruction : block.instructions) {
				try {
				emitIRInstruction(source_instruction, values, value_types, constant_indices);
				} catch (const std::exception& error) {
					throw std::runtime_error("RTIR function " + std::string(function_name) +
						" instruction " + std::string(opcodeName(source_instruction.opcode)) + ": " + error.what());
				}
			}
			if (!block.terminator) throw std::runtime_error("RTIR block is missing a terminator");
			if (block.merge.kind == rtsl::ir::MergeKind::merge_selection)
				instruction(247, {block_labels.at(block.merge.merge_block.value()), 0});
			else if (block.merge.kind == rtsl::ir::MergeKind::merge_loop)
				instruction(246, {block_labels.at(block.merge.merge_block.value()), block_labels.at(block.merge.continue_block.value()), 0});

			const rtsl::ir::Terminator& terminator = *block.terminator;
			switch (terminator.kind) {
			case rtsl::ir::TerminatorKind::terminator_branch:
				if (terminator.successors.size() != 1) throw std::runtime_error("RTIR branch terminator is malformed");
				instruction(249, {block_labels.at(terminator.successors[0].block.value())});
				break;
			case rtsl::ir::TerminatorKind::terminator_conditional_branch:
				if (terminator.operands.size() != 1 || terminator.successors.size() != 2)
					throw std::runtime_error("RTIR conditional branch terminator is malformed");
				instruction(250, {valueFor(values, terminator.operands[0]),
					block_labels.at(terminator.successors[0].block.value()), block_labels.at(terminator.successors[1].block.value())});
				break;
			case rtsl::ir::TerminatorKind::terminator_return:
				instruction(253, {});
				break;
			case rtsl::ir::TerminatorKind::terminator_return_value:
				if (terminator.operands.size() != 1) throw std::runtime_error("RTIR value return terminator is malformed");
				if (entry.stage == rtsl::ir::Stage::stage_geometry && source.id == entry.function) instruction(219, {}); // OpEndPrimitive
				instruction(254, {valueFor(values, terminator.operands[0])});
				break;
			case rtsl::ir::TerminatorKind::terminator_kill:
				instruction(252, {});
				break;
			default:
				throw std::runtime_error("RTIR terminator is not supported by the SPIR-V translator");
			}
		}
		instruction(56, {});
	}

	void emitWrapper(const rtsl::ir::Function& stage_function, std::uint32_t wrapper_type) {
		instruction(54, {typeVoid(), wrapper_function, 0, wrapper_type});
		instruction(248, {id()});
		std::vector<std::uint32_t> arguments;
		std::vector<std::uint32_t> path;
		for (const rtsl::ir::Parameter& parameter : stage_function.parameters) {
			if (!parameter.builtin) {
				if (patchType(parameter.type)) arguments.push_back(undef(parameter.type));
				else arguments.push_back(loadInterfaceValue(parameter.type, path));
				continue;
			}
			if (entry.stage != rtsl::ir::Stage::stage_compute || !compute_invocation_id)
				throw std::runtime_error("RTIR parameter builtin is not valid for this entry stage");
			const rtsl::ir::Type* type = module.findType(parameter.type);
			if (!type || type->kind != rtsl::ir::TypeKind::type_unsigned_integer || type->bit_width != 32)
				throw std::runtime_error("RTIR global invocation builtin parameter must be usize");
			std::uint32_t component{};
			switch (*parameter.builtin) {
			case rtsl::ir::Builtin::builtin_global_invocation_x: component = 0; break;
			case rtsl::ir::Builtin::builtin_global_invocation_y: component = 1; break;
			case rtsl::ir::Builtin::builtin_global_invocation_z: component = 2; break;
			default: throw std::runtime_error("RTIR parameter has an unsupported builtin");
			}
			const std::uint32_t invocation = id();
			instruction(61, {typeVector(typeUnsignedInteger(), 3), invocation, compute_invocation_id});
			const std::uint32_t value = id();
			instruction(81, {typeFor(parameter.type), value, invocation, component});
			arguments.push_back(value);
		}
		const std::uint32_t result = id();
		std::vector<std::uint32_t> call{typeFor(stage_function.return_type), result,
			function_ids.at(stage_function.id.value())};
		call.insert(call.end(), arguments.begin(), arguments.end());
		instruction(57, call);
		if (entry.stage == rtsl::ir::Stage::stage_tessellation_control) {
			const std::uint32_t invocation = id();
			instruction(61, {typeUnsignedInteger(), invocation, invocation_id_variable});
			storePatchInterfaceValue(stage_function.return_type, result, invocation, path);
		} else if (entry.stage != rtsl::ir::Stage::stage_geometry) storeInterfaceValue(stage_function.return_type, result, path);
		instruction(253, {});
		instruction(56, {});
	}

	void emitEntryPoint(std::uint32_t function, const std::vector<std::uint32_t>& interfaces) {
		std::vector<std::uint32_t> operands{executionModel(), function};
		appendString(operands, "main");
		operands.insert(operands.end(), interfaces.begin(), interfaces.end());
		instruction(15, operands);
	}

	std::uint32_t executionModel() const {
		switch (entry.stage) {
		case rtsl::ir::Stage::stage_vertex: return 0;
		case rtsl::ir::Stage::stage_tessellation_control: return 1;
		case rtsl::ir::Stage::stage_tessellation_evaluation: return 2;
		case rtsl::ir::Stage::stage_geometry: return 3;
		case rtsl::ir::Stage::stage_fragment: return 4;
		case rtsl::ir::Stage::stage_compute: return 5;
		}
		return 0;
	}

	void emitExecutionModes(std::uint32_t function) {
		if (entry.stage == rtsl::ir::Stage::stage_fragment) instruction(16, {function, 7});
		if (const auto* compute = std::get_if<rtsl::ir::ComputeConfiguration>(&entry.configuration)) instruction(16, {function, 17, compute->workgroup_size[0], compute->workgroup_size[1], compute->workgroup_size[2]});
		if (entry.stage == rtsl::ir::Stage::stage_tessellation_control)
			instruction(16, {function, 26, tessellationControlInvocations()});
		if (const auto* evaluation = std::get_if<rtsl::ir::TessellationEvaluationConfiguration>(&entry.configuration)) {
			switch (evaluation->domain) {
			case rtsl::ir::TessellationDomain::tessellation_domain_triangles: instruction(16, {function, 22}); break;
			case rtsl::ir::TessellationDomain::tessellation_domain_quads: instruction(16, {function, 24}); break;
			case rtsl::ir::TessellationDomain::tessellation_domain_isolines: instruction(16, {function, 25}); break;
			}
			switch (evaluation->spacing) {
			case rtsl::ir::TessellationSpacing::tessellation_spacing_equal: instruction(16, {function, 1}); break;
			case rtsl::ir::TessellationSpacing::tessellation_spacing_fractional_even: instruction(16, {function, 2}); break;
			case rtsl::ir::TessellationSpacing::tessellation_spacing_fractional_odd: instruction(16, {function, 3}); break;
			}
			instruction(16, {function, evaluation->winding == rtsl::ir::Winding::winding_clockwise ? 4u : 5u});
		}
		if (const auto* geometry = std::get_if<rtsl::ir::GeometryConfiguration>(&entry.configuration)) {
			switch (geometry->input) {
			case rtsl::ir::PrimitiveTopology::primitive_points: instruction(16, {function, 19}); break;
			case rtsl::ir::PrimitiveTopology::primitive_lines: instruction(16, {function, 20}); break;
			case rtsl::ir::PrimitiveTopology::primitive_lines_adjacency: instruction(16, {function, 21}); break;
			case rtsl::ir::PrimitiveTopology::primitive_triangles: instruction(16, {function, 22}); break;
			case rtsl::ir::PrimitiveTopology::primitive_triangles_adjacency: instruction(16, {function, 23}); break;
			default: throw std::runtime_error("RTIR geometry input topology is invalid");
			}
			switch (geometry->output) {
			case rtsl::ir::PrimitiveTopology::primitive_points: instruction(16, {function, 27}); break;
			case rtsl::ir::PrimitiveTopology::primitive_line_strip: instruction(16, {function, 28}); break;
			case rtsl::ir::PrimitiveTopology::primitive_triangle_strip: instruction(16, {function, 29}); break;
			default: throw std::runtime_error("RTIR geometry output topology is invalid");
			}
			instruction(16, {function, 26, geometry->maximum_vertices});
			instruction(16, {function, 0, geometry->invocations});
		}
	}

	const rtsl::ir::Module& module;
	const rtsl::ir::EntryPoint& entry;
	std::vector<std::uint32_t> preamble;
	std::vector<std::uint32_t> entry_points;
	std::vector<std::uint32_t> execution_modes;
	std::vector<std::uint32_t> debug;
	std::vector<std::uint32_t> annotations;
	std::vector<std::uint32_t> types_constants;
	std::vector<std::uint32_t> functions;
	std::unordered_map<std::uint32_t, std::uint32_t> types;
	std::unordered_map<std::uint64_t, std::uint32_t> integer_types;
	std::unordered_map<std::uint64_t, std::uint32_t> vectors;
	std::unordered_map<std::uint64_t, std::uint32_t> matrices;
	std::unordered_map<std::uint64_t, std::uint32_t> arrays;
	std::unordered_map<std::uint64_t, std::uint32_t> pointers;
	std::unordered_map<std::uint32_t, std::uint32_t> float_constants;
	std::unordered_map<std::uint32_t, std::uint32_t> unsigned_integer_constants;
	std::unordered_map<std::uint32_t, std::uint32_t> function_ids;
	std::unordered_map<std::uint32_t, std::uint32_t> function_types;
	std::unordered_map<std::uint32_t, std::uint32_t> resource_variables;
	std::unordered_map<std::uint32_t, const rtsl::ir::Resource*> resources;
	std::unordered_map<std::uint64_t, std::uint32_t> runtime_arrays;
	std::unordered_map<std::uint64_t, std::uint32_t> storage_images;
	std::uint32_t compute_invocation_id{};
	std::unordered_map<std::uint32_t, std::uint32_t> uniform_members;
	std::unordered_map<std::uint32_t, std::uint32_t> storage_members;
	std::unordered_map<std::uint32_t, std::uint32_t> storage_sample_variables;
	std::unordered_set<std::uint32_t> uniform_arrays;
	std::unordered_set<std::uint32_t> uniform_structures;
	struct FunctionTypeRecord {
		std::uint32_t result;
		std::vector<std::uint32_t> parameters;
		std::uint32_t id;
	};
	std::vector<FunctionTypeRecord> function_type_records;
	std::vector<InterfaceLeaf> input_leaves;
	std::vector<InterfaceLeaf> output_leaves;
	std::vector<InterfaceLeaf> patch_input_leaves;
	std::vector<InterfaceLeaf> patch_output_leaves;
	std::vector<std::uint32_t> interface_ids;
	std::uint32_t next_id{1};
	std::uint32_t void_type{};
	std::uint32_t float_type{};
	std::uint32_t sampled_image_type{};
	std::uint32_t next_resource_binding{};
	std::uint32_t uniform_block_variable{};
	std::uint32_t storage_block_variable{};
	std::uint32_t wrapper_function{};
	std::uint32_t invocation_id_variable{};
	std::uint32_t tessellation_outer_variable{};
	std::uint32_t tessellation_coordinate_variable{};
};

rt_spirv_stage stage(const rtsl::ir::Stage value) {
	return static_cast<rt_spirv_stage>(value);
}

std::string symbolName(const rtsl::ir::Module& module, rtsl::ir::SymbolId id) {
	const rtsl::ir::Symbol* symbol = module.findSymbol(id);
	return symbol ? std::string(module.strings.get(symbol->fully_qualified_name)) : std::string{};
}

std::size_t typeSize(const rtsl::ir::Module& module, rtsl::ir::TypeId id) {
	const rtsl::ir::Type* type = module.findType(id);
	if (!type) return 0;
	switch (type->kind) {
	case rtsl::ir::TypeKind::type_boolean:
	case rtsl::ir::TypeKind::type_signed_integer:
	case rtsl::ir::TypeKind::type_unsigned_integer:
	case rtsl::ir::TypeKind::type_floating: return std::max<std::size_t>(1, type->bit_width / 8);
	case rtsl::ir::TypeKind::type_vector:
	case rtsl::ir::TypeKind::type_matrix:
	case rtsl::ir::TypeKind::type_array: return typeSize(module, type->element_type) * type->element_count;
	case rtsl::ir::TypeKind::type_structure: {
		std::size_t size{};
		for (const rtsl::ir::StructMember& member : type->members) size += typeSize(module, member.type);
		return size;
	}
	default: return 0;
	}
}

void reflect(const rtsl::ir::Module& module, std::uint32_t selected_stages, rt_spirv_program& program) {
	std::uint32_t next_binding{};
	for (const rtsl::ir::Resource& resource : module.resources) {
		rt_spirv_owned_location location;
		location.name = symbolName(module, resource.symbol);
		location.info.kind = static_cast<rt_spirv_location_kind>(static_cast<std::uint32_t>(resource.kind) + 2u);
		location.info.stages = selected_stages;
		location.info.descriptor_set = resource.binding ? resource.binding->set : 0;
		location.info.binding = resource.binding ? resource.binding->binding : next_binding++;
		program.locations.push_back(std::move(location));
	}
	const std::uint32_t uniform_binding = next_binding++;
	std::size_t uniform_offset{};
	for (const rtsl::ir::Uniform& uniform : module.uniforms) {
		const UniformLayout layout = uniformLayout(module, uniform.type);
		uniform_offset = uniform.offset ? *uniform.offset : roundUp(static_cast<std::uint32_t>(uniform_offset), layout.alignment);
		rt_spirv_owned_location location;
		location.name = symbolName(module, uniform.symbol);
		location.info.kind = RT_SPIRV_UNIFORM_DATA;
		location.info.stages = selected_stages;
		location.info.descriptor_set = 0;
		location.info.binding = uniform_binding;
		location.info.offset = uniform_offset;
		location.info.size = uniform.size ? *uniform.size : layout.size;
		uniform_offset = location.info.offset + location.info.size;
		program.locations.push_back(std::move(location));
	}
	const std::size_t uniform_block_size = roundUp(static_cast<std::uint32_t>(uniform_offset), 16);
	for (rt_spirv_owned_location& location : program.locations) if (location.info.kind == RT_SPIRV_UNIFORM_DATA) location.info.block_size = uniform_block_size;
	const std::uint32_t storage_binding = next_binding++;
	std::size_t storage_offset{};
	for (const rtsl::ir::StorageObject& object : module.storage_objects) {
		if (object.address_space != rtsl::ir::AddressSpace::address_space_storage) continue;
		const UniformLayout layout = uniformLayout(module, object.type);
		storage_offset = roundUp(static_cast<std::uint32_t>(storage_offset), layout.alignment);
		rt_spirv_owned_location location;
		location.name = symbolName(module, object.symbol);
		location.info.kind = RT_SPIRV_STORAGE_DATA;
		location.info.stages = selected_stages;
		location.info.descriptor_set = object.binding ? object.binding->set : 0;
		location.info.binding = object.binding ? object.binding->binding : storage_binding;
		location.info.offset = storage_offset;
		location.info.size = layout.size;
		storage_offset += location.info.size;
		program.locations.push_back(std::move(location));
	}
	for (rt_spirv_owned_location& location : program.locations) if (location.info.kind == RT_SPIRV_STORAGE_DATA) location.info.block_size = storage_offset;
	for (rt_spirv_owned_location& location : program.locations) location.info.name = location.name.c_str();
}

std::string artifactError(const rtsl::Error& artifact_error) {
	std::string result{"RTSL program artifact"};
	if (!artifact_error.context.empty()) {
		result += " ";
		result += artifact_error.context;
		result += " at byte ";
		result += std::to_string(artifact_error.offset);
	}
	if (!artifact_error.message.empty()) {
		result += ": ";
		result += artifact_error.message;
	}
	return result;
}

bool linkedModule(std::span<const std::byte> bytes, rtsl::ir::Module& module, std::string& error) {
	rtsl::ArtifactReader reader;
	rtsl::ReadResult read = reader.read(bytes);
	if (!read) {
		error = read.error ? artifactError(*read.error) : "RTSL program artifact is invalid";
		return false;
	}
	if (read.artifact->kind != rtsl::ArtifactKind::artifact_program) {
		error = "artifact is not a linked RTSL program";
		return false;
	}
	module = std::move(read.artifact->module);
	return true;
}

bool structurallyValid(std::span<const std::uint32_t> words, std::string& error) {
	if (words.size() < 5 || words[0] != 0x07230203u) {
		error = "SPIR-V module header is invalid";
		return false;
	}
	if (!words[3]) {
		error = "SPIR-V module declares a zero id bound";
		return false;
	}
	std::vector<std::vector<std::uint32_t>> function_types;
	for (std::size_t offset = 5; offset < words.size();) {
		const std::uint32_t instruction = words[offset];
		const std::uint16_t word_count = static_cast<std::uint16_t>(instruction >> 16);
		const std::uint16_t opcode = static_cast<std::uint16_t>(instruction);
		if (!word_count || offset + word_count > words.size()) {
			error = "SPIR-V instruction has an invalid word count";
			return false;
		}
		if (opcode == 33) {
			if (word_count < 3) {
				error = "SPIR-V function type is truncated";
				return false;
			}
			std::vector<std::uint32_t> signature(words.begin() + offset + 2, words.begin() + offset + word_count);
			for (const std::vector<std::uint32_t>& existing : function_types) {
				if (existing == signature) {
					error = "SPIR-V module contains a duplicate function type";
					return false;
				}
			}
			function_types.push_back(signature);
		}
		if (opcode == 45 && word_count != 6) {
			error = "SPIR-V constant sampler is truncated or malformed";
			return false;
		}
		offset += word_count;
	}
	return true;
}

}

extern "C" {

int rt_spirv_validate(const uint32_t* words, size_t word_count, char* message, size_t message_size) {
	std::string error;
	if (!words || !rutile::spirv::structurallyValid(std::span<const std::uint32_t>{words, word_count}, error)) {
		if (message && message_size) {
			if (error.empty()) error = "SPIR-V module is empty";
			std::strncpy(message, error.c_str(), message_size - 1);
			message[message_size - 1] = 0;
		}
		return 0;
	}

	spv_context context = spvContextCreate(SPV_ENV_VULKAN_1_3);
	if (!context) {
		if (message && message_size) {
			std::strncpy(message, "SPIR-V Tools could not create a Vulkan 1.3 validation context", message_size - 1);
			message[message_size - 1] = 0;
		}
		return 0;
	}
	spv_diagnostic diagnostic = nullptr;
	const spv_result_t result = spvValidateBinary(context, words, word_count, &diagnostic);
	if (result != SPV_SUCCESS) {
		error = diagnostic && diagnostic->error ? diagnostic->error : "SPIR-V Tools rejected the module";
	}
	spvDiagnosticDestroy(diagnostic);
	spvContextDestroy(context);
	const bool valid = result == SPV_SUCCESS;
	if (!valid && message && message_size) {
		std::strncpy(message, error.c_str(), message_size - 1);
		message[message_size - 1] = 0;
	}
	return valid ? 1 : 0;
}

rt_spirv_status rt_spirv_transpile(const uint8_t* bytes, size_t byte_size, const char* entry_name, rt_spirv_program** program, char* message, size_t message_size) {
	if (program) *program = nullptr;
	if (!bytes || !byte_size || !entry_name || !entry_name[0] || !program) return RT_SPIRV_INVALID_ARTIFACT;
	try {
		rtsl::ir::Module module;
		std::string error;
		if (!rutile::spirv::linkedModule(
			std::as_bytes(std::span{bytes, byte_size}), module, error)) {
			if (message && message_size) { std::strncpy(message, error.c_str(), message_size - 1); message[message_size - 1] = 0; }
			return RT_SPIRV_INVALID_ARTIFACT;
		}
		const rtsl::ir::VerificationResult verification = rtsl::ir::verify(module);
		if (!verification.valid()) {
			if (message && message_size) {
				const rtsl::ir::VerificationIssue& issue = verification.issues().front();
				const std::string diagnostic = issue.context.empty() ? issue.message : issue.context + ": " + issue.message;
				std::strncpy(message, diagnostic.c_str(), message_size - 1);
				message[message_size - 1] = 0;
			}
			return RT_SPIRV_INVALID_MODULE;
		}
		std::vector<const rtsl::ir::EntryPoint*> selected_entries;
		std::uint32_t selected_stages{};
		for (const rtsl::ir::EntryPoint& entry : module.entry_points) {
			if (module.strings.get(entry.source_name) != entry_name) continue;
			const rt_spirv_stage output_stage = rutile::spirv::stage(entry.stage);
			const std::uint32_t stage_bit = 1u << static_cast<std::uint32_t>(output_stage);
			if (selected_stages & stage_bit) {
				if (message && message_size) {
					std::strncpy(message, "requested entry name has more than one overload for the same shader stage", message_size - 1);
					message[message_size - 1] = 0;
				}
				return RT_SPIRV_INVALID_MODULE;
			}
			selected_stages |= stage_bit;
			selected_entries.push_back(&entry);
		}
		if (selected_entries.empty()) {
			if (message && message_size) {
				std::strncpy(message, "requested entry name has no shader-stage overloads", message_size - 1);
				message[message_size - 1] = 0;
			}
			return RT_SPIRV_INVALID_MODULE;
		}
		const std::uint32_t compute = 1u << RT_SPIRV_COMPUTE;
		const std::uint32_t vertex = 1u << RT_SPIRV_VERTEX;
		const std::uint32_t tessellation_control = 1u << RT_SPIRV_TESSELLATION_CONTROL;
		const std::uint32_t tessellation_evaluation = 1u << RT_SPIRV_TESSELLATION_EVALUATION;
		// Linked tessellation programs may carry the linker's identity vertex entry
		// under its internal name. It is part of the selected graphics pipeline even
		// though it does not share the user-facing overload name.
		if ((selected_stages & tessellation_control) && !(selected_stages & vertex)) {
			for (const rtsl::ir::EntryPoint& entry : module.entry_points) {
				if (entry.stage != rtsl::ir::Stage::stage_vertex ||
					module.strings.get(entry.source_name) != "__rtsl_identity_vertex") continue;
				selected_entries.push_back(&entry);
				selected_stages |= vertex;
				break;
			}
		}
		if ((selected_stages & compute) && selected_stages != compute) {
			if (message && message_size) {
				std::strncpy(message, "compute entry cannot be combined with graphics shader stages", message_size - 1);
				message[message_size - 1] = 0;
			}
			return RT_SPIRV_INVALID_MODULE;
		}
		if (!(selected_stages & compute) && !(selected_stages & vertex)) {
			if (message && message_size) {
				std::strncpy(message, "graphics entry requires a vertex-stage overload", message_size - 1);
				message[message_size - 1] = 0;
			}
			return RT_SPIRV_INVALID_MODULE;
		}
		if (!!(selected_stages & tessellation_control) != !!(selected_stages & tessellation_evaluation)) {
			if (message && message_size) {
				std::strncpy(message, "tessellation control and evaluation stages must be provided together", message_size - 1);
				message[message_size - 1] = 0;
			}
			return RT_SPIRV_INVALID_MODULE;
		}

		auto result = new rt_spirv_program;
		for (const rtsl::ir::EntryPoint* entry : selected_entries) {
			const rt_spirv_stage output_stage = rutile::spirv::stage(entry->stage);
			result->stages[output_stage].words = rutile::spirv::ModuleBuilder(module, *entry).build();
			if (!rt_spirv_validate(
					result->stages[output_stage].words.data(),
					result->stages[output_stage].words.size(),
					message,
					message_size
				)) {
				delete result;
				return RT_SPIRV_INVALID_MODULE;
			}
			result->stages[output_stage].entry_point = "main";
		}
		rutile::spirv::reflect(module, selected_stages, *result);
		*program = result;
		return RT_SPIRV_SUCCESS;
	} catch (const std::bad_alloc&) {
		return RT_SPIRV_OUT_OF_MEMORY;
	} catch (const std::exception& exception) {
		if (message && message_size) {
			std::strncpy(message, exception.what(), message_size - 1);
			message[message_size - 1] = 0;
		}
		return RT_SPIRV_UNSUPPORTED_IR;
	}
}

void rt_spirv_program_destroy(rt_spirv_program* program) { delete program; }

const uint32_t* rt_spirv_stage_words(const rt_spirv_program* program, rt_spirv_stage stage, size_t* word_count) {
	if (word_count) *word_count = 0;
	if (!program || stage >= RT_SPIRV_STAGE_COUNT) return nullptr;
	if (word_count) *word_count = program->stages[stage].words.size();
	return program->stages[stage].words.data();
}

const char* rt_spirv_stage_entry_point(const rt_spirv_program* program, rt_spirv_stage stage) {
	return program && stage < RT_SPIRV_STAGE_COUNT && !program->stages[stage].entry_point.empty() ? program->stages[stage].entry_point.c_str() : nullptr;
}

uint32_t rt_spirv_location_count(const rt_spirv_program* program) { return program ? static_cast<uint32_t>(program->locations.size()) : 0; }

int rt_spirv_location(const rt_spirv_program* program, uint32_t index, rt_spirv_location_info* location) {
	if (!program || !location || index >= program->locations.size()) return 0;
	*location = program->locations[index].info;
	return 1;
}

}
