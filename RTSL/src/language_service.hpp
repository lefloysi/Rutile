#pragma once

#include "ast.hpp"
#include "basic_diagnostics.hpp"
#include "compiler.hpp"
#include "lexer.hpp"
#include "parser.hpp"
#include "sema.hpp"

#include <string>
#include <string_view>
#include <vector>

namespace rtsl {

enum class LanguageSymbolKind : u8 {
	unknown,
	import,
	function,
	struct_decl,
	uniform,
	input,
	output,
	namespace_decl,
	type_alias,
	parameter,
	field,
	local,
};

struct LanguageToken {
	TokenKind kind = TokenKind::invalid;
	std::size_t offset = 0;
	std::size_t length = 0;
	u32 line = 1;
	u32 column = 1;
};

struct LanguageSymbol {
	LanguageSymbolKind kind = LanguageSymbolKind::unknown;
	std::string name;
	std::string detail;
	SourceSpan span{};
	bool exported = false;
};

struct LanguageAnalysis {
	TranslationUnit ast;
	SemanticModule sema;
	std::vector<Token> tokens;
	std::vector<LanguageSymbol> symbols;
};

class LanguageService {
  public:
	LanguageAnalysis analyze(std::string_view source, CompilerInvocation invocation = {});

  private:
	CompilerInstance compiler_;
};

} // namespace rtsl
