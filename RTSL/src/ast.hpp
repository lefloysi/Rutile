#pragma once

#include "basic_source_manager.hpp"

#include <string>
#include <string_view>
#include <vector>

namespace rtsl {

enum class DeclKind {
    unknown,
    import,
    function,
    struct_decl,
    uniform,
    varying,
    input,
    output,
    namespace_decl,
};

// Backend shader stage. The 4-letter spellings (vert/frag) are the names
// of the compiler-generated backend entry points for each stage.
enum class StageKind : u8 {
    none,
    vertex,
    fragment,
};

[[nodiscard]] inline std::string_view stage_entry_name(StageKind stage) {
    switch (stage) {
    case StageKind::vertex: return "vert";
    case StageKind::fragment: return "frag";
    case StageKind::none: return "";
    }
    return "";
}

enum class StageRole : u8 {
    input,
    varying,
    output,
};

// One field of a stage interface payload, with its ABI placement.
struct StageIOField {
    std::string name;
    std::string interpolation; // "smooth" | "flat" | "clip" | ""
    std::string builtin;       // builtin slot name, or "" for a user location
    u32 location = 0;
    bool has_location = false;
};

// A declared stage interface: how a payload struct's fields cross a stage
// boundary (input attributes, interpolated varyings, or stage outputs).
struct StageInterface {
    StageRole role = StageRole::varying;
    std::string type_name;
    std::vector<StageIOField> fields;
};

struct ParameterDecl {
    std::string type;
    std::string name;
    bool is_const = false;
    bool is_reference = false;
};

struct Decl {
    struct Expr {
        enum class Kind {
            unknown,
            name,
            literal_int,
            literal_float,
            call,
            member,
            unary,
            binary,
        };

        Kind kind = Kind::unknown;
        std::string text;
        std::string op;
        std::vector<Expr> children;
        SourceSpan span{};
    };

    enum class BodyStatementKind {
        unknown,
        block,
        if_stmt,
        while_stmt,
        do_stmt,
        for_stmt,
        declaration,
        assignment,
        return_stmt,
        expression,
    };

    struct BodyStatement {
        BodyStatementKind kind = BodyStatementKind::unknown;
        std::string text;
        std::string type_name;
        std::string name;
        std::string initializer;
        std::string lhs;
        std::string rhs;
        std::string condition;
        std::string loop_init;
        std::string loop_continue;
        Expr expr{};
        std::vector<BodyStatement> children;
        std::vector<BodyStatement> else_children;
        SourceSpan span{};
    };

    DeclKind kind = DeclKind::unknown;
    std::string name;
    std::vector<ParameterDecl> parameters;
    std::string return_type;
    std::vector<BodyStatement> body_statements;
    SourceSpan span{};
    bool exported = false;
};

struct StructField {
    std::string type;
    std::string name;
};

struct ExportSymbol {
    std::string name;
    std::string kind;
    std::string type;
};

struct StructDecl {
    std::string name;
    std::vector<StructField> fields;
    std::vector<ParameterDecl> constructor_parameters;
};

struct UniformBinding {
    std::string scope_name;
    std::string name;
    std::string type;
    std::vector<StructField> inline_fields;
    std::string access;
    u32 set = 0;
    u32 binding = 0;
    // Anonymous `uniform { ... }` blocks have no source-visible scope name.
    // Each anonymous block is its own descriptor set; only named scopes can be
    // reopened across multiple blocks. The parser assigns each anonymous block
    // a unique anonymous_block_id; Sema uses it to keep their sets distinct
    // without leaking compiler-generated names into the C API or mangling.
    bool is_anonymous = false;
    u32 anonymous_block_id = 0;
};

struct TranslationUnit {
    u32 file_id = 0;
    std::vector<Decl> declarations;
    std::vector<std::string> imports;
    std::vector<ExportSymbol> exports;
    std::vector<StructDecl> structs;
    std::vector<UniformBinding> uniforms;
    std::vector<StageInterface> stage_interfaces;
};

} // namespace rtsl
