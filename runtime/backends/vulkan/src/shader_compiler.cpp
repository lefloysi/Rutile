#include "shader_compiler.h"
#include "context.h"
#include "error.h"

#include "../../../backend-tools/src/rtslp_package.cpp"
#include <rtslp_package.hpp>

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
	VkShaderModuleCreateInfo create_info = { VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO };
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

static void rtvk_spv_emit(rtvk_spv_writer* writer, SpvOp op, std::span<u32> operands) {
	writer->words.push_back(((1u + static_cast<u32>(operands.size())) << SpvWordCountShift) | static_cast<u32>(op));
	writer->words.insert(writer->words.end(), operands.begin(), operands.end());
}

static void rtvk_spv_emit(rtvk_spv_writer* writer, SpvOp op, std::initializer_list<u32> operands) {
	std::vector<u32> temp(operands.begin(), operands.end());
	rtvk_spv_emit(writer, op, std::span<u32>(temp.data(), temp.size()));
}

static const rt::RTInstruction* rtvk_rtslp_find_inst(std::span<const rt::RTInstruction> insts, u32 result_id) {
	for (const auto& inst : insts) {
		if (inst.result_id == result_id)
			return &inst;
	}
	return NULL;
}

static u32 rtvk_rtslp_storage_class(u32 storage) {
	switch (storage) {
	case 0:
		return SpvStorageClassFunction;
	case 1:
		return SpvStorageClassInput;
	case 2:
		return SpvStorageClassOutput;
	case 3:
		return SpvStorageClassUniform;
	case 4:
		return SpvStorageClassUniformConstant;
	case 5:
		return SpvStorageClassStorageBuffer;
	case 6:
		return SpvStorageClassPushConstant;
	case 7:
		return SpvStorageClassPrivate;
	default:
		return storage;
	}
}

static const rt::RTFunction* rtvk_rtslp_find_stage_function(const rt::RTArtifactModule& module, rt::RTStageKind stage) {
	for (const auto& fn : module.functions) {
		if (fn.stage == stage)
			return &fn;
	}
	return NULL;
}

static std::unordered_map<u32, std::string> rtvk_rtslp_struct_names(const rt::RTArtifactModule& module) {
	std::unordered_map<u32, std::string> names;
	size_t struct_index = 0;
	for (const auto& inst : module.type_constant_pool) {
		if (inst.op == rt::RTIROp::TypeStruct && struct_index < module.structs.size()) {
			names.emplace(inst.result_id, module.structs[struct_index].name);
			++struct_index;
		}
	}
	return names;
}

static const rt::RTStageInterface* rtvk_rtslp_find_interface(
	const rt::RTArtifactModule& module,
	std::string_view type_name,
	rt::RTStageRole role
) {
	for (const auto& interface : module.stage_interfaces) {
		if (interface.type_name == type_name && interface.role == role)
			return &interface;
	}
	return NULL;
}

static SpvBuiltIn rtvk_rtslp_builtin(const rt::RTStageField& field) {
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

static SpvOp rtvk_rtslp_op(rt::RTIROp op) {
	using rt::RTIROp;
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
	case RTIROp::TypeArray:
		return SpvOpTypeArray;
	case RTIROp::TypeImage:
		return SpvOpTypeImage;
	case RTIROp::TypeSampler:
		return SpvOpTypeSampler;
	case RTIROp::TypeSampledImage:
		return SpvOpTypeSampledImage;
	case RTIROp::TypeFunction:
		return SpvOpTypeFunction;
	case RTIROp::ConstantInt:
	case RTIROp::ConstantUInt:
	case RTIROp::ConstantFloat:
		return SpvOpConstant;
	case RTIROp::ConstantComposite:
		return SpvOpConstantComposite;
	case RTIROp::Variable:
		return SpvOpVariable;
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
	case RTIROp::SampledImage:
		return SpvOpSampledImage;
	case RTIROp::ImageSampleImplicitLod:
		return SpvOpImageSampleImplicitLod;
	default:
		return SpvOpNop;
	}
}

