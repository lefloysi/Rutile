#pragma once

#include "source_location.hpp"
#include "types.hpp"

#include <string_view>

#define RTSL_KEYWORD_TOKENS(X) \
	X(Import, "import") \
	X(Export, "export") \
	X(Namespace, "namespace") \
	X(Struct, "struct") \
	X(Using, "using") \
	X(Uniform, "uniform") \
	X(Varying, "varying") \
	X(Function, "fn") \
	X(Const, "const") \
	X(Auto, "auto") \
	X(Void, "void") \
	X(If, "if") \
	X(Else, "else") \
	X(While, "while") \
	X(Do, "do") \
	X(For, "for") \
	X(Return, "return") \
	X(Clip, "clip") \
	X(Smooth, "smooth") \
	X(Flat, "flat") \
	X(ReadOnly, "readonly") \
	X(WriteOnly, "writeonly") \
	X(True, "true") \
	X(False, "false") \
	X(InOut, "inout")

#define RTSL_OPERATOR_TOKENS(X) \
	X(Plus, "+") \
	X(Minus, "-") \
	X(Star, "*") \
	X(Slash, "/") \
	X(Percent, "%") \
	X(Equal, "=") \
	X(EqualEqual, "==") \
	X(BangEqual, "!=") \
	X(Less, "<") \
	X(LessEqual, "<=") \
	X(Greater, ">") \
	X(GreaterEqual, ">=") \
	X(Bang, "!") \
	X(AndAnd, "&&") \
	X(OrOr, "||") \
	X(Ampersand, "&") \
	X(Pipe, "|") \
	X(Caret, "^") \
	X(Tilde, "~") \
	X(Arrow, "->") \
	X(ColonColon, "::")

#define RTSL_PUNCTUATOR_TOKENS(X) \
	X(LeftParen, "(") \
	X(RightParen, ")") \
	X(LeftBrace, "{") \
	X(RightBrace, "}") \
	X(LeftBracket, "[") \
	X(RightBracket, "]") \
	X(Comma, ",") \
	X(Semicolon, ";") \
	X(Dot, ".")

enum class TokenType : u16 {
	invalid,
	end_of_file,
	identifier,
	integer_literal,
	float_literal,

#define RTSL_KEYWORD_ENUM(name, spelling) kw##name,
	RTSL_KEYWORD_TOKENS(RTSL_KEYWORD_ENUM)
#undef RTSL_KEYWORD_ENUM

#define RTSL_OPERATOR_ENUM(name, spelling) op##name,
	RTSL_OPERATOR_TOKENS(RTSL_OPERATOR_ENUM)
#undef RTSL_OPERATOR_ENUM

#define RTSL_PUNCTUATOR_ENUM(name, spelling) punct##name,
	RTSL_PUNCTUATOR_TOKENS(RTSL_PUNCTUATOR_ENUM)
#undef RTSL_PUNCTUATOR_ENUM

};

struct Token {
	TokenType type = TokenType::invalid;
	std::string_view lexeme{};
	SourceSpan span{};
	SourceLocation location{};

	Token() {}
};

struct TokenInfo {
	TokenType type;
	std::string_view text;
};
