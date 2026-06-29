#pragma once

#include "basic_diagnostics.hpp"
#include "basic_source_manager.hpp"
#include "artifact.hpp"

#include <string>
#include <string_view>
#include <vector>

namespace rtsl {

struct CompilerInvocation {
    std::string source_name = "<memory>";
    std::vector<std::string> defines;
    std::vector<std::string> import_paths;
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
