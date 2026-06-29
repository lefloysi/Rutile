#ifndef RUTILE_BACKEND_TOOLS_RTSLP_PACKAGE_HPP
#define RUTILE_BACKEND_TOOLS_RTSLP_PACKAGE_HPP

#include "rutile.h"

#include <cstdint>
#include <string>
#include <vector>

namespace rt {

enum class RTIROp : u16 {
	Nop = 0,
	TypeVoid,
	TypeBool,
	TypeInt,
	TypeUInt,
	TypeFloat,
	TypeVector,
	TypeMatrix,
	TypeStruct,
	TypePointer,
	TypeArray,
	TypeFunction,
	TypeImage,
	TypeSampler,
	TypeSampledImage,
	ConstantBool,
	ConstantInt,
	ConstantUInt,
	ConstantFloat,
	ConstantComposite,
	Variable,
	Load,
	Store,
	AccessChain,
	CompositeConstruct,
	CompositeExtract,
	CompositeInsert,
	VectorShuffle,
	FAdd,
	FSub,
	FMul,
	FDiv,
	FMod,
	FNegate,
	IAdd,
	ISub,
	IMul,
	SDiv,
	UDiv,
	SMod,
	UMod,
	VectorTimesScalar,
	MatrixTimesScalar,
	MatrixTimesVector,
	MatrixTimesMatrix,
	Dot,
	Cross,
	FOrdEqual,
	FOrdNotEqual,
	FOrdLess,
	FOrdLessEqual,
	FOrdGreater,
	FOrdGreaterEqual,
	IEqual,
	INotEqual,
	SLess,
	SLessEqual,
	SGreater,
	SGreaterEqual,
	ULess,
	ULessEqual,
	UGreater,
	UGreaterEqual,
	LogicalAnd,
	LogicalOr,
	LogicalNot,
	ConvertFToU,
	ConvertFToS,
	ConvertSToF,
	ConvertUToF,
	Bitcast,
	Label,
	Branch,
	BranchConditional,
	SelectionMerge,
	LoopMerge,
	Return,
	ReturnValue,
	FunctionParameter,
	FunctionCall,
	SampledImage,
	ImageSampleImplicitLod,
	ImageSampleExplicitLod,
	ImageRead,
	ImageWrite,
	ReadInput,
	WriteOutput,
	ReadBuiltin,
	WriteBuiltin,
	ExtInst,
};

enum class RTStageKind : std::uint8_t {
	None = 0,
	Vertex = 1,
	Fragment = 2,
	Compute = 4,
};

enum class RTStageRole : std::uint8_t {
	Input = 0,
	Varying = 1,
	Output = 2,
};

struct RTInstruction {
	RTIROp op = RTIROp::Nop;
	u32 result_id = 0;
	u32 type_id = 0;
	std::vector<u32> operands;
	std::vector<u32> literals;
};

struct RTFunction {
	u32 result_id = 0;
	u32 return_type_id = 0;
	RTStageKind stage = RTStageKind::None;
	std::vector<u32> parameter_ids;
	std::vector<RTInstruction> body;
};

struct RTStructField {
	std::string type;
	std::string name;
};

struct RTStructDecl {
	std::string name;
	std::vector<RTStructField> fields;
};

struct RTUniform {
	std::string scope_name;
	std::string name;
	std::string type;
	std::string access;
	u32 set = 0;
	u32 binding = 0;
};

struct RTStageField {
	std::string name;
	std::string interpolation;
	std::string builtin;
	u32 location = 0;
	bool has_location = false;
};

struct RTStageInterface {
	RTStageRole role = RTStageRole::Input;
	std::string type_name;
	std::vector<RTStageField> fields;
};

struct RTArtifactModule {
	std::vector<RTInstruction> type_constant_pool;
	std::vector<RTInstruction> global_variables;
	std::vector<RTFunction> functions;
	std::vector<RTStructDecl> structs;
	std::vector<RTUniform> uniforms;
	std::vector<RTStageInterface> stage_interfaces;
	u32 next_id = 1;
};

RTArtifactModule read_rtslp_module(u64 program_size, const void* program_source);

} // namespace rt

#endif
