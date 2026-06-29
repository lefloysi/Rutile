#include "Parse/Parser.hpp"

namespace rtsl {

namespace {

std::string trim(std::string_view text) {
    while (!text.empty() && std::isspace(static_cast<unsigned char>(text.front()))) {
        text.remove_prefix(1);
    }
    while (!text.empty() && std::isspace(static_cast<unsigned char>(text.back()))) {
        text.remove_suffix(1);
    }
    return std::string(text);
}

struct ExprToken {
    enum class Kind {
        end,
        ident,
        integer,
        floating,
        lparen,
        rparen,
        comma,
        dot,
        scope,
        plus,
        minus,
        star,
        slash,
        percent,
    };

    Kind kind = Kind::end;
    std::string text;
};

class ExprLexer {
public:
    explicit ExprLexer(std::string_view text) : text_(text) {}

    ExprToken peek() {
        const auto save = pos_;
        const auto tok = next();
        pos_ = save;
        return tok;
    }

    ExprToken next() {
        skip_ws();
        if (pos_ >= text_.size()) return {};
        const char c = text_[pos_];
        if (std::isalpha(static_cast<unsigned char>(c)) || c == '_') return ident();
        if (std::isdigit(static_cast<unsigned char>(c))) return number();
        if (c == '(') { ++pos_; return {ExprToken::Kind::lparen, "("}; }
        if (c == ')') { ++pos_; return {ExprToken::Kind::rparen, ")"}; }
        if (c == ',') { ++pos_; return {ExprToken::Kind::comma, ","}; }
        if (c == '.') { ++pos_; return {ExprToken::Kind::dot, "."}; }
        if (c == ':' && pos_ + 1 < text_.size() && text_[pos_ + 1] == ':') {
            pos_ += 2;
            return {ExprToken::Kind::scope, "::"};
        }
        if (c == '+') { ++pos_; return {ExprToken::Kind::plus, "+"}; }
        if (c == '-') { ++pos_; return {ExprToken::Kind::minus, "-"}; }
        if (c == '*') { ++pos_; return {ExprToken::Kind::star, "*"}; }
        if (c == '/') { ++pos_; return {ExprToken::Kind::slash, "/"}; }
        if (c == '%') { ++pos_; return {ExprToken::Kind::percent, "%"}; }
        ++pos_;
        return {};
    }

private:
    void skip_ws() {
        while (pos_ < text_.size() && std::isspace(static_cast<unsigned char>(text_[pos_]))) {
            ++pos_;
        }
    }

    ExprToken ident() {
        const auto start = pos_;
        while (pos_ < text_.size() &&
               (std::isalnum(static_cast<unsigned char>(text_[pos_])) || text_[pos_] == '_')) {
            ++pos_;
        }
        return {ExprToken::Kind::ident, std::string(text_.substr(start, pos_ - start))};
    }

    ExprToken number() {
        const auto start = pos_;
        bool floating = false;
        while (pos_ < text_.size()) {
            const char c = text_[pos_];
            if (std::isdigit(static_cast<unsigned char>(c))) {
                ++pos_;
            } else if (c == '.') {
                floating = true;
                ++pos_;
            } else {
                break;
            }
        }
        return {floating ? ExprToken::Kind::floating : ExprToken::Kind::integer,
                std::string(text_.substr(start, pos_ - start))};
    }

