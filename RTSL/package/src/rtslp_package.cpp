#include "rtslp_package.hpp"

#include <cstdint>
#include <span>
#include <stdexcept>

namespace rtsl {
namespace {

constexpr u32 kRtslMagic = 0x4c535452;

u16 read_u16(std::span<const std::uint8_t> data, size_t offset) {
	return static_cast<u16>(data[offset] | (data[offset + 1] << 8));
}

u32 read_u32(std::span<const std::uint8_t> data, size_t offset) {
	return static_cast<u32>(data[offset]) |
		   (static_cast<u32>(data[offset + 1]) << 8) |
		   (static_cast<u32>(data[offset + 2]) << 16) |
		   (static_cast<u32>(data[offset + 3]) << 24);
}

u64 read_u64(std::span<const std::uint8_t> data, size_t offset) {
	u64 value = 0;
	for (int i = 0; i < 8; ++i) {
		value |= static_cast<u64>(data[offset + i]) << (i * 8);
	}
	return value;
}

std::string read_string(std::span<const std::uint8_t> data, size_t& cursor) {
	if (cursor + 4 > data.size()) {
		throw std::runtime_error("truncated RTSLP string");
	}
	const u32 length = read_u32(data, cursor);
	cursor += 4;
	if (cursor + length > data.size()) {
		throw std::runtime_error("truncated RTSLP string payload");
	}
	std::string value(reinterpret_cast<const char*>(data.data() + cursor), length);
	cursor += length;
	return value;
}

// Keep in sync with rtsl::SectionKind in src/Serialization/Artifact.cpp.
enum class SectionKind : u32 {
	IrModule = 2,
	StructTable = 6,
	ResourceTable = 7,
	StageInterfaceTable = 8,
};

instruction read_instruction(std::span<const std::uint8_t> data, size_t& cursor) {
	if (cursor + 26 > data.size()) {
		throw std::runtime_error("truncated RTIR instruction");
	}
    instruction inst;
    inst.op = static_cast<ir_op>(read_u16(data, cursor));
	cursor += 2;
	inst.result_id = read_u32(data, cursor);
	cursor += 4;
	inst.type_id = read_u32(data, cursor);
	cursor += 4;
	const u32 operand_count = read_u32(data, cursor);
	cursor += 4;
	const u32 literal_count = read_u32(data, cursor);
	cursor += 4;
	cursor += 12;
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

} // namespace

artifact_module read_rtslp_module(u64 program_size, const void* program_source) {
	if (!program_source || program_size < 48) {
		throw std::runtime_error("invalid RTSLP payload");
	}

	std::span<const std::uint8_t> data(static_cast<const std::uint8_t*>(program_source), static_cast<size_t>(program_size));
	if (read_u32(data, 0) != kRtslMagic) {
		throw std::runtime_error("unsupported RTSLP artifact");
	}

	const u32 section_count = read_u32(data, 20);
	const u64 section_table_offset = read_u64(data, 24);
    artifact_module module;

	for (u32 section_index = 0; section_index < section_count; ++section_index) {
		const size_t entry = static_cast<size_t>(section_table_offset + section_index * 32);
		const auto kind = static_cast<SectionKind>(read_u32(data, entry));
		const size_t offset = static_cast<size_t>(read_u64(data, entry + 8));
		const size_t size = static_cast<size_t>(read_u64(data, entry + 16));
		if (offset + size > data.size()) {
			throw std::runtime_error("RTSLP section out of bounds");
		}

		std::span<const std::uint8_t> section = data.subspan(offset, size);
		size_t cursor = 0;
		switch (kind) {
		case SectionKind::IrModule: {
			module.next_id = read_u32(section, cursor);
			cursor += 4;
			const u32 type_count = read_u32(section, cursor);
			cursor += 4;
			module.type_constant_pool.reserve(type_count);
			for (u32 i = 0; i < type_count; ++i) {
				module.type_constant_pool.push_back(read_instruction(section, cursor));
			}
			const u32 global_count = read_u32(section, cursor);
			cursor += 4;
			module.global_variables.reserve(global_count);
			for (u32 i = 0; i < global_count; ++i) {
				module.global_variables.push_back(read_instruction(section, cursor));
			}
			const u32 function_count = read_u32(section, cursor);
			cursor += 4;
			module.functions.reserve(function_count);
			for (u32 i = 0; i < function_count; ++i) {
                function fn;
				fn.result_id = read_u32(section, cursor);
				cursor += 4;
				cursor += 4;
				fn.return_type_id = read_u32(section, cursor);
				cursor += 4;
                fn.stage = static_cast<stage_kind>(section[cursor++]);
				cursor += 1; // generated
				cursor += 1; // exported
				cursor += 8; // display_name, mangled_name (StringIds)
				// Skip source_name (string) and parameter_type_names
				// (string[]). The runtime never resolves user functions -
				// the linker has already inlined them - so these fields
				// are pure linker metadata as far as Rutile is concerned.
				(void)read_string(section, cursor);
				const u32 type_name_count = read_u32(section, cursor);
				cursor += 4;
				for (u32 t = 0; t < type_name_count; ++t) {
					(void)read_string(section, cursor);
				}
				const u32 parameter_count = read_u32(section, cursor);
				cursor += 4;
				fn.parameter_ids.reserve(parameter_count);
				for (u32 p = 0; p < parameter_count; ++p) {
					fn.parameter_ids.push_back(read_u32(section, cursor));
					cursor += 4;
				}
				const u32 body_count = read_u32(section, cursor);
				cursor += 4;
				fn.body.reserve(body_count);
				for (u32 b = 0; b < body_count; ++b) {
					fn.body.push_back(read_instruction(section, cursor));
				}
				module.functions.push_back(std::move(fn));
			}
			// call_target_names tail. A well-formed rtslp has no unresolved
			// user-function calls (the inliner cleared them), so this count
			// is typically zero - but we have to advance past it either way.
			const u32 call_target_count = read_u32(section, cursor);
			cursor += 4;
			for (u32 t = 0; t < call_target_count; ++t) {
				(void)read_string(section, cursor);
			}
			break;
		}
		case SectionKind::StructTable: {
			const u32 count = read_u32(section, cursor);
			cursor += 4;
			module.structs.reserve(count);
			for (u32 i = 0; i < count; ++i) {
                struct_decl decl;
				decl.name = read_string(section, cursor);
				const u32 field_count = read_u32(section, cursor);
				cursor += 4;
				decl.fields.reserve(field_count);
				for (u32 f = 0; f < field_count; ++f) {
                    struct_field field;
					field.type = read_string(section, cursor);
					field.name = read_string(section, cursor);
					decl.fields.push_back(std::move(field));
				}
				module.structs.push_back(std::move(decl));
			}
			break;
		}
		case SectionKind::ResourceTable: {
			const u32 count = read_u32(section, cursor);
			cursor += 4;
			module.uniforms.reserve(count);
			for (u32 i = 0; i < count; ++i) {
                uniform uniform;
				uniform.scope_name = read_string(section, cursor);
				uniform.name = read_string(section, cursor);
				uniform.type = read_string(section, cursor);
				uniform.access = read_string(section, cursor);
				uniform.set = read_u32(section, cursor);
				cursor += 4;
				uniform.binding = read_u32(section, cursor);
				cursor += 4;
				// Skip is_anonymous (u8) and anonymous_block_id (u32). The
				// runtime only needs the resolved set/binding; the anonymity
				// metadata exists for Sema/lowering during compilation.
				cursor += 1;
				cursor += 4;
				const u32 field_count = read_u32(section, cursor);
				cursor += 4;
				for (u32 f = 0; f < field_count; ++f) {
					(void)read_string(section, cursor);
					(void)read_string(section, cursor);
				}
				module.uniforms.push_back(std::move(uniform));
			}
			break;
		}
		case SectionKind::StageInterfaceTable: {
			const u32 count = read_u32(section, cursor);
			cursor += 4;
			module.stage_interfaces.reserve(count);
			for (u32 i = 0; i < count; ++i) {
                stage_interface stage_interface;
                stage_interface.role = static_cast<stage_role>(section[cursor++]);
				stage_interface.type_name = read_string(section, cursor);
				const u32 field_count = read_u32(section, cursor);
				cursor += 4;
				stage_interface.fields.reserve(field_count);
				for (u32 f = 0; f < field_count; ++f) {
                    stage_field field;
					field.name = read_string(section, cursor);
					field.interpolation = read_string(section, cursor);
					field.builtin = read_string(section, cursor);
					field.has_location = section[cursor++] != 0;
					field.location = read_u32(section, cursor);
					cursor += 4;
					stage_interface.fields.push_back(std::move(field));
				}
				module.stage_interfaces.push_back(std::move(stage_interface));
			}
			break;
		}
		}
	}

	return module;
}

} // namespace rtsl
