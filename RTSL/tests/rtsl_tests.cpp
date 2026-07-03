#include "artifact.hpp"
#include "basic_diagnostics.hpp"
#include "basic_source_manager.hpp"
#include "compiler.hpp"
#include "lexer.hpp"
#include "linker.hpp"
#include "mangler.hpp"
#include "parser.hpp"
#include "rtsl.h"
#include "text_rtir.hpp"

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

void diagnostics_render_visual_studio_format() {
	SourceManager sources;
	DiagnosticEngine diagnostics;
	const auto file = sources.add_buffer("bad.rtsl", "x");
	diagnostics.report(42, DiagnosticSeverity::error, sources.location_at(file, 0), "bad.rtsl", "boom");
	std::ostringstream out;
	diagnostics.render(out, &sources);
	const auto text = out.str();
	assert(text.find("bad.rtsl(") != std::string::npos);
	assert(text.find(": error RTSL42: boom") != std::string::npos);
}

void compiler_honors_basic_preprocessor_blocks() {
	CompilerInstance compiler;
	const char* source = R"(
#define ENABLED
#ifdef ENABLED
export fn main() {}
#endif
)";
	auto artifact = compiler.compile_source(source, CompilerInvocation{ .source_name = "pp.rtsl" });
	assert(!compiler.diagnostics().has_error());
	assert(artifact.kind == ArtifactKind::object);
	assert(!artifact.bytes.empty());
}

void lexer_recognizes_string_imports() {
	SourceManager sources;
	DiagnosticEngine diagnostics;
	const auto file = sources.add_buffer("import.rtsl", "import \"foo\";");
	Lexer lexer(sources, diagnostics, file);
	const auto tokens = lexer.lex();
	assert(!diagnostics.has_error());
	assert(tokens[0].kind == TokenKind::kw_Import);
	assert(tokens[1].kind == TokenKind::string_literal);
}

void compiler_reports_missing_imports() {
	CompilerInstance compiler;
	const char* source = R"(
import "missing_module";
export fn main() {}
)";
	auto artifact = compiler.compile_source(source, CompilerInvocation{ .source_name = "missing.rtsl" });
	(void)artifact;
	assert(compiler.diagnostics().has_error());
}

void artifact_round_trips() {
	IRModule module{ .source_name = "test.rtsl" };
	auto bytes = write_artifact(ArtifactKind::program, module);
	Artifact artifact;
	DiagnosticEngine diagnostics;
	assert(read_artifact(bytes, artifact, &diagnostics));
	assert(!diagnostics.has_error());
	assert(artifact.kind == ArtifactKind::program);
	assert(!artifact.bytes.empty());
}

void text_rtir_round_trips_minimal_artifact() {
	IRModule module{ .source_name = "test.rtsl" };
	Artifact artifact;
	assert(read_artifact(write_artifact(ArtifactKind::program, module), artifact));

	const auto text = disassemble_artifact(artifact);
	assert(text.find("strings") == std::string::npos);
}

void artifact_round_trips_imports() {
	IRModule module{ .source_name = "test.rtsl" };
	module.imports = { "shared/math", "core/types" };
	module.imported_exports = { ExportSymbol{ .name = "dot", .kind = "function", .type = "float" } };
	module.exports = { ExportSymbol{ .name = "main", .kind = "function", .type = "void" } };
	Artifact artifact;
	assert(read_artifact(write_artifact(ArtifactKind::module, module), artifact));
	assert(artifact.module.imports.size() == 2);
	assert(artifact.module.imports[0] == "shared/math");
	assert(artifact.module.imports[1] == "core/types");
	assert(artifact.module.exports.size() == 1);
	assert(artifact.module.exports[0].name == "main");
	assert(artifact.module.imported_exports.size() == 1);
	assert(artifact.module.imported_exports[0].name == "dot");
}

void mangler_keeps_readable_and_glsl_modes_separate() {
	const MangleInput input{
		.name = "Vertex::Vertex",
		.stage = StageKind::none,
		.parameter_types = { "Point" },
	};
	const Mangler mangler;
	assert(mangler.mangle_rtsl(input) == "_ZN6Vertex6VertexE5Point");
	assert(mangler.mangle_for_glsl(input) == "___ZN6Vertex6VertexE5Point");
}

void compiler_emits_object() {
	CompilerInstance compiler;
	auto artifact = compiler.compile_source("export fn main(Point p) -> Vertex { return Vertex(p); }", CompilerInvocation{ .source_name = "test.rtsl" });
	assert(!compiler.diagnostics().has_error());
	assert(artifact.kind == ArtifactKind::object);
	assert(!artifact.bytes.empty());
	assert(artifact.module.functions.size() == 1);
	assert(artifact.module.functions.front().parameter_ids.size() == 1);
}

constexpr const char* kGraphicsSource = R"(
uniform { mat4 mvp; }
struct Point { vec3 position; vec2 uv; }
struct Vertex { vec4 position; vec2 uv; u32 material; }
struct Fragment { vec4 color; }
input Point {
    location(0) position;
    location(1) uv;
}
using RasterVertex = Vertex : clip position, smooth uv, flat material;
output Fragment {
    location(0) color;
}
export fn vert_main(Point p) -> RasterVertex { return Vertex(p); }
export fn frag_main(Vertex v) -> Fragment { return Fragment(v); }
)";

