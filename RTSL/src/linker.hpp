#pragma once

#include "artifact.hpp"
#include "basic_diagnostics.hpp"

#include <vector>

namespace rtsl {

class Linker {
  public:
	explicit Linker(DiagnosticEngine& diagnostics);

	bool add_artifact_bytes(std::span<const u8> bytes);
	bool add_artifact(Artifact artifact);
	// Produce a runnable program. Requires at least one stage entry point;
	// the program is what Rutile backends actually load.
	[[nodiscard]] Artifact link_program();
	// Produce a linked library. No entry points required; can be fed into
	// subsequent link-program or link-library invocations.
	[[nodiscard]] Artifact link_library();

  private:
	// Diagnose missing or conflicting stage entry points for a linked program.
	void validate_program_stages(const Artifact& program);

	DiagnosticEngine& diagnostics_;
	std::vector<Artifact> inputs_;
};

// Extract the public-interface view of a library or object artifact. The
// returned artifact is suitable for serializing as .rtslm: it keeps only
// exported function signatures (with empty bodies, like forward
// declarations) and the exported structs the signatures reference. Returns
// an artifact with kind=module and an empty functions list when nothing is
// exported, so callers can skip writing an .rtslm sidecar.
[[nodiscard]] Artifact extract_module_interface(const Artifact& source);

} // namespace rtsl