static const char* rtvk_rtslp_op_name(rt::RTIROp op) {
	switch (op) {
	case rt::RTIROp::Nop:
		return "Nop";
	case rt::RTIROp::TypeVoid:
		return "TypeVoid";
	case rt::RTIROp::TypeBool:
		return "TypeBool";
	case rt::RTIROp::TypeInt:
		return "TypeInt";
	case rt::RTIROp::TypeUInt:
		return "TypeUInt";
	case rt::RTIROp::TypeFloat:
		return "TypeFloat";
	case rt::RTIROp::TypeVector:
		return "TypeVector";
	case rt::RTIROp::TypeMatrix:
		return "TypeMatrix";
	case rt::RTIROp::TypeStruct:
		return "TypeStruct";
	case rt::RTIROp::TypePointer:
		return "TypePointer";
	case rt::RTIROp::TypeArray:
		return "TypeArray";
	case rt::RTIROp::TypeFunction:
		return "TypeFunction";
	case rt::RTIROp::TypeImage:
		return "TypeImage";
	case rt::RTIROp::TypeSampler:
		return "TypeSampler";
	case rt::RTIROp::TypeSampledImage:
		return "TypeSampledImage";
	case rt::RTIROp::ConstantBool:
		return "ConstantBool";
	case rt::RTIROp::ConstantInt:
		return "ConstantInt";
	case rt::RTIROp::ConstantUInt:
		return "ConstantUInt";
	case rt::RTIROp::ConstantFloat:
		return "ConstantFloat";
	case rt::RTIROp::ConstantComposite:
		return "ConstantComposite";
	case rt::RTIROp::Variable:
		return "Variable";
	case rt::RTIROp::Load:
		return "Load";
	case rt::RTIROp::Store:
		return "Store";
	case rt::RTIROp::AccessChain:
		return "AccessChain";
	case rt::RTIROp::CompositeConstruct:
		return "CompositeConstruct";
	case rt::RTIROp::CompositeExtract:
		return "CompositeExtract";
	case rt::RTIROp::CompositeInsert:
		return "CompositeInsert";
	case rt::RTIROp::VectorShuffle:
		return "VectorShuffle";
	case rt::RTIROp::FAdd:
		return "FAdd";
	case rt::RTIROp::FSub:
		return "FSub";
	case rt::RTIROp::FMul:
		return "FMul";
	case rt::RTIROp::FDiv:
		return "FDiv";
	case rt::RTIROp::FMod:
		return "FMod";
	case rt::RTIROp::FNegate:
		return "FNegate";
	case rt::RTIROp::IAdd:
		return "IAdd";
	case rt::RTIROp::ISub:
		return "ISub";
	case rt::RTIROp::IMul:
		return "IMul";
	case rt::RTIROp::SDiv:
		return "SDiv";
	case rt::RTIROp::UDiv:
		return "UDiv";
	case rt::RTIROp::SMod:
		return "SMod";
	case rt::RTIROp::UMod:
		return "UMod";
	case rt::RTIROp::VectorTimesScalar:
		return "VectorTimesScalar";
	case rt::RTIROp::MatrixTimesScalar:
		return "MatrixTimesScalar";
	case rt::RTIROp::MatrixTimesVector:
		return "MatrixTimesVector";
	case rt::RTIROp::MatrixTimesMatrix:
		return "MatrixTimesMatrix";
	case rt::RTIROp::Dot:
		return "Dot";
	case rt::RTIROp::Cross:
		return "Cross";
	case rt::RTIROp::FOrdEqual:
		return "FOrdEqual";
	case rt::RTIROp::FOrdNotEqual:
		return "FOrdNotEqual";
	case rt::RTIROp::FOrdLess:
		return "FOrdLess";
	case rt::RTIROp::FOrdLessEqual:
		return "FOrdLessEqual";
	case rt::RTIROp::FOrdGreater:
		return "FOrdGreater";
	case rt::RTIROp::FOrdGreaterEqual:
		return "FOrdGreaterEqual";
	case rt::RTIROp::IEqual:
		return "IEqual";
	case rt::RTIROp::INotEqual:
		return "INotEqual";
	case rt::RTIROp::SLess:
		return "SLess";
	case rt::RTIROp::SLessEqual:
		return "SLessEqual";
	case rt::RTIROp::SGreater:
		return "SGreater";
	case rt::RTIROp::SGreaterEqual:
		return "SGreaterEqual";
	case rt::RTIROp::ULess:
		return "ULess";
	case rt::RTIROp::ULessEqual:
		return "ULessEqual";
	case rt::RTIROp::UGreater:
		return "UGreater";
	case rt::RTIROp::UGreaterEqual:
		return "UGreaterEqual";
	case rt::RTIROp::LogicalAnd:
		return "LogicalAnd";
	case rt::RTIROp::LogicalOr:
		return "LogicalOr";
	case rt::RTIROp::LogicalNot:
		return "LogicalNot";
	case rt::RTIROp::ConvertFToU:
		return "ConvertFToU";
	case rt::RTIROp::ConvertFToS:
		return "ConvertFToS";
	case rt::RTIROp::ConvertSToF:
		return "ConvertSToF";
	case rt::RTIROp::ConvertUToF:
		return "ConvertUToF";
	case rt::RTIROp::Bitcast:
		return "Bitcast";
	case rt::RTIROp::Label:
		return "Label";
	case rt::RTIROp::Branch:
		return "Branch";
	case rt::RTIROp::BranchConditional:
		return "BranchConditional";
	case rt::RTIROp::SelectionMerge:
		return "SelectionMerge";
	case rt::RTIROp::LoopMerge:
		return "LoopMerge";
	case rt::RTIROp::Return:
		return "Return";
	case rt::RTIROp::ReturnValue:
		return "ReturnValue";
	case rt::RTIROp::FunctionParameter:
		return "FunctionParameter";
	case rt::RTIROp::FunctionCall:
		return "FunctionCall";
	case rt::RTIROp::SampledImage:
		return "SampledImage";
	case rt::RTIROp::ImageSampleImplicitLod:
		return "ImageSampleImplicitLod";
	case rt::RTIROp::ImageSampleExplicitLod:
		return "ImageSampleExplicitLod";
	case rt::RTIROp::ImageRead:
		return "ImageRead";
	case rt::RTIROp::ImageWrite:
		return "ImageWrite";
	case rt::RTIROp::ReadInput:
		return "ReadInput";
	case rt::RTIROp::WriteOutput:
		return "WriteOutput";
	case rt::RTIROp::ReadBuiltin:
		return "ReadBuiltin";
	case rt::RTIROp::WriteBuiltin:
		return "WriteBuiltin";
	case rt::RTIROp::ExtInst:
		return "ExtInst";
	default:
		return "Unknown";
	}
}

