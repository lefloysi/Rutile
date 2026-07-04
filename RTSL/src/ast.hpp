#pragma once

#include "basic_source_manager.hpp"

#include <string>
#include <string_view>
#include <vector>

namespace rtsl {

enum class DeclKind {
	unknown,
	import,
	function,
	struct_decl,
	uniform,
	varying,
	input,
	output,
	namespace_decl,
};

// Backend shader stage. The 4-letter spellings (vert/frag) are the names
// of the compiler-generated backend entry points for each stage.
enum class StageKind : u8 {
	none,
	vertex,
	fragment,
};

[[nodiscard]] inline std::string_view stage_entry_name(StageKind stage) {
	switch (stage) {
	case StageKind::vertex:
		return "vert";
	case StageKind::fragment:
		return "frag";
	case StageKind::none:
		return "";
	}
	return "";
}

enum class StageRole : u8 {
	input,
	varying,
	output,
};

// Interpolation kind for a stage interface field. Encoded as u8 in the
// serialized stage_interface_table; the source spellings ("smooth", "flat",
// "clip") never reach the binary.
enum class InterpolationKind : u8 {
	none = 0,
	smooth = 1,
	flat = 2,
	clip = 3,
};

// Builtin pipeline slot (subset of SPIR-V BuiltIn) carried by a stage I/O
// field. `none` means the field has a user-assigned location instead.
enum class BuiltinSlot : u8 {
	none = 0,
	position = 1,
	point_size = 2,
	vertex_index = 3,
	instance_index = 4,
	frag_coord = 5,
	front_facing = 6,
	frag_depth = 7,
	global_invocation_id = 8,
	local_invocation_id = 9,
	work_group_id = 10,
	local_invocation_index = 11,
	num_work_groups = 12,
};

// Uniform access qualifier.
enum class AccessKind : u8 {
	read_write = 0,
	read_only = 1,
	write_only = 2,
};

// Encode/decode helpers — strings live in the source language only.
[[nodiscard]] inline InterpolationKind parse_interpolation(std::string_view text) {
	if (text == "smooth")
		return InterpolationKind::smooth;
	if (text == "flat")
		return InterpolationKind::flat;
	if (text == "clip")
		return InterpolationKind::clip;
	return InterpolationKind::none;
}
[[nodiscard]] inline BuiltinSlot parse_builtin_slot(std::string_view text) {
	if (text == "position")
		return BuiltinSlot::position;
	if (text == "point_size")
		return BuiltinSlot::point_size;
	if (text == "vertex_index")
		return BuiltinSlot::vertex_index;
	if (text == "instance_index")
		return BuiltinSlot::instance_index;
	if (text == "frag_coord")
		return BuiltinSlot::frag_coord;
	if (text == "front_facing")
		return BuiltinSlot::front_facing;
	if (text == "frag_depth")
		return BuiltinSlot::frag_depth;
	if (text == "global_invocation_id")
		return BuiltinSlot::global_invocation_id;
	if (text == "local_invocation_id")
		return BuiltinSlot::local_invocation_id;
	if (text == "work_group_id")
		return BuiltinSlot::work_group_id;
	if (text == "local_invocation_index")
		return BuiltinSlot::local_invocation_index;
	if (text == "num_work_groups")
		return BuiltinSlot::num_work_groups;
	return BuiltinSlot::none;
}
[[nodiscard]] inline AccessKind parse_access(std::string_view text) {
	if (text == "readonly")
		return AccessKind::read_only;
	if (text == "writeonly")
		return AccessKind::write_only;
	return AccessKind::read_write;
}

// One field of a stage interface payload, with its ABI placement.
// location == kNoLocation means "no explicit location" (e.g. builtin-driven).
struct StageIOField {
	static constexpr u32 kNoLocation = static_cast<u32>(-1);
	std::string name;
	InterpolationKind interpolation = InterpolationKind::none;
	BuiltinSlot builtin = BuiltinSlot::none;
	u32 location = kNoLocation;
};

// A declared stage interface: how a payload struct's fields cross a stage
// boundary (input attributes, interpolated varyings, or stage outputs).
struct StageInterface {
	StageRole role = StageRole::varying;
	std::string type_name;
	std::vector<StageIOField> fields;
};

struct ParameterDecl {
	std::string type;
	std::string name;
	bool is_const = false;
	bool is_reference = false;
};

struct Decl {
	struct Expr {
		enum class Kind {
			unknown,
			name,
			literal_int,
			literal_float,
			call,
			member,
			unary,
			binary,
		};

