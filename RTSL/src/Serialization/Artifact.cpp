#include "Serialization/Artifact.hpp"

#include "AST/AST.hpp"

#include <cstring>
#include <optional>
#include <unordered_map>

namespace rtsl {

namespace {

constexpr u32 Magic = 0x4c535452;
constexpr u16 VersionMajor = 0;
constexpr u16 VersionMinor = 1;
constexpr u32 HeaderSize = 48;
constexpr u32 SectionEntrySize = 32;

enum class SectionKind : u32 {
    string_table = 1,
    ir_module = 2,
    decoration_table = 3,
    struct_table = 4,
    resource_table = 5,
    stage_interface_table = 6,
    entry_table = 7,
    debug_table = 8,
};

struct Section {
    SectionKind kind;
    std::vector<u8> bytes;
};

struct StringPool {
    std::vector<std::string> values;
    std::unordered_map<std::string, u32> ids;

    u32 intern(std::string_view value) {
        const auto it = ids.find(std::string(value));
        if (it != ids.end()) {
            return it->second;
        }
        const u32 id = static_cast<u32>(values.size());
        values.emplace_back(value);
        ids.emplace(values.back(), id);
        return id;
    }
};

void append_u8(std::vector<u8> &out, u8 value) { out.push_back(value); }
void append_u16(std::vector<u8> &out, u16 value) {
    out.push_back(static_cast<u8>(value & 0xff));
    out.push_back(static_cast<u8>((value >> 8) & 0xff));
}
void append_u32(std::vector<u8> &out, u32 value) {
    for (int i = 0; i < 4; ++i) out.push_back(static_cast<u8>((value >> (i * 8)) & 0xff));
}
void append_u64(std::vector<u8> &out, u64 value) {
    for (int i = 0; i < 8; ++i) out.push_back(static_cast<u8>((value >> (i * 8)) & 0xff));
}

u16 read_u16(std::span<const u8> data, std::size_t offset) {
    return static_cast<u16>(data[offset] | (data[offset + 1] << 8));
}
u32 read_u32(std::span<const u8> data, std::size_t offset) {
    return static_cast<u32>(data[offset]) |
           (static_cast<u32>(data[offset + 1]) << 8) |
           (static_cast<u32>(data[offset + 2]) << 16) |
           (static_cast<u32>(data[offset + 3]) << 24);
}
u64 read_u64(std::span<const u8> data, std::size_t offset) {
    u64 value = 0;
    for (int i = 0; i < 8; ++i) value |= static_cast<u64>(data[offset + i]) << (i * 8);
    return value;
}

void append_string(std::vector<u8> &out, std::string_view text) {
    append_u32(out, static_cast<u32>(text.size()));
    out.insert(out.end(), text.begin(), text.end());
}

std::optional<std::string> read_string(std::span<const u8> data, std::size_t &cursor) {
    if (cursor + 4 > data.size()) return std::nullopt;
    const auto length = read_u32(data, cursor);
    cursor += 4;
    if (cursor + length > data.size()) return std::nullopt;
    std::string value(reinterpret_cast<const char *>(data.data() + cursor), length);
    cursor += length;
    return value;
}

StringPool build_string_pool(const IRModule &module, std::span<const Artifact::EntryPoint> entries) {
    StringPool pool;
    pool.intern(module.source_name);
    for (const auto &uniform : module.uniforms) {
        pool.intern(uniform.scope_name);
        pool.intern(uniform.name);
        pool.intern(uniform.type);
        pool.intern(uniform.access);
        for (const auto &field : uniform.inline_fields) {
            pool.intern(field.type);
            pool.intern(field.name);
        }
    }
    for (const auto &interface : module.stage_interfaces) {
        pool.intern(interface.type_name);
        for (const auto &field : interface.fields) {
            pool.intern(field.name);
            pool.intern(field.interpolation);
            pool.intern(field.builtin);
        }
    }
    for (const auto &decl : module.structs) {
        pool.intern(decl.name);
        for (const auto &field : decl.fields) {
            pool.intern(field.type);
            pool.intern(field.name);
        }
    }
    for (const auto &entry : entries) {
        pool.intern(entry.name);
        pool.intern(entry.mangled_name);
    }
    return pool;
}

Section make_string_table(const StringPool &pool) {
    Section section{.kind = SectionKind::string_table};
    append_u32(section.bytes, static_cast<u32>(pool.values.size()));
    for (const auto &value : pool.values) append_string(section.bytes, value);
    return section;
}

void write_instruction(std::vector<u8> &out, const IRInstruction &inst) {
    append_u16(out, static_cast<u16>(inst.op));
    append_u32(out, inst.result_id);
    append_u32(out, inst.type_id);
    append_u32(out, static_cast<u32>(inst.operands.size()));
    append_u32(out, static_cast<u32>(inst.literals.size()));
    append_u32(out, inst.loc.file_id);
    append_u32(out, inst.loc.line);
    append_u32(out, inst.loc.column);
    for (IRId operand : inst.operands) append_u32(out, operand);
    for (u32 literal : inst.literals) append_u32(out, literal);
}

std::optional<IRInstruction> read_instruction(std::span<const u8> data, std::size_t &cursor) {
    if (cursor + 26 > data.size()) return std::nullopt;
    IRInstruction inst;
    inst.op = static_cast<IROp>(read_u16(data, cursor));
    cursor += 2;
    inst.result_id = read_u32(data, cursor); cursor += 4;
    inst.type_id = read_u32(data, cursor); cursor += 4;
    const u32 operand_count = read_u32(data, cursor); cursor += 4;
    const u32 literal_count = read_u32(data, cursor); cursor += 4;
    inst.loc.file_id = read_u32(data, cursor); cursor += 4;
    inst.loc.line = read_u32(data, cursor); cursor += 4;
    inst.loc.column = read_u32(data, cursor); cursor += 4;
    if (cursor + static_cast<std::size_t>(operand_count + literal_count) * 4 > data.size()) return std::nullopt;
    inst.operands.reserve(operand_count);
    inst.literals.reserve(literal_count);
    for (u32 i = 0; i < operand_count; ++i) {
        inst.operands.push_back(read_u32(data, cursor));
        cursor += 4;
    }
    for (u32 i = 0; i < literal_count; ++i) {
        inst.literals.push_back(read_u32(data, cursor));
        cursor += 4;
    }
    return inst;
}

Section make_ir_module_section(const IRModule &module) {
    Section section{.kind = SectionKind::ir_module};
    append_u32(section.bytes, module.next_id);
    append_u32(section.bytes, static_cast<u32>(module.type_constant_pool.size()));
    for (const auto &inst : module.type_constant_pool) write_instruction(section.bytes, inst);
    append_u32(section.bytes, static_cast<u32>(module.global_variables.size()));
    for (const auto &inst : module.global_variables) write_instruction(section.bytes, inst);
    append_u32(section.bytes, static_cast<u32>(module.functions.size()));
    for (const auto &fn : module.functions) {
        append_u32(section.bytes, fn.result_id);
        append_u32(section.bytes, fn.function_type_id);
        append_u32(section.bytes, fn.return_type_id);
        append_u8(section.bytes, static_cast<u8>(fn.stage));
        append_u8(section.bytes, fn.generated ? 1 : 0);
        append_u8(section.bytes, fn.exported ? 1 : 0);
        append_u32(section.bytes, fn.display_name.value);
        append_u32(section.bytes, fn.mangled_name.value);
        // Source-level identity used by the linker's cross-module call
        // resolver. Embedded directly here (not via the string pool) so
        // the linker can match callees without first interning the pool.
        append_string(section.bytes, fn.source_name);
        append_u32(section.bytes, static_cast<u32>(fn.parameter_type_names.size()));
        for (const auto &t : fn.parameter_type_names) append_string(section.bytes, t);
        append_u32(section.bytes, static_cast<u32>(fn.parameter_ids.size()));
        for (IRId id : fn.parameter_ids) append_u32(section.bytes, id);
        append_u32(section.bytes, static_cast<u32>(fn.body.size()));
        for (const auto &inst : fn.body) write_instruction(section.bytes, inst);
    }
    // Pending FunctionCall target names. Indexed by FunctionCall.literals[0].
    // The single-module inliner clears this; only cross-module-pending entries
    // survive into the serialized rtslo.
    append_u32(section.bytes, static_cast<u32>(module.call_target_names.size()));
    for (const auto &name : module.call_target_names) append_string(section.bytes, name);
    append_u32(section.bytes, static_cast<u32>(module.function_debug.size()));
    for (const auto &dbg : module.function_debug) {
        append_u32(section.bytes, dbg.display_name.value);
        append_u32(section.bytes, static_cast<u32>(dbg.parameter_names.size()));
        for (const auto &name : dbg.parameter_names) append_u32(section.bytes, name.value);
    }
    return section;
}

Section make_decoration_section(const IRModule &module) {
    Section section{.kind = SectionKind::decoration_table};
    append_u32(section.bytes, static_cast<u32>(module.decorations.size()));
    for (const auto &dec : module.decorations) {
        append_u32(section.bytes, dec.target);
        append_u16(section.bytes, static_cast<u16>(dec.kind));
        append_u32(section.bytes, dec.member_index);
        append_u32(section.bytes, static_cast<u32>(dec.literals.size()));
        for (u32 literal : dec.literals) append_u32(section.bytes, literal);
    }
    return section;
}

Section make_struct_section(const IRModule &module) {
    Section section{.kind = SectionKind::struct_table};
    append_u32(section.bytes, static_cast<u32>(module.structs.size()));
    for (const auto &decl : module.structs) {
        append_string(section.bytes, decl.name);
        append_u32(section.bytes, static_cast<u32>(decl.fields.size()));
        for (const auto &field : decl.fields) {
            append_string(section.bytes, field.type);
            append_string(section.bytes, field.name);
        }
    }
    return section;
}

Section make_resource_section(const IRModule &module) {
    Section section{.kind = SectionKind::resource_table};
    append_u32(section.bytes, static_cast<u32>(module.uniforms.size()));
    for (const auto &uniform : module.uniforms) {
        append_string(section.bytes, uniform.scope_name);
        append_string(section.bytes, uniform.name);
        append_string(section.bytes, uniform.type);
        append_string(section.bytes, uniform.access);
        append_u32(section.bytes, uniform.set);
        append_u32(section.bytes, uniform.binding);
        append_u8(section.bytes, uniform.is_anonymous ? 1 : 0);
        append_u32(section.bytes, uniform.anonymous_block_id);
        append_u32(section.bytes, static_cast<u32>(uniform.inline_fields.size()));
        for (const auto &field : uniform.inline_fields) {
            append_string(section.bytes, field.type);
            append_string(section.bytes, field.name);
        }
    }
    return section;
}

Section make_stage_interface_section(const IRModule &module) {
    Section section{.kind = SectionKind::stage_interface_table};
    append_u32(section.bytes, static_cast<u32>(module.stage_interfaces.size()));
    for (const auto &interface : module.stage_interfaces) {
        append_u8(section.bytes, static_cast<u8>(interface.role));
        append_string(section.bytes, interface.type_name);
        append_u32(section.bytes, static_cast<u32>(interface.fields.size()));
        for (const auto &field : interface.fields) {
            append_string(section.bytes, field.name);
            append_string(section.bytes, field.interpolation);
            append_string(section.bytes, field.builtin);
            append_u8(section.bytes, field.has_location ? 1 : 0);
            append_u32(section.bytes, field.location);
        }
    }
    return section;
}

Section make_entry_section(std::span<const Artifact::EntryPoint> entries) {
    Section section{.kind = SectionKind::entry_table};
    append_u32(section.bytes, static_cast<u32>(entries.size()));
    for (const auto &entry : entries) {
        append_string(section.bytes, entry.name);
        append_string(section.bytes, entry.mangled_name);
        append_u8(section.bytes, static_cast<u8>(entry.stage));
        append_u32(section.bytes, entry.function_id);
    }
    return section;
}

std::vector<u8> write_container(ArtifactKind kind, std::vector<Section> sections) {
    const u32 section_count = static_cast<u32>(sections.size());
    const u64 section_table_offset = HeaderSize;
    u64 data_offset = HeaderSize + static_cast<u64>(SectionEntrySize) * section_count;
    std::vector<u64> offsets;
    offsets.reserve(sections.size());
    for (const auto &section : sections) {
        offsets.push_back(data_offset);
        data_offset += section.bytes.size();
    }

    std::vector<u8> out;
    append_u32(out, Magic);
    append_u16(out, VersionMajor);
    append_u16(out, VersionMinor);
    append_u16(out, static_cast<u16>(kind));
    append_u8(out, 1);
    append_u8(out, 0);
    append_u32(out, 0);
    append_u32(out, HeaderSize);
    append_u32(out, section_count);
    append_u64(out, section_table_offset);
    append_u64(out, data_offset);
    out.resize(HeaderSize, 0);

    for (std::size_t i = 0; i < sections.size(); ++i) {
        append_u32(out, static_cast<u32>(sections[i].kind));
        append_u32(out, 0);
        append_u64(out, offsets[i]);
        append_u64(out, static_cast<u64>(sections[i].bytes.size()));
        append_u32(out, 1);
        append_u32(out, 0);
    }
    for (const auto &section : sections) out.insert(out.end(), section.bytes.begin(), section.bytes.end());
    return out;
}

void report_read_error(DiagnosticEngine *diagnostics, std::string message) {
    if (diagnostics) diagnostics->report(5001, DiagnosticSeverity::error, {}, "<artifact>", std::move(message));
}

std::vector<Artifact::EntryPoint> default_entries_from_module(const IRModule &module) {
    std::vector<Artifact::EntryPoint> entries;
    for (const auto &fn : module.functions) {
        if (fn.stage == StageKind::none) continue;
        entries.push_back(Artifact::EntryPoint{
            .name = std::string(stage_entry_name(fn.stage)),
            .mangled_name = std::string(stage_entry_name(fn.stage)),
            .stage = fn.stage,
            .function_id = fn.result_id,
        });
    }
    return entries;
}

} // namespace

std::vector<u8> write_artifact(ArtifactKind kind, const IRModule &module) {
    const auto entries = default_entries_from_module(module);
    const auto strings = build_string_pool(module, entries);
    std::vector<Section> sections;
    sections.push_back(make_string_table(strings));
    sections.push_back(make_ir_module_section(module));
    sections.push_back(make_decoration_section(module));
    sections.push_back(make_struct_section(module));
    sections.push_back(make_resource_section(module));
    sections.push_back(make_stage_interface_section(module));
    sections.push_back(make_entry_section(entries));
    return write_container(kind, std::move(sections));
}

std::vector<u8> write_debug_artifact(const IRModule &) {
    return write_container(ArtifactKind::object, {});
}

std::vector<u8> write_linked_program(std::span<const Artifact> inputs) {
    if (inputs.empty()) return {};
    return write_artifact(ArtifactKind::program, inputs.front().module);
}

bool read_artifact(std::span<const u8> data, Artifact &artifact, DiagnosticEngine *diagnostics) {
    if (data.size() < HeaderSize) {
        report_read_error(diagnostics, "artifact is smaller than the header");
        return false;
    }
    if (read_u32(data, 0) != Magic) {
        report_read_error(diagnostics, "invalid RTSL artifact magic");
        return false;
    }
    if (read_u16(data, 4) != VersionMajor) {
        report_read_error(diagnostics, "unsupported RTSL artifact version");
        return false;
    }
    const auto kind = static_cast<ArtifactKind>(read_u16(data, 8));
    const auto endian = data[10];
    const auto header_size = read_u32(data, 16);
    const auto section_count = read_u32(data, 20);
    const auto section_table_offset = read_u64(data, 24);
    const auto file_size = read_u64(data, 32);
    if (endian != 1 || header_size != HeaderSize || file_size != data.size()) {
        report_read_error(diagnostics, "invalid RTSL artifact header");
        return false;
    }
    artifact = Artifact{};
    artifact.kind = kind;
    artifact.bytes.assign(data.begin(), data.end());

    for (u32 i = 0; i < section_count; ++i) {
        const auto entry = static_cast<std::size_t>(section_table_offset + i * SectionEntrySize);
        const auto section_kind = static_cast<SectionKind>(read_u32(data, entry));
        const auto offset = read_u64(data, entry + 8);
        const auto size = read_u64(data, entry + 16);
        if (offset + size > data.size()) {
            report_read_error(diagnostics, "section payload is out of bounds");
            return false;
        }
        auto section = data.subspan(static_cast<std::size_t>(offset), static_cast<std::size_t>(size));
        std::size_t cursor = 0;
        switch (section_kind) {
        case SectionKind::string_table: {
            if (section.size() < 4) return false;
            const u32 count = read_u32(section, cursor);
            cursor += 4;
            artifact.strings.reserve(count);
            for (u32 s = 0; s < count; ++s) {
                auto value = read_string(section, cursor);
                if (!value) return false;
                artifact.strings.push_back(std::move(*value));
            }
            if (!artifact.strings.empty()) artifact.module.source_name = artifact.strings.front();
            break;
        }
        case SectionKind::ir_module: {
            artifact.module.next_id = read_u32(section, cursor); cursor += 4;
            u32 count = read_u32(section, cursor); cursor += 4;
            artifact.module.type_constant_pool.reserve(count);
            for (u32 idx = 0; idx < count; ++idx) {
                auto inst = read_instruction(section, cursor);
                if (!inst) return false;
                artifact.module.type_constant_pool.push_back(std::move(*inst));
            }
            count = read_u32(section, cursor); cursor += 4;
            artifact.module.global_variables.reserve(count);
            for (u32 idx = 0; idx < count; ++idx) {
                auto inst = read_instruction(section, cursor);
                if (!inst) return false;
                artifact.module.global_variables.push_back(std::move(*inst));
            }
            count = read_u32(section, cursor); cursor += 4;
            artifact.module.functions.reserve(count);
            for (u32 idx = 0; idx < count; ++idx) {
                IRFunction fn;
                if (cursor + 22 > section.size()) return false;
                fn.result_id = read_u32(section, cursor); cursor += 4;
                fn.function_type_id = read_u32(section, cursor); cursor += 4;
                fn.return_type_id = read_u32(section, cursor); cursor += 4;
                fn.stage = static_cast<StageKind>(section[cursor++]);
                fn.generated = section[cursor++] != 0;
                fn.exported = section[cursor++] != 0;
                fn.display_name.value = read_u32(section, cursor); cursor += 4;
                fn.mangled_name.value = read_u32(section, cursor); cursor += 4;
                auto source_name = read_string(section, cursor);
                if (!source_name) return false;
                fn.source_name = std::move(*source_name);
                u32 type_name_count = read_u32(section, cursor); cursor += 4;
                fn.parameter_type_names.reserve(type_name_count);
                for (u32 t = 0; t < type_name_count; ++t) {
                    auto type_name = read_string(section, cursor);
                    if (!type_name) return false;
                    fn.parameter_type_names.push_back(std::move(*type_name));
                }
                u32 param_count = read_u32(section, cursor); cursor += 4;
                fn.parameter_ids.reserve(param_count);
                for (u32 p = 0; p < param_count; ++p) {
                    fn.parameter_ids.push_back(read_u32(section, cursor)); cursor += 4;
                }
                u32 body_count = read_u32(section, cursor); cursor += 4;
                fn.body.reserve(body_count);
                for (u32 b = 0; b < body_count; ++b) {
                    auto inst = read_instruction(section, cursor);
                    if (!inst) return false;
                    fn.body.push_back(std::move(*inst));
                }
                artifact.module.functions.push_back(std::move(fn));
            }
            // Pending FunctionCall target names. See make_ir_module_section.
            const u32 target_name_count = read_u32(section, cursor); cursor += 4;
            artifact.module.call_target_names.reserve(target_name_count);
            for (u32 t = 0; t < target_name_count; ++t) {
                auto name = read_string(section, cursor);
                if (!name) return false;
                artifact.module.call_target_names.push_back(std::move(*name));
            }
            count = read_u32(section, cursor); cursor += 4;
            artifact.module.function_debug.reserve(count);
            for (u32 idx = 0; idx < count; ++idx) {
                IRFunctionDebugInfo dbg;
                dbg.display_name.value = read_u32(section, cursor); cursor += 4;
                u32 param_count = read_u32(section, cursor); cursor += 4;
                dbg.parameter_names.reserve(param_count);
                for (u32 p = 0; p < param_count; ++p) {
                    dbg.parameter_names.push_back(StringId{read_u32(section, cursor)});
                    cursor += 4;
                }
                artifact.module.function_debug.push_back(std::move(dbg));
            }
            break;
        }
        case SectionKind::decoration_table: {
            const u32 count = read_u32(section, cursor); cursor += 4;
            artifact.module.decorations.reserve(count);
            for (u32 idx = 0; idx < count; ++idx) {
                IRDecoration dec;
                dec.target = read_u32(section, cursor); cursor += 4;
                dec.kind = static_cast<IRDecorationKind>(read_u16(section, cursor)); cursor += 2;
                dec.member_index = read_u32(section, cursor); cursor += 4;
                u32 literal_count = read_u32(section, cursor); cursor += 4;
                dec.literals.reserve(literal_count);
                for (u32 l = 0; l < literal_count; ++l) {
                    dec.literals.push_back(read_u32(section, cursor)); cursor += 4;
                }
                artifact.module.decorations.push_back(std::move(dec));
            }
            break;
        }
        case SectionKind::struct_table: {
            const u32 count = read_u32(section, cursor); cursor += 4;
            artifact.module.structs.reserve(count);
            for (u32 idx = 0; idx < count; ++idx) {
                auto name = read_string(section, cursor);
                if (!name) return false;
                StructDecl decl{.name = std::move(*name)};
                const u32 field_count = read_u32(section, cursor); cursor += 4;
                for (u32 f = 0; f < field_count; ++f) {
                    auto type = read_string(section, cursor);
                    auto field = read_string(section, cursor);
                    if (!type || !field) return false;
                    decl.fields.push_back(StructField{.type = std::move(*type), .name = std::move(*field)});
                }
                artifact.module.structs.push_back(std::move(decl));
            }
            break;
        }
        case SectionKind::resource_table: {
            const u32 count = read_u32(section, cursor); cursor += 4;
            artifact.module.uniforms.reserve(count);
            for (u32 idx = 0; idx < count; ++idx) {
                auto scope = read_string(section, cursor);
                auto name = read_string(section, cursor);
                auto type = read_string(section, cursor);
                auto access = read_string(section, cursor);
                if (!scope || !name || !type || !access) return false;
                UniformBinding uniform{
                    .scope_name = std::move(*scope),
                    .name = std::move(*name),
                    .type = std::move(*type),
                    .access = std::move(*access),
                    .set = read_u32(section, cursor),
                    .binding = read_u32(section, cursor + 4),
                };
                cursor += 8;
                uniform.is_anonymous = section[cursor] != 0;
                cursor += 1;
                uniform.anonymous_block_id = read_u32(section, cursor);
                cursor += 4;
                const u32 field_count = read_u32(section, cursor); cursor += 4;
                for (u32 f = 0; f < field_count; ++f) {
                    auto field_type = read_string(section, cursor);
                    auto field_name = read_string(section, cursor);
                    if (!field_type || !field_name) return false;
                    uniform.inline_fields.push_back(StructField{.type = std::move(*field_type), .name = std::move(*field_name)});
                }
                artifact.module.uniforms.push_back(std::move(uniform));
            }
            break;
        }
        case SectionKind::stage_interface_table: {
            const u32 count = read_u32(section, cursor); cursor += 4;
            artifact.module.stage_interfaces.reserve(count);
            for (u32 idx = 0; idx < count; ++idx) {
                StageInterface interface;
                interface.role = static_cast<StageRole>(section[cursor++]);
                auto type_name = read_string(section, cursor);
                if (!type_name) return false;
                interface.type_name = std::move(*type_name);
                const u32 field_count = read_u32(section, cursor); cursor += 4;
                for (u32 f = 0; f < field_count; ++f) {
                    auto name = read_string(section, cursor);
                    auto interpolation = read_string(section, cursor);
                    auto builtin = read_string(section, cursor);
                    if (!name || !interpolation || !builtin) return false;
                    StageIOField field;
                    field.name = std::move(*name);
                    field.interpolation = std::move(*interpolation);
                    field.builtin = std::move(*builtin);
                    field.has_location = section[cursor++] != 0;
                    field.location = read_u32(section, cursor);
                    cursor += 4;
                    interface.fields.push_back(std::move(field));
                }
                artifact.module.stage_interfaces.push_back(std::move(interface));
            }
            break;
        }
        case SectionKind::entry_table: {
            const u32 count = read_u32(section, cursor); cursor += 4;
            artifact.entries.reserve(count);
            for (u32 idx = 0; idx < count; ++idx) {
                auto name = read_string(section, cursor);
                auto mangled = read_string(section, cursor);
                if (!name || !mangled) return false;
                Artifact::EntryPoint entry_point;
                entry_point.name = std::move(*name);
                entry_point.mangled_name = std::move(*mangled);
                entry_point.stage = static_cast<StageKind>(section[cursor++]);
                entry_point.function_id = read_u32(section, cursor);
                cursor += 4;
                artifact.entries.push_back(std::move(entry_point));
            }
            break;
        }
        case SectionKind::debug_table:
            break;
        }
    }

    artifact.structs = artifact.module.structs;
    artifact.uniforms = artifact.module.uniforms;
    artifact.stage_interfaces = artifact.module.stage_interfaces;
    return true;
}

const char *artifact_extension(ArtifactKind kind) {
    switch (kind) {
    case ArtifactKind::object: return ".rtslo";
    case ArtifactKind::module: return ".rtslm";
    case ArtifactKind::library: return ".rtsll";
    case ArtifactKind::program: return ".rtslp";
    }
    return ".rtslbin";
}

const char *debug_artifact_extension() { return ".rtsld"; }

} // namespace rtsl