static bool rtvk_rtslp_emit_type_or_constant(rtvk_spv_writer* writer, const rt::RTInstruction& inst) {
	using rt::RTIROp;
	rtvk_spv_reserve(writer, inst.result_id);
	std::vector<u32> operands;
	switch (inst.op) {
	case RTIROp::TypeVoid:
	case RTIROp::TypeBool:
		operands = { inst.result_id };
		break;
	case RTIROp::TypeInt:
		if (inst.literals.empty())
			return false;
		operands = { inst.result_id, inst.literals[0], 1 };
		break;
	case RTIROp::TypeUInt:
		if (inst.literals.empty())
			return false;
		operands = { inst.result_id, inst.literals[0], 0 };
		break;
	case RTIROp::TypeFloat:
		if (inst.literals.empty())
			return false;
		operands = { inst.result_id, inst.literals[0] };
		break;
	case RTIROp::TypeVector:
	case RTIROp::TypeMatrix:
		if (inst.operands.empty() || inst.literals.empty())
			return false;
		operands = { inst.result_id, inst.operands[0], inst.literals[0] };
		break;
	case RTIROp::TypeStruct:
	case RTIROp::TypeFunction:
	case RTIROp::TypeArray:
	case RTIROp::TypeImage:
	case RTIROp::TypeSampler:
	case RTIROp::TypeSampledImage:
		operands = { inst.result_id };
		operands.insert(operands.end(), inst.operands.begin(), inst.operands.end());
		operands.insert(operands.end(), inst.literals.begin(), inst.literals.end());
		break;
	case RTIROp::TypePointer:
		if (inst.literals.empty() || inst.operands.empty())
			return false;
		operands = { inst.result_id, rtvk_rtslp_storage_class(inst.literals[0]), inst.operands[0] };
		break;
	case RTIROp::ConstantInt:
	case RTIROp::ConstantUInt:
	case RTIROp::ConstantFloat:
		operands = { inst.type_id, inst.result_id };
		operands.insert(operands.end(), inst.literals.begin(), inst.literals.end());
		break;
	case RTIROp::ConstantComposite:
		operands = { inst.type_id, inst.result_id };
		operands.insert(operands.end(), inst.operands.begin(), inst.operands.end());
		break;
	default:
		return false;
	}
	rtvk_spv_emit(writer, rtvk_rtslp_op(inst.op), operands);
	return true;
}

