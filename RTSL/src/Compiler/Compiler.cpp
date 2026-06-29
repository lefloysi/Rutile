#include "Compiler/Compiler.hpp"

#include "IR/IR.hpp"
#include "Lex/Lexer.hpp"
#include "Parse/Parser.hpp"
#include "Sema/Sema.hpp"

namespace rtsl {

Artifact CompilerInstance::compile_source(std::string_view source, CompilerInvocation invocation) {
    Artifact artifact;
    compile_source_to(artifact, source, std::move(invocation));
    return artifact;
}

void CompilerInstance::compile_source_to(Artifact& artifact, std::string_view source, CompilerInvocation invocation) {
    diagnostics_.clear();
    artifact = Artifact{.kind = ArtifactKind::object};
    const auto file_id = sources_.add_buffer(std::move(invocation.source_name), std::string(source));

    Lexer lexer(sources_, diagnostics_, file_id);
    const auto tokens = lexer.lex();

    Parser parser(sources_, diagnostics_, file_id, tokens);
    auto ast = parser.parse_translation_unit();

    Sema sema(sources_, diagnostics_);
    auto semantic_module = sema.analyze(ast);
    auto ir = lower_to_ir(semantic_module, &diagnostics_);

    if (!diagnostics_.has_error() && verify_ir(ir)) {
        artifact.bytes = write_artifact(ArtifactKind::object, ir);
        artifact.module = ir;
        artifact.strings.clear();
        artifact.structs = ir.structs;
        artifact.uniforms = ir.uniforms;
        artifact.stage_interfaces = ir.stage_interfaces;
        artifact.entries.clear();
        for (const auto &function : ir.functions) {
            if (function.stage == StageKind::none) {
                continue;
            }
            artifact.entries.push_back(Artifact::EntryPoint{
                .name = std::string(stage_entry_name(function.stage)),
                .mangled_name = std::string(stage_entry_name(function.stage)),
                .stage = function.stage,
                .function_id = function.result_id,
            });
        }
        artifact.debug_bytes = write_debug_artifact(ir);
    }
}

} // namespace rtsl