		Kind kind = Kind::unknown;
		std::string text;
		std::string op;
		std::vector<Expr> children;
		SourceSpan span{};
	};

	enum class BodyStatementKind {
		unknown,
		block,
		if_stmt,
		while_stmt,
		do_stmt,
		for_stmt,
		declaration,
		assignment,
		return_stmt,
		expression,
	};

	struct BodyStatement {
		BodyStatementKind kind = BodyStatementKind::unknown;
		std::string text;
		std::string type_name;
		std::string name;
		std::string initializer;
		std::string lhs;
		std::string rhs;
		std::string condition;
		std::string loop_init;
		std::string loop_continue;
		Expr expr{};
		std::vector<BodyStatement> children;
		std::vector<BodyStatement> else_children;
		SourceSpan span{};
	};

	DeclKind kind = DeclKind::unknown;
	std::string name;
	std::vector<ParameterDecl> parameters;
	std::string return_type;
	std::vector<BodyStatement> body_statements;
	SourceSpan span{};
	bool exported = false;
};

struct StructField {
	std::string type;
	std::string name;
};

struct ExportSymbol {
	std::string name;
	std::string kind;
	std::string type;
};

struct StructDecl {
	std::string name;
	std::vector<StructField> fields;
	std::vector<ParameterDecl> constructor_parameters;
};

struct UniformBinding {
	std::string scope_name;
	std::string name;
	std::string type;
	std::vector<StructField> inline_fields;
	AccessKind access = AccessKind::read_write;
	u32 set = 0;
	u32 member = 0;
	// IR type id of the member's value type (TypeStruct / sampler / image / ...).
	// Populated during IR lowering; 0 in the AST before lowering.
	u32 type_id = 0;
	// Anonymous `uniform { ... }` blocks have no source-visible scope name.
	// Each anonymous block is its own descriptor set; only named scopes can be
	// reopened across multiple blocks. The parser assigns each anonymous block
	// a unique anonymous_block_id; Sema uses it to keep their sets distinct
	// without leaking compiler-generated names into the C API or mangling.
	bool is_anonymous = false;
	u32 anonymous_block_id = 0;
};

// Memory-layout rule for a resource binding's payload. Governs offsets and
// paddings when a host-writable struct becomes bytes in device memory.
// `unset` means the binding kind's default applies (std140 for UniformBuffer,
// std430 for StorageBuffer) — resolved during sema/IR lowering, never
// present past that point.
enum class LayoutRule : u8 {
	unset = 0,
	std140 = 1,
	std430 = 2,
	scalar = 3,
};

[[nodiscard]] inline LayoutRule parse_layout_rule(std::string_view text) {
	if (text == "std140")
		return LayoutRule::std140;
	if (text == "std430")
		return LayoutRule::std430;
	if (text == "scalar")
		return LayoutRule::scalar;
	return LayoutRule::unset;
}

// `layout PATH : [RULE] TYPE;` — attaches a payload type to a
// UniformBuffer/StorageBuffer binding. PATH is a `::`-separated path resolved
// against declared uniforms (e.g. `mat::camera`). RULE is an optional layout
// rule (std140/std430/scalar); when omitted the binding kind's default is
// used.
//
// TYPE is either a named type spelling (`type_spelling` populated, e.g.
// `mat4`) or an inline `struct { ... }` body (`is_inline_struct` true,
// members in `inline_fields`).
struct LayoutDecl {
	std::vector<std::string> path;
	LayoutRule rule = LayoutRule::unset;
	bool is_inline_struct = false;
	std::string type_spelling;              // only when !is_inline_struct
	std::vector<StructField> inline_fields; // only when is_inline_struct
	SourceSpan span{};
};

// `using Alias = Base : intrinsic field, ...;` — records that source-level
// uses of `Alias` in this translation unit refer to the base type `Base`. The
// boundary spec itself is materialized as a StageInterface (role=varying) on
// the base type; the alias name never leaks past parse. A `using Alias = Base;`
// without a spec is a plain type alias.
struct TypeAlias {
	std::string name;
	std::string base;
};

struct TranslationUnit {
	u32 file_id = 0;
	std::vector<Decl> declarations;
	std::vector<std::string> imports;
	std::vector<ExportSymbol> exports;
	std::vector<StructDecl> structs;
	std::vector<UniformBinding> uniforms;
	std::vector<LayoutDecl> layouts;
	std::vector<StageInterface> stage_interfaces;
	std::vector<TypeAlias> type_aliases;
};

} // namespace rtsl
