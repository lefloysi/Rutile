#include "text_rtir.hpp"

#include <sstream>

namespace rtsl {


std::string_view kind_name(ArtifactKind kind) {
	switch (kind) {
	case ArtifactKind::object:
		return "rtslo";
	case ArtifactKind::module:
		return "rtslm";
	case ArtifactKind::library:
		return "rtsll";
	case ArtifactKind::program:
		return "rtslp";
	}
	return "unknown";
}

const char* op_name(IROp op) {
	switch (op) {
	case IROp::Nop:
		return "OpNop";
	case IROp::TypeVoid:
		return "OpTypeVoid";
	case IROp::TypeBool:
		return "OpTypeBool";
	case IROp::TypeInt:
		return "OpTypeInt";
	case IROp::TypeUInt:
		return "OpTypeUInt";
	case IROp::TypeFloat:
		return "OpTypeFloat";
	case IROp::TypeVector:
		return "OpTypeVector";
	case IROp::TypeMatrix:
		return "OpTypeMatrix";
	case IROp::TypeStruct:
		return "OpTypeStruct";
	case IROp::TypePointer:
		return "OpTypePointer";
	case IROp::TypeArray:
		return "OpTypeArray";
	case IROp::TypeFunction:
		return "OpTypeFunction";
	case IROp::TypeImage:
		return "OpTypeImage";
	case IROp::TypeSampler:
		return "OpTypeSampler";
	case IROp::TypeSampledImage:
		return "OpTypeSampledImage";
	case IROp::ConstantBool:
		return "OpConstantTrueFalse";
	case IROp::ConstantInt:
		return "OpConstant";
	case IROp::ConstantUInt:
		return "OpConstant";
	case IROp::ConstantFloat:
		return "OpConstant";
	case IROp::ConstantComposite:
		return "OpConstantComposite";
	case IROp::Variable:
		return "OpVariable";
	case IROp::Load:
		return "OpLoad";
	case IROp::Store:
		return "OpStore";
	case IROp::AccessChain:
		return "OpAccessChain";
	case IROp::CompositeConstruct:
		return "OpCompositeConstruct";
	case IROp::CompositeExtract:
		return "OpCompositeExtract";
	case IROp::CompositeInsert:
		return "OpCompositeInsert";
	case IROp::VectorShuffle:
		return "OpVectorShuffle";
	case IROp::FAdd:
		return "OpFAdd";
	case IROp::FSub:
		return "OpFSub";
	case IROp::FMul:
		return "OpFMul";
	case IROp::FDiv:
		return "OpFDiv";
	case IROp::FMod:
		return "OpFRem";
	case IROp::FNegate:
		return "OpFNegate";
	case IROp::IAdd:
		return "OpIAdd";
	case IROp::ISub:
		return "OpISub";
	case IROp::IMul:
		return "OpIMul";
	case IROp::SDiv:
		return "OpSDiv";
	case IROp::UDiv:
		return "OpUDiv";
	case IROp::SMod:
		return "OpSMod";
	case IROp::UMod:
		return "OpUMod";
	case IROp::VectorTimesScalar:
		return "OpVectorTimesScalar";
	case IROp::MatrixTimesScalar:
		return "OpMatrixTimesScalar";
	case IROp::MatrixTimesVector:
		return "OpMatrixTimesVector";
	case IROp::MatrixTimesMatrix:
		return "OpMatrixTimesMatrix";
	case IROp::Dot:
		return "OpDot";
	case IROp::Cross:
		return "OpExtInstCross";
	case IROp::FOrdEqual:
		return "OpFOrdEqual";
	case IROp::FOrdNotEqual:
		return "OpFOrdNotEqual";
	case IROp::FOrdLess:
		return "OpFOrdLessThan";
	case IROp::FOrdLessEqual:
		return "OpFOrdLessThanEqual";
	case IROp::FOrdGreater:
		return "OpFOrdGreaterThan";
	case IROp::FOrdGreaterEqual:
		return "OpFOrdGreaterThanEqual";
	case IROp::IEqual:
		return "OpIEqual";
	case IROp::INotEqual:
		return "OpINotEqual";
	case IROp::SLess:
		return "OpSLessThan";
	case IROp::SLessEqual:
		return "OpSLessThanEqual";
	case IROp::SGreater:
		return "OpSGreaterThan";
	case IROp::SGreaterEqual:
		return "OpSGreaterThanEqual";
	case IROp::ULess:
		return "OpULessThan";
	case IROp::ULessEqual:
		return "OpULessThanEqual";
	case IROp::UGreater:
		return "OpUGreaterThan";
	case IROp::UGreaterEqual:
		return "OpUGreaterThanEqual";
	case IROp::LogicalAnd:
		return "OpLogicalAnd";
	case IROp::LogicalOr:
		return "OpLogicalOr";
	case IROp::LogicalNot:
		return "OpLogicalNot";
	case IROp::ConvertFToU:
		return "OpConvertFToU";
	case IROp::ConvertFToS:
		return "OpConvertFToS";
	case IROp::ConvertSToF:
		return "OpConvertSToF";
	case IROp::ConvertUToF:
		return "OpConvertUToF";
	case IROp::Bitcast:
		return "OpBitcast";
	case IROp::Label:
		return "OpLabel";
	case IROp::Branch:
		return "OpBranch";
	case IROp::BranchConditional:
		return "OpBranchConditional";
	case IROp::SelectionMerge:
		return "OpSelectionMerge";
	case IROp::LoopMerge:
		return "OpLoopMerge";
	case IROp::Return:
		return "OpReturn";
	case IROp::ReturnValue:
		return "OpReturnValue";
	case IROp::FunctionParameter:
		return "OpFunctionParameter";
	case IROp::FunctionCall:
		return "OpFunctionCall";
	case IROp::SampledImage:
		return "OpSampledImage";
	case IROp::ImageSampleImplicitLod:
		return "OpImageSampleImplicitLod";
	case IROp::ImageSampleExplicitLod:
		return "OpImageSampleExplicitLod";
	case IROp::ImageRead:
		return "OpImageRead";
	case IROp::ImageWrite:
		return "OpImageWrite";
	case IROp::ReadInput:
		return "OpRTReadInput";
	case IROp::WriteOutput:
		return "OpRTWriteOutput";
	case IROp::ReadBuiltin:
		return "OpRTReadBuiltin";
	case IROp::WriteBuiltin:
		return "OpRTWriteBuiltin";
	case IROp::ExtInst:
		return "OpExtInst";
	}
	return "OpUnknown";
}

void append_instruction(std::ostringstream& out, const IRInstruction& inst) {
	if (inst.result_id != IRId_None) {
		out << "%" << inst.result_id << " = ";
	}
	out << op_name(inst.op);
	if (inst.type_id != IRId_None)
		out << " %" << inst.type_id;
	for (IRId operand : inst.operands)
		out << " %" << operand;
	for (u32 literal : inst.literals)
		out << " " << literal;
	out << "\n";
}


std::string disassemble_artifact(const Artifact& artifact) {
	std::ostringstream out;
	out << "artifact " << kind_name(artifact.kind) << " 2.0\n";
	out << "source \"" << artifact.module.source_name << "\"\n\n";
	if (!artifact.module.imports.empty()) {
		out << "; imports\n";
		for (const auto& name : artifact.module.imports) {
			out << "import \"" << name << "\";\n";
		}
		out << "\n";
	}
	out << "; type_constant_pool\n";
	for (const auto& inst : artifact.module.type_constant_pool)
		append_instruction(out, inst);
	out << "\n; global_variables\n";
	for (const auto& inst : artifact.module.global_variables)
		append_instruction(out, inst);
	for (const auto& fn : artifact.module.functions) {
		out << "\n; function %" << fn.result_id;
		if (fn.stage != StageKind::none)
			out << " stage=" << stage_entry_name(fn.stage);
		if (fn.generated)
			out << " generated";
		out << "\n";
		for (const auto& inst : fn.body)
			append_instruction(out, inst);
	}
	return out.str();
}

bool assemble_text_rtir(std::string_view, Artifact&, DiagnosticEngine* diagnostics) {
	if (diagnostics) {
		diagnostics->report(7001, DiagnosticSeverity::error, {}, "<rtir>", "text RTIR assembly is not implemented for the SSA format");
	}
	return false;
}

} // namespace rtsl
