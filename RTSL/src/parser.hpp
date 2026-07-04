#pragma once

#include "ast.hpp"
#include "basic_diagnostics.hpp"
#include "token.hpp"

#include <span>

namespace rtsl {

class Parser {
  public:
	Parser(SourceManager& sources, DiagnosticEngine& diagnostics, u32 file_id, std::span<const Token> tokens);

	[[nodiscard]] TranslationUnit parse_translation_unit();

  private:
	[[nodiscard]] const Token& peek(std::size_t lookahead = 0) const;
	[[nodiscard]] bool at(TokenKind kind) const;
	[[nodiscard]] bool consume(TokenKind kind);
	[[nodiscard]] bool at_end() const;

	Decl parse_declaration();
	Decl parse_import(bool exported);
	Decl parse_named_declaration(DeclKind kind, bool exported);
	void parse_function_signature(Decl& decl);
	void parse_function_body(Decl& decl);
	void parse_function_body_into(Decl& decl, bool stop_on_right_brace);
	void parse_statement_list_into(std::vector<Decl::BodyStatement>& out, bool stop_on_right_brace);
	Decl::BodyStatement parse_statement_from_tokens(const std::vector<Token>& tokens) const;
	Decl::BodyStatement parse_if_statement(const std::vector<Token>& tokens) const;
	Decl::BodyStatement parse_while_statement(const std::vector<Token>& tokens) const;
	Decl::BodyStatement parse_do_statement(const std::vector<Token>& tokens) const;
	Decl::BodyStatement parse_for_statement(const std::vector<Token>& tokens) const;
	std::string tokens_to_text(std::span<const Token> tokens) const;
	void parse_uniform_scope(const Decl& decl);
	// Read `A[::B[::C[::~D]]]` at the current cursor. Returns empty when the
	// cursor isn't on an identifier. Diagnoses mid-name failures against the
	// offending token.
	std::string parse_scoped_name();
	// Body-specific handlers for parse_named_declaration's post-name dispatch.
	// Each expects `decl` to have its name populated and drives the rest.
	void parse_function_decl(Decl& decl);
	void parse_struct_decl(Decl& decl);
	void parse_stage_interface(const Decl& decl);
	void parse_layout();
	void parse_using_alias();
	// Parse a comma-separated `intrinsic field` list forming a return-type
	// boundary spec. The leading `:` must have been consumed by the caller.
	// Terminates at `{` (function body) or `;` (forward decl). Only
	// compiler-known pipeline intrinsics are accepted for the operation;
	// unknown identifiers are diagnosed.
	void parse_return_boundary(std::string base_type);
	// If a `:` follows the return type, consume it and parse the boundary
	// spec. Otherwise leaves the cursor untouched.
	void maybe_parse_return_boundary(const std::string& base_type);
	// Resolve `name` through recorded type aliases to its ultimate base type.
	// Types the parser doesn't know as aliases pass through unchanged.
	[[nodiscard]] std::string resolve_alias(std::string_view name) const;
	StructField parse_field_declaration();
	std::string collect_type_until(TokenKind stop_a, TokenKind stop_b);
	void skip_to_declaration_boundary(bool consume_right_brace = false);
	void skip_balanced_block();
	std::string append_token_text(std::string statement, const Token& token) const;
	SourceSpan statement_span(const Token& begin, const Token& end) const;
	Decl::Expr parse_expression(std::string_view text) const;
	std::string collect_type_tokens_until_identifier();
	void diagnose(const Token& token, std::string_view message);

	TranslationUnit* unit_ = nullptr;

	SourceManager& sources_;
	DiagnosticEngine& diagnostics_;
	u32 file_id_ = 0;
	std::span<const Token> tokens_;
	std::size_t cursor_ = 0;
	u32 next_anonymous_block_id_ = 0;
};

} // namespace rtsl
