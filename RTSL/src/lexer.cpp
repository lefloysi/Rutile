#include "lexer.hpp"

#include <array>
#include <cctype>

namespace rtsl {

namespace {

bool is_identifier_start(char c) {
    return std::isalpha(static_cast<unsigned char>(c)) || c == '_';
}

bool is_identifier_continue(char c) {
    return std::isalnum(static_cast<unsigned char>(c)) || c == '_';
}

} // namespace

std::string_view token_spelling(TokenKind kind) {
    switch (kind) {
#define RTSL_KEYWORD_SPELLING(name, spelling) case TokenKind::kw_##name: return spelling;
        RTSL_KEYWORD_TOKENS(RTSL_KEYWORD_SPELLING)
#undef RTSL_KEYWORD_SPELLING
    case TokenKind::plus: return "+";
    case TokenKind::minus: return "-";
    case TokenKind::star: return "*";
    case TokenKind::slash: return "/";
    case TokenKind::percent: return "%";
    case TokenKind::equal: return "=";
    case TokenKind::equal_equal: return "==";
    case TokenKind::bang_equal: return "!=";
    case TokenKind::less: return "<";
    case TokenKind::less_equal: return "<=";
    case TokenKind::greater: return ">";
    case TokenKind::greater_equal: return ">=";
    case TokenKind::bang: return "!";
    case TokenKind::amp_amp: return "&&";
    case TokenKind::pipe_pipe: return "||";
    case TokenKind::amp: return "&";
    case TokenKind::pipe: return "|";
    case TokenKind::caret: return "^";
    case TokenKind::tilde: return "~";
    case TokenKind::arrow: return "->";
    case TokenKind::colon_colon: return "::";
    case TokenKind::colon: return ":";
    case TokenKind::left_paren: return "(";
    case TokenKind::right_paren: return ")";
    case TokenKind::left_brace: return "{";
    case TokenKind::right_brace: return "}";
    case TokenKind::left_bracket: return "[";
    case TokenKind::right_bracket: return "]";
    case TokenKind::comma: return ",";
    case TokenKind::semicolon: return ";";
    case TokenKind::dot: return ".";
    default: return "";
    }
}

TokenKind keyword_kind(std::string_view text) {
#define RTSL_KEYWORD_MATCH(name, spelling) if (text == spelling) return TokenKind::kw_##name;
    RTSL_KEYWORD_TOKENS(RTSL_KEYWORD_MATCH)
#undef RTSL_KEYWORD_MATCH
    return TokenKind::identifier;
}

Lexer::Lexer(SourceManager &sources, DiagnosticEngine &diagnostics, u32 file_id)
    : sources_(sources), diagnostics_(diagnostics), file_id_(file_id), input_(sources.buffer(file_id)) {}

std::vector<Token> Lexer::lex() {
    std::vector<Token> tokens;

    while (true) {
        skip_whitespace_and_comments();
        if (at_end()) {
            tokens.push_back(make_token(TokenKind::end_of_file, cursor_, cursor_));
            break;
        }

        const char c = peek();
    if (is_identifier_start(c)) {
        tokens.push_back(lex_identifier_or_keyword());
    } else if (c == '"') {
        tokens.push_back(lex_string());
    } else if (std::isdigit(static_cast<unsigned char>(c))) {
        tokens.push_back(lex_number());
    } else {
            tokens.push_back(lex_punctuation());
        }
    }

    return tokens;
}

char Lexer::peek(std::size_t lookahead) const {
    return at_end(lookahead) ? '\0' : input_[cursor_ + lookahead];
}

bool Lexer::at_end(std::size_t lookahead) const {
    return cursor_ + lookahead >= input_.size();
}

void Lexer::skip_whitespace_and_comments() {
    bool consumed = true;
    while (consumed && !at_end()) {
        consumed = false;
        while (!at_end() && std::isspace(static_cast<unsigned char>(peek()))) {
            ++cursor_;
            consumed = true;
        }

        if (peek() == '/' && peek(1) == '/') {
            cursor_ += 2;
            while (!at_end() && peek() != '\n') {
                ++cursor_;
            }
            consumed = true;
        } else if (peek() == '/' && peek(1) == '*') {
            const auto comment_begin = cursor_;
            cursor_ += 2;
            while (!at_end() && !(peek() == '*' && peek(1) == '/')) {
                ++cursor_;
            }
            if (at_end()) {
                diagnose(comment_begin, "unterminated block comment");
                return;
            }
            cursor_ += 2;
            consumed = true;
        }
    }
}

Token Lexer::lex_identifier_or_keyword() {
    const auto begin = cursor_;
    ++cursor_;
    while (!at_end() && is_identifier_continue(peek())) {
        ++cursor_;
    }
    const auto text = input_.substr(begin, cursor_ - begin);
    return make_token(keyword_kind(text), begin, cursor_);
}

Token Lexer::lex_number() {
    const auto begin = cursor_;
    while (!at_end() && std::isdigit(static_cast<unsigned char>(peek()))) {
        ++cursor_;
    }

    bool is_float = false;
    if (peek() == '.' && std::isdigit(static_cast<unsigned char>(peek(1)))) {
        is_float = true;
        ++cursor_;
        while (!at_end() && std::isdigit(static_cast<unsigned char>(peek()))) {
            ++cursor_;
        }
    }

    return make_token(is_float ? TokenKind::float_literal : TokenKind::integer_literal, begin, cursor_);
}

Token Lexer::lex_string() {
    const auto begin = cursor_;
    ++cursor_;
    while (!at_end() && peek() != '"') {
        if (peek() == '\\' && !at_end(1)) {
            cursor_ += 2;
            continue;
        }
        ++cursor_;
    }
    if (!at_end()) {
        ++cursor_;
    }
    return make_token(TokenKind::string_literal, begin, cursor_);
}

Token Lexer::lex_punctuation() {
    const auto begin = cursor_;
    const char c = peek();
    const char n = peek(1);

    auto two = [&](TokenKind kind) {
        cursor_ += 2;
        return make_token(kind, begin, cursor_);
    };
    auto one = [&](TokenKind kind) {
        ++cursor_;
        return make_token(kind, begin, cursor_);
    };

    if (c == '=' && n == '=') return two(TokenKind::equal_equal);
    if (c == '!' && n == '=') return two(TokenKind::bang_equal);
    if (c == '<' && n == '=') return two(TokenKind::less_equal);
    if (c == '>' && n == '=') return two(TokenKind::greater_equal);
    if (c == '&' && n == '&') return two(TokenKind::amp_amp);
    if (c == '|' && n == '|') return two(TokenKind::pipe_pipe);
    if (c == '-' && n == '>') return two(TokenKind::arrow);
    if (c == ':' && n == ':') return two(TokenKind::colon_colon);

#define RTSL_PUNCTUATION_MATCH(name, spelling) if (c == spelling) return one(TokenKind::name);
    RTSL_PUNCTUATION_TOKENS(RTSL_PUNCTUATION_MATCH)
#undef RTSL_PUNCTUATION_MATCH

    diagnose(begin, "invalid character in source");
    ++cursor_;
    return make_token(TokenKind::invalid, begin, cursor_);
}

Token Lexer::make_token(TokenKind kind, std::size_t begin, std::size_t end) const {
    return Token{
        .kind = kind,
        .text = input_.substr(begin, end - begin),
        .span = SourceSpan{.begin = sources_.location_at(file_id_, begin), .length = end - begin},
    };
}

void Lexer::diagnose(std::size_t offset, std::string message) {
    diagnostics_.report(1001, DiagnosticSeverity::error, sources_.location_at(file_id_, offset),
                        std::string(sources_.name(file_id_)), std::move(message));
}

} // namespace rtsl
