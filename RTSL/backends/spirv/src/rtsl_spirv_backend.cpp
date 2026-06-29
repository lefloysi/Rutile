#include "rtsl_spirv_backend.h"

#include <rtslp_package.hpp>

#include <spirv/unified1/spirv.h>

#include <algorithm>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <initializer_list>
#include <new>
#include <span>
#include <sstream>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace rtsl {
namespace {

void set_error(std::string* error_out, std::string_view msg) {
	if (error_out)
		error_out->assign(msg);
}

void set_error_fmt(std::string* error_out, const char* fmt, ...) {
	if (!error_out)
		return;
	char buf[512];
	va_list args;
	va_start(args, fmt);
	std::vsnprintf(buf, sizeof(buf), fmt, args);
	va_end(args);
	error_out->assign(buf);
}

struct spv_writer {
	std::vector<u32> words;
	u32 bound;
};

void spv_begin(spv_writer* writer) {
	writer->words = {
		SpvMagicNumber,
		SpvVersion,
		0,
		0,
		0,
	};
	writer->bound = 1;
}

void spv_reserve(spv_writer* writer, u32 id) {
	writer->bound = std::max(writer->bound, id + 1);
}

u32 spv_fresh(spv_writer* writer) {
	return writer->bound++;
}

void spv_emit(spv_writer* writer, SpvOp op, std::span<u32> operands) {
	writer->words.push_back(((1u + static_cast<u32>(operands.size())) << SpvWordCountShift) | static_cast<u32>(op));
	writer->words.insert(writer->words.end(), operands.begin(), operands.end());
}

void spv_emit(spv_writer* writer, SpvOp op, std::initializer_list<u32> operands) {
	std::vector<u32> temp(operands.begin(), operands.end());
	spv_emit(writer, op, std::span<u32>(temp.data(), temp.size()));
}

const instruction* find_inst(std::span<const instruction> insts, u32 result_id) {
	for (const auto& inst : insts) {
		if (inst.result_id == result_id)
			return &inst;
	}
	return nullptr;
}

u32 storage_class(u32 storage) {
	switch (storage) {
	case 0: return SpvStorageClassFunction;
	case 1: return SpvStorageClassInput;
	case 2: return SpvStorageClassOutput;
	case 3: return SpvStorageClassUniform;
	case 4: return SpvStorageClassUniformConstant;
	case 5: return SpvStorageClassStorageBuffer;
	case 6: return SpvStorageClassPushConstant;
	case 7: return SpvStorageClassPrivate;
	default: return storage;
	}
}

const function* find_stage_function(const artifact_module& module, stage_kind stage) {
	for (const auto& fn : module.functions) {
		if (fn.stage == stage)
			return &fn;
	}
	return nullptr;
}

std::unordered_map<u32, std::string> struct_names(const artifact_module& module) {
	std::unordered_map<u32, std::string> names;
	std::unordered_set<u32> taken;
	for (const auto& decl : module.structs) {
		const size_t expected_fields = decl.fields.size();
		for (const auto& inst : module.type_constant_pool) {
			if (inst.op != ir_op::type_struct) continue;
			if (!inst.result_id) continue;
			if (taken.contains(inst.result_id)) continue;
			if (inst.operands.size() != expected_fields) continue;
			names.emplace(inst.result_id, decl.name);
			taken.insert(inst.result_id);
			break;
		}
	}
	return names;
}

const stage_interface* find_interface(const artifact_module& module, std::string_view type_name, stage_role role) {
	for (const auto& iface : module.stage_interfaces) {
		if (iface.type_name == type_name && iface.role == role)
			return &iface;
	}
	return nullptr;
}

SpvBuiltIn builtin_for(const stage_field& field) {
	if (field.builtin == "position" || field.interpolation == "clip")
		return SpvBuiltInPosition;
	if (field.builtin == "frag_coord")
		return SpvBuiltInFragCoord;
	if (field.builtin == "front_facing")
		return SpvBuiltInFrontFacing;
	if (field.builtin == "frag_depth")
		return SpvBuiltInFragDepth;
	return SpvBuiltInPosition;
}

SpvOp spv_op(ir_op op) {
	switch (op) {
	case ir_op::type_void: return SpvOpTypeVoid;
	case ir_op::type_bool: return SpvOpTypeBool;
	case ir_op::type_int:
	case ir_op::type_uint: return SpvOpTypeInt;
	case ir_op::type_float: return SpvOpTypeFloat;
	case ir_op::type_vector: return SpvOpTypeVector;
	case ir_op::type_matrix: return SpvOpTypeMatrix;
	case ir_op::type_struct: return SpvOpTypeStruct;
	case ir_op::type_pointer: return SpvOpTypePointer;
	case ir_op::type_array: return SpvOpTypeArray;
	case ir_op::type_image: return SpvOpTypeImage;
	case ir_op::type_sampler: return SpvOpTypeSampler;
	case ir_op::type_sampled_image: return SpvOpTypeSampledImage;
	case ir_op::type_function: return SpvOpTypeFunction;
	case ir_op::constant_int:
	case ir_op::constant_uint:
	case ir_op::constant_float: return SpvOpConstant;
	case ir_op::constant_composite: return SpvOpConstantComposite;
	case ir_op::variable: return SpvOpVariable;
	case ir_op::load: return SpvOpLoad;
	case ir_op::store: return SpvOpStore;
	case ir_op::composite_construct: return SpvOpCompositeConstruct;
	case ir_op::composite_extract: return SpvOpCompositeExtract;
	case ir_op::f_mul: return SpvOpFMul;
	case ir_op::matrix_times_vector: return SpvOpMatrixTimesVector;
	case ir_op::sampled_image: return SpvOpSampledImage;
	case ir_op::image_sample_implicit_lod: return SpvOpImageSampleImplicitLod;
	default: return SpvOpNop;
	}
}

const char* op_name(ir_op op) {
	switch (op) {
	case ir_op::nop: return "Nop";
	case ir_op::type_void: return "TypeVoid";
	case ir_op::type_bool: return "TypeBool";
	case ir_op::type_int: return "TypeInt";
	case ir_op::type_uint: return "TypeUInt";
	case ir_op::type_float: return "TypeFloat";
	case ir_op::type_vector: return "TypeVector";
	case ir_op::type_matrix: return "TypeMatrix";
	case ir_op::type_struct: return "TypeStruct";
	case ir_op::type_pointer: return "TypePointer";
	case ir_op::type_array: return "TypeArray";
	case ir_op::type_function: return "TypeFunction";
	case ir_op::type_image: return "TypeImage";
	case ir_op::type_sampler: return "TypeSampler";
	case ir_op::type_sampled_image: return "TypeSampledImage";
	case ir_op::constant_bool: return "ConstantBool";
	case ir_op::constant_int: return "ConstantInt";
	case ir_op::constant_uint: return "ConstantUInt";
	case ir_op::constant_float: return "ConstantFloat";
	case ir_op::constant_composite: return "ConstantComposite";
	case ir_op::variable: return "Variable";
	case ir_op::load: return "Load";
	case ir_op::store: return "Store";
	case ir_op::access_chain: return "AccessChain";
	case ir_op::composite_construct: return "CompositeConstruct";
	case ir_op::composite_extract: return "CompositeExtract";
	case ir_op::composite_insert: return "CompositeInsert";
	case ir_op::vector_shuffle: return "VectorShuffle";
	case ir_op::f_add: return "FAdd";
	case ir_op::f_sub: return "FSub";
	case ir_op::f_mul: return "FMul";
	case ir_op::f_div: return "FDiv";
	case ir_op::f_mod: return "FMod";
	case ir_op::f_negate: return "FNegate";
	case ir_op::i_add: return "IAdd";
	case ir_op::i_sub: return "ISub";
	case ir_op::i_mul: return "IMul";
	case ir_op::s_div: return "SDiv";
	case ir_op::u_div: return "UDiv";
	case ir_op::s_mod: return "SMod";
	case ir_op::u_mod: return "UMod";
	case ir_op::vector_times_scalar: return "VectorTimesScalar";
	case ir_op::matrix_times_scalar: return "MatrixTimesScalar";
	case ir_op::matrix_times_vector: return "MatrixTimesVector";
	case ir_op::matrix_times_matrix: return "MatrixTimesMatrix";
	case ir_op::dot: return "Dot";
	case ir_op::cross: return "Cross";
	case ir_op::f_ord_equal: return "FOrdEqual";
	case ir_op::f_ord_not_equal: return "FOrdNotEqual";
	case ir_op::f_ord_less: return "FOrdLess";
	case ir_op::f_ord_less_equal: return "FOrdLessEqual";
	case ir_op::f_ord_greater: return "FOrdGreater";
	case ir_op::f_ord_greater_equal: return "FOrdGreaterEqual";
	case ir_op::i_equal: return "IEqual";
	case ir_op::i_not_equal: return "INotEqual";
	case ir_op::s_less: return "SLess";
	case ir_op::s_less_equal: return "SLessEqual";
	case ir_op::s_greater: return "SGreater";
	case ir_op::s_greater_equal: return "SGreaterEqual";
	case ir_op::u_less: return "ULess";
	case ir_op::u_less_equal: return "ULessEqual";
	case ir_op::u_greater: return "UGreater";
	case ir_op::u_greater_equal: return "UGreaterEqual";
	case ir_op::logical_and: return "LogicalAnd";
	case ir_op::logical_or: return "LogicalOr";
	case ir_op::logical_not: return "LogicalNot";
	case ir_op::convert_f_to_u: return "ConvertFToU";
	case ir_op::convert_f_to_s: return "ConvertFToS";
	case ir_op::convert_s_to_f: return "ConvertSToF";
	case ir_op::convert_u_to_f: return "ConvertUToF";
	case ir_op::bitcast: return "Bitcast";
	case ir_op::label: return "Label";
	case ir_op::branch: return "Branch";
	case ir_op::branch_conditional: return "BranchConditional";
	case ir_op::selection_merge: return "SelectionMerge";
	case ir_op::loop_merge: return "LoopMerge";
	case ir_op::return_op: return "Return";
	case ir_op::return_value: return "ReturnValue";
	case ir_op::function_parameter: return "FunctionParameter";
	case ir_op::function_call: return "FunctionCall";
	case ir_op::sampled_image: return "SampledImage";
	case ir_op::image_sample_implicit_lod: return "ImageSampleImplicitLod";
	case ir_op::image_sample_explicit_lod: return "ImageSampleExplicitLod";
	case ir_op::image_read: return "ImageRead";
	case ir_op::image_write: return "ImageWrite";
	case ir_op::read_input: return "ReadInput";
	case ir_op::write_output: return "WriteOutput";
	case ir_op::read_builtin: return "ReadBuiltin";
	case ir_op::write_builtin: return "WriteBuiltin";
	case ir_op::ext_inst: return "ExtInst";
	default: return "Unknown";
	}
}

bool emit_type_or_constant_section(std::vector<u32>* section, const instruction& inst) {
	std::vector<u32> operands;
	switch (inst.op) {
	case ir_op::type_void:
	case ir_op::type_bool:
		operands = { inst.result_id };
		break;
	case ir_op::type_int:
		if (inst.literals.empty()) return false;
		operands = { inst.result_id, inst.literals[0], 1 };
		break;
	case ir_op::type_uint:
		if (inst.literals.empty()) return false;
		operands = { inst.result_id, inst.literals[0], 0 };
		break;
	case ir_op::type_float:
		if (inst.literals.empty()) return false;
		operands = { inst.result_id, inst.literals[0] };
		break;
	case ir_op::type_vector:
	case ir_op::type_matrix:
		if (inst.operands.empty() || inst.literals.empty()) return false;
		operands = { inst.result_id, inst.operands[0], inst.literals[0] };
		break;
	case ir_op::type_struct:
	case ir_op::type_function:
	case ir_op::type_array:
	case ir_op::type_image:
	case ir_op::type_sampler:
	case ir_op::type_sampled_image:
		operands = { inst.result_id };
		operands.insert(operands.end(), inst.operands.begin(), inst.operands.end());
		operands.insert(operands.end(), inst.literals.begin(), inst.literals.end());
		break;
	case ir_op::type_pointer:
		if (inst.literals.empty() || inst.operands.empty()) return false;
		operands = { inst.result_id, storage_class(inst.literals[0]), inst.operands[0] };
		break;
	case ir_op::constant_int:
	case ir_op::constant_uint:
	case ir_op::constant_float:
		operands = { inst.type_id, inst.result_id };
		operands.insert(operands.end(), inst.literals.begin(), inst.literals.end());
		break;
	case ir_op::constant_composite:
		operands = { inst.type_id, inst.result_id };
		operands.insert(operands.end(), inst.operands.begin(), inst.operands.end());
		break;
	default:
		return false;
	}
	section->push_back(((1u + static_cast<u32>(operands.size())) << SpvWordCountShift) | static_cast<u32>(spv_op(inst.op)));
	section->insert(section->end(), operands.begin(), operands.end());
	return true;
}

std::vector<u32> remap_operands(const std::vector<u32>& operands, const std::unordered_map<u32, u32>& remap) {
	std::vector<u32> out;
	out.reserve(operands.size());
	for (u32 operand : operands) {
		auto it = remap.find(operand);
		out.push_back(it != remap.end() ? it->second : operand);
	}
	return out;
}

bool is_type_op(ir_op op) {
	return op >= ir_op::type_void && op <= ir_op::type_sampled_image;
}

std::string type_key(const instruction& inst) {
	std::ostringstream key;
	key << static_cast<u32>(inst.op) << '|';
	key << inst.type_id << '|';
	for (u32 operand : inst.operands)
		key << operand << ',';
	key << '|';
	for (u32 literal : inst.literals)
		key << literal << ',';
	return key.str();
}

std::unordered_set<u32> collect_used_value_ids(const function& fn) {
	std::unordered_set<u32> used;
	for (const auto& inst : fn.body) {
		for (u32 operand : inst.operands) {
			used.insert(operand);
		}
	}
	return used;
}

bool has_type_id(const artifact_module& module, u32 type_id) {
	if (!type_id)
		return false;
	for (const auto& inst : module.type_constant_pool) {
		if (inst.result_id == type_id)
			return true;
	}
	return false;
}

bool validate_type_refs(const artifact_module& module, const function& fn, std::string* error_out) {
	for (const auto& inst : module.type_constant_pool) {
		for (u32 operand : inst.operands) {
			if (!has_type_id(module, operand)) {
				set_error(error_out, "dangling RTSLP type reference " + std::to_string(operand));
				return false;
			}
		}
	}
	u32 input_type = 0;
	if (fn.parameter_ids.size() >= 2) {
		for (const auto& inst : fn.body) {
			if (inst.op == ir_op::function_parameter && inst.result_id == fn.parameter_ids[1]) {
				input_type = inst.type_id;
				break;
			}
		}
	}
	const u32 output_type = fn.return_type_id;
	if (input_type && !has_type_id(module, input_type)) {
		set_error(error_out, "dangling RTSLP input type reference " + std::to_string(input_type));
		return false;
	}
	if (output_type && !has_type_id(module, output_type)) {
		set_error(error_out, "dangling RTSLP output type reference " + std::to_string(output_type));
		return false;
	}
	return true;
}

bool type_ready(const artifact_module& /*module*/, const instruction& inst, const std::unordered_set<u32>& emitted_ids) {
	auto has_emitted = [&](u32 id) {
		return !id || emitted_ids.contains(id);
	};
	switch (inst.op) {
	case ir_op::type_void:
	case ir_op::type_bool:
		return true;
	case ir_op::type_int:
	case ir_op::type_uint:
	case ir_op::type_float:
		return !inst.literals.empty();
	case ir_op::type_vector:
	case ir_op::type_matrix:
		return !inst.operands.empty() && !inst.literals.empty() && has_emitted(inst.operands[0]);
	case ir_op::type_struct:
		for (u32 operand : inst.operands) {
			if (!has_emitted(operand))
				return false;
		}
		return true;
	case ir_op::type_pointer:
		return !inst.literals.empty() && !inst.operands.empty() && has_emitted(inst.operands[0]);
	case ir_op::type_array:
		return !inst.operands.empty() && !inst.literals.empty() && has_emitted(inst.operands[0]);
	case ir_op::type_function:
		for (u32 operand : inst.operands) {
			if (!has_emitted(operand))
				return false;
		}
		return true;
	case ir_op::type_image:
	case ir_op::type_sampler:
	case ir_op::type_sampled_image:
		for (u32 operand : inst.operands) {
			if (!has_emitted(operand))
				return false;
		}
		return true;
	default:
		return true;
	}
}

bool emit_global_variable_section(std::vector<u32>* section, const instruction& inst) {
	if (inst.op != ir_op::variable || !inst.type_id || !inst.result_id || inst.literals.empty()) {
		return false;
	}
	std::vector<u32> operands = { inst.type_id, inst.result_id, storage_class(inst.literals[0]) };
	operands.insert(operands.end(), inst.operands.begin(), inst.operands.end());
	section->push_back(((1u + static_cast<u32>(operands.size())) << SpvWordCountShift) | static_cast<u32>(SpvOpVariable));
	section->insert(section->end(), operands.begin(), operands.end());
	return true;
}

struct wrapped_uniform {
	u32 value_type;
	u32 struct_type;
	u32 pointer_type;
};

artifact_module normalize_vulkan_types(const artifact_module& module) {
	artifact_module normalized = module;
	u32 float_type = 0;
	u32 image_type = 0;
	for (const auto& inst : normalized.type_constant_pool) {
		if (!float_type && inst.op == ir_op::type_float) {
			float_type = inst.result_id;
		}
		if (!image_type && inst.op == ir_op::type_image) {
			image_type = inst.result_id;
		}
	}
	for (auto& inst : normalized.type_constant_pool) {
		if (inst.op == ir_op::type_image && inst.operands.empty() && inst.literals.empty() && float_type) {
			inst.operands = { float_type };
			inst.literals = { SpvDim2D, 0, 0, 0, 1, SpvImageFormatUnknown };
		}
		if (inst.op == ir_op::type_sampled_image && inst.operands.empty() && image_type) {
			inst.operands = { image_type };
		}
	}
	return normalized;
}

const char* stage_name(stage_kind stage) {
	switch (stage) {
	case stage_kind::vertex: return "vertex";
	case stage_kind::fragment: return "fragment";
	case stage_kind::compute: return "compute";
	default: return "unknown";
	}
}

bool emit_rtslp_stage_spirv(const artifact_module& module, stage_kind stage, std::vector<u32>* spirv_out, std::string* error_out) {
	if (!spirv_out) {
		set_error(error_out, "spirv_out must not be null");
		return false;
	}

	const artifact_module vulkan_module = normalize_vulkan_types(module);
	const auto* fn = find_stage_function(vulkan_module, stage);
	if (!fn) {
		set_error_fmt(error_out, "RTSLP is missing %s stage", stage_name(stage));
		return false;
	}

	const auto names = struct_names(vulkan_module);
	std::string type_error;
	if (!validate_type_refs(vulkan_module, *fn, &type_error)) {
		set_error(error_out, type_error);
		return false;
	}
	const u32 input_type = [&]() -> u32 {
		if (fn->parameter_ids.size() < 2)
			return 0;
		for (const auto& inst : fn->body) {
			if (inst.op == ir_op::function_parameter && inst.result_id == fn->parameter_ids[1]) {
				return inst.type_id;
			}
		}
		return 0;
	}();
	const u32 output_type = fn->return_type_id;
	const auto* input_inst = input_type ? find_inst(vulkan_module.type_constant_pool, input_type) : nullptr;
	const auto* output_inst = output_type ? find_inst(vulkan_module.type_constant_pool, output_type) : nullptr;
	const std::string input_name = (input_type && names.contains(input_type)) ? names.at(input_type) : std::string{};
	const std::string output_name = (output_type && names.contains(output_type)) ? names.at(output_type) : std::string{};
	const auto* input_interface = input_name.empty() ? nullptr : find_interface(vulkan_module, input_name, stage == stage_kind::vertex ? stage_role::input : stage_role::varying);
	const auto* output_interface = output_name.empty() ? nullptr : find_interface(vulkan_module, output_name, stage == stage_kind::fragment ? stage_role::output : stage_role::varying);
	const auto used_value_ids = collect_used_value_ids(*fn);

	spv_writer writer;
	spv_begin(&writer);
	std::vector<u32> entry_ops;
	std::vector<u32> execution_ops;
	std::vector<u32> annotation_ops;
	std::vector<u32> declaration_ops;
	std::vector<u32> function_ops;

	auto emit_section = [](std::vector<u32>* section, SpvOp op, const std::vector<u32>& operands) {
		section->push_back(((1u + static_cast<u32>(operands.size())) << SpvWordCountShift) | static_cast<u32>(op));
		section->insert(section->end(), operands.begin(), operands.end());
	};
	auto emit_section_list = [&](std::vector<u32>* section, SpvOp op, std::initializer_list<u32> operands) {
		emit_section(section, op, std::vector<u32>(operands));
	};

	spv_emit(&writer, SpvOpCapability, { SpvCapabilityShader });
	spv_emit(&writer, SpvOpMemoryModel, { SpvAddressingModelLogical, SpvMemoryModelGLSL450 });

	for (const auto& inst : vulkan_module.type_constant_pool) {
		if (spv_op(inst.op) == SpvOpNop) {
			set_error(error_out, op_name(inst.op));
			return false;
		}
	}
	for (const auto& inst : vulkan_module.global_variables) {
		if (spv_op(inst.op) == SpvOpNop) {
			set_error(error_out, op_name(inst.op));
			return false;
		}
	}
	for (const auto& inst : fn->body) {
		if (inst.op == ir_op::label || inst.op == ir_op::function_parameter)
			continue;
		if (inst.op == ir_op::return_op || inst.op == ir_op::return_value)
			continue;
		if (inst.op >= ir_op::type_void && inst.op <= ir_op::constant_composite)
			continue;
		if (spv_op(inst.op) == SpvOpNop) {
			set_error(error_out, op_name(inst.op));
			return false;
		}
	}

	for (const auto& inst : module.type_constant_pool) {
		if (inst.result_id)
			spv_reserve(&writer, inst.result_id);
	}
	for (const auto& inst : vulkan_module.global_variables) {
		if (inst.result_id)
			spv_reserve(&writer, inst.result_id);
	}
	for (const auto& function_def : vulkan_module.functions) {
		if (function_def.result_id)
			spv_reserve(&writer, function_def.result_id);
		for (const auto& inst : function_def.body) {
			if (inst.result_id)
				spv_reserve(&writer, inst.result_id);
		}
	}

	std::unordered_map<u32, u32> canonical_type_ids;
	{
		std::unordered_map<std::string, u32> canonical_types;
		for (const auto& inst : vulkan_module.type_constant_pool) {
			if (!is_type_op(inst.op) || !inst.result_id)
				continue;
			const std::string key = type_key(inst);
			auto [it, inserted] = canonical_types.emplace(key, inst.result_id);
			canonical_type_ids.emplace(inst.result_id, it->second);
			(void)inserted;
		}
	}

	u32 void_type = 0;
	for (const auto& inst : vulkan_module.type_constant_pool) {
		if (inst.op == ir_op::type_void) {
			void_type = inst.result_id;
			break;
		}
	}
	if (!void_type) {
		set_error_fmt(error_out, "RTSLP %s stage is missing void type", stage_name(stage));
		return false;
	}
	const u32 entry_fn_type = spv_fresh(&writer);
	std::unordered_set<u32> emitted_decl_ids;
	while (true) {
		bool progress = false;
		for (const auto& inst : vulkan_module.type_constant_pool) {
			if (!inst.result_id || emitted_decl_ids.contains(inst.result_id))
				continue;
			if (auto it = canonical_type_ids.find(inst.result_id); it != canonical_type_ids.end() && it->second != inst.result_id) {
				emitted_decl_ids.insert(inst.result_id);
				continue;
			}
			if (!type_ready(vulkan_module, inst, emitted_decl_ids))
				continue;
			instruction remapped = inst;
			if (!emit_type_or_constant_section(&declaration_ops, remapped)) {
				set_error_fmt(
					error_out,
					"RTSLP %s stage cannot emit type/constant %u (%s)",
					stage_name(stage),
					inst.result_id,
					op_name(inst.op)
				);
				return false;
			}
			emitted_decl_ids.insert(inst.result_id);
			progress = true;
		}
		if (!progress)
			break;
	}
	for (const auto& inst : vulkan_module.type_constant_pool) {
		if (inst.result_id && !emitted_decl_ids.contains(inst.result_id)) {
			set_error_fmt(
				error_out,
				"RTSLP %s stage cannot order type/constant %u (%s)",
				stage_name(stage),
				inst.result_id,
				op_name(inst.op)
			);
			return false;
		}
	}
	emit_section_list(&declaration_ops, SpvOpTypeFunction, { entry_fn_type, void_type });
	std::vector<u32> entry_interfaces;
	std::unordered_map<u32, wrapped_uniform> wrapped_uniforms;
	for (size_t i = 0; i < vulkan_module.global_variables.size(); ++i) {
		const auto& inst = vulkan_module.global_variables[i];
		if (spv_op(inst.op) == SpvOpNop) {
			set_error(error_out, op_name(inst.op));
			return false;
		}
		instruction global_inst = inst;
		if (!inst.literals.empty() && storage_class(inst.literals[0]) == SpvStorageClassUniform) {
			const auto* pointer_type = find_inst(vulkan_module.type_constant_pool, inst.type_id);
			const auto* pointee_type = pointer_type && !pointer_type->operands.empty()
										   ? find_inst(vulkan_module.type_constant_pool, pointer_type->operands[0])
										   : nullptr;
			if (pointer_type && pointee_type && pointee_type->op != ir_op::type_struct) {
				wrapped_uniform wrapped = {};
				wrapped.value_type = pointer_type->operands[0];
				wrapped.struct_type = spv_fresh(&writer);
				wrapped.pointer_type = spv_fresh(&writer);
				emit_section_list(&declaration_ops, SpvOpTypeStruct, { wrapped.struct_type, wrapped.value_type });
				emit_section_list(&declaration_ops, SpvOpTypePointer, { wrapped.pointer_type, SpvStorageClassUniform, wrapped.struct_type });
				emit_section_list(&annotation_ops, SpvOpDecorate, { wrapped.struct_type, SpvDecorationBlock });
				emit_section_list(&annotation_ops, SpvOpMemberDecorate, { wrapped.struct_type, 0, SpvDecorationOffset, 0 });
				if (pointee_type->op == ir_op::type_matrix) {
					emit_section_list(&annotation_ops, SpvOpMemberDecorate, { wrapped.struct_type, 0, SpvDecorationColMajor });
					emit_section_list(&annotation_ops, SpvOpMemberDecorate, { wrapped.struct_type, 0, SpvDecorationMatrixStride, 16 });
				}
				wrapped_uniforms.emplace(inst.result_id, wrapped);
				global_inst.type_id = wrapped.pointer_type;
			}
		}
		if (!emit_global_variable_section(&declaration_ops, global_inst)) {
			set_error_fmt(
				error_out,
				"RTSLP %s stage cannot emit global variable %u",
				stage_name(stage),
				inst.result_id
			);
			return false;
		}
		if (i < vulkan_module.uniforms.size()) {
			emit_section_list(&annotation_ops, SpvOpDecorate, { inst.result_id, SpvDecorationDescriptorSet, vulkan_module.uniforms[i].set });
			emit_section_list(&annotation_ops, SpvOpDecorate, { inst.result_id, SpvDecorationBinding, vulkan_module.uniforms[i].binding });
		}
		// SPIR-V 1.4+ requires every non-Function global referenced by the
		// entry point to be listed in OpEntryPoint's interface.
		if (!inst.literals.empty()) {
			const u32 sc = storage_class(inst.literals[0]);
			if (sc != SpvStorageClassFunction) {
				entry_interfaces.push_back(inst.result_id);
			}
		}
	}

	std::vector<u32> input_vars;
	std::vector<u32> output_vars;

	auto emit_interface_vars =
		[&](const instruction* struct_inst,
			const stage_interface* iface,
			SpvStorageClass storage,
			std::vector<u32>* vars) {
			if (!struct_inst || !iface)
				return;
			for (size_t i = 0; i < iface->fields.size() && i < struct_inst->operands.size(); ++i) {
				const auto& field = iface->fields[i];
				if (!struct_inst->operands[i])
					return;
				const bool is_clip_position = field.builtin == "position" || field.interpolation == "clip";
				if (storage == SpvStorageClassInput && is_clip_position) {
					vars->push_back(0);
					continue;
				}
				const u32 ptr_type = spv_fresh(&writer);
				emit_section_list(&declaration_ops, SpvOpTypePointer, { ptr_type, static_cast<u32>(storage), struct_inst->operands[i] });
				const u32 var_id = spv_fresh(&writer);
				emit_section_list(&declaration_ops, SpvOpVariable, { ptr_type, var_id, static_cast<u32>(storage) });
				if (field.has_location) {
					emit_section_list(&annotation_ops, SpvOpDecorate, { var_id, SpvDecorationLocation, field.location });
				} else if (!field.builtin.empty() || is_clip_position) {
					emit_section_list(&annotation_ops, SpvOpDecorate, { var_id, SpvDecorationBuiltIn, static_cast<u32>(builtin_for(field)) });
				}
				entry_interfaces.push_back(var_id);
				vars->push_back(var_id);
			}
		};

	emit_interface_vars(input_inst, input_interface, SpvStorageClassInput, &input_vars);
	emit_interface_vars(output_inst, output_interface, SpvStorageClassOutput, &output_vars);

	const u32 entry_id = spv_fresh(&writer);
	std::vector<u32> entry_words = {
		static_cast<u32>(stage == stage_kind::vertex ? SpvExecutionModelVertex : SpvExecutionModelFragment),
		entry_id,
		0x6e69616d,
		0
	};
	entry_words.insert(entry_words.end(), entry_interfaces.begin(), entry_interfaces.end());
	emit_section(&entry_ops, SpvOpEntryPoint, entry_words);
	if (stage == stage_kind::fragment) {
		emit_section_list(&execution_ops, SpvOpExecutionMode, { entry_id, SpvExecutionModeOriginUpperLeft });
	}

	emit_section_list(&function_ops, SpvOpFunction, { void_type, entry_id, 0, entry_fn_type });
	const u32 label_id = spv_fresh(&writer);
	emit_section_list(&function_ops, SpvOpLabel, { label_id });

	std::unordered_map<u32, u32> remap;
	if (input_inst && fn->parameter_ids.size() >= 2) {
		std::vector<u32> field_values;
		const bool parameter_used = used_value_ids.contains(fn->parameter_ids[1]);
		if (parameter_used) {
			for (size_t i = 0; i < input_vars.size() && i < input_inst->operands.size(); ++i) {
				const u32 field_type = input_inst->operands[i];
				const u32 value_id = spv_fresh(&writer);
				if (input_vars[i] == 0) {
					emit_section_list(&function_ops, SpvOpUndef, { field_type, value_id });
				} else {
					emit_section_list(&function_ops, SpvOpLoad, { field_type, value_id, input_vars[i] });
				}
				field_values.push_back(value_id);
			}
			const u32 param_value = spv_fresh(&writer);
			std::vector<u32> construct = { input_type, param_value };
			construct.insert(construct.end(), field_values.begin(), field_values.end());
			emit_section(&function_ops, SpvOpCompositeConstruct, construct);
			remap.emplace(fn->parameter_ids[1], param_value);
		} else {
			for (size_t i = 0; i < input_vars.size() && i < input_inst->operands.size(); ++i) {
				if (i + 1 >= fn->parameter_ids.size())
					continue;
				if (!used_value_ids.contains(fn->parameter_ids[i + 1]))
					continue;
				if (input_vars[i] == 0)
					continue;
				const u32 load_id = spv_fresh(&writer);
				emit_section_list(&function_ops, SpvOpLoad, { input_inst->operands[i], load_id, input_vars[i] });
				remap.emplace(fn->parameter_ids[i + 1], load_id);
			}
		}
	}

	for (const auto& inst : fn->body) {
		if (inst.op == ir_op::label || inst.op == ir_op::function_parameter)
			continue;
		if (inst.op >= ir_op::type_void && inst.op <= ir_op::constant_composite)
			continue;
		if (inst.op == ir_op::return_op) {
			emit_section(&function_ops, SpvOpReturn, {});
			continue;
		}
		if (inst.op == ir_op::return_value) {
			const u32 value_id = remap.contains(inst.operands[0]) ? remap.at(inst.operands[0]) : inst.operands[0];
			if (output_inst && output_interface) {
				for (size_t i = 0; i < output_vars.size() && i < output_inst->operands.size(); ++i) {
					const u32 field_id = spv_fresh(&writer);
					emit_section_list(&function_ops, SpvOpCompositeExtract, { output_inst->operands[i], field_id, value_id, static_cast<u32>(i) });
					emit_section_list(&function_ops, SpvOpStore, { output_vars[i], field_id });
				}
			}
			emit_section(&function_ops, SpvOpReturn, {});
			continue;
		}
		if (inst.op == ir_op::load && !inst.operands.empty()) {
			const u32 source_id = remap.contains(inst.operands[0]) ? remap.at(inst.operands[0]) : inst.operands[0];
			if (auto it = wrapped_uniforms.find(source_id); it != wrapped_uniforms.end()) {
				const u32 wrapper_value = spv_fresh(&writer);
				emit_section_list(&function_ops, SpvOpLoad, { it->second.struct_type, wrapper_value, source_id });
				emit_section_list(&function_ops, SpvOpCompositeExtract, { it->second.value_type, inst.result_id, wrapper_value, 0 });
				continue;
			}
		}

		spv_reserve(&writer, inst.result_id);
		std::vector<u32> words;
		if (inst.result_id && inst.type_id) {
			words.push_back(inst.type_id);
			words.push_back(inst.result_id);
		}
		auto operands = remap_operands(inst.operands, remap);
		words.insert(words.end(), operands.begin(), operands.end());
		// OpVariable's first literal is a SPIR-V storage class; remap through storage_class().
		if (inst.op == ir_op::variable && !inst.literals.empty()) {
			words.push_back(storage_class(inst.literals[0]));
			words.insert(words.end(), inst.literals.begin() + 1, inst.literals.end());
		} else {
			words.insert(words.end(), inst.literals.begin(), inst.literals.end());
		}
		const SpvOp op = spv_op(inst.op);
		if (op == SpvOpNop) {
			set_error_fmt(
				error_out,
				"RTSLP %s stage cannot lower op %s",
				stage_name(stage),
				op_name(inst.op)
			);
			return false;
		}
		emit_section(&function_ops, op, words);
	}

	emit_section(&function_ops, SpvOpFunctionEnd, {});
	writer.words.insert(writer.words.end(), entry_ops.begin(), entry_ops.end());
	writer.words.insert(writer.words.end(), execution_ops.begin(), execution_ops.end());
	writer.words.insert(writer.words.end(), annotation_ops.begin(), annotation_ops.end());
	writer.words.insert(writer.words.end(), declaration_ops.begin(), declaration_ops.end());
	writer.words.insert(writer.words.end(), function_ops.begin(), function_ops.end());
	writer.words[3] = writer.bound;
	*spirv_out = std::move(writer.words);
	return true;
}

} // namespace
} // namespace rtsl

