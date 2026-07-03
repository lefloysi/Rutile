#pragma once

// RTSL Intermediate Representation.
//
// SSA, typed, instruction-stream IR shaped after SPIR-V. See
// docs/rtir.md for the full design. The old statement-string BodyOp model
// has been removed.

#include "sema.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace rtsl {

// SSA / type / constant / function id. Shared id space within a module.
// Id 0 is reserved as "no id".
using IRId = u32;
inline constexpr IRId IRId_None = 0;

// String pool reference into the artifact's string_table.
struct StringId {
	u32 value = 0;
};

// Source-debug origin for an instruction. file_id is a SourceManager file id
// at compile time and a debug_table file id once serialized.
struct DebugLocation {
	u32 file_id = 0;
	u32 line = 0;
	u32 column = 0;
};

// SPIR-V-like storage class for OpVariable / OpTypePointer. Encoded as a u8
// literal on those instructions.
enum class StorageClass : u8 {
	Function = 0,
	Input = 1,
	Output = 2,
	Uniform = 3,
	UniformConstant = 4,
	StorageBuffer = 5,
	PushConstant = 6,
	Private = 7,
};

// Opcode set. Open enum — new ops can be appended. Stable wire encoding lives
// in Serialization/Artifact.cpp via an op_id table so reordering this enum
// does not break .rtslo/.rtslp files written by older builds.
enum class IROp : u16 {
	Nop = 0,

	// --- Types (results go into IRModule::type_constant_pool) ---
	TypeVoid,
	TypeBool,
	TypeInt,	  // literal: width (bits)
	TypeUInt,	  // literal: width (bits)
	TypeFloat,	  // literal: width (bits)
	TypeVector,	  // operand: scalar type id; literal: component count
	TypeMatrix,	  // operand: column-vector type id; literal: column count
	TypeStruct,	  // operands: member type ids
	TypePointer,  // operand: pointee type id; literal: StorageClass
	TypeArray,	  // operand: element type id; literal: length (0 = runtime)
	TypeFunction, // operands: return type id, parameter type ids...
	TypeImage,
	TypeSampler,
	TypeSampledImage, // operand: image type id

	// --- Constants ---
	ConstantBool,	   // literal: 0 or 1
	ConstantInt,	   // literal: low/high bits
	ConstantUInt,	   // literal: low/high bits
	ConstantFloat,	   // literal: bit pattern (low/high for 64-bit)
	ConstantComposite, // operands: element ids

	// --- Variables / memory ---
	Variable,	 // literal: StorageClass; operand[0]: optional initializer id
	Load,		 // operand[0]: pointer id
	Store,		 // operand[0]: pointer id; operand[1]: value id
	AccessChain, // operand[0]: base pointer id; operand[1..]: index ids

	// --- Composite ---
	CompositeConstruct, // operands: element ids
	CompositeExtract,	// operand[0]: composite id; literals: indices
	CompositeInsert,	// operand[0]: composite id; operand[1]: value id; literals: indices
	VectorShuffle,		// operand[0..1]: vec ids; literals: component indices

	// --- Arithmetic ---
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

	// --- Vector / matrix ---
	VectorTimesScalar,
	MatrixTimesScalar,
	MatrixTimesVector,
	MatrixTimesMatrix,
	Dot,
	Cross,

	// --- Comparison / logical ---
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

	// --- Conversion ---
	ConvertFToU,
	ConvertFToS,
	ConvertSToF,
	ConvertUToF,
	Bitcast,

	// --- Control flow ---
	Label,			   // begins a basic block; no operands
	Branch,			   // operand[0]: target label id
	BranchConditional, // operand[0]: cond; operand[1]: true label; operand[2]: false label
	SelectionMerge,	   // operand[0]: merge label; literal: SelectionControl
	LoopMerge,		   // operand[0]: merge label; operand[1]: continue label; literal: LoopControl
	Return,
	ReturnValue, // operand[0]: value id

	// --- Function ---
	FunctionParameter, // result: parameter id; type_id: parameter type
	FunctionCall,	   // operand[0]: callee function id; operand[1..]: arg ids

	// --- Image / sampler ---
	SampledImage,			// operand[0]: image id; operand[1]: sampler id
	ImageSampleImplicitLod, // operand[0]: sampled image id; operand[1]: coords id
	ImageSampleExplicitLod, // operand[0]: sampled image; operand[1]: coords; operand[2]: lod
	ImageRead,
	ImageWrite,

	// --- Stage I/O primitives ---
	// High-level RTSL primitives. The SPIR-V backend lowers these to Load/Store
	// against Input/Output storage class variables. Keeping them as first-class
	// ops means stage interface assignment can stay metadata-driven instead of
	// requiring the IR to spell out every input/output OpVariable up front.
	ReadInput,	  // literal: location
	WriteOutput,  // operand[0]: value id; literal: location
	ReadBuiltin,  // literal: BuiltIn enum
	WriteBuiltin, // operand[0]: value id; literal: BuiltIn enum

