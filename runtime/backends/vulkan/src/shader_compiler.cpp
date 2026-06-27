#include "shader_compiler.h"
#include "context.h"
#include "error.h"

#include <rutile/backend_tools/rtslp_package.hpp>

#include <spirv/unified1/spirv.h>

#include <algorithm>
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
#include <utility>
#include <vector>

void rtvk_shader_reflection_clear(rtvk_shader_reflection* reflection) {
	if (!reflection)
		return;
	std::free(reflection->uniform_blocks);
	std::free(reflection->textures);
	std::free(reflection->resources);
	*reflection = {};
}

static VkShaderModule rtvk_shader_compile_failed(enum rt_error error, const char* detail) {
	rtvk_throwf(error, detail);
	return VK_NULL_HANDLE;
}

static bool rtvk_spirv_create_shader_module(struct rtvk_context* ctx, std::span<u32> spirv, VkShaderModule& shader_out) {
	VkShaderModuleCreateInfo create_info = {VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
	create_info.codeSize = spirv.size() * sizeof(u32);
	create_info.pCode = spirv.data();
	const VkResult result = vkCreateShaderModule(ctx->vk_device, &create_info, VK_ALLOCATOR, &shader_out);
	if (result != VK_SUCCESS) {
		rtvk_throwf(rtvk_error_from_vk(result), NULL);
		return false;
	}
	return true;
}

typedef struct rtvk_spv_writer {
	std::vector<u32> words;
	u32 bound;
} rtvk_spv_writer;

static void rtvk_spv_begin(rtvk_spv_writer* writer) {
	writer->words = {
		SpvMagicNumber,
		SpvVersion,
		0,
		0,
		0,
	};
	writer->bound = 1;
}

static void rtvk_spv_reserve(rtvk_spv_writer* writer, u32 id) {
	writer->bound = std::max(writer->bound, id + 1);
}

static u32 rtvk_spv_fresh(rtvk_spv_writer* writer) {
	return writer->bound++;
}

static void rtvk_spv_emit(rtvk_spv_writer* writer, SpvOp op, const std::vector<u32>& operands) {
	writer->words.push_back(((1u + static_cast<u32>(operands.size())) << SpvWordCountShift) | static_cast<u32>(op));
	writer->words.insert(writer->words.end(), operands.begin(), operands.end());
}

static void rtvk_spv_emit(rtvk_spv_writer* writer, SpvOp op, std::initializer_list<u32> operands) {
	rtvk_spv_emit(writer, op, std::vector<u32>(operands));
}

static const rutile::backend_tools::RTInstruction* rtvk_rtslp_find_inst(
	const std::vector<rutile::backend_tools::RTInstruction>& insts,
	u32 result_id
) {
	for (const auto& inst : insts) {
		if (inst.result_id == result_id)
			return &inst;
	}
	return NULL;
}

static const rutile::backend_tools::RTFunction* rtvk_rtslp_find_stage_function(
	const rutile::backend_tools::RTArtifactModule& module,
	rutile::backend_tools::RTStageKind stage
) {
	for (const auto& fn : module.functions) {
		if (fn.stage == stage)
			return &fn;
	}
	return NULL;
}

static std::unordered_map<u32, std::string> rtvk_rtslp_struct_names(const rutile::backend_tools::RTArtifactModule& module) {
	std::unordered_map<u32, std::string> names;
	size_t struct_index = 0;
	for (const auto& inst : module.type_constant_pool) {
		if (inst.op == rutile::backend_tools::RTIROp::TypeStruct && struct_index < module.structs.size()) {
			names.emplace(inst.result_id, module.structs[struct_index].name);
			++struct_index;
		}
	}
	return names;
}

static const rutile::backend_tools::RTStageInterface* rtvk_rtslp_find_interface(
	const rutile::backend_tools::RTArtifactModule& module,
	std::string_view type_name,
	rutile::backend_tools::RTStageRole role
) {
	for (const auto& interface : module.stage_interfaces) {
		if (interface.type_name == type_name && interface.role == role)
			return &interface;
	}
	return NULL;
}

static SpvBuiltIn rtvk_rtslp_builtin(const rutile::backend_tools::RTStageField& field) {
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

static SpvOp rtvk_rtslp_op(rutile::backend_tools::RTIROp op) {
	using rutile::backend_tools::RTIROp;
	switch (op) {
	case RTIROp::TypeVoid:
		return SpvOpTypeVoid;
	case RTIROp::TypeBool:
		return SpvOpTypeBool;
	case RTIROp::TypeInt:
	case RTIROp::TypeUInt:
		return SpvOpTypeInt;
	case RTIROp::TypeFloat:
		return SpvOpTypeFloat;
	case RTIROp::TypeVector:
		return SpvOpTypeVector;
	case RTIROp::TypeMatrix:
		return SpvOpTypeMatrix;
	case RTIROp::TypeStruct:
		return SpvOpTypeStruct;
	case RTIROp::TypePointer:
		return SpvOpTypePointer;
	case RTIROp::TypeFunction:
		return SpvOpTypeFunction;
	case RTIROp::ConstantInt:
	case RTIROp::ConstantUInt:
	case RTIROp::ConstantFloat:
		return SpvOpConstant;
	case RTIROp::ConstantComposite:
		return SpvOpConstantComposite;
	case RTIROp::Load:
		return SpvOpLoad;
	case RTIROp::Store:
		return SpvOpStore;
	case RTIROp::CompositeConstruct:
		return SpvOpCompositeConstruct;
	case RTIROp::CompositeExtract:
		return SpvOpCompositeExtract;
	case RTIROp::FMul:
		return SpvOpFMul;
	case RTIROp::MatrixTimesVector:
		return SpvOpMatrixTimesVector;
	case RTIROp::ImageSampleImplicitLod:
		return SpvOpImageSampleImplicitLod;
	default:
		return SpvOpNop;
	}
}

static bool rtvk_rtslp_emit_type_or_constant(rtvk_spv_writer* writer, const rutile::backend_tools::RTInstruction& inst) {
	using rutile::backend_tools::RTIROp;
	rtvk_spv_reserve(writer, inst.result_id);
	std::vector<u32> operands;
	switch (inst.op) {
	case RTIROp::TypeVoid:
	case RTIROp::TypeBool:
		operands = {inst.result_id};
		break;
	case RTIROp::TypeInt:
		operands = {inst.result_id, inst.literals[0], 1};
		break;
	case RTIROp::TypeUInt:
		operands = {inst.result_id, inst.literals[0], 0};
		break;
	case RTIROp::TypeFloat:
		operands = {inst.result_id, inst.literals[0]};
		break;
	case RTIROp::TypeVector:
	case RTIROp::TypeMatrix:
		operands = {inst.result_id, inst.operands[0], inst.literals[0]};
		break;
	case RTIROp::TypeStruct:
	case RTIROp::TypeFunction:
		operands = {inst.result_id};
		operands.insert(operands.end(), inst.operands.begin(), inst.operands.end());
		break;
	case RTIROp::TypePointer:
		operands = {inst.result_id, inst.literals[0], inst.operands[0]};
		break;
	case RTIROp::ConstantInt:
	case RTIROp::ConstantUInt:
	case RTIROp::ConstantFloat:
		operands = {inst.type_id, inst.result_id};
		operands.insert(operands.end(), inst.literals.begin(), inst.literals.end());
		break;
	case RTIROp::ConstantComposite:
		operands = {inst.type_id, inst.result_id};
		operands.insert(operands.end(), inst.operands.begin(), inst.operands.end());
		break;
	default:
		return true;
	}
	rtvk_spv_emit(writer, rtvk_rtslp_op(inst.op), operands);
	return true;
}

static bool rtvk_rtslp_emit_type_or_constant_section(std::vector<u32>* section, const rutile::backend_tools::RTInstruction& inst) {
	using rutile::backend_tools::RTIROp;
	std::vector<u32> operands;
	switch (inst.op) {
	case RTIROp::TypeVoid:
	case RTIROp::TypeBool:
		operands = {inst.result_id};
		break;
	case RTIROp::TypeInt:
		operands = {inst.result_id, inst.literals[0], 1};
		break;
	case RTIROp::TypeUInt:
		operands = {inst.result_id, inst.literals[0], 0};
		break;
	case RTIROp::TypeFloat:
		operands = {inst.result_id, inst.literals[0]};
		break;
	case RTIROp::TypeVector:
	case RTIROp::TypeMatrix:
		operands = {inst.result_id, inst.operands[0], inst.literals[0]};
		break;
	case RTIROp::TypeStruct:
	case RTIROp::TypeFunction:
		operands = {inst.result_id};
		operands.insert(operands.end(), inst.operands.begin(), inst.operands.end());
		break;
	case RTIROp::TypePointer:
		operands = {inst.result_id, inst.literals[0], inst.operands[0]};
		break;
	case RTIROp::ConstantInt:
	case RTIROp::ConstantUInt:
	case RTIROp::ConstantFloat:
		operands = {inst.type_id, inst.result_id};
		operands.insert(operands.end(), inst.literals.begin(), inst.literals.end());
		break;
	case RTIROp::ConstantComposite:
		operands = {inst.type_id, inst.result_id};
		operands.insert(operands.end(), inst.operands.begin(), inst.operands.end());
		break;
	default:
		return true;
	}
	section->push_back(((1u + static_cast<u32>(operands.size())) << SpvWordCountShift) | static_cast<u32>(rtvk_rtslp_op(inst.op)));
	section->insert(section->end(), operands.begin(), operands.end());
	return true;
}

static std::vector<u32> rtvk_rtslp_remap_operands(
	const std::vector<u32>& operands,
	const std::unordered_map<u32, u32>& remap
) {
	std::vector<u32> out;
	out.reserve(operands.size());
	for (u32 operand : operands) {
		auto it = remap.find(operand);
		out.push_back(it != remap.end() ? it->second : operand);
	}
	return out;
}

static bool rtvk_rtslp_is_type_op(rutile::backend_tools::RTIROp op) {
	using rutile::backend_tools::RTIROp;
	return op >= RTIROp::TypeVoid && op <= RTIROp::TypeSampledImage;
}

static std::string rtvk_rtslp_type_key(const rutile::backend_tools::RTInstruction& inst) {
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

static u32 rtvk_rtslp_canonical_id(u32 id, const std::unordered_map<u32, u32>& aliases) {
	auto it = aliases.find(id);
	while (it != aliases.end() && it->second != id) {
		id = it->second;
		it = aliases.find(id);
	}
	return id;
}

static std::unordered_set<u32> rtvk_rtslp_collect_used_value_ids(const rutile::backend_tools::RTFunction& fn) {
	std::unordered_set<u32> used;
	for (const auto& inst : fn.body) {
		for (u32 operand : inst.operands) {
			used.insert(operand);
		}
	}
	return used;
}

static bool rtvk_rtslp_emit_stage_spirv(
	const rutile::backend_tools::RTArtifactModule& module,
	rutile::backend_tools::RTStageKind stage,
	std::vector<u32>* spirv_out
) {
	const auto* fn = rtvk_rtslp_find_stage_function(module, stage);
	if (!fn)
		return false;

	const auto struct_names = rtvk_rtslp_struct_names(module);
	const u32 input_type = [&]() -> u32 {
		if (fn->parameter_ids.size() < 2)
			return 0;
		for (const auto& inst : fn->body) {
			if (inst.op == rutile::backend_tools::RTIROp::FunctionParameter && inst.result_id == fn->parameter_ids[1]) {
				return inst.type_id;
			}
		}
		return 0;
	}();
	const u32 output_type = fn->return_type_id;
	const auto* input_inst = input_type ? rtvk_rtslp_find_inst(module.type_constant_pool, input_type) : NULL;
	const auto* output_inst = output_type ? rtvk_rtslp_find_inst(module.type_constant_pool, output_type) : NULL;
	const std::string input_name = (input_type && struct_names.contains(input_type)) ? struct_names.at(input_type) : std::string{};
	const std::string output_name = (output_type && struct_names.contains(output_type)) ? struct_names.at(output_type) : std::string{};
	const auto* input_interface =
		input_name.empty() ? NULL : rtvk_rtslp_find_interface(module, input_name, stage == rutile::backend_tools::RTStageKind::Vertex ? rutile::backend_tools::RTStageRole::Input : rutile::backend_tools::RTStageRole::Varying);
	const auto* output_interface =
		output_name.empty() ? NULL : rtvk_rtslp_find_interface(module, output_name, stage == rutile::backend_tools::RTStageKind::Fragment ? rutile::backend_tools::RTStageRole::Output : rutile::backend_tools::RTStageRole::Varying);
	const auto used_value_ids = rtvk_rtslp_collect_used_value_ids(*fn);

	rtvk_spv_writer writer;
	rtvk_spv_begin(&writer);
	std::vector<u32> entry_ops;
	std::vector<u32> execution_ops;
	std::vector<u32> annotation_ops;
	std::vector<u32> declaration_ops;
	std::vector<u32> function_ops;
	std::unordered_map<u32, u32> type_aliases;

	auto emit_section = [](std::vector<u32>* section, SpvOp op, const std::vector<u32>& operands) {
		section->push_back(((1u + static_cast<u32>(operands.size())) << SpvWordCountShift) | static_cast<u32>(op));
		section->insert(section->end(), operands.begin(), operands.end());
	};
	auto emit_section_list = [&](std::vector<u32>* section, SpvOp op, std::initializer_list<u32> operands) {
		emit_section(section, op, std::vector<u32>(operands));
	};

	rtvk_spv_emit(&writer, SpvOpCapability, {SpvCapabilityShader});
	rtvk_spv_emit(&writer, SpvOpMemoryModel, {SpvAddressingModelLogical, SpvMemoryModelGLSL450});

	for (const auto& inst : module.type_constant_pool) {
		if (inst.result_id)
			rtvk_spv_reserve(&writer, inst.result_id);
	}
	for (const auto& function : module.functions) {
		if (function.result_id)
			rtvk_spv_reserve(&writer, function.result_id);
		for (const auto& inst : function.body) {
			if (inst.result_id)
				rtvk_spv_reserve(&writer, inst.result_id);
		}
	}

	{
		std::unordered_map<std::string, u32> canonical_types;
		for (const auto& inst : module.type_constant_pool) {
			if (!rtvk_rtslp_is_type_op(inst.op) || !inst.result_id)
				continue;
			auto remapped_operands = rtvk_rtslp_remap_operands(inst.operands, type_aliases);
			rutile::backend_tools::RTInstruction remapped = inst;
			remapped.operands = std::move(remapped_operands);
			const std::string key = rtvk_rtslp_type_key(remapped);
			auto [it, inserted] = canonical_types.emplace(key, inst.result_id);
			if (!inserted) {
				type_aliases.emplace(inst.result_id, rtvk_rtslp_canonical_id(it->second, type_aliases));
			}
		}
	}

	u32 void_type = 0;
	for (const auto& inst : module.type_constant_pool) {
		if (inst.op == rutile::backend_tools::RTIROp::TypeVoid) {
			void_type = rtvk_rtslp_canonical_id(inst.result_id, type_aliases);
			break;
		}
	}
	if (!void_type)
		return false;
	const u32 entry_fn_type = rtvk_spv_fresh(&writer);
	std::unordered_set<u32> emitted_decl_ids;
	for (const auto& inst : module.type_constant_pool) {
		if (inst.result_id && rtvk_rtslp_canonical_id(inst.result_id, type_aliases) != inst.result_id)
			continue;
		if (inst.result_id && !emitted_decl_ids.insert(inst.result_id).second)
			continue;
		rutile::backend_tools::RTInstruction remapped = inst;
		remapped.type_id = rtvk_rtslp_canonical_id(remapped.type_id, type_aliases);
		remapped.operands = rtvk_rtslp_remap_operands(remapped.operands, type_aliases);
		if (!rtvk_rtslp_emit_type_or_constant_section(&declaration_ops, remapped))
			return false;
	}
	emit_section_list(&declaration_ops, SpvOpTypeFunction, {entry_fn_type, void_type});

	std::vector<u32> entry_interfaces;
	std::vector<u32> input_vars;
	std::vector<u32> output_vars;

	auto emit_interface_vars =
		[&](const rutile::backend_tools::RTInstruction* struct_inst,
			const rutile::backend_tools::RTStageInterface* interface,
			SpvStorageClass storage,
			std::vector<u32>* vars) {
			if (!struct_inst || !interface)
				return;
			for (size_t i = 0; i < interface->fields.size() && i < struct_inst->operands.size(); ++i) {
				const auto& field = interface->fields[i];
				// BuiltIn Position is only valid as an output of vertex-class
				// stages; a fragment shader cannot have it as an input. Skip
				// declaring the variable here and let the struct reconstruction
				// substitute OpUndef for this slot (the value carries no useful
				// data into the fragment stage anyway).
				const bool is_clip_position = field.builtin == "position" || field.interpolation == "clip";
				if (storage == SpvStorageClassInput && is_clip_position) {
					vars->push_back(0);
					continue;
				}
				const u32 ptr_type = rtvk_spv_fresh(&writer);
				emit_section_list(&declaration_ops, SpvOpTypePointer, {ptr_type, static_cast<u32>(storage), rtvk_rtslp_canonical_id(struct_inst->operands[i], type_aliases)});
				const u32 var_id = rtvk_spv_fresh(&writer);
				emit_section_list(&declaration_ops, SpvOpVariable, {ptr_type, var_id, static_cast<u32>(storage)});
				if (field.has_location) {
					emit_section_list(&annotation_ops, SpvOpDecorate, {var_id, SpvDecorationLocation, field.location});
				} else if (!field.builtin.empty() || is_clip_position) {
					emit_section_list(&annotation_ops, SpvOpDecorate, {var_id, SpvDecorationBuiltIn, static_cast<u32>(rtvk_rtslp_builtin(field))});
				}
				entry_interfaces.push_back(var_id);
				vars->push_back(var_id);
			}
		};

	emit_interface_vars(input_inst, input_interface, SpvStorageClassInput, &input_vars);
	emit_interface_vars(output_inst, output_interface, SpvStorageClassOutput, &output_vars);

	const u32 entry_id = rtvk_spv_fresh(&writer);
	std::vector<u32> entry_words = {
		static_cast<u32>(stage == rutile::backend_tools::RTStageKind::Vertex ? SpvExecutionModelVertex : SpvExecutionModelFragment),
		entry_id,
		0x6e69616d,
		0
	};
	entry_words.insert(entry_words.end(), entry_interfaces.begin(), entry_interfaces.end());
	emit_section(&entry_ops, SpvOpEntryPoint, entry_words);
	if (stage == rutile::backend_tools::RTStageKind::Fragment) {
		emit_section_list(&execution_ops, SpvOpExecutionMode, {entry_id, SpvExecutionModeOriginUpperLeft});
	}

	emit_section_list(&function_ops, SpvOpFunction, {void_type, entry_id, 0, entry_fn_type});
	const u32 label_id = rtvk_spv_fresh(&writer);
	emit_section_list(&function_ops, SpvOpLabel, {label_id});

	std::unordered_map<u32, u32> remap;
	if (input_inst && fn->parameter_ids.size() >= 2) {
		std::vector<u32> field_values;
		const bool parameter_used = used_value_ids.contains(fn->parameter_ids[1]);
		if (parameter_used) {
			for (size_t i = 0; i < input_vars.size() && i < input_inst->operands.size(); ++i) {
				const u32 field_type = rtvk_rtslp_canonical_id(input_inst->operands[i], type_aliases);
				const u32 value_id = rtvk_spv_fresh(&writer);
				// input_vars[i] == 0 marks a field whose Input variable was
				// skipped (e.g. fragment-input clip position). Substitute
				// OpUndef so the reconstructed aggregate still type-checks.
				if (input_vars[i] == 0) {
					emit_section_list(&function_ops, SpvOpUndef, {field_type, value_id});
				} else {
					emit_section_list(&function_ops, SpvOpLoad, {field_type, value_id, input_vars[i]});
				}
				field_values.push_back(value_id);
			}
			const u32 param_value = rtvk_spv_fresh(&writer);
			std::vector<u32> construct = {rtvk_rtslp_canonical_id(input_type, type_aliases), param_value};
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
				const u32 load_id = rtvk_spv_fresh(&writer);
				emit_section_list(&function_ops, SpvOpLoad, {rtvk_rtslp_canonical_id(input_inst->operands[i], type_aliases), load_id, input_vars[i]});
				remap.emplace(fn->parameter_ids[i + 1], load_id);
			}
		}
	}

	for (const auto& inst : fn->body) {
		if (inst.op == rutile::backend_tools::RTIROp::Label || inst.op == rutile::backend_tools::RTIROp::FunctionParameter)
			continue;
		if (inst.op >= rutile::backend_tools::RTIROp::TypeVoid && inst.op <= rutile::backend_tools::RTIROp::ConstantComposite)
			continue;
		if (inst.op == rutile::backend_tools::RTIROp::Return) {
			emit_section(&function_ops, SpvOpReturn, {});
			continue;
		}
		if (inst.op == rutile::backend_tools::RTIROp::ReturnValue) {
			const u32 value_id = remap.contains(inst.operands[0]) ? remap.at(inst.operands[0]) : inst.operands[0];
			if (output_inst && output_interface) {
				for (size_t i = 0; i < output_vars.size() && i < output_inst->operands.size(); ++i) {
					const u32 field_id = rtvk_spv_fresh(&writer);
					emit_section_list(&function_ops, SpvOpCompositeExtract, {rtvk_rtslp_canonical_id(output_inst->operands[i], type_aliases), field_id, value_id, static_cast<u32>(i)});
					emit_section_list(&function_ops, SpvOpStore, {output_vars[i], field_id});
				}
			}
			emit_section(&function_ops, SpvOpReturn, {});
			continue;
		}

		rtvk_spv_reserve(&writer, inst.result_id);
		std::vector<u32> words;
		if (inst.result_id && inst.type_id) {
			words.push_back(rtvk_rtslp_canonical_id(inst.type_id, type_aliases));
			words.push_back(inst.result_id);
		}
		auto operands = rtvk_rtslp_remap_operands(inst.operands, remap);
		for (u32& operand : operands) {
			operand = rtvk_rtslp_canonical_id(operand, type_aliases);
		}
		words.insert(words.end(), operands.begin(), operands.end());
		words.insert(words.end(), inst.literals.begin(), inst.literals.end());
		const SpvOp op = rtvk_rtslp_op(inst.op);
		if (op == SpvOpNop)
			return false;
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

static bool rtvk_fill_reflection_from_module(
	const rutile::backend_tools::RTArtifactModule& module,
	rtvk_shader_reflection* reflection
) {
	u32 uniform_block_count = 0;
	u32 texture_count = 0;
	for (const auto& uniform : module.uniforms) {
		if (uniform.type == "Sampler2D")
			++texture_count;
		else
			++uniform_block_count;
	}

	if (uniform_block_count) {
		reflection->uniform_blocks = static_cast<rtvk_shader_uniform_block*>(std::calloc(uniform_block_count, sizeof(rtvk_shader_uniform_block)));
		if (!reflection->uniform_blocks)
			return false;
	}
	if (texture_count) {
		reflection->textures = static_cast<rtvk_shader_texture*>(std::calloc(texture_count, sizeof(rtvk_shader_texture)));
		if (!reflection->textures)
			return false;
	}

	reflection->uniform_block_count = uniform_block_count;
	reflection->texture_count = texture_count;
	reflection->resource_count = 0;

	u32 uniform_index = 0;
	u32 texture_index = 0;
	for (const auto& uniform : module.uniforms) {
		if (uniform.type == "Sampler2D") {
			auto& dst = reflection->textures[texture_index++];
			std::snprintf(dst.name, sizeof(dst.name), "%s", uniform.name.c_str());
			dst.binding = uniform.binding;
		} else {
			auto& dst = reflection->uniform_blocks[uniform_index++];
			std::snprintf(dst.name, sizeof(dst.name), "%s", uniform.name.c_str());
			dst.binding = uniform.binding;
		}
	}
	return true;
}

rtvk_graphics_shader_compile_result rtvk_shader_compile_graphics(
	struct rtvk_context* ctx,
	const rt_vertex_layout* vertex_layout,
	u64 vertex_size,
	const void* vertex_source,
	u64 fragment_size,
	const void* fragment_source
) {
	(void)ctx;
	(void)vertex_layout;
	(void)vertex_size;
	(void)vertex_source;
	(void)fragment_size;
	(void)fragment_source;
	rtvk_shader_compile_failed(RT_SHADER_COMPILATION_FAILED, "GLSL graphics compilation is not available in this Vulkan build");
	return {};
}

rtvk_graphics_shader_compile_result rtvk_shader_compile_graphics_rtslp(struct rtvk_context* ctx, u64 program_size, const void* program_source) {
	rtvk_graphics_shader_compile_result result = {};
	try {
		rutile::backend_tools::RTArtifactModule module = rutile::backend_tools::read_rtslp_module(program_size, program_source);
		std::vector<u32> vertex_spirv;
		std::vector<u32> fragment_spirv;
		if (!rtvk_rtslp_emit_stage_spirv(module, rutile::backend_tools::RTStageKind::Vertex, &vertex_spirv) ||
			!rtvk_rtslp_emit_stage_spirv(module, rutile::backend_tools::RTStageKind::Fragment, &fragment_spirv)) {
			rtvk_throwf(RT_SHADER_COMPILATION_FAILED, "failed to lower RTSLP to SPIR-V");
			return {};
		}

		if (!rtvk_fill_reflection_from_module(module, &result.vertex_reflection)) {
			rtvk_shader_reflection_clear(&result.vertex_reflection);
			rtvk_throwf(RT_OUT_OF_HOST_MEMORY, "failed to allocate RTSLP reflection");
			return {};
		}

		if (!rtvk_spirv_create_shader_module(ctx, vertex_spirv, result.vertex_shader)) {
			rtvk_shader_reflection_clear(&result.vertex_reflection);
			return {};
		}
		if (!rtvk_spirv_create_shader_module(ctx, fragment_spirv, result.fragment_shader)) {
			vkDestroyShaderModule(ctx->vk_device, result.vertex_shader, VK_ALLOCATOR);
			result.vertex_shader = VK_NULL_HANDLE;
			rtvk_shader_reflection_clear(&result.vertex_reflection);
			return {};
		}
		return result;
	} catch (const std::bad_alloc&) {
		rtvk_shader_compile_failed(RT_OUT_OF_HOST_MEMORY, "out of host memory");
		return {};
	} catch (const std::exception& error) {
		rtvk_shader_compile_failed(RT_SHADER_COMPILATION_FAILED, error.what());
		return {};
	} catch (...) {
		rtvk_shader_compile_failed(RT_SHADER_COMPILATION_FAILED, NULL);
		return {};
	}
}