void stage_interfaces_parse_and_assign_locations() {
	CompilerInstance compiler;
	auto object = compiler.compile_source(kGraphicsSource, CompilerInvocation{ .source_name = "gfx.rtsl" });
	assert(!compiler.diagnostics().has_error());
	assert(object.stage_interfaces.size() == 3);

	const StageInterface* varying = nullptr;
	for (const auto& interface : object.stage_interfaces) {
		if (interface.role == StageRole::varying) {
			varying = &interface;
		}
	}
	assert(varying);
	assert(varying->type_name == "Vertex");
	assert(varying->fields[0].name == "position");
	assert(varying->fields[0].builtin == BuiltinSlot::position);
	assert(varying->fields[0].location == StageIOField::kNoLocation);
	assert(varying->fields[1].name == "uv" && varying->fields[1].location == 0);
	assert(varying->fields[2].name == "material" && varying->fields[2].location == 1);
}

void linker_emits_program() {
	CompilerInstance compiler;
	auto object = compiler.compile_source("export fn main() {}", CompilerInvocation{ .source_name = "test.rtsl" });
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
		"using RasterVertex = Vertex : clip position;\n"
		"export fn vert_main(Point p) -> RasterVertex { return Vertex(p); }",
		CompilerInvocation{ .source_name = "vertonly.rtsl" }
	);
	Linker linker(compiler.diagnostics());
	assert(linker.add_artifact(object));
	auto program = linker.link_program();
	(void)program;
	assert(compiler.diagnostics().has_error());
}

void c_abi_lifetime_and_errors() {
	rtsl_context ctx = rtslCreateContext();
	assert(ctx);
	const char* source = "export fn main() {}";
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

// Confirms the layout rule (implicit or explicit) actually reaches the IR
// decoration table with the right offsets for a mixed float/vec3/float struct
// — that's the case where std140/std430 (padding) and scalar (no padding)
// disagree.
void layout_rule_emits_offsets() {
	auto compile_and_offsets = [](const char* src, u32& off_a, u32& off_b, u32& off_c) {
		CompilerInstance compiler;
		auto artifact = compiler.compile_source(src, CompilerInvocation{ .source_name = "layout.rtsl" });
		assert(!compiler.diagnostics().has_error());
		std::vector<u32> offsets;
		for (const auto& dec : artifact.module.decorations) {
			if (dec.kind == IRDecorationKind::Offset) {
				assert(!dec.literals.empty());
				offsets.push_back(dec.literals[0]);
			}
		}
		assert(offsets.size() >= 3);
		off_a = offsets[0];
		off_b = offsets[1];
		off_c = offsets[2];
	};

	// std140 default (UniformBuffer): vec3 rounds to vec4 alignment (16), so
	// `float a` at 0, `vec3 b` at 16, `float c` after b at 28.
	u32 a, b, c;
	compile_and_offsets(
		"uniform { UniformBuffer u; }\n"
		"layout u : struct { f32 a; vec3 b; f32 c; };\n"
		"export fn main() {}\n",
		a,
		b,
		c
	);
	assert(a == 0 && b == 16 && c == 28);

	// Scalar override: no vec3 alignment inflation. `a` at 0, `b` at 4,
	// `c` at 16.
	compile_and_offsets(
		"uniform { UniformBuffer u; }\n"
		"layout u : scalar struct { f32 a; vec3 b; f32 c; };\n"
		"export fn main() {}\n",
		a,
		b,
		c
	);
	assert(a == 0 && b == 4 && c == 16);
}

void layout_rule_qualifier_parses() {
	SourceManager sources;
	DiagnosticEngine diagnostics;
	const char* src = "uniform { UniformBuffer a; StorageBuffer b; StorageBuffer c; }\n"
					  "layout a : mat4;\n"
					  "layout b : std430 struct { f32 x; };\n"
					  "layout c : scalar struct { vec3 v; };\n";
	const auto file = sources.add_buffer("layout.rtsl", src);
	Lexer lexer(sources, diagnostics, file);
	const auto tokens = lexer.lex();
	Parser parser(sources, diagnostics, file, tokens);
	const auto unit = parser.parse_translation_unit();
	assert(!diagnostics.has_error());
	assert(unit.layouts.size() == 3);
	assert(unit.layouts[0].rule == LayoutRule::unset);
	assert(unit.layouts[1].rule == LayoutRule::std430);
	assert(unit.layouts[2].rule == LayoutRule::scalar);
}

} // namespace

int main() {
	source_locations_are_line_column_mapped();
	diagnostics_render_with_caret();
	lexer_tokenizes_keywords_and_punctuation();
	parser_builds_translation_unit();
	parser_reports_invalid_function_syntax();
	diagnostics_render_visual_studio_format();
	compiler_honors_basic_preprocessor_blocks();
	lexer_recognizes_string_imports();
	compiler_reports_missing_imports();
	artifact_round_trips();
	artifact_round_trips_imports();
	text_rtir_round_trips_minimal_artifact();
	mangler_keeps_readable_and_glsl_modes_separate();
	compiler_emits_object();
	stage_interfaces_parse_and_assign_locations();
	layout_rule_qualifier_parses();
	layout_rule_emits_offsets();
	linker_emits_program();
	compiler_reports_missing_fragment_stage();
	c_abi_lifetime_and_errors();
	std::cout << "rtsl-tests passed\n";
	return 0;
}
