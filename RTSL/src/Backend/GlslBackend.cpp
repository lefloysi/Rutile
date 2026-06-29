#include "Backend/GlslBackend.hpp"

#include "IR/StageBuiltins.hpp"
#include "IR/UniformLowering.hpp"
#include "Mangle/Mangler.hpp"

#include <algorithm>
#include <array>
#include <iterator>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>

namespace rtsl {

namespace {

bool is_identifier_char(char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_';
}

bool is_identifier_start(char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_';
}

// Replace whole-identifier occurrences of `from` with `to` (so "in" does not
// match inside "in_position" or "__rt_read_input").
std::string replace_word(std::string text, std::string_view from, std::string_view to) {
    std::size_t pos = 0;
    while ((pos = text.find(from, pos)) != std::string::npos) {
        const bool left_ok = pos == 0 || !is_identifier_char(text[pos - 1]);
        const std::size_t after = pos + from.size();
        const bool right_ok = after >= text.size() || !is_identifier_char(text[after]);
        if (left_ok && right_ok) {
            text.replace(pos, from.size(), to);
            pos += to.size();
        } else {
            pos += from.size();
        }
    }
    return text;
}

// Map RTSL scalar spellings to GLSL. Vectors/matrices already share spelling.
std::string glsl_type(std::string_view type) {
    static constexpr std::array mappings{
        std::pair{std::string_view{"f32"}, std::string_view{"float"}},
        std::pair{std::string_view{"f64"}, std::string_view{"double"}},
        std::pair{std::string_view{"i32"}, std::string_view{"int"}},
        std::pair{std::string_view{"u32"}, std::string_view{"uint"}},
        std::pair{std::string_view{"Sampler2D"}, std::string_view{"sampler2D"}},
    };
    for (const auto& [from, to] : mappings) {
        if (type == from) return std::string(to);
    }
    return std::string(type);
}

// Rewrite RTSL scalar type spellings appearing as whole words inside a rendered
// statement (e.g. "f32 z = ...").
std::string map_scalar_words(std::string text) {
    static constexpr std::array mappings{
        std::pair{std::string_view{"f32"}, std::string_view{"float"}},
        std::pair{std::string_view{"f64"}, std::string_view{"double"}},
        std::pair{std::string_view{"i32"}, std::string_view{"int"}},
        std::pair{std::string_view{"u32"}, std::string_view{"uint"}},
        std::pair{std::string_view{"sample"}, std::string_view{"texture"}},
    };
    for (const auto& [from, to] : mappings) {
        text = replace_word(std::move(text), from, to);
    }
    return text;
}

const StructDecl* find_struct(const std::vector<StructDecl>& structs, std::string_view name) {
    for (const auto& s : structs) {
        if (s.name == name) return &s;
    }
    return nullptr;
}

std::string find_field_type(const std::vector<StructDecl>& structs, std::string_view type, std::string_view field) {
    if (const auto* s = find_struct(structs, type)) {
        for (const auto& f : s->fields) {
            if (f.name == field) return f.type;
        }
        throw std::runtime_error("unknown field '" + std::string(field) + "' on struct '" + std::string(type) + "'");
    }
    throw std::runtime_error("unknown struct '" + std::string(type) + "'");
}

const StageInterface* find_interface(const std::vector<StageInterface>& interfaces, std::string_view type) {
    for (const auto& i : interfaces) {
        if (i.type_name == type) return &i;
    }
    return nullptr;
}

const StageInterface* find_interface_by_role(const std::vector<StageInterface>& interfaces, StageRole role) {
    for (const auto& i : interfaces) {
        if (i.role == role) return &i;
    }
    return nullptr;
}

std::string field_at_location(const StageInterface* interface, u32 location) {
    if (interface) {
        for (const auto& f : interface->fields) {
            if (f.has_location && f.location == location) return f.name;
        }
    }
    return {};
}

const StageIOField* find_builtin_field(const StageInterface* interface, std::string_view builtin) {
    if (!interface) {
        return nullptr;
    }
    for (const auto& field : interface->fields) {
        if (field.builtin == builtin) {
            return &field;
        }
    }
    return nullptr;
}

std::string interpolation_qualifier(const StageIOField& field) {
    static constexpr std::array mappings{
        std::pair{std::string_view{"flat"}, std::string_view{"flat "}},
        std::pair{std::string_view{"smooth"}, std::string_view{""}},
    };
    for (const auto& [from, to] : mappings) {
        if (field.interpolation == from) return std::string(to);
    }
    return {};
}

std::string member_owner(std::string_view name) {
    const auto scope = name.find("::");
    return scope == std::string_view::npos ? std::string{} : std::string(name.substr(0, scope));
}

bool is_constructor(const IRFunction &function) {
    const auto owner = member_owner(function.name);
    if (owner.empty()) return false;
    const auto scope = function.name.find("::");
    return scope != std::string::npos && function.name.substr(scope + 2) == owner;
}

// The mangled name of a struct's constructor (first one found), or "".
std::string constructor_symbol(const Artifact &artifact, std::string_view type) {
    for (std::size_t i = 0; i < artifact.functions.size(); ++i) {
        const auto &f = artifact.functions[i];
        if (!is_constructor(f) || member_owner(f.name) != type) {
            continue;
        }
        return "func_" + std::to_string(i);
    }
    return {};
}

// Rewrite source-style constructor calls `Type(args)` into calls to the actual
// constructor function. GLSL has no struct-constructor syntax, so `Vertex(p)`
// must become a call to the emitted `_ZN6Vertex6VertexE5Point` function.
std::string rewrite_constructor_calls(std::string text, const Artifact &artifact) {
    std::string out;
    std::size_t i = 0;
    while (i < text.size()) {
        if (is_identifier_start(text[i])) {
            std::size_t j = i;
            while (j < text.size() && is_identifier_char(text[j])) ++j;
            std::string id = text.substr(i, j - i);
            if (j < text.size() && text[j] == '(') {
                const auto symbol = constructor_symbol(artifact, id);
                if (!symbol.empty()) {
                    out += symbol;
                    i = j;
                    continue;
                }
            }
            out += id;
            i = j;
            continue;
        }
        out += text[i++];
    }
    return out;
}

const char *stage_extension(StageKind stage) {
    switch (stage) {
    case StageKind::vertex: return ".vert";
    case StageKind::fragment: return ".frag";
    case StageKind::none: return ".glsl";
    }
    return ".glsl";
}

struct StageGlobalField {
    StageKind stage;
    std::string_view type;
    std::string_view member;
    std::string_view glsl_name;
    bool readable = false;
    bool writable = false;
};

constexpr std::array StageGlobalFields{
    StageGlobalField{StageKind::vertex, "float", "point_size", "gl_PointSize", true, true},
    StageGlobalField{StageKind::vertex, "int", "vertex_index", "gl_VertexIndex", true, false},
    StageGlobalField{StageKind::vertex, "int", "instance_index", "gl_InstanceIndex", true, false},
    StageGlobalField{StageKind::fragment, "vec4", "frag_coord", "gl_FragCoord", true, false},
    StageGlobalField{StageKind::fragment, "bool", "front_facing", "gl_FrontFacing", true, false},
    StageGlobalField{StageKind::fragment, "float", "frag_depth", "gl_FragDepth", false, true},
};

// Resolve a builtin slot tag to its GLSL global.
std::string builtin_to_glsl(std::string_view slot) {
    if (slot == "position") return "gl_Position";
    for (const auto& field : StageGlobalFields) {
        if (slot == field.member) return std::string(field.glsl_name);
    }
    throw std::runtime_error("unknown builtin slot '" + std::string(slot) + "'");
}

// Leading identifier of an lhs expression (the assignment target's first name).
std::string leading_identifier(std::string_view text) {
    std::size_t i = 0;
    while (i < text.size() && is_identifier_char(text[i])) ++i;
    return std::string(text.substr(0, i));
}

u32 parse_index(std::string_view text) {
    u32 value = 0;
    bool seen = false;
    for (const char c : text) {
        if (c >= '0' && c <= '9') {
            value = value * 10 + static_cast<u32>(c - '0');
            seen = true;
        } else if (seen) {
            break;
        }
    }
    return value;
}

std::string between_quotes(std::string_view text) {
    const auto open = text.find('"');
    if (open == std::string_view::npos) return {};
    const auto close = text.find('"', open + 1);
    if (close == std::string_view::npos) return {};
    return std::string(text.substr(open + 1, close - open - 1));
}

std::string replace_identifier(std::string text, std::string_view from, std::string_view to) {
    if (from.empty() || from == to) {
        return text;
    }
    std::size_t pos = 0;
    while ((pos = text.find(from, pos)) != std::string::npos) {
        const bool left_ok = pos == 0 || !is_identifier_char(text[pos - 1]);
        const auto after = pos + from.size();
        const bool right_ok = after >= text.size() || !is_identifier_char(text[after]);
        if (left_ok && right_ok) {
            text.replace(pos, from.size(), to);
            pos += to.size();
        } else {
            pos += from.size();
        }
    }
    return text;
}

bool is_globals_type(std::string_view type) {
    return type == "vert_globals" || type == "frag_globals" || type == "comp_globals" || type == "globals";
}

void emit_structs(std::ostringstream &out, const Artifact &artifact) {
    for (const auto &s : artifact.structs) {
        out << "struct " << s.name << " {\n";
        for (const auto &field : s.fields) {
            out << "    " << glsl_type(field.type) << " " << field.name << ";\n";
        }
        out << "};\n\n";
    }
}

void emit_uniforms(std::ostringstream &out, const Artifact &artifact) {
    for (const auto &uniform : artifact.uniforms) {
        const auto binding = uniform_binding_name(uniform);
        out << "layout(set = " << uniform.set << ", binding = " << uniform.binding << ") ";
        if (is_resource_uniform_type(uniform.type)) {
            if (!uniform.access.empty()) out << uniform.access << " ";
            out << "uniform " << glsl_type(uniform.type) << " " << binding << ";\n";
        } else {
            out << "uniform " << uniform_block_name(uniform) << " {\n";
            if (!uniform.inline_fields.empty()) {
                for (const auto &f : uniform.inline_fields) {
                    out << "    " << glsl_type(f.type) << " " << f.name << ";\n";
                }
            } else {
                out << "    " << glsl_type(uniform.type) << " value;\n";
            }
            out << "} " << binding << ";\n";
        }
    }
    if (!artifact.uniforms.empty()) out << "\n";
}

// Emit the user (non-generated) functions shared by every stage file.
void emit_user_functions(std::ostringstream &out, const Artifact &artifact) {
    for (std::size_t fi = 0; fi < artifact.functions.size(); ++fi) {
        const auto &function = artifact.functions[fi];
        if (function.generated) continue;

        const auto owner = member_owner(function.name);
        const bool constructor = is_constructor(function);
        const auto name = "func_" + std::to_string(fi);

        std::vector<std::pair<std::string, std::string>> renames;
        renames.reserve(function.parameters.size());
        for (std::size_t i = 0; i < function.parameters.size(); ++i) {
            if (!function.parameters[i].name.empty()) {
                renames.emplace_back(function.parameters[i].name,
                                     is_globals_type(function.parameters[i].type) ? "g" : "p" + std::to_string(i));
            }
        }

        out << (constructor ? owner : glsl_type(function.return_type.empty() ? "void" : function.return_type))
            << " " << name << "(";
        for (std::size_t i = 0; i < function.parameters.size(); ++i) {
            if (i) out << ", ";
            const std::string param_name = is_globals_type(function.parameters[i].type)
                                               ? "g"
                                               : "p" + std::to_string(i);
            // Builtin carriers are passed by reference; GLSL expresses that as an
            // inout parameter (copy-in/copy-out).
            if (is_stage_builtin_carrier(function.parameters[i].type)) {
                out << "inout ";
            }
            out << glsl_type(function.parameters[i].type) << " " << param_name;
        }
        out << ") {\n";
        if (constructor) {
            out << "    " << owner << " _self;\n";
        }

        const StructDecl *owner_struct = constructor ? find_struct(artifact.structs, owner) : nullptr;
        for (const auto &op : function.body_ops) {
            std::string line;
            switch (op.kind) {
            case IRFunction::BodyOpKind::ret:
                line = "return " + op.value + ";";
                break;
            case IRFunction::BodyOpKind::assign:
            case IRFunction::BodyOpKind::compound_assign: {
                std::string lhs = op.name;
                // Constructor member writes target the result aggregate.
                if (owner_struct) {
                    const auto lead = leading_identifier(lhs);
                    for (const auto &f : owner_struct->fields) {
                        if (f.name == lead) {
                            lhs = "_self." + lhs;
                            break;
                        }
                    }
                }
                const std::string assign_op =
                    op.kind == IRFunction::BodyOpKind::compound_assign ? (" " + op.op + "= ") : " = ";
                line = lhs + assign_op + op.value + ";";
                break;
            }
            case IRFunction::BodyOpKind::call: {
                line = op.callee + "(";
                for (std::size_t i = 0; i < op.args.size(); ++i) {
                    if (i) line += ", ";
                    line += op.args[i];
                }
                line += ");";
                break;
            }
            default:
                line = op.value + ";";
                break;
            }
            for (const auto &[from, to] : renames) {
                line = replace_identifier(std::move(line), from, to);
            }
            line = rewrite_constructor_calls(std::move(line), artifact);
            out << "    " << map_scalar_words(std::move(line)) << "\n";
        }
        if (constructor) {
            out << "    return _self;\n";
        }
        out << "}\n\n";
    }
}

// Find the user function a stage wrapper calls, so we can recover its input and
// output payload types from the signature.
const IRFunction *find_called_function(const Artifact &artifact, const IRFunction &wrapper, const Mangler &mangler) {
    for (const auto &op : wrapper.body_ops) {
        const std::string &value = op.kind == IRFunction::BodyOpKind::call ? op.callee : op.value;
        if (value.find("__rt_") != std::string::npos) continue;
        const auto paren = value.find('(');
        if (paren == std::string::npos) continue;
        const std::string callee = value.substr(0, paren);
        for (const auto &candidate : artifact.functions) {
            if (!candidate.generated && mangler.mangle_rtsl(candidate) == callee) {
                return &candidate;
            }
        }
    }
    return nullptr;
}

std::string translate_wrapper_op(const IRFunction::BodyOp &op, const StageInterface *incoming,
                                 const StageInterface *outgoing) {
    auto rename_locals = [](std::string text) {
        text = replace_word(std::move(text), "in", "_in");
        text = replace_word(std::move(text), "out", "_out");
        text = replace_word(std::move(text), "bi", "_bi");
        return text;
    };

    // A builtin copy-in: name = "bi.member", value = __rt_read_builtin("gl_X").
    if (op.kind == IRFunction::BodyOpKind::assign && op.value.rfind("__rt_read_builtin(", 0) == 0) {
        return rename_locals(op.name) + " = " + builtin_to_glsl(between_quotes(op.value)) + ";";
    }
    // An input read: name = "in.field", value = __rt_read_input(N).
    if (op.kind == IRFunction::BodyOpKind::assign && op.value.rfind("__rt_read_input(", 0) == 0) {
        const auto field = field_at_location(incoming, parse_index(op.value));
        return rename_locals(op.name) + " = in_" + field + ";";
    }
    // A built-in copy-out: name = __rt_write_builtin("gl_X"), value = bi.member / out.field.
    if (op.kind == IRFunction::BodyOpKind::assign && op.name.rfind("__rt_write_builtin(", 0) == 0) {
        return builtin_to_glsl(between_quotes(op.name)) + " = " + rename_locals(op.value) + ";";
    }
    // A location write: name = __rt_write_output(N), value = out.field.
    if (op.kind == IRFunction::BodyOpKind::assign && op.name.rfind("__rt_write_output(", 0) == 0) {
        const auto field = field_at_location(outgoing, parse_index(op.name));
        return "out_" + field + " = " + rename_locals(op.value) + ";";
    }
    if (op.kind == IRFunction::BodyOpKind::assign) {
        return rename_locals(op.name) + " = " + rename_locals(op.value) + ";";
    }
    return rename_locals(op.value) + ";";
}

std::string globals_type(StageKind stage) {
    switch (stage) {
    case StageKind::vertex: return "vert_globals";
    case StageKind::fragment: return "frag_globals";
    case StageKind::none: return "globals";
    }
    return "globals";
}

// Emit the builtin carrier struct definitions referenced by any stage.
void emit_carrier_structs(std::ostringstream &out, const Artifact &artifact) {
    std::vector<std::string> seen;
    for (const auto &function : artifact.functions) {
        for (const auto &parameter : function.parameters) {
            if (!is_stage_builtin_carrier(parameter.type) ||
                std::find(seen.begin(), seen.end(), parameter.type) != seen.end()) {
                continue;
            }
            seen.push_back(parameter.type);
            out << "struct " << parameter.type << " {\n";
            for (const auto &builtin : stage_builtins(parameter.type)) {
                out << "    " << glsl_type(builtin.type) << " " << builtin.member << ";  // " << builtin.gl_name
                    << (builtin.is_output ? " (out)" : " (in)") << "\n";
            }
            out << "};\n\n";
        }
    }
}

void emit_stage_globals(std::ostringstream &out, StageKind stage) {
    const auto type = globals_type(stage);
    out << "struct " << type << " {\n";
    for (const auto& field : StageGlobalFields) {
        if (field.stage == stage) {
            out << "    " << field.type << " " << field.member << ";\n";
        }
    }
    out << "};\n\n";
}

// Emit empty placeholder structs for the OTHER stages' globals types so user
// functions like `frag_main(frag_globals g, ...)` still compile when included
// in a file for a different stage. Only the active stage's struct gets the
// real fields; the others are declared so the type name resolves.
void emit_foreign_stage_globals(std::ostringstream &out, StageKind active) {
    for (auto stage : {StageKind::vertex, StageKind::fragment}) {
        if (stage == active) continue;
        out << "struct " << globals_type(stage) << " {\n";
        for (const auto& field : StageGlobalFields) {
            if (field.stage == stage) {
                out << "    " << field.type << " " << field.member << ";\n";
            }
        }
        out << "};\n\n";
    }
}

void emit_globals_copy_in(std::ostringstream &out, StageKind stage) {
    for (const auto& field : StageGlobalFields) {
        if (field.stage == stage && field.readable) {
            out << "    _g." << field.member << " = " << field.glsl_name << ";\n";
        }
    }
}

void emit_globals_copy_out(std::ostringstream &out, StageKind stage) {
    for (const auto& field : StageGlobalFields) {
        if (field.stage == stage && field.writable) {
            out << "    " << field.glsl_name << " = _g." << field.member << ";\n";
        }
    }
}

GlslStageFile emit_stage(const Artifact &artifact, const IRFunction &wrapper, const Mangler &mangler) {
    GlslStageFile file{.stage = wrapper.stage, .extension = stage_extension(wrapper.stage)};
    std::ostringstream out;

    out << "#version 450\n";
    out << "#extension GL_KHR_vulkan_glsl : enable\n";

    emit_structs(out, artifact);
    emit_stage_globals(out, wrapper.stage);
    emit_foreign_stage_globals(out, wrapper.stage);
    emit_carrier_structs(out, artifact);
    emit_uniforms(out, artifact);

    const IRFunction *user = find_called_function(artifact, wrapper, mangler);
    std::size_t user_index = 0;
    if (user) {
        user_index = static_cast<std::size_t>(std::distance(artifact.functions.begin(),
            std::find_if(artifact.functions.begin(), artifact.functions.end(),
                [&](const IRFunction &f) { return &f == user; })));
    }
    // The builtin carrier is a reference parameter of a carrier type; the input
    // payload is the first non-carrier parameter.
    std::string input_type;
    if (user) {
        for (const auto &parameter : user->parameters) {
            if (is_stage_builtin_carrier(parameter.type)) {
                continue;
            }
            if (parameter.type == globals_type(wrapper.stage)) {
                continue;
            }
            if (input_type.empty()) {
                input_type = parameter.type;
            }
        }
    }
    const std::string output_type = user ? user->return_type : std::string{};
    const StageInterface *incoming = input_type.empty() ? nullptr : find_interface(artifact.stage_interfaces, input_type);
    const StageInterface *outgoing =
        output_type.empty() || output_type == "void" ? nullptr : find_interface(artifact.stage_interfaces, output_type);
    if (!incoming) {
        incoming = wrapper.stage == StageKind::vertex ? find_interface_by_role(artifact.stage_interfaces, StageRole::input)
                                                      : find_interface_by_role(artifact.stage_interfaces, StageRole::varying);
    }
    if (!outgoing) {
        outgoing = wrapper.stage == StageKind::fragment ? find_interface_by_role(artifact.stage_interfaces, StageRole::output)
                                                        : find_interface_by_role(artifact.stage_interfaces, StageRole::varying);
    }
    if (input_type.empty() && incoming) input_type = incoming->type_name;

    // Stage inputs.
    if (incoming) {
        for (const auto &field : incoming->fields) {
            if (!field.builtin.empty()) continue;
            out << "layout(location = " << field.location << ") ";
            out << interpolation_qualifier(field);
            out << "in " << glsl_type(find_field_type(artifact.structs, input_type, field.name)) << " in_" << field.name
                << ";\n";
        }
    }
    // Stage outputs (clip/builtin fields route to gl_* and need no declaration).
    if (outgoing) {
        for (const auto &field : outgoing->fields) {
            if (!field.builtin.empty()) continue;
            out << "layout(location = " << field.location << ") ";
            out << interpolation_qualifier(field);
            out << "out " << glsl_type(find_field_type(artifact.structs, output_type, field.name)) << " out_"
                << field.name << ";\n";
        }
    }
    out << "\n";

    emit_user_functions(out, artifact);

    out << "void main() {\n";
    out << "    " << globals_type(wrapper.stage) << " _g;\n";
    emit_globals_copy_in(out, wrapper.stage);
    if (incoming && !input_type.empty()) {
        out << "    " << input_type << " _in;\n";
        for (const auto &field : incoming->fields) {
            if (field.builtin.empty()) {
                out << "    _in." << field.name << " = in_" << field.name << ";\n";
            }
        }
    }
    out << "    ";
    if (user && user->return_type != "void") {
        out << user->return_type << " _out = ";
    }
    out << "func_" << user_index << "(_g";
    if (incoming && !input_type.empty()) out << ", _in";
    out << ");\n";
    if (outgoing && user && user->return_type != "void") {
        for (const auto &field : outgoing->fields) {
            if (!field.builtin.empty()) continue;
            out << "    out_" << field.name << " = _out." << field.name << ";\n";
        }
    }
    emit_globals_copy_out(out, wrapper.stage);
    if (user && user->return_type != "void" && wrapper.stage == StageKind::vertex) {
        const auto* clip_field = find_builtin_field(outgoing, "position");
        if (!clip_field) {
            throw std::runtime_error("vertex output payload is missing a clip position field");
        }
        out << "    gl_Position = _out." << clip_field->name << ";\n";
    }
    out << "}\n";

    file.source = out.str();
    return file;
}

} // namespace

std::vector<GlslStageFile> emit_vulkan_glsl(const Artifact &artifact) {
    const Mangler mangler;
    std::vector<GlslStageFile> files;
    for (const auto &function : artifact.functions) {
        if (function.stage != StageKind::none) {
            files.push_back(emit_stage(artifact, function, mangler));
        }
    }
    return files;
}

} // namespace rtsl
