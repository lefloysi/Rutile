#include "artifact.hpp"

#include "ast.hpp"
#include "binary.hpp"

#include <cstring>
#include <optional>
#include <unordered_map>

namespace rtsl {


constexpr u32 Magic = 0x4c535452;
constexpr u16 VersionMajor = 0;
constexpr u16 VersionMinor = 4;
constexpr u32 HeaderSize = 48;
constexpr u32 SectionEntrySize = 32;

enum class SectionKind : u32 {
	string_table = 1,
	ir_module = 2,
	import_table = 3,
	export_table = 4,
	decoration_table = 5,
	struct_table = 6,
	resource_table = 7,
	stage_interface_table = 8,
	entry_table = 9,
	debug_table = 10,
	imported_export_table = 11,
};

struct Section {
	SectionKind kind;
	std::vector<u8> bytes;
};

// Heterogeneous hash + equal so `intern(std::string_view)` can probe the map
// without materializing a std::string just to hash it. C++20 lookup on
// unordered containers requires both the Hash and KeyEqual to declare
// `is_transparent` and to compare against the query type directly.
struct StringPoolHash {
	using is_transparent = void;
	std::size_t operator()(std::string_view s) const noexcept {
		return std::hash<std::string_view>{}(s);
	}
	std::size_t operator()(const std::string& s) const noexcept {
		return std::hash<std::string_view>{}(s);
	}
};

struct StringPoolEqual {
	using is_transparent = void;
	bool operator()(std::string_view a, std::string_view b) const noexcept { return a == b; }
};

struct StringPool {
	std::vector<std::string> values;
	std::unordered_map<std::string, u32, StringPoolHash, StringPoolEqual> ids;

