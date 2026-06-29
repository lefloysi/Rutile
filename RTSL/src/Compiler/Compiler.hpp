#pragma once

#include "Basic/Diagnostics.hpp"
#include "Basic/SourceManager.hpp"
#include "Serialization/Artifact.hpp"

#include <string>
#include <string_view>

namespace rtsl {

struct CompilerInvocation {
    std::string source_name = "<memory>";
};

class CompilerInstance {
public:
    [[nodiscard]] Artifact compile_source(std::string_view source, CompilerInvocation invocation = {});
    void compile_source_to(Artifact& artifact, std::string_view source, CompilerInvocation invocation = {});

    [[nodiscard]] DiagnosticEngine &diagnostics() { return diagnostics_; }
    [[nodiscard]] SourceManager &sources() { return sources_; }

private:
    SourceManager sources_;
    DiagnosticEngine diagnostics_;
};

} // namespace rtsl
