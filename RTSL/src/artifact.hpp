#pragma once

#include "basic_diagnostics.hpp"
#include "ir.hpp"

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace rtsl {

enum class ArtifactKind : u16 {
	object = 1,
	module = 2,
	library = 3,
	program = 4,
};

struct Artifact {
	ArtifactKind kind = ArtifactKind::object;
	std::vector<std::string> strings;
	IRModule module;
	std::vector<std::string> imports;
	std::vector<ExportSymbol> imported_exports;
	std::vector<ExportSymbol> exports;
	std::vector<StructDecl> structs;
	std::vector<UniformBinding> uniforms;
	std::vector<StageInterface> stage_interfaces;
	struct EntryPoint {
		std::string name;
		std::string mangled_name;
		StageKind stage = StageKind::none;
		IRId function_id = IRId_None;
	};
	std::vector<EntryPoint> entries;
	std::vector<u8> bytes;
	std::vector<u8> debug_bytes;
};

[[nodiscard]] std::vector<u8> write_artifact(ArtifactKind kind, const IRModule& module);
[[nodiscard]] std::vector<u8> write_debug_artifact(const IRModule& module);
[[nodiscard]] std::vector<u8> write_linked_program(std::span<const Artifact> inputs);
[[nodiscard]] bool read_artifact(std::span<const u8> data, Artifact& artifact, DiagnosticEngine* diagnostics = nullptr);
[[nodiscard]] const char* artifact_extension(ArtifactKind kind);
[[nodiscard]] const char* debug_artifact_extension();

} // namespace rtsl
