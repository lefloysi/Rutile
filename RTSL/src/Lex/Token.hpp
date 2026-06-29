#pragma once

#include "Basic/SourceManager.hpp"
#include "Basic/Types.hpp"

#include <string_view>

namespace rtsl {

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
    X(InOut, "inout") \
    X(Input, "input") \
    X(Output, "output") \
    X(Location, "location") \
    X(Builtin, "builtin")

#define RTSL_PUNCTUATION_TOKENS(X) \
    X(plus, '+') \
    X(minus, '-') \
    X(star, '*') \
    X(slash, '/') \
    X(percent, '%') \
    X(equal, '=') \
    X(less, '<') \
    X(greater, '>') \
    X(bang, '!') \
    X(amp, '&') \
    X(pipe, '|') \
    X(caret, '^') \
    X(tilde, '~') \
    X(left_paren, '(') \
    X(right_paren, ')') \
    X(left_brace, '{') \
    X(right_brace, '}') \
    X(left_bracket, '[') \
    X(right_bracket, ']') \
    X(comma, ',') \
    X(semicolon, ';') \
    X(dot, '.') \
    X(colon, ':')

enum class TokenKind : u16 {
    invalid,
    end_of_file,
    identifier,
    integer_literal,
    float_literal,

#define RTSL_KEYWORD_ENUM(name, spelling) kw_##name,
    RTSL_KEYWORD_TOKENS(RTSL_KEYWORD_ENUM)
#undef RTSL_KEYWORD_ENUM

    equal_equal,
    bang_equal,
    less_equal,
    greater_equal,
    amp_amp,
    pipe_pipe,
    arrow,
    colon_colon,

#define RTSL_PUNCTUATION_ENUM(name, spelling) name,
    RTSL_PUNCTUATION_TOKENS(RTSL_PUNCTUATION_ENUM)
#undef RTSL_PUNCTUATION_ENUM
};

struct Token {
    TokenKind kind = TokenKind::invalid;
    std::string_view text{};
    SourceSpan span{};
};

[[nodiscard]] std::string_view token_spelling(TokenKind kind);
[[nodiscard]] TokenKind keyword_kind(std::string_view text);

} // namespace rtsl
