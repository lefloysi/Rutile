#pragma once

#include "AST/AST.hpp"
#include "Basic/Diagnostics.hpp"

#include <string>
#include <vector>

namespace rtsl {

struct SemanticSymbol {
    DeclKind kind = DeclKind::unknown;
    std::string name;
    std::vector<ParameterDecl> parameters;
    std::string return_type;
    std::vector<Decl::BodyStatement> body_statements;
    bool exported = false;
};

struct SemanticModule {
    std::string source_name;
    std::vector<SemanticSymbol> symbols;
    std::vector<StructDecl> structs;
    std::vector<UniformBinding> uniforms;
    std::vector<StageInterface> stage_interfaces;
};

class Sema {
public:
    Sema(SourceManager &sources, DiagnosticEngine &diagnostics);

    [[nodiscard]] SemanticModule analyze(const TranslationUnit &unit);

private:
    SourceManager &sources_;
    DiagnosticEngine &diagnostics_;
};

} // namespace rtsl