    std::string_view text_;
    std::size_t pos_ = 0;
};

} // namespace

Parser::Parser(SourceManager &sources, DiagnosticEngine &diagnostics, u32 file_id, std::span<const Token> tokens)
    : sources_(sources), diagnostics_(diagnostics), file_id_(file_id), tokens_(tokens) {}

TranslationUnit Parser::parse_translation_unit() {
    TranslationUnit unit{.file_id = file_id_};
    unit_ = &unit;
    while (!at_end()) {
        auto decl = parse_declaration();
        if (decl.kind != DeclKind::unknown) {
            unit.declarations.push_back(std::move(decl));
        }
    }
    unit_ = nullptr;
    return unit;
}

const Token &Parser::peek(std::size_t lookahead) const {
    const auto index = cursor_ + lookahead;
    return tokens_[index < tokens_.size() ? index : tokens_.size() - 1];
}

bool Parser::at(TokenKind kind) const {
    return peek().kind == kind;
}

bool Parser::consume(TokenKind kind) {
    if (!at(kind)) {
        return false;
    }
    ++cursor_;
    return true;
}

bool Parser::at_end() const {
    return at(TokenKind::end_of_file);
}

Decl Parser::parse_declaration() {
    bool exported = consume(TokenKind::kw_Export);

    if (at(TokenKind::kw_Import)) {
        return parse_import(exported);
    }
    if (at(TokenKind::kw_Function)) {
        return parse_named_declaration(DeclKind::function, exported);
    }
    if (at(TokenKind::kw_Struct)) {
        auto decl = parse_named_declaration(DeclKind::struct_decl, exported);
        return decl;
    }
    if (at(TokenKind::kw_Uniform)) {
        return parse_named_declaration(DeclKind::uniform, exported);
    }
    if (at(TokenKind::kw_Varying)) {
        return parse_named_declaration(DeclKind::varying, exported);
    }
    if (at(TokenKind::kw_Input)) {
        return parse_named_declaration(DeclKind::input, exported);
    }
    if (at(TokenKind::kw_Output)) {
        return parse_named_declaration(DeclKind::output, exported);
    }
    if (at(TokenKind::kw_Namespace)) {
        return parse_named_declaration(DeclKind::namespace_decl, exported);
    }
    diagnose(peek(), "unsupported top-level declaration");
    skip_to_declaration_boundary();
    return {};
}

Decl Parser::parse_import(bool exported) {
    const Token start = peek();
    const bool consumed_import = consume(TokenKind::kw_Import);
    (void)consumed_import;
    std::string name;

    if (consume(TokenKind::less)) {
        while (!at_end() && !at(TokenKind::greater)) {
            name += std::string(peek().text);
            ++cursor_;
        }
        if (!consume(TokenKind::greater)) {
            diagnose(peek(), "unterminated import path");
        }
    } else {
        diagnose(peek(), "expected '<' after import");
    }

    if (!consume(TokenKind::semicolon)) {
        diagnose(peek(), "expected ';' after import");
        skip_to_declaration_boundary();
    }

    return Decl{.kind = DeclKind::import, .name = std::move(name), .span = start.span, .exported = exported};
}

Decl Parser::parse_named_declaration(DeclKind kind, bool exported) {
    const Token start = peek();
    ++cursor_;

    std::string name;
    if (at(TokenKind::identifier)) {
        name = std::string(peek().text);
        ++cursor_;
        while (consume(TokenKind::colon_colon)) {
            if (consume(TokenKind::tilde)) {
                name += "::~";
                if (!at(TokenKind::identifier)) {
                    diagnose(peek(), "expected destructor name after '~'");
                    break;
                }
                name += std::string(peek().text);
                ++cursor_;
                break;
            }
            if (!at(TokenKind::identifier)) {
                diagnose(peek(), "expected identifier after '::'");
                break;
            }
            name += "::";
            name += std::string(peek().text);
            ++cursor_;
        }
    } else if (kind != DeclKind::uniform) {
        diagnose(peek(), "expected declaration name");
    }

    Decl decl{.kind = kind, .name = std::move(name), .span = start.span, .exported = exported};
    if (kind == DeclKind::function) {
        parse_function_signature(decl);
        if (const auto owner_end = decl.name.find("::");
            owner_end != std::string::npos && decl.name.substr(owner_end + 2) == decl.name.substr(0, owner_end) &&
            decl.return_type != "void") {
            diagnose(peek(), "constructors must not specify a return type");
        }
        parse_function_body(decl);
    } else if (kind == DeclKind::struct_decl) {
        StructDecl struct_decl{.name = decl.name};
        if (consume(TokenKind::left_brace)) {
            while (!at_end() && !at(TokenKind::right_brace)) {
                if (at(TokenKind::kw_Function)) {
                    ++cursor_;
                    if (!at(TokenKind::identifier)) {
                        diagnose(peek(), "expected constructor name");
                        skip_to_declaration_boundary();
                        continue;
                    }
                    const std::string ctor_name = std::string(peek().text);
                    ++cursor_;
                    if (ctor_name != decl.name) {
                        diagnose(peek(), "expected constructor declaration");
                        skip_to_declaration_boundary();
                        continue;
                    }
                    Decl ctor{.kind = DeclKind::function, .name = decl.name + "::" + decl.name, .span = start.span, .exported = exported};
                    parse_function_signature(ctor);
                    if (ctor.return_type != "void") {
                        diagnose(peek(), "constructor declarations must not specify a return type");
                    }
                    if (!consume(TokenKind::semicolon)) {
                        diagnose(peek(), "expected ';' after constructor declaration");
                    }
                    struct_decl.constructor_parameters = ctor.parameters;
                    continue;
                }
                auto field = parse_field_declaration();
                if (!field.type.empty() && !field.name.empty()) {
                    struct_decl.fields.push_back(std::move(field));
                    continue;
                }
                skip_to_declaration_boundary();
            }
            const bool consumed_right_brace = consume(TokenKind::right_brace);
            (void)consumed_right_brace;
        }
        if (unit_) {
            unit_->structs.push_back(std::move(struct_decl));
        }
    } else if (kind == DeclKind::uniform) {
        parse_uniform_scope(decl);
    } else if (kind == DeclKind::varying || kind == DeclKind::input || kind == DeclKind::output) {
        parse_stage_interface(decl);
    } else {
        skip_balanced_block();
    }
    if (at(TokenKind::semicolon)) {
        ++cursor_;
    }

    return decl;
}

void Parser::parse_uniform_scope(const Decl &decl) {
    if (!consume(TokenKind::left_brace)) {
        return;
    }

    // Anonymous `uniform { ... }` blocks cannot be reopened: each one becomes
    // its own descriptor set. Allocate one block id per anonymous block here
    // so every uniform inside this brace shares the same id, and different
    // anonymous blocks get different ids. Named scopes pass through unchanged
    // and remain reopen-able via shared scope_name.
    const bool is_anonymous = decl.name.empty();
    const u32 anonymous_block_id = is_anonymous ? next_anonymous_block_id_++ : 0;

    while (!at_end() && !at(TokenKind::right_brace)) {
        std::string access;
        if (consume(TokenKind::kw_ReadOnly)) {
            access = "readonly";
        } else if (consume(TokenKind::kw_WriteOnly)) {
            access = "writeonly";
        }

        if (consume(TokenKind::kw_Struct) && consume(TokenKind::left_brace)) {
            UniformBinding binding{
                .scope_name = decl.name,
                .access = std::move(access),
                .is_anonymous = is_anonymous,
                .anonymous_block_id = anonymous_block_id,
            };
            while (!at_end() && !at(TokenKind::right_brace)) {
                auto field = parse_field_declaration();
                if (!field.type.empty() && !field.name.empty()) {
                    binding.inline_fields.push_back(std::move(field));
                    continue;
                }
                skip_to_declaration_boundary();
            }
            const bool consumed_struct_end = consume(TokenKind::right_brace);
            (void)consumed_struct_end;
            if (at(TokenKind::identifier)) {
                binding.name = std::string(peek().text);
                ++cursor_;
            }
            binding.type = "struct";
            if (consume(TokenKind::semicolon) && unit_ && !binding.name.empty()) {
                unit_->uniforms.push_back(std::move(binding));
            }
            continue;
        }

        auto field = parse_field_declaration();
        if (!field.type.empty() && !field.name.empty() && unit_) {
            unit_->uniforms.push_back(UniformBinding{
                .scope_name = decl.name,
                .name = std::move(field.name),
                .type = std::move(field.type),
                .access = std::move(access),
                .is_anonymous = is_anonymous,
                .anonymous_block_id = anonymous_block_id,
            });
            continue;
        }
        skip_to_declaration_boundary();
    }

    const bool consumed_uniform_end = consume(TokenKind::right_brace);
    (void)consumed_uniform_end;
}

void Parser::parse_stage_interface(const Decl &decl) {
    StageRole role = StageRole::varying;
    if (decl.kind == DeclKind::input) {
        role = StageRole::input;
    } else if (decl.kind == DeclKind::output) {
        role = StageRole::output;
    }

    StageInterface interface{.role = role, .type_name = decl.name};

    if (!consume(TokenKind::left_brace)) {
        return;
    }

    while (!at_end() && !at(TokenKind::right_brace)) {
        StageIOField field;

        // Field qualifiers may appear in any order before the field name.
        bool consuming_qualifiers = true;
        while (consuming_qualifiers && !at_end()) {
            if (consume(TokenKind::kw_Smooth)) {
                field.interpolation = "smooth";
            } else if (consume(TokenKind::kw_Flat)) {
                field.interpolation = "flat";
            } else if (consume(TokenKind::kw_Clip)) {
                // A clip-space position is delivered through the rasterizer's
                // built-in position slot rather than a user location.
                field.interpolation = "clip";
                if (field.builtin.empty()) {
                    field.builtin = "position";
                }
            } else if (consume(TokenKind::kw_Location)) {
                if (consume(TokenKind::left_paren) && at(TokenKind::integer_literal)) {
                    field.location = static_cast<u32>(std::stoul(std::string(peek().text)));
                    field.has_location = true;
                    ++cursor_;
                    if (!consume(TokenKind::right_paren)) {
                        diagnose(peek(), "expected ')' after location index");
                    }
                } else {
                    diagnose(peek(), "expected '(' index ')' after 'location'");
                }
            } else if (consume(TokenKind::kw_Builtin)) {
                if (consume(TokenKind::left_paren) && at(TokenKind::identifier)) {
                    field.builtin = std::string(peek().text);
                    ++cursor_;
                    if (!consume(TokenKind::right_paren)) {
                        diagnose(peek(), "expected ')' after builtin name");
                    }
                } else {
                    diagnose(peek(), "expected '(' name ')' after 'builtin'");
                }
            } else {
                consuming_qualifiers = false;
            }
        }

        if (at(TokenKind::identifier)) {
            field.name = std::string(peek().text);
            ++cursor_;
            if (consume(TokenKind::semicolon)) {
                interface.fields.push_back(std::move(field));
                continue;
            }
            diagnose(peek(), "expected ';' after stage interface field");
        } else {
            diagnose(peek(), "expected stage interface field name");
        }
        skip_to_declaration_boundary();
    }

    const bool consumed_end = consume(TokenKind::right_brace);
    (void)consumed_end;

    if (unit_ && !interface.type_name.empty()) {
        unit_->stage_interfaces.push_back(std::move(interface));
    }
}

StructField Parser::parse_field_declaration() {
    auto field_type = collect_type_tokens_until_identifier();
    if (field_type.empty() || !at(TokenKind::identifier)) {
        return {};
    }
    auto field_name = std::string(peek().text);
    ++cursor_;
    if (!consume(TokenKind::semicolon)) {
        return {};
    }
    return StructField{.type = std::move(field_type), .name = std::move(field_name)};
}

std::string Parser::collect_type_tokens_until_identifier() {
    std::string text;
    int angle_depth = 0;
    while (!at_end()) {
        if (angle_depth == 0 && peek().kind == TokenKind::identifier && !text.empty()) {
            break;
        }
        if (peek().kind == TokenKind::less) {
            ++angle_depth;
        } else if (peek().kind == TokenKind::greater && angle_depth > 0) {
            --angle_depth;
        }
        text += std::string(peek().text);
        ++cursor_;
    }
    return text;
}

void Parser::parse_function_body(Decl &decl) {
    if (!consume(TokenKind::left_brace)) {
        return;
    }
    parse_function_body_into(decl, true);
}

void Parser::parse_function_body_into(Decl &decl, bool stop_on_right_brace) {
    parse_statement_list_into(decl.body_statements, stop_on_right_brace);
}

void Parser::parse_statement_list_into(std::vector<Decl::BodyStatement> &out, bool stop_on_right_brace) {
    while (!at_end()) {
        if (stop_on_right_brace && at(TokenKind::right_brace)) {
            ++cursor_;
            return;
        }

        std::vector<Token> stmt_tokens;
        int paren_depth = 0;
        int bracket_depth = 0;
        int brace_depth = 0;
        bool saw_tokens = false;
        Token begin = peek();
        Token end = begin;

        while (!at_end()) {
            const Token token = peek();
            if (token.kind == TokenKind::left_brace && paren_depth == 0 && bracket_depth == 0 && brace_depth == 0 &&
                saw_tokens) {
                break;
            }
            if (token.kind == TokenKind::semicolon && paren_depth == 0 && bracket_depth == 0 && brace_depth == 0) {
                end = token;
                stmt_tokens.push_back(token);
                ++cursor_;
                saw_tokens = true;
                break;
            }
            if (token.kind == TokenKind::right_brace && paren_depth == 0 && bracket_depth == 0 && brace_depth == 0) {
                if (stop_on_right_brace) {
                    break;
                }
                ++cursor_;
                continue;
            }

            end = token;
            saw_tokens = true;
            stmt_tokens.push_back(token);
            ++cursor_;
            if (token.kind == TokenKind::left_paren) ++paren_depth;
            else if (token.kind == TokenKind::right_paren && paren_depth > 0) --paren_depth;
            else if (token.kind == TokenKind::left_bracket) ++bracket_depth;
            else if (token.kind == TokenKind::right_bracket && bracket_depth > 0) --bracket_depth;
            else if (token.kind == TokenKind::left_brace) ++brace_depth;
            else if (token.kind == TokenKind::right_brace && brace_depth > 0) --brace_depth;
        }

        if (!saw_tokens) {
            break;
        }

        auto body = parse_statement_from_tokens(stmt_tokens);
        if (body.kind == Decl::BodyStatementKind::if_stmt) {
            if (at(TokenKind::left_brace)) {
                ++cursor_;
                parse_statement_list_into(body.children, true);
            }
            if (at(TokenKind::kw_Else)) {
                ++cursor_;
                if (at(TokenKind::left_brace)) {
                    ++cursor_;
                    parse_statement_list_into(body.else_children, true);
                }
            }
        } else if (body.kind == Decl::BodyStatementKind::while_stmt ||
                   body.kind == Decl::BodyStatementKind::do_stmt ||
                   body.kind == Decl::BodyStatementKind::for_stmt) {
            if (at(TokenKind::left_brace)) {
                ++cursor_;
                parse_statement_list_into(body.children, true);
            }
        }
        out.push_back(std::move(body));
    }
}

Decl::BodyStatement Parser::parse_if_statement(const std::vector<Token> &tokens) const {
    Decl::BodyStatement body{};
    body.kind = Decl::BodyStatementKind::if_stmt;
    if (!tokens.empty()) {
        body.span.begin = tokens.front().span.begin;
        body.span.length = tokens.back().span.begin.offset + tokens.back().span.length - tokens.front().span.begin.offset;
    }
    std::size_t open = std::string::npos;
    std::size_t close = std::string::npos;
    int depth = 0;
    for (std::size_t i = 0; i < tokens.size(); ++i) {
        if (tokens[i].kind == TokenKind::left_paren) {
            if (depth++ == 0) open = i + 1;
        } else if (tokens[i].kind == TokenKind::right_paren) {
            if (depth == 0) break;
            --depth;
            if (depth == 0) {
                close = i;
                break;
            }
        }
    }
    if (open != std::string::npos && close != std::string::npos && close > open) {
        body.condition = trim(tokens_to_text(std::span<const Token>(tokens.data() + open, close - open)));
        body.expr = parse_expression(body.condition);
    }
    return body;
}

Decl::BodyStatement Parser::parse_while_statement(const std::vector<Token> &tokens) const {
    Decl::BodyStatement body{};
    body.kind = Decl::BodyStatementKind::while_stmt;
    if (!tokens.empty()) {
        body.span.begin = tokens.front().span.begin;
        body.span.length = tokens.back().span.begin.offset + tokens.back().span.length - tokens.front().span.begin.offset;
    }
    std::size_t open = std::string::npos;
    std::size_t close = std::string::npos;
    int depth = 0;
    for (std::size_t i = 0; i < tokens.size(); ++i) {
        if (tokens[i].kind == TokenKind::left_paren) {
            if (depth++ == 0) open = i + 1;
        } else if (tokens[i].kind == TokenKind::right_paren) {
            if (depth == 0) break;
            --depth;
            if (depth == 0) { close = i; break; }
        }
    }
    if (open != std::string::npos && close != std::string::npos && close > open) {
        body.condition = trim(tokens_to_text(std::span<const Token>(tokens.data() + open, close - open)));
        body.expr = parse_expression(body.condition);
    }
    return body;
}

Decl::BodyStatement Parser::parse_do_statement(const std::vector<Token> &tokens) const {
    Decl::BodyStatement body{};
    body.kind = Decl::BodyStatementKind::do_stmt;
    if (!tokens.empty()) {
        body.span.begin = tokens.front().span.begin;
        body.span.length = tokens.back().span.begin.offset + tokens.back().span.length - tokens.front().span.begin.offset;
    }
    return body;
}

Decl::BodyStatement Parser::parse_for_statement(const std::vector<Token> &tokens) const {
    Decl::BodyStatement body{};
    body.kind = Decl::BodyStatementKind::for_stmt;
    if (!tokens.empty()) {
        body.span.begin = tokens.front().span.begin;
        body.span.length = tokens.back().span.begin.offset + tokens.back().span.length - tokens.front().span.begin.offset;
    }
    std::size_t open = std::string::npos;
    std::size_t close = std::string::npos;
    int depth = 0;
    for (std::size_t i = 0; i < tokens.size(); ++i) {
        if (tokens[i].kind == TokenKind::left_paren) {
            if (depth++ == 0) open = i + 1;
        } else if (tokens[i].kind == TokenKind::right_paren) {
            if (depth == 0) break;
            --depth;
            if (depth == 0) { close = i; break; }
        }
    }
    if (open == std::string::npos || close == std::string::npos || close <= open) {
        return body;
    }
    const auto header = std::span<const Token>(tokens.data() + open, close - open);
    std::size_t first = header.size();
    std::size_t second = header.size();
    int split_depth = 0;
    for (std::size_t i = 0; i < header.size(); ++i) {
        const auto kind = header[i].kind;
        if (kind == TokenKind::left_paren || kind == TokenKind::left_bracket || kind == TokenKind::left_brace) {
            ++split_depth;
        } else if ((kind == TokenKind::right_paren || kind == TokenKind::right_bracket || kind == TokenKind::right_brace) && split_depth > 0) {
            --split_depth;
        } else if (kind == TokenKind::semicolon && split_depth == 0) {
            if (first == header.size()) first = i;
            else { second = i; break; }
        }
    }
    if (first != header.size()) {
        body.loop_init = trim(tokens_to_text(header.first(first)));
        if (first + 1 < header.size()) {
            if (second == header.size()) {
                body.condition = trim(tokens_to_text(header.subspan(first + 1)));
            } else {
                body.condition = trim(tokens_to_text(header.subspan(first + 1, second - first - 1)));
                if (second + 1 < header.size()) {
                    body.loop_continue = trim(tokens_to_text(header.subspan(second + 1)));
                }
            }
        }
    }
    return body;
}

Decl::BodyStatement Parser::parse_statement_from_tokens(const std::vector<Token> &tokens) const {
    Decl::BodyStatement body{};
    if (!tokens.empty()) {
        body.span.begin = tokens.front().span.begin;
        body.span.length = tokens.back().span.begin.offset + tokens.back().span.length - tokens.front().span.begin.offset;
    }
    if (tokens.empty()) {
        body.kind = Decl::BodyStatementKind::unknown;
        return body;
    }
    const Token first = tokens.front();
    if (first.kind == TokenKind::kw_If) {
        return parse_if_statement(tokens);
    }
    if (first.kind == TokenKind::kw_While) {
        return parse_while_statement(tokens);
    }
    if (first.kind == TokenKind::kw_Do) {
        return parse_do_statement(tokens);
    }
    if (first.kind == TokenKind::kw_For) {
        return parse_for_statement(tokens);
    }

    if (first.kind == TokenKind::kw_Return) {
        body.kind = Decl::BodyStatementKind::return_stmt;
        if (tokens.size() > 1) {
            body.rhs = trim(tokens_to_text(std::span<const Token>(tokens.data() + 1, tokens.size() - 1)));
        }
        body.expr = parse_expression(body.rhs);
    } else {
        const auto equals = std::find_if(tokens.begin(), tokens.end(), [](const Token &tok) { return tok.kind == TokenKind::equal; });
        const auto first_ident = std::find_if(tokens.begin(), tokens.end(), [](const Token &tok) {
            return tok.kind == TokenKind::identifier;
        });
        if (equals != tokens.end() && first_ident != tokens.end() && first_ident < equals) {
            body.kind = Decl::BodyStatementKind::declaration;
            body.type_name = std::string(tokens.front().text);
            body.name = std::string(first_ident->text);
            const auto eq_index = static_cast<std::size_t>(std::distance(tokens.begin(), equals));
            body.initializer = eq_index + 1 < tokens.size()
                ? trim(tokens_to_text(std::span<const Token>(tokens.data() + eq_index + 1, tokens.size() - eq_index - 1)))
                : std::string{};
            body.expr = parse_expression(body.initializer);
        } else if (equals != tokens.end()) {
            body.kind = Decl::BodyStatementKind::assignment;
            const auto eq_index = static_cast<std::size_t>(std::distance(tokens.begin(), equals));
            body.lhs = trim(tokens_to_text(std::span<const Token>(tokens.data(), eq_index)));
            body.rhs = eq_index + 1 < tokens.size()
                ? trim(tokens_to_text(std::span<const Token>(tokens.data() + eq_index + 1, tokens.size() - eq_index - 1)))
                : std::string{};
            body.expr = parse_expression(body.rhs);
        } else {
            body.kind = Decl::BodyStatementKind::expression;
            body.expr = parse_expression(trim(tokens_to_text(std::span<const Token>(tokens.data(), tokens.size()))));
        }
    }
    return body;
}

void Parser::parse_function_signature(Decl &decl) {
    if (!consume(TokenKind::left_paren)) {
        diagnose(peek(), "expected '(' after function name");
        return;
    }

    while (!at_end() && !at(TokenKind::right_paren)) {
        if (consume(TokenKind::kw_InOut)) {
        }

        std::vector<Token> parameter_tokens;
        int angle_depth = 0;
        while (!at_end()) {
            const auto kind = peek().kind;
            if (angle_depth == 0 && (kind == TokenKind::comma || kind == TokenKind::right_paren)) {
                break;
            }
            if (kind == TokenKind::less) {
                ++angle_depth;
            } else if (kind == TokenKind::greater && angle_depth > 0) {
                --angle_depth;
            }
            parameter_tokens.push_back(peek());
            ++cursor_;
        }

        if (!parameter_tokens.empty()) {
            std::string param_name;
            if (parameter_tokens.size() >= 2 && parameter_tokens.back().kind == TokenKind::identifier) {
                param_name = std::string(parameter_tokens.back().text);
                parameter_tokens.pop_back();
            }
            // Parameters may be passed by reference: `const Type& name`. The
            // const/& qualifiers are stripped from the recorded type; reference
            // semantics are intrinsic to builtin carrier types (e.g. RtVertex).
            bool is_const = false;
            bool is_reference = false;
            std::string param_type;
            for (const auto &token : parameter_tokens) {
                if (token.kind == TokenKind::kw_Const) {
                    is_const = true;
                } else if (token.kind == TokenKind::amp) {
                    is_reference = true;
                } else {
                    param_type += std::string(token.text);
                }
            }
            if (!param_type.empty()) {
                decl.parameters.push_back(ParameterDecl{
                    .type = std::move(param_type),
                    .name = std::move(param_name),
                    .is_const = is_const,
                    .is_reference = is_reference,
                });
            }
        }

        if (!consume(TokenKind::comma)) {
            break;
        }
    }

    if (!consume(TokenKind::right_paren)) {
        diagnose(peek(), "expected ')' after function parameters");
        return;
    }

    if (consume(TokenKind::arrow)) {
        decl.return_type = collect_type_until(TokenKind::left_brace, TokenKind::semicolon);
    } else {
        decl.return_type = "void";
    }
}

std::string Parser::collect_type_until(TokenKind stop_a, TokenKind stop_b) {
    std::string text;
    int angle_depth = 0;
    while (!at_end()) {
        const auto kind = peek().kind;
        if (angle_depth == 0 && (kind == stop_a || kind == stop_b || kind == TokenKind::right_paren)) {
            break;
        }
        if (kind == TokenKind::less) {
            ++angle_depth;
        } else if (kind == TokenKind::greater && angle_depth > 0) {
            --angle_depth;
        }
        text += std::string(peek().text);
        ++cursor_;
    }
    return text;
}

void Parser::skip_to_declaration_boundary(bool consume_right_brace) {
    while (!at_end() && !at(TokenKind::semicolon) && !at(TokenKind::right_brace)) {
        ++cursor_;
    }
    if (at(TokenKind::semicolon) || (consume_right_brace && at(TokenKind::right_brace))) {
        ++cursor_;
    }
}

void Parser::skip_balanced_block() {
    int parens = 0;
    int brackets = 0;
    int braces = 0;

    while (!at_end()) {
        const auto kind = peek().kind;
        ++cursor_;

        if (kind == TokenKind::left_paren) ++parens;
        else if (kind == TokenKind::right_paren && parens > 0) --parens;
        else if (kind == TokenKind::left_bracket) ++brackets;
        else if (kind == TokenKind::right_bracket && brackets > 0) --brackets;
        else if (kind == TokenKind::left_brace) ++braces;
        else if (kind == TokenKind::right_brace && braces > 0) --braces;

        if (parens == 0 && brackets == 0 && braces == 0 &&
            (kind == TokenKind::semicolon || kind == TokenKind::right_brace)) {
            break;
        }
    }
}

std::string Parser::append_token_text(std::string statement, const Token &token) const {
    if (!statement.empty()) {
        const char last = statement.back();
        const bool last_ident = (last >= 'a' && last <= 'z') || (last >= 'A' && last <= 'Z') ||
                                (last >= '0' && last <= '9') || last == '_';
        const bool next_ident = token.kind == TokenKind::identifier ||
                                token.kind == TokenKind::integer_literal ||
                                token.kind == TokenKind::float_literal ||
                                (token.kind >= TokenKind::kw_Import && token.kind <= TokenKind::kw_Builtin);
        if (last_ident && next_ident) {
            statement.push_back(' ');
        }
    }
    statement += std::string(token.text);
    return statement;
}

std::string Parser::tokens_to_text(std::span<const Token> tokens) const {
    std::string statement;
    for (const auto &token : tokens) {
        statement = append_token_text(std::move(statement), token);
    }
    return statement;
}

void Parser::diagnose(const Token &token, std::string message) {
    diagnostics_.report(2001, DiagnosticSeverity::error, token.span.begin,
                        std::string(sources_.name(file_id_)), std::move(message));
}

SourceSpan Parser::statement_span(const Token &begin, const Token &end) const {
    const auto start = begin.span.begin;
    const auto finish = end.span.begin.offset + end.span.length;
    SourceSpan span{};
    span.begin = start;
    span.length = finish >= start.offset ? finish - start.offset : 0;
    return span;
}

Decl::Expr Parser::parse_expression(std::string_view text) const {
    struct ParserImpl {
        ExprLexer lex;
        explicit ParserImpl(std::string_view text) : lex(text) {}

        Decl::Expr parse() { return parse_add(); }

        Decl::Expr parse_add() {
            auto lhs = parse_mul();
            while (true) {
                const auto tok = lex.peek();
                if (tok.kind != ExprToken::Kind::plus && tok.kind != ExprToken::Kind::minus) break;
                lex.next();
                auto rhs = parse_mul();
                Decl::Expr expr;
                expr.kind = Decl::Expr::Kind::binary;
                expr.op = tok.text;
                expr.children = {std::move(lhs), std::move(rhs)};
                lhs = std::move(expr);
            }
            return lhs;
        }

        Decl::Expr parse_mul() {
            auto lhs = parse_unary();
            while (true) {
                const auto tok = lex.peek();
                if (tok.kind != ExprToken::Kind::star && tok.kind != ExprToken::Kind::slash &&
                    tok.kind != ExprToken::Kind::percent) break;
                lex.next();
                auto rhs = parse_unary();
                Decl::Expr expr;
                expr.kind = Decl::Expr::Kind::binary;
                expr.op = tok.text;
                expr.children = {std::move(lhs), std::move(rhs)};
                lhs = std::move(expr);
            }
            return lhs;
        }

        Decl::Expr parse_unary() {
            const auto tok = lex.peek();
            if (tok.kind == ExprToken::Kind::plus || tok.kind == ExprToken::Kind::minus) {
                lex.next();
                auto child = parse_unary();
                Decl::Expr expr;
                expr.kind = Decl::Expr::Kind::unary;
                expr.op = tok.text;
                expr.children = {std::move(child)};
                return expr;
            }
            return parse_postfix();
        }

        Decl::Expr parse_postfix() {
            auto base = parse_primary();
            while (true) {
                const auto tok = lex.peek();
                if (tok.kind == ExprToken::Kind::dot || tok.kind == ExprToken::Kind::scope) {
                    lex.next();
                    const auto name = lex.next();
                    Decl::Expr expr;
                    expr.kind = Decl::Expr::Kind::member;
                    expr.op = tok.text;
                    expr.text = name.text;
                    expr.children = {std::move(base)};
                    base = std::move(expr);
                    continue;
                }
                if (tok.kind == ExprToken::Kind::lparen) {
                    lex.next();
                    Decl::Expr expr;
                    expr.kind = Decl::Expr::Kind::call;
                    expr.children = {std::move(base)};
                    if (lex.peek().kind != ExprToken::Kind::rparen) {
                        while (true) {
                            expr.children.push_back(parse());
                            if (lex.peek().kind == ExprToken::Kind::comma) {
                                lex.next();
                                continue;
                            }
                            break;
                        }
                    }
                    (void)lex.next();
                    base = std::move(expr);
                    continue;
                }
                break;
            }
            return base;
        }

        Decl::Expr parse_primary() {
            const auto tok = lex.next();
            Decl::Expr expr;
            switch (tok.kind) {
            case ExprToken::Kind::ident:
                expr.kind = Decl::Expr::Kind::name;
                expr.text = tok.text;
                return expr;
            case ExprToken::Kind::integer:
                expr.kind = Decl::Expr::Kind::literal_int;
                expr.text = tok.text;
                return expr;
            case ExprToken::Kind::floating:
                expr.kind = Decl::Expr::Kind::literal_float;
                expr.text = tok.text;
                return expr;
            case ExprToken::Kind::lparen: {
                auto inner = parse();
                (void)lex.next();
                return inner;
            }
            default:
                return expr;
            }
        }
    };

    return ParserImpl(trim(text)).parse();
}

} // namespace rtsl
