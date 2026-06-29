#pragma once

#include "Basic/Diagnostics.hpp"
#include "Lex/Token.hpp"

#include <vector>

namespace rtsl {

class Lexer {
  public:
	Lexer(SourceManager& sources, DiagnosticEngine& diagnostics, u32 file_id);

	[[nodiscard]] std::vector<Token> lex();

private:
	[[nodiscard]] char peek(std::size_t lookahead = 0) const;
	[[nodiscard]] bool at_end(std::size_t lookahead = 0) const;

	void skip_whitespace_and_comments();
	Token lex_identifier_or_keyword();
	Token lex_number();
	Token lex_punctuation();
	Token make_token(TokenKind kind, std::size_t begin, std::size_t end) const;
	void diagnose(std::size_t offset, std::string message);

	SourceManager& sources_;
	DiagnosticEngine& diagnostics_;
	u32 file_id_ = 0;
	std::string_view input_;
	std::size_t cursor_ = 0;
};

} // namespace rtsl
