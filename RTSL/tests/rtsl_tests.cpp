#include "Basic/Diagnostics.hpp"
#include "Basic/SourceManager.hpp"
#include "Compiler/Compiler.hpp"
#include "Lex/Lexer.hpp"
#include "Link/Linker.hpp"
#include "Mangle/Mangler.hpp"
#include "Parse/Parser.hpp"
#include "Serialization/Artifact.hpp"
#include "Serialization/TextRTIR.hpp"
#include "rtsl.hpp"

#include <cassert>
#include <cstring>
#include <iostream>
#include <sstream>
#include <string_view>

using namespace rtsl;

namespace {

void source_locations_are_line_column_mapped() {
    SourceManager sources;
    const auto file = sources.add_buffer("test.rtsl", "a\nbc\n");
    const auto loc = sources.location_at(file, 3);
    assert(loc.line == 2);
    assert(loc.column == 2);
}

void diagnostics_render_with_caret() {
    SourceManager sources;
    DiagnosticEngine diagnostics;
    const auto file = sources.add_buffer("test.rtsl", "line one\nbroken line\n");
    diagnostics.report(1, DiagnosticSeverity::error, sources.location_at(file, 9), "test.rtsl", "broken");
    std::ostringstream out;
    diagnostics.render(out, &sources);
    const auto text = out.str();
    assert(text.find("test.rtsl(") != std::string::npos);
    assert(text.find(": error") != std::string::npos);
    assert(text.find("broken line") != std::string::npos);
    assert(text.find('^') != std::string::npos);
}

void lexer_tokenizes_keywords_and_punctuation() {
    SourceManager sources;
    DiagnosticEngine diagnostics;
    const auto file = sources.add_buffer("test.rtsl", "export fn main() -> void {}");
    Lexer lexer(sources, diagnostics, file);
    const auto tokens = lexer.lex();
    assert(!diagnostics.has_error());
    assert(tokens[0].kind == TokenKind::kw_Export);
    assert(tokens[1].kind == TokenKind::kw_Function);
    assert(tokens[2].kind == TokenKind::identifier);
    assert(tokens[5].kind == TokenKind::arrow);
}

void parser_builds_translation_unit() {
    SourceManager sources;
    DiagnosticEngine diagnostics;
    const auto file = sources.add_buffer("test.rtsl", "export fn main() {}");
    Lexer lexer(sources, diagnostics, file);
    const auto tokens = lexer.lex();
    Parser parser(sources, diagnostics, file, tokens);
    const auto unit = parser.parse_translation_unit();
    assert(!diagnostics.has_error());
    assert(unit.declarations.size() == 1);
    assert(unit.declarations.front().kind == DeclKind::function);
    assert(unit.declarations.front().exported);
}

void parser_reports_invalid_function_syntax() {
    SourceManager sources;
    DiagnosticEngine diagnostics;
    const auto file = sources.add_buffer("bad.rtsl", "export fn main( { }");
    Lexer lexer(sources, diagnostics, file);
    const auto tokens = lexer.lex();
    Parser parser(sources, diagnostics, file, tokens);
    const auto unit = parser.parse_translation_unit();
    (void)unit;
    assert(diagnostics.has_error());
    std::ostringstream out;
    diagnostics.render(out, &sources);
    assert(out.str().find("error") != std::string::npos);
}

void artifact_round_trips() {
    IRModule module{.source_name = "test.rtsl"};
    auto bytes = write_artifact(ArtifactKind::program, module);
    Artifact artifact;
    DiagnosticEngine diagnostics;
    assert(read_artifact(bytes, artifact, &diagnostics));
    assert(!diagnostics.has_error());
    assert(artifact.kind == ArtifactKind::program);
    assert(!artifact.bytes.empty());
}

void text_rtir_round_trips_minimal_artifact() {
    IRModule module{.source_name = "test.rtsl"};
    Artifact artifact;
    assert(read_artifact(write_artifact(ArtifactKind::program, module), artifact));

    const auto text = disassemble_artifact(artifact);
    assert(text.find("strings") == std::string::npos);
}

void mangler_keeps_readable_and_glsl_modes_separate() {
    const MangleInput input{
        .name = "Vertex::Vertex",
        .stage = StageKind::none,
        .parameter_types = {"Point"},
    };
    const Mangler mangler;
    assert(mangler.mangle_rtsl(input) == "_ZN6Vertex6VertexE5Point");
    assert(mangler.mangle_for_glsl(input) == "___ZN6Vertex6VertexE5Point");
}

void compiler_emits_object() {
    CompilerInstance compiler;
    auto artifact = compiler.compile_source("export fn main(Point p) -> Vertex { return Vertex(p); }", CompilerInvocation{.source_name = "test.rtsl"});
    assert(!compiler.diagnostics().has_error());
    assert(artifact.kind == ArtifactKind::object);
    assert(!artifact.bytes.empty());
    assert(artifact.module.functions.size() == 1);
    assert(artifact.module.functions.front().parameter_type_names.size() == 1);
    assert(artifact.module.functions.front().parameter_type_names.front() == "Point");
}

constexpr const char *kGraphicsSource = R"(
uniform { mat4 mvp; }
struct Point { vec3 position; vec2 uv; }
struct Vertex { vec4 position; vec2 uv; u32 material; }
struct Fragment { vec4 color; }
input Point {
    location(0) position;
    location(1) uv;
}
varying Vertex {
    clip position;
    smooth uv;
    flat material;
}
output Fragment {
    location(0) color;
}
export fn vert_main(Point p) -> Vertex { return Vertex(p); }
export fn frag_main(Vertex v) -> Fragment { return Fragment(v); }
)";

