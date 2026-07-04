#include "language_service.hpp"

namespace rtsl {

static LanguageSymbolKind symbol_kind_from_decl(DeclKind kind) {
	switch (kind) {
	case DeclKind::import: return LanguageSymbolKind::import;
	case DeclKind::function: return LanguageSymbolKind::function;
	case DeclKind::struct_decl: return LanguageSymbolKind::struct_decl;
	case DeclKind::uniform: return LanguageSymbolKind::uniform;
	case DeclKind::input: return LanguageSymbolKind::input;
	case DeclKind::output: return LanguageSymbolKind::output;
	case DeclKind::namespace_decl: return LanguageSymbolKind::namespace_decl;
	default: return LanguageSymbolKind::unknown;
	}
}

LanguageAnalysis LanguageService::analyze(std::string_view source, CompilerInvocation invocation) {
	LanguageAnalysis analysis;
	compiler_.diagnostics().clear();

	const auto file_id = compiler_.sources().add_buffer(std::move(invocation.source_name), source);
	Lexer lexer(compiler_.sources(), compiler_.diagnostics(), file_id);
	analysis.tokens = lexer.lex();

	Parser parser(compiler_.sources(), compiler_.diagnostics(), file_id, analysis.tokens);
	analysis.ast = parser.parse_translation_unit();

	Sema sema(compiler_.sources(), compiler_.diagnostics());
	analysis.sema = sema.analyze(analysis.ast);

	for (const auto& decl : analysis.ast.declarations) {
		analysis.symbols.push_back(LanguageSymbol{
			.kind = symbol_kind_from_decl(decl.kind),
			.name = decl.name,
			.detail = decl.return_type,
			.span = decl.span,
			.exported = decl.exported,
		});
	}
	for (const auto& alias : analysis.ast.type_aliases) {
		analysis.symbols.push_back(LanguageSymbol{
			.kind = LanguageSymbolKind::type_alias,
			.name = alias.name,
			.detail = alias.base,
		});
	}
	for (const auto& structure : analysis.ast.structs) {
		analysis.symbols.push_back(LanguageSymbol{
			.kind = LanguageSymbolKind::struct_decl,
			.name = structure.name,
		});
	}
	for (const auto& uniform : analysis.ast.uniforms) {
		analysis.symbols.push_back(LanguageSymbol{
			.kind = LanguageSymbolKind::uniform,
			.name = uniform.name,
			.detail = uniform.type,
		});
	}

	return analysis;
}

} // namespace rtsl