extern "C" void rtsl_graphics_spirv_free(rtsl_graphics_spirv* module) {
	if (!module)
		return;
	std::free(module->vertex_words);
	std::free(module->fragment_words);
	std::free(module->uniforms);
	*module = {};
}

extern "C" rtsl_compile_status rtsl_compile_rtslp_graphics(size_t program_size, const void* program_source, rtsl_graphics_spirv* out) {
	if (!out) {
		return RTSL_COMPILE_INVALID_ARGUMENT;
	}
	*out = {};
	try {
		rtsl::artifact_module module = rtsl::read_rtslp_module(program_size, program_source);
		std::vector<std::uint32_t> vertex_spirv;
		std::vector<std::uint32_t> fragment_spirv;
		std::string err;
		if (!rtsl::emit_rtslp_stage_spirv(module, rtsl::stage_kind::vertex, &vertex_spirv, &err)) {
			return RTSL_COMPILE_FAILED;
		}
		if (!rtsl::emit_rtslp_stage_spirv(module, rtsl::stage_kind::fragment, &fragment_spirv, &err)) {
			return RTSL_COMPILE_FAILED;
		}

		const size_t vertex_bytes = vertex_spirv.size() * sizeof(std::uint32_t);
		const size_t fragment_bytes = fragment_spirv.size() * sizeof(std::uint32_t);
		out->vertex_words = static_cast<std::uint32_t*>(std::malloc(vertex_bytes ? vertex_bytes : 1));
		out->fragment_words = static_cast<std::uint32_t*>(std::malloc(fragment_bytes ? fragment_bytes : 1));
		if (!out->vertex_words || !out->fragment_words) {
			rtsl_graphics_spirv_free(out);
			return RTSL_COMPILE_OUT_OF_MEMORY;
		}
		std::memcpy(out->vertex_words, vertex_spirv.data(), vertex_bytes);
		std::memcpy(out->fragment_words, fragment_spirv.data(), fragment_bytes);
		out->vertex_word_count = vertex_spirv.size();
		out->fragment_word_count = fragment_spirv.size();

		if (!module.uniforms.empty()) {
			out->uniforms = static_cast<rtsl_uniform_info*>(std::calloc(module.uniforms.size(), sizeof(rtsl_uniform_info)));
			if (!out->uniforms) {
				rtsl_graphics_spirv_free(out);
				return RTSL_COMPILE_OUT_OF_MEMORY;
			}
			for (size_t i = 0; i < module.uniforms.size(); ++i) {
				const auto& src = module.uniforms[i];
				auto& dst = out->uniforms[i];
				std::snprintf(dst.name, sizeof(dst.name), "%s", src.name.c_str());
				std::snprintf(dst.type, sizeof(dst.type), "%s", src.type.c_str());
				dst.set = src.set;
				dst.binding = src.binding;
			}
			out->uniform_count = module.uniforms.size();
		}
		return RTSL_COMPILE_OK;
	} catch (const std::bad_alloc&) {
		rtsl_graphics_spirv_free(out);
		return RTSL_COMPILE_OUT_OF_MEMORY;
	} catch (const std::exception& ex) {
		rtsl_graphics_spirv_free(out);
		return RTSL_COMPILE_FAILED;
	} catch (...) {
		rtsl_graphics_spirv_free(out);
		return RTSL_COMPILE_FAILED;
	}
}