	u32 intern(std::string_view value) {
		const auto it = ids.find(value);
		if (it != ids.end()) {
			return it->second;
		}
		const u32 id = static_cast<u32>(values.size());
		values.emplace_back(value);
		ids.emplace(values.back(), id);
		return id;
	}
};

StringPool build_string_pool(const IRModule& module, std::span<const Artifact::EntryPoint> entries, bool linked_program) {
	StringPool pool;
	pool.intern(module.source_name);
	// Reflection-visible names only: a host queries uniforms by scope/name and
	// stage I/O by payload struct + field. Interpolation, builtin, access, and
	// every type spelling are encoded as enums or type-pool ids — they do not
	// appear in the string table.
	for (const auto& uniform : module.uniforms) {
		pool.intern(uniform.scope_name);
		pool.intern(uniform.name);
		for (const auto& field : uniform.inline_fields) {
			pool.intern(field.name);
		}
	}
	for (const auto& interface : module.stage_interfaces) {
		// Inputs (vertex buffer) and outputs (framebuffer attachments) are
		// both host-visible. Varyings are pipeline-internal — the linker
		// matches them by location, not by name. The carrier struct's name
		// ("Point", "Vertex", "Fragment") is never reflected; queries are by
		// field name only, against the single implicit input/output payload
		// for the stage.
		if (interface.role == StageRole::varying)
			continue;
		for (const auto& field : interface.fields) {
			pool.intern(field.name);
		}
	}
	// Struct decls are part of cross-module link state; they only live in
	// unlinked artifacts. A linked program drops them entirely — the runtime
	// queries types via the IR pool, not by source-level struct names.
	if (!linked_program) {
		for (const auto& decl : module.structs) {
			pool.intern(decl.name);
			for (const auto& field : decl.fields) {
				pool.intern(field.name);
			}
		}
	}
	for (const auto& entry : entries) {
		pool.intern(entry.name);
		pool.intern(entry.mangled_name);
	}
	return pool;
}

Section make_string_table(const StringPool& pool) {
	Section section{ .kind = SectionKind::string_table };
	BinaryWriter w{ section.bytes };
	w.write<u32>(static_cast<u32>(pool.values.size()));
	for (const auto& value : pool.values)
		w.write_string(value);
	return section;
}

void write_instruction(BinaryWriter& w, const IRInstruction& inst) {
	w.write<u16>(static_cast<u16>(inst.op));
	w.write<u32>(inst.result_id);
	w.write<u32>(inst.type_id);
	w.write<u32>(static_cast<u32>(inst.operands.size()));
	w.write<u32>(static_cast<u32>(inst.literals.size()));
	w.write<u32>(inst.loc.file_id);
	w.write<u32>(inst.loc.line);
	w.write<u32>(inst.loc.column);
	for (IRId operand : inst.operands)
		w.write<u32>(operand);
	for (u32 literal : inst.literals)
		w.write<u32>(literal);
}

std::optional<IRInstruction> read_instruction(BinaryReader& r) {
	IRInstruction inst;
	const auto op = r.read<u16>();
	const auto result_id = r.read<u32>();
	const auto type_id = r.read<u32>();
	const auto operand_count = r.read<u32>();
	const auto literal_count = r.read<u32>();
	const auto file_id = r.read<u32>();
	const auto line = r.read<u32>();
	const auto column = r.read<u32>();
	if (!op || !result_id || !type_id || !operand_count || !literal_count ||
		!file_id || !line || !column)
		return std::nullopt;
	inst.op = static_cast<IROp>(*op);
	inst.result_id = *result_id;
	inst.type_id = *type_id;
	inst.loc.file_id = *file_id;
	inst.loc.line = *line;
	inst.loc.column = *column;
	inst.operands.reserve(*operand_count);
	inst.literals.reserve(*literal_count);
	for (u32 i = 0; i < *operand_count; ++i) {
		const auto v = r.read<u32>();
		if (!v)
			return std::nullopt;
		inst.operands.push_back(*v);
	}
	for (u32 i = 0; i < *literal_count; ++i) {
		const auto v = r.read<u32>();
		if (!v)
			return std::nullopt;
		inst.literals.push_back(*v);
	}
	return inst;
}

Section make_ir_module_section(const IRModule& module, bool linked_program) {
	Section section{ .kind = SectionKind::ir_module };
	BinaryWriter w{ section.bytes };
	w.write<u32>(module.next_id);
	w.write<u32>(static_cast<u32>(module.type_constant_pool.size()));
	for (const auto& inst : module.type_constant_pool)
		write_instruction(w, inst);
	w.write<u32>(static_cast<u32>(module.global_variables.size()));
	for (const auto& inst : module.global_variables)
		write_instruction(w, inst);
	// Constructors are dead post-inlining (every call site has been replaced
	// by the inliner). Skip them on serialization to keep `Foo::Foo` strings
	// out of the artifact.
	u32 emit_count = 0;
	for (const auto& fn : module.functions) {
		if (!fn.is_constructor)
			++emit_count;
	}
	w.write<u32>(emit_count);
	for (const auto& fn : module.functions) {
		if (fn.is_constructor)
			continue;
		w.write<u32>(fn.result_id);
		w.write<u32>(fn.function_type_id);
		w.write<u32>(fn.return_type_id);
		w.write<u8>(static_cast<u8>(fn.stage));
		w.write<u8>(fn.generated ? 1 : 0);
		w.write<u8>(fn.exported ? 1 : 0);
		w.write<u32>(fn.display_name.value);
		w.write<u32>(fn.mangled_name.value);
		// Source-level identity used by the linker's cross-module call
		// resolver. A linked program no longer relinks, so source_name is
		// elided.
		w.write_string(linked_program ? std::string_view{} : std::string_view{ fn.source_name });
		w.write<u32>(static_cast<u32>(fn.parameter_ids.size()));
		for (IRId id : fn.parameter_ids)
			w.write<u32>(id);
		w.write<u32>(static_cast<u32>(fn.body.size()));
		for (const auto& inst : fn.body)
			write_instruction(w, inst);
	}
	// Pending FunctionCall target names. Linked programs have none.
	if (linked_program) {
		w.write<u32>(0);
	} else {
		w.write<u32>(static_cast<u32>(module.call_target_names.size()));
		for (const auto& name : module.call_target_names)
			w.write_string(name);
	}
	w.write<u32>(static_cast<u32>(module.function_debug.size()));
	for (const auto& dbg : module.function_debug) {
		w.write<u32>(dbg.display_name.value);
		w.write<u32>(static_cast<u32>(dbg.parameter_names.size()));
		for (const auto& name : dbg.parameter_names)
			w.write<u32>(name.value);
	}
	return section;
}

Section make_import_section(const IRModule& module) {
	Section section{ .kind = SectionKind::import_table };
	BinaryWriter w{ section.bytes };
	w.write<u32>(static_cast<u32>(module.imports.size()));
	for (const auto& name : module.imports)
		w.write_string(name);
	return section;
}

Section make_export_section(const IRModule& module) {
	Section section{ .kind = SectionKind::export_table };
	BinaryWriter w{ section.bytes };
	w.write<u32>(static_cast<u32>(module.exports.size()));
	for (const auto& symbol : module.exports) {
		w.write_string(symbol.name);
		w.write_string(symbol.kind);
		w.write_string(symbol.type);
	}
	return section;
}

Section make_imported_export_section(const IRModule& module) {
	Section section{ .kind = SectionKind::imported_export_table };
	BinaryWriter w{ section.bytes };
	w.write<u32>(static_cast<u32>(module.imported_exports.size()));
	for (const auto& symbol : module.imported_exports) {
		w.write_string(symbol.name);
		w.write_string(symbol.kind);
		w.write_string(symbol.type);
	}
	return section;
}

Section make_decoration_section(const IRModule& module) {
	Section section{ .kind = SectionKind::decoration_table };
	BinaryWriter w{ section.bytes };
	w.write<u32>(static_cast<u32>(module.decorations.size()));
	for (const auto& dec : module.decorations) {
		w.write<u32>(dec.target);
		w.write<u16>(static_cast<u16>(dec.kind));
		w.write<u32>(dec.member_index);
		w.write<u32>(static_cast<u32>(dec.literals.size()));
		for (u32 literal : dec.literals)
			w.write<u32>(literal);
	}
	return section;
}

Section make_struct_section(const IRModule& module) {
	// Struct table carries cross-module link metadata. Type spellings of fields
	// stay (the linker still resolves by name across translation units); field
	// *names* stay because they participate in reflection through uniforms and
	// stage interfaces. Anything purely structural lives in the IR type pool.
	Section section{ .kind = SectionKind::struct_table };
	BinaryWriter w{ section.bytes };
	w.write<u32>(static_cast<u32>(module.structs.size()));
	for (const auto& decl : module.structs) {
		w.write_string(decl.name);
		w.write<u32>(static_cast<u32>(decl.fields.size()));
		for (const auto& field : decl.fields) {
			w.write_string(field.type);
			w.write_string(field.name);
		}
	}
	return section;
}

Section make_resource_section(const IRModule& module) {
	// Uniform reflection: numeric group/member, type_id into the IR pool, and
	// an access kind as u8. No type spelling and no per-field type strings —
	// the type pool answers "what's the byte layout" via TypeStruct + Offset
	// decorations.
	Section section{ .kind = SectionKind::resource_table };
	BinaryWriter w{ section.bytes };
	w.write<u32>(static_cast<u32>(module.uniforms.size()));
	for (const auto& uniform : module.uniforms) {
		w.write_string(uniform.scope_name);
		w.write_string(uniform.name);
		w.write<u32>(uniform.type_id);
		w.write<u8>(static_cast<u8>(uniform.access));
		w.write<u32>(uniform.set);
		w.write<u32>(uniform.member);
		w.write<u8>(uniform.is_anonymous ? 1 : 0);
		w.write<u32>(uniform.anonymous_block_id);
		w.write<u32>(static_cast<u32>(uniform.inline_fields.size()));
		for (const auto& field : uniform.inline_fields)
			w.write_string(field.name);
	}
	return section;
}

Section make_stage_interface_section(const IRModule& module) {
	// Stage interface reflection covers only the host-visible boundaries:
	// `input` (vertex buffer attributes) and `output` (framebuffer
	// attachments). Varyings are entirely pipeline-internal — the linker
	// matches them by location — so they are not serialized.
	//
	// A record carries only role + (interpolation, builtin, location) per
	// field, plus the field name. The host queries by field name within the
	// single implicit input/output payload for a stage; the carrier struct
	// name is never written.
	Section section{ .kind = SectionKind::stage_interface_table };
	BinaryWriter w{ section.bytes };
	u32 emit_count = 0;
	for (const auto& interface : module.stage_interfaces) {
		if (interface.role != StageRole::varying)
			++emit_count;
	}
	w.write<u32>(emit_count);
	for (const auto& interface : module.stage_interfaces) {
		if (interface.role == StageRole::varying)
			continue;
		w.write<u8>(static_cast<u8>(interface.role));
		w.write<u32>(static_cast<u32>(interface.fields.size()));
		for (const auto& field : interface.fields) {
			w.write_string(field.name);
			w.write<u8>(static_cast<u8>(field.interpolation));
			w.write<u8>(static_cast<u8>(field.builtin));
			w.write<u32>(field.location);
		}
	}
	return section;
}

Section make_entry_section(std::span<const Artifact::EntryPoint> entries) {
	Section section{ .kind = SectionKind::entry_table };
	BinaryWriter w{ section.bytes };
	w.write<u32>(static_cast<u32>(entries.size()));
	for (const auto& entry : entries) {
		w.write_string(entry.name);
		w.write_string(entry.mangled_name);
		w.write<u8>(static_cast<u8>(entry.stage));
		w.write<u32>(entry.function_id);
	}
	return section;
}

std::vector<u8> write_container(ArtifactKind kind, std::vector<Section> sections) {
	const u32 section_count = static_cast<u32>(sections.size());
	const u64 section_table_offset = HeaderSize;
	u64 data_offset = HeaderSize + static_cast<u64>(SectionEntrySize) * section_count;
	std::vector<u64> offsets;
	offsets.reserve(sections.size());
	for (const auto& section : sections) {
		offsets.push_back(data_offset);
		data_offset += section.bytes.size();
	}

	std::vector<u8> out;
	BinaryWriter w{ out };
	w.write<u32>(Magic);
	w.write<u16>(VersionMajor);
	w.write<u16>(VersionMinor);
	w.write<u16>(static_cast<u16>(kind));
	w.write<u8>(1);
	w.write<u8>(0);
	w.write<u32>(0);
	w.write<u32>(HeaderSize);
	w.write<u32>(section_count);
	w.write<u64>(section_table_offset);
	w.write<u64>(data_offset);
	out.resize(HeaderSize, 0);

	for (std::size_t i = 0; i < sections.size(); ++i) {
		w.write<u32>(static_cast<u32>(sections[i].kind));
		w.write<u32>(0);
		w.write<u64>(offsets[i]);
		w.write<u64>(static_cast<u64>(sections[i].bytes.size()));
		w.write<u32>(1);
		w.write<u32>(0);
	}
	for (const auto& section : sections)
		out.insert(out.end(), section.bytes.begin(), section.bytes.end());
	return out;
}

void report_read_error(DiagnosticEngine* diagnostics, std::string message) {
	if (diagnostics)
		diagnostics->report(5001, DiagnosticSeverity::error, {}, "<artifact>", std::move(message));
}

std::vector<Artifact::EntryPoint> default_entries_from_module(const IRModule& module) {
	std::vector<Artifact::EntryPoint> entries;
	for (const auto& fn : module.functions) {
		if (fn.stage == StageKind::none)
			continue;
		entries.push_back(Artifact::EntryPoint{
			.name = std::string(stage_entry_name(fn.stage)),
			.mangled_name = std::string(stage_entry_name(fn.stage)),
			.stage = fn.stage,
			.function_id = fn.result_id,
		});
	}
	return entries;
}


std::vector<u8> write_artifact(ArtifactKind kind, const IRModule& module) {
	const auto entries = default_entries_from_module(module);
	// For a fully-linked program, drop link-only state: no cross-module call
	// resolution will run again, so the struct_table (parser-level type
	// spellings used by the linker) and the import/imported_export tables
	// are dead weight. The IR type pool + decorations + reflection tables
	// are everything a runtime needs.
	const bool linked_program = kind == ArtifactKind::program;
	const auto strings = build_string_pool(module, entries, linked_program);
	std::vector<Section> sections;
	sections.push_back(make_string_table(strings));
	sections.push_back(make_ir_module_section(module, linked_program));
	if (!linked_program) {
		sections.push_back(make_import_section(module));
		sections.push_back(make_export_section(module));
		sections.push_back(make_imported_export_section(module));
		sections.push_back(make_struct_section(module));
	}
	sections.push_back(make_decoration_section(module));
	sections.push_back(make_resource_section(module));
	sections.push_back(make_stage_interface_section(module));
	sections.push_back(make_entry_section(entries));
	return write_container(kind, std::move(sections));
}

std::vector<u8> write_debug_artifact(const IRModule&) {
	return write_container(ArtifactKind::object, {});
}

std::vector<u8> write_linked_program(std::span<const Artifact> inputs) {
	if (inputs.empty())
		return {};
	return write_artifact(ArtifactKind::program, inputs.front().module);
}

bool read_artifact(std::span<const u8> data, Artifact& artifact, DiagnosticEngine* diagnostics) {
	if (data.size() < HeaderSize) {
		report_read_error(diagnostics, "artifact is smaller than the header");
		return false;
	}
	BinaryReader header{ data };
	const auto magic = header.read<u32>();
	if (!magic || *magic != Magic) {
		report_read_error(diagnostics, "invalid RTSL artifact magic");
		return false;
	}
	const auto version_major = header.read<u16>();
	if (!version_major || *version_major != VersionMajor) {
		report_read_error(diagnostics, "unsupported RTSL artifact version");
		return false;
	}
	(void)header.read<u16>(); // version_minor
	const auto kind_raw = header.read<u16>();
	const auto endian = header.read<u8>();
	(void)header.read<u8>();  // reserved
	(void)header.read<u32>(); // reserved
	const auto header_size = header.read<u32>();
	const auto section_count = header.read<u32>();
	const auto section_table_offset = header.read<u64>();
	const auto file_size = header.read<u64>();
	if (!kind_raw || !endian || !header_size || !section_count ||
		!section_table_offset || !file_size ||
		*endian != 1 || *header_size != HeaderSize || *file_size != data.size()) {
		report_read_error(diagnostics, "invalid RTSL artifact header");
		return false;
	}
	artifact = Artifact{};
	artifact.kind = static_cast<ArtifactKind>(*kind_raw);
	artifact.bytes.assign(data.begin(), data.end());

	for (u32 i = 0; i < *section_count; ++i) {
		BinaryReader entry_reader{ data };
		entry_reader.seek(static_cast<std::size_t>(*section_table_offset + i * SectionEntrySize));
		const auto section_kind_raw = entry_reader.read<u32>();
		(void)entry_reader.read<u32>(); // reserved
		const auto section_offset = entry_reader.read<u64>();
		const auto section_size = entry_reader.read<u64>();
		if (!section_kind_raw || !section_offset || !section_size ||
			*section_offset + *section_size > data.size()) {
			report_read_error(diagnostics, "section payload is out of bounds");
			return false;
		}
		const auto section_kind = static_cast<SectionKind>(*section_kind_raw);
		auto section_bytes = data.subspan(static_cast<std::size_t>(*section_offset), static_cast<std::size_t>(*section_size));
		BinaryReader r{ section_bytes };
		switch (section_kind) {
		case SectionKind::string_table: {
			const auto count = r.read<u32>();
			if (!count)
				return false;
			artifact.strings.reserve(*count);
			for (u32 s = 0; s < *count; ++s) {
				auto value = r.read_string();
				if (!value)
					return false;
				artifact.strings.push_back(std::move(*value));
			}
			if (!artifact.strings.empty())
				artifact.module.source_name = artifact.strings.front();
			break;
		}
		case SectionKind::ir_module: {
			const auto next_id = r.read<u32>();
			if (!next_id)
				return false;
			artifact.module.next_id = *next_id;

			const auto type_count = r.read<u32>();
			if (!type_count)
				return false;
			artifact.module.type_constant_pool.reserve(*type_count);
			for (u32 idx = 0; idx < *type_count; ++idx) {
				auto inst = read_instruction(r);
				if (!inst)
					return false;
				artifact.module.type_constant_pool.push_back(std::move(*inst));
			}
			const auto global_count = r.read<u32>();
			if (!global_count)
				return false;
			artifact.module.global_variables.reserve(*global_count);
			for (u32 idx = 0; idx < *global_count; ++idx) {
				auto inst = read_instruction(r);
				if (!inst)
					return false;
				artifact.module.global_variables.push_back(std::move(*inst));
			}
			const auto fn_count = r.read<u32>();
			if (!fn_count)
				return false;
			artifact.module.functions.reserve(*fn_count);
			for (u32 idx = 0; idx < *fn_count; ++idx) {
				IRFunction fn;
				const auto result_id = r.read<u32>();
				const auto function_type_id = r.read<u32>();
				const auto return_type_id = r.read<u32>();
				const auto stage = r.read<u8>();
				const auto generated = r.read<u8>();
				const auto exported = r.read<u8>();
				const auto display_name = r.read<u32>();
				const auto mangled_name = r.read<u32>();
				if (!result_id || !function_type_id || !return_type_id || !stage ||
					!generated || !exported || !display_name || !mangled_name)
					return false;
				fn.result_id = *result_id;
				fn.function_type_id = *function_type_id;
				fn.return_type_id = *return_type_id;
				fn.stage = static_cast<StageKind>(*stage);
				fn.generated = *generated != 0;
				fn.exported = *exported != 0;
				fn.display_name.value = *display_name;
				fn.mangled_name.value = *mangled_name;
				auto source_name = r.read_string();
				if (!source_name)
					return false;
				fn.source_name = std::move(*source_name);
				const auto param_count = r.read<u32>();
				if (!param_count)
					return false;
				fn.parameter_ids.reserve(*param_count);
				for (u32 p = 0; p < *param_count; ++p) {
					const auto id = r.read<u32>();
					if (!id)
						return false;
					fn.parameter_ids.push_back(*id);
				}
				const auto body_count = r.read<u32>();
				if (!body_count)
					return false;
				fn.body.reserve(*body_count);
				for (u32 b = 0; b < *body_count; ++b) {
					auto inst = read_instruction(r);
					if (!inst)
						return false;
					fn.body.push_back(std::move(*inst));
				}
				artifact.module.functions.push_back(std::move(fn));
			}
			// Pending FunctionCall target names. See make_ir_module_section.
			const auto target_name_count = r.read<u32>();
			if (!target_name_count)
				return false;
			artifact.module.call_target_names.reserve(*target_name_count);
			for (u32 t = 0; t < *target_name_count; ++t) {
				auto name = r.read_string();
				if (!name)
					return false;
				artifact.module.call_target_names.push_back(std::move(*name));
			}
			const auto debug_count = r.read<u32>();
			if (!debug_count)
				return false;
			artifact.module.function_debug.reserve(*debug_count);
			for (u32 idx = 0; idx < *debug_count; ++idx) {
				IRFunctionDebugInfo dbg;
				const auto display_name = r.read<u32>();
				const auto param_count = r.read<u32>();
				if (!display_name || !param_count)
					return false;
				dbg.display_name.value = *display_name;
				dbg.parameter_names.reserve(*param_count);
				for (u32 p = 0; p < *param_count; ++p) {
					const auto id = r.read<u32>();
					if (!id)
						return false;
					dbg.parameter_names.push_back(StringId{ *id });
				}
				artifact.module.function_debug.push_back(std::move(dbg));
			}
			break;
		}
		case SectionKind::import_table: {
			const auto count = r.read<u32>();
			if (!count)
				return false;
			artifact.module.imports.reserve(*count);
			for (u32 idx = 0; idx < *count; ++idx) {
				auto name = r.read_string();
				if (!name)
					return false;
				artifact.module.imports.push_back(std::move(*name));
			}
			artifact.imports = artifact.module.imports;
			break;
		}
		case SectionKind::export_table: {
			const auto count = r.read<u32>();
			if (!count)
				return false;
			artifact.module.exports.reserve(*count);
			for (u32 idx = 0; idx < *count; ++idx) {
				auto name = r.read_string();
				auto ekind = r.read_string();
				auto type = r.read_string();
				if (!name || !ekind || !type)
					return false;
				artifact.module.exports.push_back(ExportSymbol{ .name = std::move(*name), .kind = std::move(*ekind), .type = std::move(*type) });
			}
			artifact.exports = artifact.module.exports;
			break;
		}
		case SectionKind::imported_export_table: {
			const auto count = r.read<u32>();
			if (!count)
				return false;
			artifact.module.imported_exports.reserve(*count);
			for (u32 idx = 0; idx < *count; ++idx) {
				auto name = r.read_string();
				auto ekind = r.read_string();
				auto type = r.read_string();
				if (!name || !ekind || !type)
					return false;
				artifact.module.imported_exports.push_back(ExportSymbol{ .name = std::move(*name), .kind = std::move(*ekind), .type = std::move(*type) });
			}
			artifact.imported_exports = artifact.module.imported_exports;
			break;
		}
		case SectionKind::decoration_table: {
			const auto count = r.read<u32>();
			if (!count)
				return false;
			artifact.module.decorations.reserve(*count);
			for (u32 idx = 0; idx < *count; ++idx) {
				IRDecoration dec;
				const auto target = r.read<u32>();
				const auto dkind = r.read<u16>();
				const auto member_index = r.read<u32>();
				const auto literal_count = r.read<u32>();
				if (!target || !dkind || !member_index || !literal_count)
					return false;
				dec.target = *target;
				dec.kind = static_cast<IRDecorationKind>(*dkind);
				dec.member_index = *member_index;
				dec.literals.reserve(*literal_count);
				for (u32 l = 0; l < *literal_count; ++l) {
					const auto literal = r.read<u32>();
					if (!literal)
						return false;
					dec.literals.push_back(*literal);
				}
				artifact.module.decorations.push_back(std::move(dec));
			}
			break;
		}
		case SectionKind::struct_table: {
			const auto count = r.read<u32>();
			if (!count)
				return false;
			artifact.module.structs.reserve(*count);
			for (u32 idx = 0; idx < *count; ++idx) {
				auto name = r.read_string();
				if (!name)
					return false;
				StructDecl decl{ .name = std::move(*name) };
				const auto field_count = r.read<u32>();
				if (!field_count)
					return false;
				for (u32 f = 0; f < *field_count; ++f) {
					auto type = r.read_string();
					auto field = r.read_string();
					if (!type || !field)
						return false;
					decl.fields.push_back(StructField{ .type = std::move(*type), .name = std::move(*field) });
				}
				artifact.module.structs.push_back(std::move(decl));
			}
			break;
		}
		case SectionKind::resource_table: {
			const auto count = r.read<u32>();
			if (!count)
				return false;
			artifact.module.uniforms.reserve(*count);
			for (u32 idx = 0; idx < *count; ++idx) {
				auto scope = r.read_string();
				auto name = r.read_string();
				if (!scope || !name)
					return false;
				UniformBinding uniform;
				uniform.scope_name = std::move(*scope);
				uniform.name = std::move(*name);
				const auto type_id = r.read<u32>();
				const auto access = r.read<u8>();
				const auto set = r.read<u32>();
				const auto member = r.read<u32>();
				const auto is_anonymous = r.read<u8>();
				const auto anon_block_id = r.read<u32>();
				const auto field_count = r.read<u32>();
				if (!type_id || !access || !set || !member || !is_anonymous ||
					!anon_block_id || !field_count)
					return false;
				uniform.type_id = *type_id;
				uniform.access = static_cast<AccessKind>(*access);
				uniform.set = *set;
				uniform.member = *member;
				uniform.is_anonymous = *is_anonymous != 0;
				uniform.anonymous_block_id = *anon_block_id;
				for (u32 f = 0; f < *field_count; ++f) {
					auto field_name = r.read_string();
					if (!field_name)
						return false;
					uniform.inline_fields.push_back(StructField{ .name = std::move(*field_name) });
				}
				artifact.module.uniforms.push_back(std::move(uniform));
			}
			break;
		}
		case SectionKind::stage_interface_table: {
			const auto count = r.read<u32>();
			if (!count)
				return false;
			artifact.module.stage_interfaces.reserve(*count);
			for (u32 idx = 0; idx < *count; ++idx) {
				StageInterface interface;
				const auto role = r.read<u8>();
				const auto field_count = r.read<u32>();
				if (!role || !field_count)
					return false;
				interface.role = static_cast<StageRole>(*role);
				for (u32 f = 0; f < *field_count; ++f) {
					auto name = r.read_string();
					if (!name)
						return false;
					StageIOField field;
					field.name = std::move(*name);
					const auto interp = r.read<u8>();
					const auto builtin = r.read<u8>();
					const auto location = r.read<u32>();
					if (!interp || !builtin || !location)
						return false;
					field.interpolation = static_cast<InterpolationKind>(*interp);
					field.builtin = static_cast<BuiltinSlot>(*builtin);
					field.location = *location;
					interface.fields.push_back(std::move(field));
				}
				artifact.module.stage_interfaces.push_back(std::move(interface));
			}
			break;
		}
		case SectionKind::entry_table: {
			const auto count = r.read<u32>();
			if (!count)
				return false;
			artifact.entries.reserve(*count);
			for (u32 idx = 0; idx < *count; ++idx) {
				auto name = r.read_string();
				auto mangled = r.read_string();
				if (!name || !mangled)
					return false;
				Artifact::EntryPoint entry_point;
				entry_point.name = std::move(*name);
				entry_point.mangled_name = std::move(*mangled);
				const auto stage = r.read<u8>();
				const auto function_id = r.read<u32>();
				if (!stage || !function_id)
					return false;
				entry_point.stage = static_cast<StageKind>(*stage);
				entry_point.function_id = *function_id;
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
	artifact.imports = artifact.module.imports;
	artifact.imported_exports = artifact.module.imported_exports;
	artifact.exports = artifact.module.exports;
	return true;
}

const char* artifact_extension(ArtifactKind kind) {
	switch (kind) {
	case ArtifactKind::object:
		return ".rtslo";
	case ArtifactKind::module:
		return ".rtslm";
	case ArtifactKind::library:
		return ".rtsll";
	case ArtifactKind::program:
		return ".rtslp";
	}
	return ".rtslbin";
}

const char* debug_artifact_extension() { return ".rtsld"; }

} // namespace rtsl
