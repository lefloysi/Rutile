#pragma once

#include "ast.hpp"
#include "basic_diagnostics.hpp"
#include "token.hpp"

#include <span>

namespace rtsl {

class Parser {
public:
    Parser(SourceManager &sources, DiagnosticEngine &diagnostics, u32 file_id, std::span<const Token> tokens);

    [[nodiscard]] TranslationUnit parse_translation_unit();

private:
    [[nodiscard]] const Token &peek(std::size_t lookahead = 0) const;
    [[nodiscard]] bool at(TokenKind kind) const;
    [[nodiscard]] bool consume(TokenKind kind);
    [[nodiscard]] bool at_end() const;

    Decl parse_declaration();
    Decl parse_import(bool exported);
    Decl parse_named_declaration(DeclKind kind, bool exported);
    void parse_function_signature(Decl &decl);
    void parse_function_body(Decl &decl);
    void parse_function_body_into(Decl &decl, bool stop_on_right_brace);
    void parse_statement_list_into(std::vector<Decl::BodyStatement> &out, bool stop_on_right_brace);
    Decl::BodyStatement parse_statement_from_tokens(const std::vector<Token> &tokens) const;
    Decl::BodyStatement parse_if_statement(const std::vector<Token> &tokens) const;
    Decl::BodyStatement parse_while_statement(const std::vector<Token> &tokens) const;
    Decl::BodyStatement parse_do_statement(const std::vector<Token> &tokens) const;
    Decl::BodyStatement parse_for_statement(const std::vector<Token> &tokens) const;
    std::string tokens_to_text(std::span<const Token> tokens) const;
    void parse_uniform_scope(const Decl &decl);
    void parse_stage_interface(const Decl &decl);
    StructField parse_field_declaration();
    std::string collect_type_until(TokenKind stop_a, TokenKind stop_b);
    void skip_to_declaration_boundary(bool consume_right_brace = false);
    void skip_balanced_block();
    std::string append_token_text(std::string statement, const Token &token) const;
    SourceSpan statement_span(const Token &begin, const Token &end) const;
    Decl::Expr parse_expression(std::string_view text) const;
    std::string collect_type_tokens_until_identifier();
    void diagnose(const Token &token, std::string message);

    TranslationUnit *unit_ = nullptr;

    SourceManager &sources_;
    DiagnosticEngine &diagnostics_;
    u32 file_id_ = 0;
    std::span<const Token> tokens_;
    std::size_t cursor_ = 0;
    u32 next_anonymous_block_id_ = 0;
};

} // namespace rtsl
