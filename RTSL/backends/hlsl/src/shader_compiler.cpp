// Windows headers pulled in transitively by error.h / graphics_program.h
// define min/max macros that clobber <algorithm>'s std::min / std::max.
#ifndef NOMINMAX
#define NOMINMAX
#endif

#include "../../include/rtsl_hlsl_backend.hpp"

#include <rtslp_package.hpp>

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace {

using rt::RTArtifactModule;
using rt::RTFunction;
using rt::RTInstruction;
using rt::RTIROp;
using rt::RTStageField;
using rt::RTStageInterface;
using rt::RTStageKind;
using rt::RTStageRole;
using rt::RTStructDecl;
using rt::RTUniform;

struct TypeRegistry {
	std::unordered_map<u32, std::string> type_names;
	std::unordered_map<u32, std::size_t> struct_index_by_type_id;
	std::unordered_map<u32, u32> pointer_pointee_type;
	std::unordered_map<u32, std::pair<u32, u32>> vector_component_and_count;
	std::unordered_map<u32, std::pair<u32, u32>> matrix_column_and_count;
	std::unordered_map<u32, std::string> constant_exprs;
};

bool is_type_op(RTIROp op) {
	switch (op) {
	case RTIROp::TypeVoid:
	case RTIROp::TypeBool:
	case RTIROp::TypeInt:
	case RTIROp::TypeUInt:
	case RTIROp::TypeFloat:
	case RTIROp::TypeVector:
	case RTIROp::TypeMatrix:
	case RTIROp::TypeStruct:
	case RTIROp::TypePointer:
	case RTIROp::TypeArray:
	case RTIROp::TypeFunction:
	case RTIROp::TypeImage:
	case RTIROp::TypeSampler:
	case RTIROp::TypeSampledImage:
		return true;
	default:
		return false;
	}
}

bool is_constant_op(RTIROp op) {
	switch (op) {
	case RTIROp::ConstantBool:
	case RTIROp::ConstantInt:
	case RTIROp::ConstantUInt:
	case RTIROp::ConstantFloat:
	case RTIROp::ConstantComposite:
		return true;
	default:
		return false;
	}
}

// Map RTSL source-level type spellings to HLSL.
std::string rtsl_type_to_hlsl(std::string_view rtsl_type) {
	if (rtsl_type == "f32" || rtsl_type == "float")
		return "float";
	if (rtsl_type == "f64" || rtsl_type == "double")
		return "double";
	if (rtsl_type == "i32" || rtsl_type == "int")
		return "int";
	if (rtsl_type == "u32" || rtsl_type == "uint")
		return "uint";
	if (rtsl_type == "bool")
		return "bool";
	if (rtsl_type == "vec2")
		return "float2";
	if (rtsl_type == "vec3")
		return "float3";
	if (rtsl_type == "vec4")
		return "float4";
	if (rtsl_type == "ivec2")
		return "int2";
	if (rtsl_type == "ivec3")
		return "int3";
	if (rtsl_type == "ivec4")
		return "int4";
	if (rtsl_type == "uvec2")
		return "uint2";
	if (rtsl_type == "uvec3")
		return "uint3";
	if (rtsl_type == "uvec4")
		return "uint4";
	if (rtsl_type == "mat2")
		return "float2x2";
	if (rtsl_type == "mat3")
		return "float3x3";
	if (rtsl_type == "mat4")
		return "float4x4";
	if (rtsl_type == "Sampler2D")
		return "Texture2D";
	return std::string(rtsl_type);
}

// True if a scalar/component type id (TypeFloat/TypeInt/TypeUInt/TypeBool).
bool is_scalar_type_id(const TypeRegistry& reg, u32 type_id) {
	auto it = reg.type_names.find(type_id);
	if (it == reg.type_names.end())
		return false;
	return it->second == "float" || it->second == "double" || it->second == "int" || it->second == "uint" || it->second == "bool";
}

// Look up the HLSL spelling of a type id; throws if missing.
const std::string& hlsl_type(const TypeRegistry& reg, u32 type_id) {
	auto it = reg.type_names.find(type_id);
	if (it == reg.type_names.end()) {
		throw std::runtime_error("rtslp HLSL emit: unknown type id " + std::to_string(type_id));
	}
	return it->second;
}

void build_type_registry(const RTArtifactModule& module, TypeRegistry& reg) {
	std::size_t struct_idx = 0;
	for (const auto& inst : module.type_constant_pool) {
		switch (inst.op) {
		case RTIROp::TypeVoid:
			reg.type_names[inst.result_id] = "void";
			break;
		case RTIROp::TypeBool:
			reg.type_names[inst.result_id] = "bool";
			break;
		case RTIROp::TypeFloat: {
			const u32 width = inst.literals.empty() ? 32u : inst.literals[0];
			reg.type_names[inst.result_id] = width >= 64 ? "double" : "float";
			break;
		}
		case RTIROp::TypeInt:
			reg.type_names[inst.result_id] = "int";
			break;
		case RTIROp::TypeUInt:
			reg.type_names[inst.result_id] = "uint";
			break;
		case RTIROp::TypeVector: {
			const u32 comp_type = inst.operands.empty() ? 0u : inst.operands[0];
			const u32 count = inst.literals.empty() ? 0u : inst.literals[0];
			reg.vector_component_and_count[inst.result_id] = { comp_type, count };
			const auto it = reg.type_names.find(comp_type);
			const std::string base = it != reg.type_names.end() ? it->second : std::string{ "float" };
			reg.type_names[inst.result_id] = base + std::to_string(count);
			break;
		}
		case RTIROp::TypeMatrix: {
			const u32 col_type = inst.operands.empty() ? 0u : inst.operands[0];
			const u32 cols = inst.literals.empty() ? 0u : inst.literals[0];
			reg.matrix_column_and_count[inst.result_id] = { col_type, cols };
			u32 component = 0;
			u32 rows = 0;
			if (auto it = reg.vector_component_and_count.find(col_type); it != reg.vector_component_and_count.end()) {
				component = it->second.first;
				rows = it->second.second;
			}
			const auto cit = reg.type_names.find(component);
			const std::string base = cit != reg.type_names.end() ? cit->second : std::string{ "float" };
			reg.type_names[inst.result_id] = base + std::to_string(rows) + "x" + std::to_string(cols);
			break;
		}
		case RTIROp::TypeStruct: {
			// The IR pre-materializes synthetic opaque structs (globals /
			// vert_globals / frag_globals) ahead of user struct decls. Those
			// have no operands; real user structs carry their field type ids.
			// Only the latter should consume the user-struct cursor.
			std::string name;
			if (!inst.operands.empty() && struct_idx < module.structs.size()) {
				name = module.structs[struct_idx].name;
				reg.struct_index_by_type_id[inst.result_id] = struct_idx;
				++struct_idx;
			}
			if (name.empty())
				name = "rtsl_struct_" + std::to_string(inst.result_id);
			reg.type_names[inst.result_id] = name;
			break;
		}
		case RTIROp::TypePointer: {
			const u32 pointee = inst.operands.empty() ? 0u : inst.operands[0];
			reg.pointer_pointee_type[inst.result_id] = pointee;
			if (auto it = reg.type_names.find(pointee); it != reg.type_names.end()) {
				reg.type_names[inst.result_id] = it->second;
			} else {
				reg.type_names[inst.result_id] = "void";
			}
			break;
		}
		case RTIROp::TypeArray:
		case RTIROp::TypeFunction:
		case RTIROp::TypeImage:
		case RTIROp::TypeSampler:
		case RTIROp::TypeSampledImage:
			// Not directly nameable in HLSL or unused by current emit paths;
			// leave the id unmapped so any accidental use throws.
			break;
		case RTIROp::ConstantBool: {
			const u32 value = inst.literals.empty() ? 0u : inst.literals[0];
			reg.constant_exprs[inst.result_id] = value ? "true" : "false";
			break;
		}
		case RTIROp::ConstantInt: {
			const u32 raw = inst.literals.empty() ? 0u : inst.literals[0];
			reg.constant_exprs[inst.result_id] = "int(" + std::to_string(static_cast<int>(raw)) + ")";
			break;
		}
		case RTIROp::ConstantUInt: {
			const u32 raw = inst.literals.empty() ? 0u : inst.literals[0];
			reg.constant_exprs[inst.result_id] = std::to_string(raw) + "u";
			break;
		}
		case RTIROp::ConstantFloat: {
			const u32 bits = inst.literals.empty() ? 0u : inst.literals[0];
			float value = 0.0f;
			std::memcpy(&value, &bits, sizeof(value));
			char buf[64];
			std::snprintf(buf, sizeof(buf), "%.9g", static_cast<double>(value));
			std::string text = buf;
			if (text.find('.') == std::string::npos && text.find('e') == std::string::npos &&
				text.find("inf") == std::string::npos && text.find("nan") == std::string::npos) {
				text += ".0";
			}
			text += "f";
			reg.constant_exprs[inst.result_id] = text;
			break;
		}
		case RTIROp::ConstantComposite: {
			const auto type_it = reg.type_names.find(inst.type_id);
			std::ostringstream out;
			if (type_it != reg.type_names.end())
				out << type_it->second;
			out << "(";
			for (std::size_t i = 0; i < inst.operands.size(); ++i) {
				if (i)
					out << ", ";
				auto eit = reg.constant_exprs.find(inst.operands[i]);
				out << (eit != reg.constant_exprs.end() ? eit->second : std::string("0"));
			}
			out << ")";
			reg.constant_exprs[inst.result_id] = out.str();
			break;
		}
		default:
			break;
		}
	}
}

const RTFunction* find_stage_function(const RTArtifactModule& module, RTStageKind stage) {
	for (const auto& fn : module.functions) {
		if (fn.stage == stage)
			return &fn;
	}
	return nullptr;
}

const RTStageInterface* find_interface(const RTArtifactModule& module, std::string_view type_name, RTStageRole role) {
	for (const auto& iface : module.stage_interfaces) {
		if (iface.type_name == type_name && iface.role == role)
			return &iface;
	}
	return nullptr;
}

const RTStructDecl* find_struct_decl(const RTArtifactModule& module, std::string_view type_name) {
	for (const auto& decl : module.structs) {
		if (decl.name == type_name)
			return &decl;
	}
	return nullptr;
}

u32 struct_type_id_for_decl(const RTArtifactModule& module, const RTStructDecl& decl) {
	for (const auto& inst : module.type_constant_pool) {
		if (inst.op != RTIROp::TypeStruct || inst.operands.size() != decl.fields.size())
			continue;
		bool same = true;
		for (std::size_t i = 0; i < decl.fields.size(); ++i) {
			const auto* field_decl = find_struct_decl(module, decl.fields[i].type);
			const bool is_nested_struct = field_decl != nullptr;
			const bool operand_is_struct = i < inst.operands.size() && inst.operands[i] != 0;
			if (is_nested_struct != operand_is_struct) {
				same = false;
				break;
			}
		}
		if (same)
			return inst.result_id;
	}
	return 0;
}

bool is_clip_position(const RTStageField& field) {
	return field.builtin == "position" || field.interpolation == "clip";
}

// Look up the input payload struct type for a stage function. The convention
// matches the rest of the toolchain: parameter 0 is the per-stage globals
// carrier; parameter 1 is the input payload (a user struct).
u32 stage_input_struct_type(const RTFunction& fn) {
	if (fn.parameter_ids.size() < 2)
		return 0;
	for (const auto& inst : fn.body) {
		if (inst.op == RTIROp::FunctionParameter && inst.result_id == fn.parameter_ids[1]) {
			return inst.type_id;
		}
	}
	return 0;
}

// Find the index of a struct field by name; -1 if not found.
int find_field_index(const RTStructDecl& decl, std::string_view name) {
	for (std::size_t i = 0; i < decl.fields.size(); ++i) {
		if (decl.fields[i].name == name)
			return static_cast<int>(i);
	}
	return -1;
}

// Compute the transitive closure of user struct names a stage actually
// depends on, seeded by the input and output payload types. Anything else
// (e.g. the vertex stage's `Fragment` struct, or the fragment stage's
// `Point`) is dead code in the emitted HLSL and is skipped.
std::unordered_set<std::string> reachable_structs(const RTArtifactModule& module, std::string_view input_payload, std::string_view output_payload) {
	std::unordered_set<std::string> reachable;
	std::vector<std::string> queue;
	auto enqueue = [&](std::string_view name) {
		if (name.empty())
			return;
		const std::string key(name);
		if (reachable.insert(key).second)
			queue.push_back(key);
	};
	enqueue(input_payload);
	enqueue(output_payload);
	while (!queue.empty()) {
		const std::string current = std::move(queue.back());
		queue.pop_back();
		for (const auto& decl : module.structs) {
			if (decl.name != current)
				continue;
			for (const auto& field : decl.fields) {
				// Only user struct types (not vec*, mat*, scalars) are stored
				// as their own decls and need pulling in transitively.
				for (const auto& other : module.structs) {
					if (other.name == field.type) {
						enqueue(field.type);
						break;
					}
				}
			}
			break;
		}
	}
	return reachable;
}

// Emit only the user struct definitions a stage reaches, in pool order to
// preserve the source-defined ordering (struct fields can reference earlier
// structs in the same file).
void emit_user_structs(std::ostringstream& out, const RTArtifactModule& module, const std::unordered_set<std::string>& reachable) {
	for (const auto& decl : module.structs) {
		if (!reachable.contains(decl.name))
			continue;
		out << "struct " << decl.name << " {\n";
		for (const auto& field : decl.fields) {
			out << "    " << rtsl_type_to_hlsl(field.type) << " " << field.name << ";\n";
		}
		out << "};\n\n";
	}
}

void emit_uniforms(std::ostringstream& out, const RTArtifactModule& module) {
	for (const auto& uniform : module.uniforms) {
		if (uniform.type == "Sampler2D") {
			out << "Texture2D " << uniform.name << "_tex : register(t" << uniform.binding << ");\n";
			out << "SamplerState " << uniform.name << "_smp : register(s" << uniform.binding << ");\n";
			continue;
		}
		out << "cbuffer cb_" << uniform.name << " : register(b" << uniform.binding << ") {\n";
		out << "    " << rtsl_type_to_hlsl(uniform.type) << " " << uniform.name << ";\n";
		out << "};\n";
	}
	if (!module.uniforms.empty())
		out << "\n";
}

const char* hlsl_semantic_for(const RTStageField& field, RTStageKind stage, RTStageRole role, std::string& storage, u32 location) {
	if (stage == RTStageKind::Fragment && role == RTStageRole::Output) {
		storage = "SV_Target" + std::to_string(location);
		return storage.c_str();
	}
	if (is_clip_position(field)) {
		return "SV_Position";
	}
	if (field.builtin == "frag_coord")
		return "SV_Position";
	if (field.builtin == "front_facing")
		return "SV_IsFrontFace";
	if (field.builtin == "frag_depth")
		return "SV_Depth";
	return field.name.c_str();
}

void emit_stage_input_struct(std::ostringstream& out, const std::string& struct_name, const RTStageInterface* iface, const std::vector<RTStructDecl>& structs, const std::string& payload_type_name, RTStageKind stage) {
	out << "struct " << struct_name << " {\n";
	if (!iface) {
		out << "};\n\n";
		return;
	}
	// Look up the struct decl for field types.
	const RTStructDecl* decl = nullptr;
	for (const auto& s : structs) {
		if (s.name == payload_type_name) {
			decl = &s;
			break;
		}
	}
	u32 next_location = 0;
	for (const auto& field : iface->fields) {
		if (stage == RTStageKind::Vertex && is_clip_position(field)) {
			// A vertex input can't carry SV_Position; this should never happen
			// for vertex stage inputs (clip is a vertex output, fragment input).
			continue;
		}
		std::string type_str = "float4";
		if (decl) {
			const int idx = find_field_index(*decl, field.name);
			if (idx >= 0)
				type_str = rtsl_type_to_hlsl(decl->fields[idx].type);
		}
		const u32 location = field.has_location ? field.location : next_location;
		std::string storage;
		const char* semantic = hlsl_semantic_for(field, stage, iface->role, storage, location);
		out << "    " << type_str << " " << field.name << " : " << semantic << ";\n";
		++next_location;
	}
	out << "};\n\n";
}

void emit_stage_output_struct(std::ostringstream& out, const std::string& struct_name, const RTStageInterface* iface, const std::vector<RTStructDecl>& structs, const std::string& payload_type_name, RTStageKind stage) {
	out << "struct " << struct_name << " {\n";
	if (!iface) {
		out << "};\n\n";
		return;
	}
	const RTStructDecl* decl = nullptr;
	for (const auto& s : structs) {
		if (s.name == payload_type_name) {
			decl = &s;
			break;
		}
	}
	u32 next_location = 0;
	for (const auto& field : iface->fields) {
		std::string type_str = "float4";
		if (decl) {
			const int idx = find_field_index(*decl, field.name);
			if (idx >= 0)
				type_str = rtsl_type_to_hlsl(decl->fields[idx].type);
		}
		const u32 location = field.has_location ? field.location : next_location;
		std::string storage;
		const char* semantic = hlsl_semantic_for(field, stage, iface->role, storage, location);
		out << "    " << type_str << " " << field.name << " : " << semantic << ";\n";
		++next_location;
	}
	out << "};\n\n";
}

// Returns the HLSL identifier for an SSA result. The map is populated as
// instructions are walked.
struct ExprCtx {
	std::unordered_map<u32, std::string> exprs;

	std::string get(u32 id) const {
		auto it = exprs.find(id);
		if (it == exprs.end())
			return "/* missing id " + std::to_string(id) + " */ (0)";
		return it->second;
	}
	void bind(u32 id, std::string text) { exprs[id] = std::move(text); }
};

std::string ssa_name(u32 id) { return "v_" + std::to_string(id); }

void emit_function_body(std::ostringstream& out, const RTFunction& fn, const TypeRegistry& reg, const RTArtifactModule& module, const RTStageInterface* input_iface, const RTStageInterface* output_iface, const std::string& payload_input_type_name, const std::string& payload_output_type_name, const std::string& output_struct_name, RTStageKind stage) {
	ExprCtx ctx;

	// Bind each uniform's global-variable id to its HLSL name. Pairing is by
	// index: module.global_variables[i] is the OpVariable for module.uniforms[i].
	// Subsequent OpLoads from these ids then resolve to the cbuffer / texture
	// identifier emitted by emit_uniforms.
	for (size_t i = 0; i < module.uniforms.size() && i < module.global_variables.size(); ++i) {
		ctx.bind(module.global_variables[i].result_id, module.uniforms[i].name);
	}

	// Reconstruct the input payload struct from the stage input parameter. The
	// stage-input struct is named "_in" with HLSL field names matching the
	// payload struct, so a flat copy is enough.
	if (!payload_input_type_name.empty() && input_iface && fn.parameter_ids.size() >= 2) {
		out << "    " << payload_input_type_name << " _in = (" << payload_input_type_name << ")0;\n";
		for (const auto& field : input_iface->fields) {
			if (stage == RTStageKind::Vertex && is_clip_position(field))
				continue;
			out << "    _in." << field.name << " = stage_input." << field.name << ";\n";
		}
		ctx.bind(fn.parameter_ids[1], "_in");
	}

	auto bind_inline = [&](const RTInstruction& inst, const std::string& expr) {
		ctx.bind(inst.result_id, ssa_name(inst.result_id));
		out << "    " << hlsl_type(reg, inst.type_id) << " " << ssa_name(inst.result_id) << " = " << expr << ";\n";
	};

	auto operand_text = [&](u32 id) -> std::string {
		if (auto it = reg.constant_exprs.find(id); it != reg.constant_exprs.end())
			return it->second;
		return ctx.get(id);
	};

	for (const auto& inst : fn.body) {
		if (inst.op == RTIROp::Label || inst.op == RTIROp::FunctionParameter)
			continue;
		if (is_type_op(inst.op) || is_constant_op(inst.op))
			continue;

		switch (inst.op) {
		case RTIROp::Return:
			out << "    return;\n";
			break;
		case RTIROp::ReturnValue: {
			if (inst.operands.empty()) {
				out << "    return;\n";
				break;
			}
			const std::string value = operand_text(inst.operands[0]);
			if (output_iface && !payload_output_type_name.empty()) {
				out << "    " << output_struct_name << " stage_output = (" << output_struct_name << ")0;\n";
				for (const auto& field : output_iface->fields) {
					out << "    stage_output." << field.name << " = " << value << "." << field.name << ";\n";
				}
				out << "    return stage_output;\n";
			} else {
				out << "    return " << value << ";\n";
			}
			break;
		}
		case RTIROp::CompositeExtract: {
			if (inst.operands.empty() || inst.literals.empty()) {
				bind_inline(inst, "(0)");
				break;
			}
			const u32 base_id = inst.operands[0];
			const u32 index = inst.literals[0];
			const std::string base = operand_text(base_id);
			// Decide accessor: struct? vector? matrix?
			u32 base_type_id = 0;
			for (const auto& earlier : fn.body) {
				if (earlier.result_id == base_id) {
					base_type_id = earlier.type_id;
					break;
				}
			}
			if (base_type_id == 0) {
				for (const auto& earlier : module.type_constant_pool) {
					if (earlier.result_id == base_id) {
						base_type_id = earlier.type_id;
						break;
					}
				}
			}
			if (auto it = reg.struct_index_by_type_id.find(base_type_id); it != reg.struct_index_by_type_id.end()) {
				const auto& decl = module.structs[it->second];
				if (index < decl.fields.size()) {
					bind_inline(inst, base + "." + decl.fields[index].name);
					break;
				}
			}
			// Vector or matrix or fallback: HLSL allows [] indexing.
			bind_inline(inst, base + "[" + std::to_string(index) + "]");
			break;
		}
		case RTIROp::CompositeConstruct: {
			const std::string ty = hlsl_type(reg, inst.type_id);
			std::ostringstream value;
			if (auto it = reg.struct_index_by_type_id.find(inst.type_id); it != reg.struct_index_by_type_id.end()) {
				// Struct: build a temporary, field-assign positionally.
				const auto& decl = module.structs[it->second];
				out << "    " << ty << " " << ssa_name(inst.result_id) << " = (" << ty << ")0;\n";
				const std::size_t fields = (std::min)(decl.fields.size(), inst.operands.size());
				for (std::size_t i = 0; i < fields; ++i) {
					out << "    " << ssa_name(inst.result_id) << "." << decl.fields[i].name << " = " << operand_text(inst.operands[i]) << ";\n";
				}
				ctx.bind(inst.result_id, ssa_name(inst.result_id));
			} else {
				// Vector / matrix / vector-of-mixed-scalars: HLSL constructor.
				value << ty << "(";
				for (std::size_t i = 0; i < inst.operands.size(); ++i) {
					if (i)
						value << ", ";
					value << operand_text(inst.operands[i]);
				}
				value << ")";
				bind_inline(inst, value.str());
			}
			break;
		}
		case RTIROp::CompositeInsert: {
			if (inst.operands.size() < 2) {
				ctx.bind(inst.result_id, "(0)");
				break;
			}
			const std::string composite = operand_text(inst.operands[0]);
			const std::string value = operand_text(inst.operands[1]);
			u32 base_type_id = 0;
			for (const auto& earlier : fn.body) {
				if (earlier.result_id == inst.operands[0]) {
					base_type_id = earlier.type_id;
					break;
				}
			}
			if (auto it = reg.struct_index_by_type_id.find(base_type_id); it != reg.struct_index_by_type_id.end()) {
				const auto& decl = module.structs[it->second];
				out << "    " << hlsl_type(reg, inst.type_id) << " " << ssa_name(inst.result_id) << " = " << composite << ";\n";
				if (!inst.literals.empty() && inst.literals[0] < decl.fields.size()) {
					out << "    " << ssa_name(inst.result_id) << "." << decl.fields[inst.literals[0]].name << " = " << value << ";\n";
					ctx.bind(inst.result_id, ssa_name(inst.result_id));
					break;
				}
			}
			if (!inst.literals.empty())
				bind_inline(inst, composite + "[" + std::to_string(inst.literals[0]) + "] = " + value);
			else
				bind_inline(inst, composite);
			break;
		}
		case RTIROp::VectorShuffle: {
			if (inst.operands.size() < 2 || inst.literals.empty()) {
				ctx.bind(inst.result_id, "(0)");
				break;
			}
			const std::string lhs = operand_text(inst.operands[0]);
			const std::string rhs = operand_text(inst.operands[1]);
			std::ostringstream value;
			value << hlsl_type(reg, inst.type_id) << "(";
			for (std::size_t i = 0; i < inst.literals.size(); ++i) {
				if (i)
					value << ", ";
				const u32 idx = inst.literals[i];
				if (idx < 4)
					value << lhs << "[" << idx << "]";
				else
					value << rhs << "[" << (idx - 4) << "]";
			}
			value << ")";
			bind_inline(inst, value.str());
			break;
		}
		case RTIROp::Load: {
			if (inst.operands.empty()) {
				ctx.bind(inst.result_id, "(0)");
				break;
			}
			// Forward the source's HLSL name without a typed temporary. HLSL can
			// read cbuffer scalars/matrices directly by name, and opaque
			// resources (Texture2D, SamplerState) cannot be copied into locals
			// at all - they must flow into Sample()/etc. by their register name.
			ctx.bind(inst.result_id, operand_text(inst.operands[0]));
			break;
		}
		case RTIROp::Store: {
			if (inst.operands.size() < 2)
				break;
			out << "    " << operand_text(inst.operands[0]) << " = " << operand_text(inst.operands[1]) << ";\n";
			break;
		}
		case RTIROp::FAdd:
		case RTIROp::IAdd:
			bind_inline(inst, "(" + operand_text(inst.operands[0]) + " + " + operand_text(inst.operands[1]) + ")");
			break;
		case RTIROp::FSub:
		case RTIROp::ISub:
			bind_inline(inst, "(" + operand_text(inst.operands[0]) + " - " + operand_text(inst.operands[1]) + ")");
			break;
		case RTIROp::FMul:
		case RTIROp::IMul:
		case RTIROp::VectorTimesScalar:
		case RTIROp::MatrixTimesScalar:
			bind_inline(inst, "(" + operand_text(inst.operands[0]) + " * " + operand_text(inst.operands[1]) + ")");
			break;
		case RTIROp::FDiv:
		case RTIROp::SDiv:
		case RTIROp::UDiv:
			bind_inline(inst, "(" + operand_text(inst.operands[0]) + " / " + operand_text(inst.operands[1]) + ")");
			break;
		case RTIROp::FNegate:
			bind_inline(inst, "(-" + operand_text(inst.operands[0]) + ")");
			break;
		case RTIROp::MatrixTimesVector:
		case RTIROp::MatrixTimesMatrix:
			bind_inline(inst, "mul(" + operand_text(inst.operands[0]) + ", " + operand_text(inst.operands[1]) + ")");
			break;
		case RTIROp::Dot:
			bind_inline(inst, "dot(" + operand_text(inst.operands[0]) + ", " + operand_text(inst.operands[1]) + ")");
			break;
		case RTIROp::Cross:
			bind_inline(inst, "cross(" + operand_text(inst.operands[0]) + ", " + operand_text(inst.operands[1]) + ")");
			break;
		case RTIROp::FOrdEqual:
		case RTIROp::IEqual:
			bind_inline(inst, "(" + operand_text(inst.operands[0]) + " == " + operand_text(inst.operands[1]) + ")");
			break;
		case RTIROp::FOrdNotEqual:
		case RTIROp::INotEqual:
			bind_inline(inst, "(" + operand_text(inst.operands[0]) + " != " + operand_text(inst.operands[1]) + ")");
			break;
		case RTIROp::FOrdLess:
		case RTIROp::SLess:
		case RTIROp::ULess:
			bind_inline(inst, "(" + operand_text(inst.operands[0]) + " < " + operand_text(inst.operands[1]) + ")");
			break;
		case RTIROp::FOrdLessEqual:
		case RTIROp::SLessEqual:
		case RTIROp::ULessEqual:
			bind_inline(inst, "(" + operand_text(inst.operands[0]) + " <= " + operand_text(inst.operands[1]) + ")");
			break;
		case RTIROp::FOrdGreater:
		case RTIROp::SGreater:
		case RTIROp::UGreater:
			bind_inline(inst, "(" + operand_text(inst.operands[0]) + " > " + operand_text(inst.operands[1]) + ")");
			break;
		case RTIROp::FOrdGreaterEqual:
		case RTIROp::SGreaterEqual:
		case RTIROp::UGreaterEqual:
			bind_inline(inst, "(" + operand_text(inst.operands[0]) + " >= " + operand_text(inst.operands[1]) + ")");
			break;
		case RTIROp::LogicalAnd:
			bind_inline(inst, "(" + operand_text(inst.operands[0]) + " && " + operand_text(inst.operands[1]) + ")");
			break;
		case RTIROp::LogicalOr:
			bind_inline(inst, "(" + operand_text(inst.operands[0]) + " || " + operand_text(inst.operands[1]) + ")");
			break;
		case RTIROp::LogicalNot:
			bind_inline(inst, "(!" + operand_text(inst.operands[0]) + ")");
			break;
		case RTIROp::ConvertFToU:
		case RTIROp::ConvertFToS:
		case RTIROp::ConvertSToF:
		case RTIROp::ConvertUToF:
		case RTIROp::Bitcast:
			bind_inline(inst, "(" + hlsl_type(reg, inst.type_id) + ")(" + operand_text(inst.operands[0]) + ")");
			break;
		case RTIROp::ImageSampleImplicitLod: {
			if (inst.operands.size() < 2) {
				bind_inline(inst, "float4(0,0,0,0)");
				break;
			}
			// Operand 0 is a Sampler2D resource (Texture2D + SamplerState pair in HLSL).
			const std::string tex = operand_text(inst.operands[0]) + "_tex";
			const std::string smp = operand_text(inst.operands[0]) + "_smp";
			bind_inline(inst, tex + ".Sample(" + smp + ", " + operand_text(inst.operands[1]) + ")");
			break;
		}
		case RTIROp::ReadInput:
		case RTIROp::WriteOutput:
		case RTIROp::ReadBuiltin:
		case RTIROp::WriteBuiltin:
			throw std::runtime_error("rtslp HLSL emit: stage I/O primitive should not reach DX12 codegen");
		default:
			if (inst.result_id && inst.type_id) {
				throw std::runtime_error("rtslp HLSL emit: unsupported op " + std::to_string(static_cast<u32>(inst.op)));
			}
			break;
		}
	}
}

std::string emit_stage(const RTArtifactModule& module, RTStageKind stage, const TypeRegistry& reg) {
	const RTFunction* fn = find_stage_function(module, stage);
	if (!fn) {
		throw std::runtime_error(stage == RTStageKind::Vertex ? "rtslp is missing a vertex stage" : "rtslp is missing a fragment stage");
	}

	std::ostringstream out;

	// Input payload type: parameter 1's type id (parameter 0 is the globals
	// carrier and currently has no body emission).
	const u32 input_type_id = stage_input_struct_type(*fn);
	const u32 output_type_id = fn->return_type_id;

	std::string input_payload_name;
	if (auto it = reg.type_names.find(input_type_id); it != reg.type_names.end())
		input_payload_name = it->second;
	std::string output_payload_name;
	if (auto it = reg.type_names.find(output_type_id); it != reg.type_names.end())
		output_payload_name = it->second;

	// Emit only the user structs this stage actually reaches via its input or
	// output payload, then uniforms.
	emit_user_structs(out, module, reachable_structs(module, input_payload_name, output_payload_name));
	emit_uniforms(out, module);

	const RTStageInterface* input_iface = input_payload_name.empty() ? nullptr : find_interface(module, input_payload_name, stage == RTStageKind::Vertex ? RTStageRole::Input : RTStageRole::Varying);
	const RTStageInterface* output_iface = output_payload_name.empty() ? nullptr : find_interface(module, output_payload_name, stage == RTStageKind::Fragment ? RTStageRole::Output : RTStageRole::Varying);

	const std::string input_struct_name = (stage == RTStageKind::Vertex ? "VS_Input" : "PS_Input");
	const std::string output_struct_name = (stage == RTStageKind::Vertex ? "VS_Output" : "PS_Output");

	emit_stage_input_struct(out, input_struct_name, input_iface, module.structs, input_payload_name, stage);
	emit_stage_output_struct(out, output_struct_name, output_iface, module.structs, output_payload_name, stage);

	out << output_struct_name << " main(" << input_struct_name << " stage_input) {\n";
	emit_function_body(out, *fn, reg, module, input_iface, output_iface, input_payload_name, output_payload_name, output_struct_name, stage);
	out << "}\n";

	return out.str();
}

char* dup_string(const std::string& s, u64* size_out) {
	char* buf = static_cast<char*>(std::malloc(s.size() + 1));
	if (!buf)
		throw std::bad_alloc();
	std::memcpy(buf, s.data(), s.size());
	buf[s.size()] = '\0';
	*size_out = s.size();
	return buf;
}

void populate_uniforms(const RTArtifactModule& module, rtdx_graphics_hlsl_compile_result& result) {
	if (module.uniforms.empty())
		return;
	result.uniforms = static_cast<rtsl_hlsl_uniform_info*>(
		std::calloc(module.uniforms.size(), sizeof(rtsl_hlsl_uniform_info))
	);
	if (!result.uniforms)
		throw std::bad_alloc();
	u32 count = 0;
	for (const auto& uniform : module.uniforms) {
		auto& info = result.uniforms[count++];
		std::snprintf(info.name, sizeof(info.name), "%s", uniform.name.c_str());
		info.binding = uniform.binding;
		info.kind = (uniform.type == "Sampler2D")
			? RTSL_HLSL_UNIFORM_TEXTURE_SAMPLED
			: RTSL_HLSL_UNIFORM_BUFFER;
	}
	result.uniform_count = count;
}

} // namespace

extern "C" void rtdx_graphics_hlsl_result_clear(rtdx_graphics_hlsl_compile_result* result) {
	if (!result)
		return;
	std::free(result->vertex_hlsl);
	std::free(result->fragment_hlsl);
	std::free(result->uniforms);
	*result = {};
}

extern "C" rtdx_graphics_hlsl_compile_result rtdx_shader_compile_graphics_rtslp(
	u64 program_size,
	const void* program_source
) {
	rtdx_graphics_hlsl_compile_result result = {};
	const RTArtifactModule module = rt::read_rtslp_module(program_size, program_source);
	TypeRegistry reg;
	build_type_registry(module, reg);
	const std::string vs = emit_stage(module, RTStageKind::Vertex, reg);
	const std::string fs = emit_stage(module, RTStageKind::Fragment, reg);
	result.vertex_hlsl = dup_string(vs, &result.vertex_hlsl_size);
	try {
		result.fragment_hlsl = dup_string(fs, &result.fragment_hlsl_size);
	} catch (...) {
		std::free(result.vertex_hlsl);
		throw;
	}
	try {
		populate_uniforms(module, result);
	} catch (...) {
		std::free(result.vertex_hlsl);
		std::free(result.fragment_hlsl);
		throw;
	}
	return result;
}