void stage_interfaces_parse_and_assign_locations() {
    CompilerInstance compiler;
    auto object = compiler.compile_source(kGraphicsSource, CompilerInvocation{.source_name = "gfx.rtsl"});
    assert(!compiler.diagnostics().has_error());
    assert(object.stage_interfaces.size() == 3);

    const StageInterface *varying = nullptr;
    for (const auto &interface : object.stage_interfaces) {
        if (interface.role == StageRole::varying) {
            varying = &interface;
        }
    }
    assert(varying);
    assert(varying->type_name == "Vertex");
    assert(varying->fields[0].name == "position");
    assert(varying->fields[0].builtin == "position");
    assert(!varying->fields[0].has_location);
    assert(varying->fields[1].name == "uv" && varying->fields[1].location == 0);
    assert(varying->fields[2].name == "material" && varying->fields[2].location == 1);
}

void linker_emits_program() {
    CompilerInstance compiler;
    auto object = compiler.compile_source("export fn main() {}", CompilerInvocation{.source_name = "test.rtsl"});
    Linker linker(compiler.diagnostics());
    assert(linker.add_artifact(object));
    auto program = linker.link_program();
    assert(!compiler.diagnostics().has_error());
    assert(program.kind == ArtifactKind::program);
    assert(!program.bytes.empty());
}

void compiler_reports_missing_fragment_stage() {
    CompilerInstance compiler;
    auto object = compiler.compile_source(
        "struct Point { vec3 p; }\nstruct Vertex { vec4 position; }\n"
        "varying Vertex { clip position; }\n"
        "export fn vert_main(Point p) -> Vertex { return Vertex(p); }",
        CompilerInvocation{.source_name = "vertonly.rtsl"});
    Linker linker(compiler.diagnostics());
    assert(linker.add_artifact(object));
    auto program = linker.link_program();
    (void)program;
    assert(compiler.diagnostics().has_error());
}

void c_abi_lifetime_and_errors() {
    rtsl_context ctx = rtslCreateContext();
    assert(ctx);
    const char *source = "export fn main() {}";
    rtsl_module object = rtslCompileSource(ctx, source, std::strlen(source), "abi.rtsl");
    assert(object);
    assert(rtslModuleGetBytecode(object).size > 0);

    rtsl_linker linker = rtslCreateLinker(ctx);
    assert(linker);
    assert(rtslLinkerAddModule(linker, object));
    rtsl_module program = rtslLinkProgram(linker);
    assert(program);
    assert(rtslModuleGetKind(program) == RTSL_OUTPUT_PROGRAM);

    rtslDestroyModule(program);
    rtslDestroyLinker(linker);
    rtslDestroyModule(object);
    rtslDestroyContext(ctx);
}

} // namespace

int main() {
    source_locations_are_line_column_mapped();
    diagnostics_render_with_caret();
    lexer_tokenizes_keywords_and_punctuation();
    parser_builds_translation_unit();
    parser_reports_invalid_function_syntax();
    artifact_round_trips();
    text_rtir_round_trips_minimal_artifact();
    mangler_keeps_readable_and_glsl_modes_separate();
    compiler_emits_object();
    stage_interfaces_parse_and_assign_locations();
    linker_emits_program();
    compiler_reports_missing_fragment_stage();
    c_abi_lifetime_and_errors();
    std::cout << "rtsl-tests passed\n";
    return 0;
}