	// --- Standard library / extended set ---
	ExtInst, // literal[0]: ext set id (0 = rt.std.450); literal[1]: ext op number;
			 // operands: arg ids
};

// SPIR-V-aligned builtin slots referenced by ReadBuiltin / WriteBuiltin and
// BuiltIn decorations.
enum class BuiltIn : u16 {
	Position,
	PointSize,
	VertexIndex,
	InstanceIndex,
	FragCoord,
	FrontFacing,
	FragDepth,
	GlobalInvocationId,
	LocalInvocationId,
	WorkGroupId,
	LocalInvocationIndex,
	NumWorkGroups,
};

struct IRInstruction {
	IROp op = IROp::Nop;
	IRId result_id = IRId_None; // 0 if no result
	IRId type_id = IRId_None;	// 0 if not typed
	std::vector<IRId> operands;
	std::vector<u32> literals;
	DebugLocation loc{};
};

// Decoration kinds. Kept in a dedicated section so backends iterate them once
// instead of scanning the SSA stream.
enum class IRDecorationKind : u16 {
	Location,
	Binding,
	DescriptorSet,
	Offset,
	ArrayStride,
	MatrixStride,
	BuiltIn,
	NoPerspective,
	Flat,
	Centroid,
	Sample,
	NonWritable,
	NonReadable,
	Block,
	BufferBlock,
	ColMajor,
	RowMajor,
};

struct IRDecoration {
	IRId target = IRId_None;
	IRDecorationKind kind = IRDecorationKind::Location;
	// u32(-1) means "decorate the target itself"; otherwise the struct member
	// at that index (OpMemberDecorate equivalent).
	u32 member_index = static_cast<u32>(-1);
	std::vector<u32> literals;
};

struct IRFunction {
	IRId result_id = IRId_None;		   // OpFunction result id
	IRId function_type_id = IRId_None; // OpTypeFunction id
	IRId return_type_id = IRId_None;
	std::vector<IRId> parameter_ids; // each is an OpFunctionParameter result
	std::vector<IRInstruction> body; // starts with Label, ends with terminator

	// Backend stage. User functions are StageKind::none; the compiler-generated
	// backend wrappers (vert/frag) carry the stage.
	StageKind stage = StageKind::none;

	// True for compiler-synthesized ABI glue (the generated stage runtime).
	bool generated = false;

	// True for struct member-init constructors (`Foo::Foo(...)` declarations).
	// The IR inliner replaces every call to one with the constructor body, so
	// the standalone function is dead after lowering. We keep it in the IR
	// module for diagnostics but elide it from the serialized artifact.
	bool is_constructor = false;

	// True if the source declared the function with `export`. Exported
	// functions reach .rtslm (the public interface) and are visible to
	// importers; non-exported functions are private to the implementation.
	bool exported = false;

	StringId display_name{}; // authored, e.g. "vert_main"
	StringId mangled_name{}; // canonical identity, e.g. "_Z9vert_main..."

	// Source-level identifier used by the inliner to match unresolved
	// FunctionCall instructions to their target. The StringId-based pair above
	// is for the artifact's string pool; this is the raw resolution key the
	// IR pipeline uses before the string pool is built.
	std::string source_name;
};

struct IRFunctionDebugInfo {
	StringId display_name{};
	std::vector<StringId> parameter_names;
};

struct IRModule {
	std::string source_name;
	std::vector<std::string> imports;
	std::vector<ExportSymbol> imported_exports;
	std::vector<ExportSymbol> exports;

	// Forward-only ordered pool: an entry may only reference earlier entries.
	std::vector<IRInstruction> type_constant_pool;

	// Module-scope OpVariable instructions (Input/Output/Uniform/...).
	std::vector<IRInstruction> global_variables;

	std::vector<IRDecoration> decorations;
	std::vector<IRFunction> functions;
	std::vector<IRFunctionDebugInfo> function_debug;

	// Reflection bridges. Not part of SSA semantics; they let
	// rtslModuleGetUniform / rtslModuleGetStageVariable / rtslModuleGetEntry
	// answer queries directly. Persisted via the existing resource_table /
	// stage_interface_table artifact sections.
	std::vector<StructDecl> structs;
	std::vector<UniformBinding> uniforms;
	std::vector<StageInterface> stage_interfaces;

	// Pending call targets, indexed by FunctionCall.literals[0]. Each entry is
	// the source-level identifier the call site asked for. The IR inliner
	// (single-module pass) and the linker (cross-module pass) resolve these
	// against function source_name + argument count. Cleared once all calls
	// in the module are inlined.
	std::vector<std::string> call_target_names;

	// Monotonic id allocator. Backends remap to their own id space.
	IRId next_id = 1;
};

[[nodiscard]] IRModule lower_to_ir(const SemanticModule& module, DiagnosticEngine* diagnostics = nullptr);
[[nodiscard]] bool verify_ir(const IRModule& module);

} // namespace rtsl
