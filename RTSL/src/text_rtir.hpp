#pragma once

#include "basic_diagnostics.hpp"
#include "artifact.hpp"

#include <string>
#include <string_view>

namespace rtsl {

[[nodiscard]] std::string disassemble_artifact(const Artifact &artifact);
[[nodiscard]] bool assemble_text_rtir(std::string_view text, Artifact &artifact,
                                      DiagnosticEngine *diagnostics = nullptr);

} // namespace rtsl