static bool rtvk_rtslp_emit_type_or_constant_section(std::vector<u32>* section, const rt::RTInstruction& inst) {
	using rt::RTIROp;
	std::vector<u32> operands;
	switch (inst.op) {
	case RTIROp::TypeVoid:
	case RTIROp::TypeBool:
		operands = { inst.result_id };
		break;
	case RTIROp::TypeInt:
		if (inst.literals.empty())
			return false;
		operands = { inst.result_id, inst.literals[0], 1 };
		break;
	case RTIROp::TypeUInt:
		if (inst.literals.empty())
			return false;
		operands = { inst.result_id, inst.literals[0], 0 };
		break;
	case RTIROp::TypeFloat:
		if (inst.literals.empty())
			return false;
		operands = { inst.result_id, inst.literals[0] };
		break;
	case RTIROp::TypeVector:
	case RTIROp::TypeMatrix:
		if (inst.operands.empty() || inst.literals.empty())
			return false;
		operands = { inst.result_id, inst.operands[0], inst.literals[0] };
		break;
	case RTIROp::TypeStruct:
	case RTIROp::TypeFunction:
	case RTIROp::TypeArray:
	case RTIROp::TypeImage:
	case RTIROp::TypeSampler:
	case RTIROp::TypeSampledImage:
		operands = { inst.result_id };
		operands.insert(operands.end(), inst.operands.begin(), inst.operands.end());
		operands.insert(operands.end(), inst.literals.begin(), inst.literals.end());
		break;
	case RTIROp::TypePointer:
		if (inst.literals.empty() || inst.operands.empty())
			return false;
		operands = { inst.result_id, rtvk_rtslp_storage_class(inst.literals[0]), inst.operands[0] };
		break;
	case RTIROp::ConstantInt:
	case RTIROp::ConstantUInt:
	case RTIROp::ConstantFloat:
		operands = { inst.type_id, inst.result_id };
		operands.insert(operands.end(), inst.literals.begin(), inst.literals.end());
		break;
	case RTIROp::ConstantComposite:
		operands = { inst.type_id, inst.result_id };
		operands.insert(operands.end(), inst.operands.begin(), inst.operands.end());
		break;
	default:
		return false;
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

static bool rtvk_rtslp_is_type_op(rt::RTIROp op) {
	using rt::RTIROp;
	return op >= RTIROp::TypeVoid && op <= RTIROp::TypeSampledImage;
}

static std::string rtvk_rtslp_type_key(const rt::RTInstruction& inst) {
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

static std::unordered_set<u32> rtvk_rtslp_collect_used_value_ids(const rt::RTFunction& fn) {
	std::unordered_set<u32> used;
	for (const auto& inst : fn.body) {
		for (u32 operand : inst.operands) {
			used.insert(operand);
		}
	}
	return used;
}

static bool rtvk_rtslp_has_type_id(const rt::RTArtifactModule& module, u32 type_id) {
	if (!type_id)
		return false;
	for (const auto& inst : module.type_constant_pool) {
		if (inst.result_id == type_id)
			return true;
	}
	return false;
}

static bool rtvk_rtslp_validate_type_refs(const rt::RTArtifactModule& module, const rt::RTFunction& fn, std::string* error_out) {
	for (const auto& inst : module.type_constant_pool) {
		for (u32 operand : inst.operands) {
			if (!rtvk_rtslp_has_type_id(module, operand)) {
				if (error_out) {
					*error_out = "dangling RTSLP type reference " + std::to_string(operand);
				}
				return false;
			}
		}
	}
	u32 input_type = 0;
	if (fn.parameter_ids.size() >= 2) {
		for (const auto& inst : fn.body) {
			if (inst.op == rt::RTIROp::FunctionParameter && inst.result_id == fn.parameter_ids[1]) {
				input_type = inst.type_id;
				break;
			}
		}
	}
	const u32 output_type = fn.return_type_id;
	if (input_type && !rtvk_rtslp_has_type_id(module, input_type)) {
		if (error_out) {
			*error_out = "dangling RTSLP input type reference " + std::to_string(input_type);
		}
		return false;
	}
	if (output_type && !rtvk_rtslp_has_type_id(module, output_type)) {
		if (error_out) {
			*error_out = "dangling RTSLP output type reference " + std::to_string(output_type);
		}
		return false;
	}
	return true;
}

static bool rtvk_rtslp_type_ready(const rt::RTArtifactModule& module, const rt::RTInstruction& inst, const std::unordered_set<u32>& emitted_ids) {
	auto has_emitted = [&](u32 id) {
		return !id || emitted_ids.contains(id);
	};
	switch (inst.op) {
	case rt::RTIROp::TypeVoid:
	case rt::RTIROp::TypeBool:
		return true;
	case rt::RTIROp::TypeInt:
	case rt::RTIROp::TypeUInt:
	case rt::RTIROp::TypeFloat:
		return !inst.literals.empty();
	case rt::RTIROp::TypeVector:
	case rt::RTIROp::TypeMatrix:
		return !inst.operands.empty() && !inst.literals.empty() && has_emitted(inst.operands[0]);
	case rt::RTIROp::TypeStruct:
		for (u32 operand : inst.operands) {
			if (!has_emitted(operand))
				return false;
		}
		return true;
	case rt::RTIROp::TypePointer:
		return !inst.literals.empty() && !inst.operands.empty() && has_emitted(inst.operands[0]);
	case rt::RTIROp::TypeArray:
		return !inst.operands.empty() && !inst.literals.empty() && has_emitted(inst.operands[0]);
	case rt::RTIROp::TypeFunction:
		for (u32 operand : inst.operands) {
			if (!has_emitted(operand))
				return false;
		}
		return true;
	case rt::RTIROp::TypeImage:
	case rt::RTIROp::TypeSampler:
	case rt::RTIROp::TypeSampledImage:
		for (u32 operand : inst.operands) {
			if (!has_emitted(operand))
				return false;
		}
		return true;
	default:
		return true;
	}
}

static bool rtvk_rtslp_emit_global_variable_section(std::vector<u32>* section, const rt::RTInstruction& inst) {
	if (inst.op != rt::RTIROp::Variable || !inst.type_id || !inst.result_id || inst.literals.empty()) {
		return false;
	}

	std::vector<u32> operands = { inst.type_id, inst.result_id, rtvk_rtslp_storage_class(inst.literals[0]) };
	operands.insert(operands.end(), inst.operands.begin(), inst.operands.end());
	section->push_back(((1u + static_cast<u32>(operands.size())) << SpvWordCountShift) | static_cast<u32>(SpvOpVariable));
	section->insert(section->end(), operands.begin(), operands.end());
	return true;
}

typedef struct rtvk_wrapped_uniform {
	u32 value_type;
	u32 struct_type;
	u32 pointer_type;
} rtvk_wrapped_uniform;

static rt::RTArtifactModule rtvk_rtslp_normalize_vulkan_types(const rt::RTArtifactModule& module) {
	rt::RTArtifactModule normalized = module;
	u32 float_type = 0;
	u32 image_type = 0;
	for (const auto& inst : normalized.type_constant_pool) {
		if (!float_type && inst.op == rt::RTIROp::TypeFloat) {
			float_type = inst.result_id;
		}
		if (!image_type && inst.op == rt::RTIROp::TypeImage) {
			image_type = inst.result_id;
		}
	}

	for (auto& inst : normalized.type_constant_pool) {
		if (inst.op == rt::RTIROp::TypeImage && inst.operands.empty() && inst.literals.empty() && float_type) {
			inst.operands = { float_type };
			inst.literals = { SpvDim2D, 0, 0, 0, 1, SpvImageFormatUnknown };
		}
		if (inst.op == rt::RTIROp::TypeSampledImage && inst.operands.empty() && image_type) {
			inst.operands = { image_type };
		}
	}
	return normalized;
}

static const char* rtvk_rtslp_stage_name(rt::RTStageKind stage) {
	switch (stage) {
	case rt::RTStageKind::Vertex:
		return "vertex";
	case rt::RTStageKind::Fragment:
		return "fragment";
	case rt::RTStageKind::Compute:
		return "compute";
	default:
		return "unknown";
	}
}

static bool rtvk_rtslp_emit_stage_spirv(const rt::RTArtifactModule& module, rt::RTStageKind stage, std::vector<u32>* spirv_out) {
	const rt::RTArtifactModule vulkan_module = rtvk_rtslp_normalize_vulkan_types(module);
	const auto* fn = rtvk_rtslp_find_stage_function(vulkan_module, stage);
	if (!fn) {
		rtvk_throwf(RT_SHADER_COMPILATION_FAILED, "RTSLP is missing %s stage", rtvk_rtslp_stage_name(stage));
		return false;
	}

	const auto struct_names = rtvk_rtslp_struct_names(vulkan_module);
	std::string type_error;
	if (!rtvk_rtslp_validate_type_refs(vulkan_module, *fn, &type_error)) {
		rtvk_throwf(RT_SHADER_COMPILATION_FAILED, type_error.c_str());
		return false;
	}
	const u32 input_type = [&]() -> u32 {
		if (fn->parameter_ids.size() < 2)
			return 0;
		for (const auto& inst : fn->body) {
			if (inst.op == rt::RTIROp::FunctionParameter && inst.result_id == fn->parameter_ids[1]) {
				return inst.type_id;
			}
		}
		return 0;
	}();
	const u32 output_type = fn->return_type_id;
	const auto* input_inst = input_type ? rtvk_rtslp_find_inst(vulkan_module.type_constant_pool, input_type) : NULL;
	const auto* output_inst = output_type ? rtvk_rtslp_find_inst(vulkan_module.type_constant_pool, output_type) : NULL;
	const std::string input_name = (input_type && struct_names.contains(input_type)) ? struct_names.at(input_type) : std::string{};
	const std::string output_name = (output_type && struct_names.contains(output_type)) ? struct_names.at(output_type) : std::string{};
	const auto* input_interface = input_name.empty() ? NULL : rtvk_rtslp_find_interface(vulkan_module, input_name, stage == rt::RTStageKind::Vertex ? rt::RTStageRole::Input : rt::RTStageRole::Varying);
	const auto* output_interface = output_name.empty() ? NULL : rtvk_rtslp_find_interface(vulkan_module, output_name, stage == rt::RTStageKind::Fragment ? rt::RTStageRole::Output : rt::RTStageRole::Varying);
	const auto used_value_ids = rtvk_rtslp_collect_used_value_ids(*fn);

	rtvk_spv_writer writer;
	rtvk_spv_begin(&writer);
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

	rtvk_spv_emit(&writer, SpvOpCapability, { SpvCapabilityShader });
	rtvk_spv_emit(&writer, SpvOpMemoryModel, { SpvAddressingModelLogical, SpvMemoryModelGLSL450 });

	for (const auto& inst : vulkan_module.type_constant_pool) {
		if (rtvk_rtslp_op(inst.op) == SpvOpNop) {
			rtvk_throwf(RT_SHADER_COMPILATION_FAILED, rtvk_rtslp_op_name(inst.op));
			return false;
		}
	}
	for (const auto& inst : vulkan_module.global_variables) {
		if (rtvk_rtslp_op(inst.op) == SpvOpNop) {
			rtvk_throwf(RT_SHADER_COMPILATION_FAILED, rtvk_rtslp_op_name(inst.op));
			return false;
		}
	}
	for (const auto& inst : fn->body) {
		if (inst.op == rt::RTIROp::Label || inst.op == rt::RTIROp::FunctionParameter)
			continue;
		if (inst.op == rt::RTIROp::Return || inst.op == rt::RTIROp::ReturnValue)
			continue;
		if (inst.op >= rt::RTIROp::TypeVoid && inst.op <= rt::RTIROp::ConstantComposite)
			continue;
		if (rtvk_rtslp_op(inst.op) == SpvOpNop) {
			rtvk_throwf(RT_SHADER_COMPILATION_FAILED, rtvk_rtslp_op_name(inst.op));
			return false;
		}
	}

	for (const auto& inst : module.type_constant_pool) {
		if (inst.result_id)
			rtvk_spv_reserve(&writer, inst.result_id);
	}
	for (const auto& inst : vulkan_module.global_variables) {
		if (inst.result_id)
			rtvk_spv_reserve(&writer, inst.result_id);
	}
	for (const auto& function : vulkan_module.functions) {
		if (function.result_id)
			rtvk_spv_reserve(&writer, function.result_id);
		for (const auto& inst : function.body) {
			if (inst.result_id)
				rtvk_spv_reserve(&writer, inst.result_id);
		}
	}

	std::unordered_map<u32, u32> canonical_type_ids;
	{
		std::unordered_map<std::string, u32> canonical_types;
		for (const auto& inst : vulkan_module.type_constant_pool) {
			if (!rtvk_rtslp_is_type_op(inst.op) || !inst.result_id)
				continue;
			const std::string key = rtvk_rtslp_type_key(inst);
			auto [it, inserted] = canonical_types.emplace(key, inst.result_id);
			canonical_type_ids.emplace(inst.result_id, it->second);
			(void)inserted;
		}
	}

	u32 void_type = 0;
	for (const auto& inst : vulkan_module.type_constant_pool) {
		if (inst.op == rt::RTIROp::TypeVoid) {
			void_type = inst.result_id;
			break;
		}
	}
	if (!void_type) {
		rtvk_throwf(RT_SHADER_COMPILATION_FAILED, "RTSLP %s stage is missing void type", rtvk_rtslp_stage_name(stage));
		return false;
	}
	const u32 entry_fn_type = rtvk_spv_fresh(&writer);
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
			if (!rtvk_rtslp_type_ready(vulkan_module, inst, emitted_decl_ids))
				continue;
			rt::RTInstruction remapped = inst;
			if (!rtvk_rtslp_emit_type_or_constant_section(&declaration_ops, remapped)) {
				rtvk_throwf(
					RT_SHADER_COMPILATION_FAILED,
					"RTSLP %s stage cannot emit type/constant %u (%s)",
					rtvk_rtslp_stage_name(stage),
					inst.result_id,
					rtvk_rtslp_op_name(inst.op)
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
			rtvk_throwf(
				RT_SHADER_COMPILATION_FAILED,
				"RTSLP %s stage cannot order type/constant %u (%s)",
				rtvk_rtslp_stage_name(stage),
				inst.result_id,
				rtvk_rtslp_op_name(inst.op)
			);
			return false;
		}
	}
	emit_section_list(&declaration_ops, SpvOpTypeFunction, { entry_fn_type, void_type });
	std::vector<u32> entry_interfaces;
	std::unordered_map<u32, rtvk_wrapped_uniform> wrapped_uniforms;
	for (size_t i = 0; i < vulkan_module.global_variables.size(); ++i) {
		const auto& inst = vulkan_module.global_variables[i];
		if (rtvk_rtslp_op(inst.op) == SpvOpNop) {
			rtvk_throwf(RT_SHADER_COMPILATION_FAILED, rtvk_rtslp_op_name(inst.op));
			return false;
		}
		rt::RTInstruction global_inst = inst;
		if (!inst.literals.empty() && rtvk_rtslp_storage_class(inst.literals[0]) == SpvStorageClassUniform) {
			const auto* pointer_type = rtvk_rtslp_find_inst(vulkan_module.type_constant_pool, inst.type_id);
			const auto* pointee_type = pointer_type && !pointer_type->operands.empty()
										   ? rtvk_rtslp_find_inst(vulkan_module.type_constant_pool, pointer_type->operands[0])
										   : NULL;
			if (pointer_type && pointee_type && pointee_type->op != rt::RTIROp::TypeStruct) {
				rtvk_wrapped_uniform wrapped = {};
				wrapped.value_type = pointer_type->operands[0];
				wrapped.struct_type = rtvk_spv_fresh(&writer);
				wrapped.pointer_type = rtvk_spv_fresh(&writer);
				emit_section_list(&declaration_ops, SpvOpTypeStruct, { wrapped.struct_type, wrapped.value_type });
				emit_section_list(&declaration_ops, SpvOpTypePointer, { wrapped.pointer_type, SpvStorageClassUniform, wrapped.struct_type });
				emit_section_list(&annotation_ops, SpvOpDecorate, { wrapped.struct_type, SpvDecorationBlock });
				emit_section_list(&annotation_ops, SpvOpMemberDecorate, { wrapped.struct_type, 0, SpvDecorationOffset, 0 });
				if (pointee_type->op == rt::RTIROp::TypeMatrix) {
					emit_section_list(&annotation_ops, SpvOpMemberDecorate, { wrapped.struct_type, 0, SpvDecorationColMajor });
					emit_section_list(&annotation_ops, SpvOpMemberDecorate, { wrapped.struct_type, 0, SpvDecorationMatrixStride, 16 });
				}
				wrapped_uniforms.emplace(inst.result_id, wrapped);
				global_inst.type_id = wrapped.pointer_type;
			}
		}
		if (!rtvk_rtslp_emit_global_variable_section(&declaration_ops, global_inst)) {
			rtvk_throwf(
				RT_SHADER_COMPILATION_FAILED,
				"RTSLP %s stage cannot emit global variable %u",
				rtvk_rtslp_stage_name(stage),
				inst.result_id
			);
			return false;
		}
		if (i < vulkan_module.uniforms.size()) {
			emit_section_list(&annotation_ops, SpvOpDecorate, { inst.result_id, SpvDecorationDescriptorSet, vulkan_module.uniforms[i].set });
			emit_section_list(&annotation_ops, SpvOpDecorate, { inst.result_id, SpvDecorationBinding, vulkan_module.uniforms[i].binding });
		}
		// SPIR-V 1.4+ requires every non-Function global referenced by the
		// entry point to be listed in OpEntryPoint's interface. spirv-val on
		// Vulkan 1.3 (SPIR-V 1.6) rejects modules where Uniform or
		// UniformConstant variables are missing from the interface list.
		if (!inst.literals.empty()) {
			const u32 storage_class = rtvk_rtslp_storage_class(inst.literals[0]);
			if (storage_class != SpvStorageClassFunction) {
				entry_interfaces.push_back(inst.result_id);
			}
		}
	}

	std::vector<u32> input_vars;
	std::vector<u32> output_vars;

	auto emit_interface_vars =
		[&](const rt::RTInstruction* struct_inst,
			const rt::RTStageInterface* interface,
			SpvStorageClass storage,
			std::vector<u32>* vars) {
			if (!struct_inst || !interface)
				return;
			for (size_t i = 0; i < interface->fields.size() && i < struct_inst->operands.size(); ++i) {
				const auto& field = interface->fields[i];
				if (!struct_inst->operands[i])
					return;
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
				emit_section_list(&declaration_ops, SpvOpTypePointer, { ptr_type, static_cast<u32>(storage), struct_inst->operands[i] });
				const u32 var_id = rtvk_spv_fresh(&writer);
				emit_section_list(&declaration_ops, SpvOpVariable, { ptr_type, var_id, static_cast<u32>(storage) });
				if (field.has_location) {
					emit_section_list(&annotation_ops, SpvOpDecorate, { var_id, SpvDecorationLocation, field.location });
				} else if (!field.builtin.empty() || is_clip_position) {
					emit_section_list(&annotation_ops, SpvOpDecorate, { var_id, SpvDecorationBuiltIn, static_cast<u32>(rtvk_rtslp_builtin(field)) });
				}
				entry_interfaces.push_back(var_id);
				vars->push_back(var_id);
			}
		};

	emit_interface_vars(input_inst, input_interface, SpvStorageClassInput, &input_vars);
	emit_interface_vars(output_inst, output_interface, SpvStorageClassOutput, &output_vars);

	const u32 entry_id = rtvk_spv_fresh(&writer);
	std::vector<u32> entry_words = {
		static_cast<u32>(stage == rt::RTStageKind::Vertex ? SpvExecutionModelVertex : SpvExecutionModelFragment),
		entry_id,
		0x6e69616d,
		0
	};
	entry_words.insert(entry_words.end(), entry_interfaces.begin(), entry_interfaces.end());
	emit_section(&entry_ops, SpvOpEntryPoint, entry_words);
	if (stage == rt::RTStageKind::Fragment) {
		emit_section_list(&execution_ops, SpvOpExecutionMode, { entry_id, SpvExecutionModeOriginUpperLeft });
	}

	emit_section_list(&function_ops, SpvOpFunction, { void_type, entry_id, 0, entry_fn_type });
	const u32 label_id = rtvk_spv_fresh(&writer);
	emit_section_list(&function_ops, SpvOpLabel, { label_id });

	std::unordered_map<u32, u32> remap;
	if (input_inst && fn->parameter_ids.size() >= 2) {
		std::vector<u32> field_values;
		const bool parameter_used = used_value_ids.contains(fn->parameter_ids[1]);
		if (parameter_used) {
			for (size_t i = 0; i < input_vars.size() && i < input_inst->operands.size(); ++i) {
				const u32 field_type = input_inst->operands[i];
				const u32 value_id = rtvk_spv_fresh(&writer);
				// input_vars[i] == 0 marks a field whose Input variable was
				// skipped (e.g. fragment-input clip position). Substitute
				// OpUndef so the reconstructed aggregate still type-checks.
				if (input_vars[i] == 0) {
					emit_section_list(&function_ops, SpvOpUndef, { field_type, value_id });
				} else {
					emit_section_list(&function_ops, SpvOpLoad, { field_type, value_id, input_vars[i] });
				}
				field_values.push_back(value_id);
			}
			const u32 param_value = rtvk_spv_fresh(&writer);
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
				const u32 load_id = rtvk_spv_fresh(&writer);
				emit_section_list(&function_ops, SpvOpLoad, { input_inst->operands[i], load_id, input_vars[i] });
				remap.emplace(fn->parameter_ids[i + 1], load_id);
			}
		}
	}

	for (const auto& inst : fn->body) {
		if (inst.op == rt::RTIROp::Label || inst.op == rt::RTIROp::FunctionParameter)
			continue;
		if (inst.op >= rt::RTIROp::TypeVoid && inst.op <= rt::RTIROp::ConstantComposite)
			continue;
		if (inst.op == rt::RTIROp::Return) {
			emit_section(&function_ops, SpvOpReturn, {});
			continue;
		}
		if (inst.op == rt::RTIROp::ReturnValue) {
			const u32 value_id = remap.contains(inst.operands[0]) ? remap.at(inst.operands[0]) : inst.operands[0];
			if (output_inst && output_interface) {
				for (size_t i = 0; i < output_vars.size() && i < output_inst->operands.size(); ++i) {
					const u32 field_id = rtvk_spv_fresh(&writer);
					emit_section_list(&function_ops, SpvOpCompositeExtract, { output_inst->operands[i], field_id, value_id, static_cast<u32>(i) });
					emit_section_list(&function_ops, SpvOpStore, { output_vars[i], field_id });
				}
			}
			emit_section(&function_ops, SpvOpReturn, {});
			continue;
		}
		if (inst.op == rt::RTIROp::Load && !inst.operands.empty()) {
			const u32 source_id = remap.contains(inst.operands[0]) ? remap.at(inst.operands[0]) : inst.operands[0];
			if (auto it = wrapped_uniforms.find(source_id); it != wrapped_uniforms.end()) {
				const u32 wrapper_value = rtvk_spv_fresh(&writer);
				emit_section_list(&function_ops, SpvOpLoad, { it->second.struct_type, wrapper_value, source_id });
				emit_section_list(&function_ops, SpvOpCompositeExtract, { it->second.value_type, inst.result_id, wrapper_value, 0 });
				continue;
			}
		}

		rtvk_spv_reserve(&writer, inst.result_id);
		std::vector<u32> words;
		if (inst.result_id && inst.type_id) {
			words.push_back(inst.type_id);
			words.push_back(inst.result_id);
		}
		auto operands = rtvk_rtslp_remap_operands(inst.operands, remap);
		words.insert(words.end(), operands.begin(), operands.end());
		words.insert(words.end(), inst.literals.begin(), inst.literals.end());
		const SpvOp op = rtvk_rtslp_op(inst.op);
		if (op == SpvOpNop) {
			rtvk_throwf(
				RT_SHADER_COMPILATION_FAILED,
				"RTSLP %s stage cannot lower op %s",
				rtvk_rtslp_stage_name(stage),
				rtvk_rtslp_op_name(inst.op)
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

static bool rtvk_fill_reflection_from_module(
	const rt::RTArtifactModule& module,
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

rtvk_graphics_shader_compile_result rtvk_shader_compile_graphics_rtslp(struct rtvk_context* ctx, u64 program_size, const void* program_source) {
	rtvk_graphics_shader_compile_result result = {};
	try {
		rt::RTArtifactModule module = rt::read_rtslp_module(program_size, program_source);
		std::vector<u32> vertex_spirv;
		std::vector<u32> fragment_spirv;
		if (!rtvk_rtslp_emit_stage_spirv(module, rt::RTStageKind::Vertex, &vertex_spirv) ||
			!rtvk_rtslp_emit_stage_spirv(module, rt::RTStageKind::Fragment, &fragment_spirv)) {
			if (rtvk_error() == RT_SUCCESS) {
				rtvk_throwf(RT_SHADER_COMPILATION_FAILED, "failed to lower RTSLP to SPIR-V");
			}
			return {};
		}

		// rtvk_rtslp_emit_stage_spirv emits every global variable into every
		// stage's SPIR-V (and the entry-point interface), so each stage's
		// reflection must list the same uniforms. Otherwise the descriptor
		// set layout ends up with stage flags that omit a stage that actually
		// reads the binding (e.g. binding flagged VERTEX_BIT only while the
		// fragment shader samples it), and Vulkan rejects pipeline creation.
		if (!rtvk_fill_reflection_from_module(module, &result.vertex_reflection) ||
			!rtvk_fill_reflection_from_module(module, &result.fragment_reflection)) {
			rtvk_shader_reflection_clear(&result.vertex_reflection);
			rtvk_shader_reflection_clear(&result.fragment_reflection);
			rtvk_throwf(RT_OUT_OF_HOST_MEMORY, "failed to allocate RTSLP reflection");
			return {};
		}

		if (!rtvk_spirv_create_shader_module(ctx, vertex_spirv, result.vertex_shader)) {
			rtvk_shader_reflection_clear(&result.vertex_reflection);
			rtvk_shader_reflection_clear(&result.fragment_reflection);
			return {};
		}
		if (!rtvk_spirv_create_shader_module(ctx, fragment_spirv, result.fragment_shader)) {
			vkDestroyShaderModule(ctx->vk_device, result.vertex_shader, VK_ALLOCATOR);
			result.vertex_shader = VK_NULL_HANDLE;
			rtvk_shader_reflection_clear(&result.vertex_reflection);
			rtvk_shader_reflection_clear(&result.fragment_reflection);
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
