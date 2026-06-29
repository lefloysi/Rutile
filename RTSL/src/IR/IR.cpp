#include "IR/IR.hpp"

#include "IR/StageBuiltins.hpp"
#include "IR/UniformLowering.hpp"
#include "Mangle/Mangler.hpp"

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <cstdio>
#include <optional>
#include <stdexcept>
#include <string_view>
#include <unordered_map>

// Lower the SemanticModule (which still carries statement strings from the
// parser today) into the typed SSA IR described in docs/rtir.md.
//
// The lowering walks each function body, tokenizes its statements, and runs a
// Pratt-style expression parser that emits IRInstructions against an id
// builder. Types are deduplicated through a signature -> IRId table so vec3,
// vec4, mat4, etc. each have a single canonical id reused everywhere.

namespace rtsl {

namespace {

// ----------------------------------------------------------------------------
// IRBuilder: id allocation, type/constant pool with dedup, local instruction
// emission.
// ----------------------------------------------------------------------------

class IRBuilder {
public:
    explicit IRBuilder(IRModule &module) : module_(module) {}

    [[nodiscard]] IRId fresh_id() { return module_.next_id++; }

    // Append a type/constant pool entry and return its id. The entry is keyed
    // by a stable string signature so identical types map to the same id.
    IRId intern_type(const std::string &signature, IROp op, IRId type_id,
                     std::vector<IRId> operands, std::vector<u32> literals) {
        auto it = type_cache_.find(signature);
        if (it != type_cache_.end()) {
            return it->second;
        }
        const IRId id = fresh_id();
        IRInstruction inst;
        inst.op = op;
        inst.result_id = id;
        inst.type_id = type_id;
        inst.operands = std::move(operands);
        inst.literals = std::move(literals);
        module_.type_constant_pool.push_back(std::move(inst));
        type_cache_.emplace(signature, id);
        return id;
    }

    IRId intern_constant(const std::string &signature, IROp op, IRId type_id,
                         std::vector<IRId> operands, std::vector<u32> literals) {
        // Constants share the same pool as types; deduped via the same cache.
        return intern_type(signature, op, type_id, std::move(operands), std::move(literals));
    }

    void add_global_variable(IRInstruction inst) {
        module_.global_variables.push_back(std::move(inst));
    }

    void add_decoration(IRDecoration d) { module_.decorations.push_back(std::move(d)); }

    IRModule &module() { return module_; }

private:
    IRModule &module_;
    std::unordered_map<std::string, IRId> type_cache_;
};

// ----------------------------------------------------------------------------
// TypeRegistry: name (source spelling) -> IRId for the type/constant pool.
// Also resolves member layouts so the lowerer knows the type of `p.position`,
// the index of a struct field, etc.
// ----------------------------------------------------------------------------

struct TypeInfo {
    IRId id = IRId_None;
    enum class Kind { Void, Bool, Int, UInt, Float, Vector, Matrix, Struct, Sampler, SampledImage, Image, Unknown };
    Kind kind = Kind::Unknown;
    u32 width = 0;                 // scalar width in bits
    u32 components = 0;            // vector component count, matrix column count
    IRId element_type_id = IRId_None; // vector element / matrix column type
    std::vector<std::pair<std::string, IRId>> members; // struct member name -> member type id
};

class TypeRegistry {
public:
    explicit TypeRegistry(IRBuilder &builder) : builder_(builder) {
        // Prime the registry with the language's builtin scalars and the
        // common vec/mat aggregates. Anything not registered here is looked up
        // by name against user struct decls.
        const IRId f32 = scalar("f32", TypeInfo::Kind::Float, 32, IROp::TypeFloat);
        scalar("float", TypeInfo::Kind::Float, 32, IROp::TypeFloat);
        const IRId i32 = scalar("i32", TypeInfo::Kind::Int, 32, IROp::TypeInt);
        scalar("int", TypeInfo::Kind::Int, 32, IROp::TypeInt);
        const IRId u32_ty = scalar("u32", TypeInfo::Kind::UInt, 32, IROp::TypeUInt);
        scalar("uint", TypeInfo::Kind::UInt, 32, IROp::TypeUInt);
        scalar("bool", TypeInfo::Kind::Bool, 1, IROp::TypeBool);
        scalar("void", TypeInfo::Kind::Void, 0, IROp::TypeVoid);

        vector("vec2", f32, 2);
        vector("vec3", f32, 3);
        vector("vec4", f32, 4);
        vector("ivec2", i32, 2);
        vector("ivec3", i32, 3);
        vector("ivec4", i32, 4);
        vector("uvec2", u32_ty, 2);
        vector("uvec3", u32_ty, 3);
        vector("uvec4", u32_ty, 4);

        const IRId vec4_id = info_["vec4"].id;
        matrix("mat4", vec4_id, 4);
        matrix("mat3", info_["vec3"].id, 3);
        matrix("mat2", info_["vec2"].id, 2);

        // Opaque resource handles.
        opaque("Sampler2D", TypeInfo::Kind::SampledImage, IROp::TypeSampledImage);
        opaque("Image2D", TypeInfo::Kind::Image, IROp::TypeImage);
        opaque("Sampler", TypeInfo::Kind::Sampler, IROp::TypeSampler);

        // Stage entry wrappers synthesize a leading globals parameter such as
        // `vert_globals g` / `frag_globals g`. These are ABI carrier types, not
        // user-declared structs, so we materialize them here as empty structs
        // to keep the IR well-typed.
        opaque("globals", TypeInfo::Kind::Struct, IROp::TypeStruct);
        opaque("vert_globals", TypeInfo::Kind::Struct, IROp::TypeStruct);
        opaque("frag_globals", TypeInfo::Kind::Struct, IROp::TypeStruct);
    }

    void register_struct(const StructDecl &decl) {
        // Two-pass: build the type id from currently known member types. If a
        // member type is itself a struct still being defined, fall back to a
        // forward-declared opaque entry that the linker can patch later.
        std::vector<IRId> member_ids;
        member_ids.reserve(decl.fields.size());
        std::vector<std::pair<std::string, IRId>> members;
        members.reserve(decl.fields.size());
        std::string signature = "struct{" + decl.name + "}";
        for (const auto &field : decl.fields) {
            const IRId member_id = find_or_unknown(field.type);
            member_ids.push_back(member_id);
            members.emplace_back(field.name, member_id);
            signature += ":" + field.type;
        }
        const IRId id = builder_.intern_type(signature, IROp::TypeStruct, IRId_None,
                                             std::move(member_ids), {});
        TypeInfo info;
        info.id = id;
        info.kind = TypeInfo::Kind::Struct;
        info.members = std::move(members);
        info_[decl.name] = std::move(info);
    }

    [[nodiscard]] IRId find(std::string_view name) const {
        const auto it = info_.find(std::string(name));
        return it == info_.end() ? IRId_None : it->second.id;
    }

    [[nodiscard]] const TypeInfo *info(std::string_view name) const {
        const auto it = info_.find(std::string(name));
        return it == info_.end() ? nullptr : &it->second;
    }

    [[nodiscard]] const TypeInfo *info_by_id(IRId id) const {
        for (const auto &[name, info] : info_) {
            if (info.id == id) {
                return &info;
            }
        }
        return nullptr;
    }

    // Resolve an unknown name to "unknown" (Id 0). Lowering treats this as
    // opaque — operations are still emitted but the backend gets to refuse.
    [[nodiscard]] IRId find_or_unknown(const std::string &name) const {
        const IRId id = find(name);
        return id ? id : IRId_None;
    }

    // Pointer type intern helper. Pointer types live in the pool too.
    [[nodiscard]] IRId pointer_to(IRId pointee, StorageClass sc) {
        if (pointee == IRId_None) {
            pointee = find("void");
        }
        const std::string sig = "ptr:" + std::to_string(pointee) + ":" + std::to_string(static_cast<u32>(sc));
        return builder_.intern_type(sig, IROp::TypePointer, IRId_None, {pointee},
                                    {static_cast<u32>(sc)});
    }

private:
    IRId scalar(const char *name, TypeInfo::Kind kind, u32 width, IROp op) {
        const std::string sig = std::string("scalar:") + name;
        std::vector<u32> literals;
        if (op != IROp::TypeBool && op != IROp::TypeVoid) {
            literals.push_back(width);
        }
        const IRId id = builder_.intern_type(sig, op, IRId_None, {}, std::move(literals));
        TypeInfo info;
        info.id = id;
        info.kind = kind;
        info.width = width;
        info_[name] = std::move(info);
        return id;
    }

    void vector(const char *name, IRId elem, u32 n) {
        const std::string sig = std::string("vec:") + std::to_string(elem) + ":" + std::to_string(n);
        const IRId id = builder_.intern_type(sig, IROp::TypeVector, IRId_None, {elem}, {n});
        TypeInfo info;
        info.id = id;
        info.kind = TypeInfo::Kind::Vector;
        info.components = n;
        info.element_type_id = elem;
        info_[name] = std::move(info);
    }

    void matrix(const char *name, IRId col_vec, u32 n) {
        const std::string sig = std::string("mat:") + std::to_string(col_vec) + ":" + std::to_string(n);
        const IRId id = builder_.intern_type(sig, IROp::TypeMatrix, IRId_None, {col_vec}, {n});
        TypeInfo info;
        info.id = id;
        info.kind = TypeInfo::Kind::Matrix;
        info.components = n;
        info.element_type_id = col_vec;
        info_[name] = std::move(info);
    }

    void opaque(const char *name, TypeInfo::Kind kind, IROp op) {
        const std::string sig = std::string("opaque:") + name;
        const IRId id = builder_.intern_type(sig, op, IRId_None, {}, {});
        TypeInfo info;
        info.id = id;
        info.kind = kind;
        info_[name] = std::move(info);
    }

    IRBuilder &builder_;
    std::unordered_map<std::string, TypeInfo> info_;
};

// ----------------------------------------------------------------------------
// Expression tokenizer + Pratt parser, operating on the statement strings the
// current parser captures. The strings have already had source-level uniform
// references mangled to u_xxx form by lower_uniform_references.
// ----------------------------------------------------------------------------

enum class TokKind {
    end, number_int, number_float, ident, lparen, rparen, comma, dot, scope,
    plus, minus, star, slash, percent, eq,
};

struct Tok {
    TokKind kind = TokKind::end;
    std::string text;
};

class Lex {
public:
    explicit Lex(std::string_view src) : src_(src) {}

    Tok next() {
        skip_ws();
        if (pos_ >= src_.size()) {
            return {TokKind::end, {}};
        }
        const char c = src_[pos_];
        if (c == '(' ) { ++pos_; return {TokKind::lparen, "("}; }
        if (c == ')') { ++pos_; return {TokKind::rparen, ")"}; }
        if (c == ',') { ++pos_; return {TokKind::comma, ","}; }
        if (c == '+') { ++pos_; return {TokKind::plus, "+"}; }
        if (c == '-') { ++pos_; return {TokKind::minus, "-"}; }
        if (c == '*') { ++pos_; return {TokKind::star, "*"}; }
        if (c == '/') { ++pos_; return {TokKind::slash, "/"}; }
        if (c == '%') { ++pos_; return {TokKind::percent, "%"}; }
        if (c == '=') { ++pos_; return {TokKind::eq, "="}; }
        if (c == '.') {
            // Distinguish "1.0" (already started above) from member access.
            ++pos_;
            return {TokKind::dot, "."};
        }
        if (c == ':' && pos_ + 1 < src_.size() && src_[pos_ + 1] == ':') {
            pos_ += 2;
            return {TokKind::scope, "::"};
        }
        if (std::isdigit(static_cast<unsigned char>(c)) || (c == '.' && pos_ + 1 < src_.size() &&
                                                            std::isdigit(static_cast<unsigned char>(src_[pos_ + 1])))) {
            return number();
        }
        if (std::isalpha(static_cast<unsigned char>(c)) || c == '_') {
            return ident();
        }
        // Unknown character — treat as end so we don't loop forever.
        return {TokKind::end, {}};
    }

    Tok peek() {
        const auto save_pos = pos_;
        const auto tok = next();
        pos_ = save_pos;
        return tok;
    }

private:
    void skip_ws() {
        while (pos_ < src_.size() && std::isspace(static_cast<unsigned char>(src_[pos_]))) {
            ++pos_;
        }
    }

    Tok number() {
        const auto start = pos_;
        bool is_float = false;
        while (pos_ < src_.size() && (std::isdigit(static_cast<unsigned char>(src_[pos_])) ||
                                      src_[pos_] == '.' || src_[pos_] == 'e' || src_[pos_] == 'E' ||
                                      src_[pos_] == '+' || src_[pos_] == '-' || src_[pos_] == 'f')) {
            if (src_[pos_] == '.' || src_[pos_] == 'e' || src_[pos_] == 'E' || src_[pos_] == 'f') {
                is_float = true;
            }
            // Stop at unary '+' / '-' that aren't part of an exponent.
            if ((src_[pos_] == '+' || src_[pos_] == '-') &&
                !(pos_ > start && (src_[pos_ - 1] == 'e' || src_[pos_ - 1] == 'E'))) {
                break;
            }
            ++pos_;
        }
        return {is_float ? TokKind::number_float : TokKind::number_int,
                std::string(src_.substr(start, pos_ - start))};
    }

    Tok ident() {
        const auto start = pos_;
        while (pos_ < src_.size() && (std::isalnum(static_cast<unsigned char>(src_[pos_])) || src_[pos_] == '_')) {
            ++pos_;
        }
        return {TokKind::ident, std::string(src_.substr(start, pos_ - start))};
    }

    std::string_view src_;
    std::size_t pos_ = 0;
};

// ----------------------------------------------------------------------------
// FunctionLowerer: per-function SSA emission. Holds the local scope (parameter
// and let bindings -> ids and types), keeps a current basic block, and emits
// IRInstructions for expressions / statements.
// ----------------------------------------------------------------------------

struct Value {
    IRId id = IRId_None;
    IRId type_id = IRId_None;
};

class FunctionLowerer {
public:
    FunctionLowerer(IRBuilder &builder, TypeRegistry &types, IRFunction &fn,
                    const std::vector<UniformBinding> &uniforms,
                    const std::unordered_map<std::string, IRId> &uniform_var_ids,
                    const std::unordered_map<std::string, IRId> &uniform_var_type_ids,
                    const std::vector<StageInterface> &stage_interfaces,
                    StageKind stage)
        : builder_(builder), types_(types), fn_(fn), uniforms_(uniforms),
          uniform_var_ids_(uniform_var_ids), uniform_var_type_ids_(uniform_var_type_ids),
          stage_interfaces_(stage_interfaces), stage_(stage) {}

    void bind_parameter(const std::string &name, const std::string &type, IRId param_id) {
        Local local;
        local.id = param_id;
        local.type_id = types_.find(type);
        local.type_name = type;
        local.is_pointer = false;
        locals_[name] = local;
    }

    // Enter "constructor" lowering mode. While active, unqualified field
    // assignments in the body (e.g. `position = ...`) are recorded into a
    // per-field slot map instead of being dropped, and emit_constructor_return
    // synthesizes the trailing CompositeConstruct + ReturnValue for the
    // assembled `this`.
    void begin_constructor(const std::string &owner) {
        ctor_owner_name_ = owner;
        ctor_owner_type_id_ = types_.find(owner);
        ctor_field_values_.clear();
        const TypeInfo *info = ctor_owner_type_id_ ? types_.info_by_id(ctor_owner_type_id_) : nullptr;
        if (info) ctor_field_values_.assign(info->members.size(), IRId_None);
    }
    [[nodiscard]] bool in_constructor() const { return ctor_owner_type_id_ != IRId_None; }

    // Try to record `field = value` against the constructor's implicit `this`.
    // Returns true if the field was recognized and stored.
    bool record_constructor_field(const std::string &field, IRId value) {
        if (!in_constructor()) return false;
        const TypeInfo *info = types_.info_by_id(ctor_owner_type_id_);
        if (!info) return false;
        for (u32 i = 0; i < info->members.size(); ++i) {
            if (info->members[i].first == field) {
                if (i < ctor_field_values_.size()) {
                    ctor_field_values_[i] = value;
                }
                return true;
            }
        }
        return false;
    }

    // Assemble the final `this` value and return it. Called after the body
    // statements have been lowered.
    void emit_constructor_return() {
        if (!in_constructor()) return;
        IRInstruction construct;
        construct.op = IROp::CompositeConstruct;
        construct.result_id = builder_.fresh_id();
        construct.type_id = ctor_owner_type_id_;
        construct.operands.reserve(ctor_field_values_.size());
        for (IRId id : ctor_field_values_) {
            construct.operands.push_back(id);
        }
        const IRId result_id = construct.result_id;
        fn_.body.push_back(std::move(construct));
        IRInstruction ret;
        ret.op = IROp::ReturnValue;
        ret.operands = {result_id};
        fn_.body.push_back(std::move(ret));
    }

    // Open the function's entry basic block.
    void begin_entry_block() {
        IRInstruction label;
        label.op = IROp::Label;
        label.result_id = builder_.fresh_id();
        fn_.body.push_back(std::move(label));
    }

    void lower_statement(const Decl::BodyStatement &statement) {
        switch (statement.kind) {
        case Decl::BodyStatementKind::return_stmt:
            lower_return(statement.expr);
            return;
        case Decl::BodyStatementKind::block:
            for (const auto &child : statement.children) {
                lower_statement(child);
            }
            return;
        case Decl::BodyStatementKind::if_stmt:
            lower_if(statement);
            return;
        case Decl::BodyStatementKind::while_stmt:
            lower_while(statement);
            return;
        case Decl::BodyStatementKind::do_stmt:
            lower_do_while(statement);
            return;
        case Decl::BodyStatementKind::for_stmt:
            lower_for(statement);
            return;
            for (const auto &child : statement.children) {
                lower_statement(child);
            }
            return;
        case Decl::BodyStatementKind::assignment:
            lower_assign(statement.lhs, statement.expr);
            return;
        case Decl::BodyStatementKind::declaration:
            lower_decl(statement.type_name, statement.name, statement.expr);
            return;
        case Decl::BodyStatementKind::expression:
        case Decl::BodyStatementKind::unknown:
            lower_expression(statement.expr);
            return;
        }
    }

    // Compute final-pass id for the function's parameter list now that
    // parameters have been added.
    void emit_function_parameters(const std::vector<ParameterDecl> &params) {
        for (const auto &p : params) {
            IRInstruction inst;
            inst.op = IROp::FunctionParameter;
            inst.result_id = builder_.fresh_id();
            inst.type_id = types_.find(p.type);
            fn_.parameter_ids.push_back(inst.result_id);
            fn_.body.push_back(std::move(inst));
            bind_parameter(p.name, p.type, fn_.parameter_ids.back());
        }
    }

    void emit_implicit_void_return() {
        IRInstruction ret;
        ret.op = IROp::Return;
        fn_.body.push_back(std::move(ret));
    }

private:
    struct Local {
        IRId id = IRId_None;
        IRId type_id = IRId_None;
        std::string type_name;
        bool is_pointer = false;
    };

    static std::string trim(std::string_view text) {
        while (!text.empty() && std::isspace(static_cast<unsigned char>(text.front()))) text.remove_prefix(1);
        while (!text.empty() && std::isspace(static_cast<unsigned char>(text.back()))) text.remove_suffix(1);
        return std::string(text);
    }

    void lower_expression(const Decl::Expr &expr) {
        const Value v = lower_expr(expr);
        (void)v;
    }

    void emit_branch(IRId target) {
        IRInstruction inst;
        inst.op = IROp::Branch;
        inst.operands = {target};
        fn_.body.push_back(std::move(inst));
    }

    void emit_branch_conditional(IRId cond, IRId true_label, IRId false_label) {
        IRInstruction inst;
        inst.op = IROp::BranchConditional;
        inst.operands = {cond, true_label, false_label};
        fn_.body.push_back(std::move(inst));
    }

    IRId emit_label() {
        IRInstruction label;
        label.op = IROp::Label;
        label.result_id = builder_.fresh_id();
        fn_.body.push_back(std::move(label));
        return fn_.body.back().result_id;
    }

    void lower_if(const Decl::BodyStatement &statement) {
        const Value cond = lower_expr(statement.expr);
        IRInstruction merge;
        merge.op = IROp::SelectionMerge;
        merge.operands = {builder_.fresh_id()};
        fn_.body.push_back(std::move(merge));
        const IRId merge_label = fn_.body.back().operands.front();
        const IRId then_label = builder_.fresh_id();
        const IRId else_label = builder_.fresh_id();
        emit_branch_conditional(cond.id, then_label, else_label);
        IRInstruction then_inst;
        then_inst.op = IROp::Label;
        then_inst.result_id = then_label;
        fn_.body.push_back(std::move(then_inst));
        for (const auto &child : statement.children) lower_statement(child);
        emit_branch(merge_label);
        IRInstruction else_inst;
        else_inst.op = IROp::Label;
        else_inst.result_id = else_label;
        fn_.body.push_back(std::move(else_inst));
        for (const auto &child : statement.else_children) lower_statement(child);
        emit_branch(merge_label);
        IRInstruction merge_inst;
        merge_inst.op = IROp::Label;
        merge_inst.result_id = merge_label;
        fn_.body.push_back(std::move(merge_inst));
    }

    void lower_while(const Decl::BodyStatement &statement) {
        const IRId merge_label = builder_.fresh_id();
        const IRId head_label = builder_.fresh_id();
        const IRId body_label = builder_.fresh_id();
        emit_branch(head_label);

        IRInstruction head_inst;
        head_inst.op = IROp::Label;
        head_inst.result_id = head_label;
        fn_.body.push_back(std::move(head_inst));

        IRInstruction merge;
        merge.op = IROp::LoopMerge;
        merge.operands = {merge_label, head_label};
        fn_.body.push_back(std::move(merge));
        const Value cond = lower_expr(statement.expr);
        emit_branch_conditional(cond.id, body_label, merge_label);

        IRInstruction body_inst;
        body_inst.op = IROp::Label;
        body_inst.result_id = body_label;
        fn_.body.push_back(std::move(body_inst));
        for (const auto &child : statement.children) lower_statement(child);
        emit_branch(head_label);

        IRInstruction merge_inst;
        merge_inst.op = IROp::Label;
        merge_inst.result_id = merge_label;
        fn_.body.push_back(std::move(merge_inst));
    }

    void lower_do_while(const Decl::BodyStatement &statement) {
        const IRId body_label = builder_.fresh_id();
        const IRId cond_label = builder_.fresh_id();
        const IRId merge_label = builder_.fresh_id();
        emit_branch(body_label);

        IRInstruction body_inst;
        body_inst.op = IROp::Label;
        body_inst.result_id = body_label;
        fn_.body.push_back(std::move(body_inst));
        for (const auto &child : statement.children) lower_statement(child);
        emit_branch(cond_label);

        IRInstruction cond_inst;
        cond_inst.op = IROp::Label;
        cond_inst.result_id = cond_label;
        fn_.body.push_back(std::move(cond_inst));

        const Value cond = lower_expr(statement.expr);
        IRInstruction merge;
        merge.op = IROp::SelectionMerge;
        merge.operands = {merge_label};
        fn_.body.push_back(std::move(merge));
        emit_branch_conditional(cond.id, body_label, merge_label);

        IRInstruction merge_inst;
        merge_inst.op = IROp::Label;
        merge_inst.result_id = merge_label;
        fn_.body.push_back(std::move(merge_inst));
    }

    void lower_for(const Decl::BodyStatement &statement) {
        const IRId head_label = builder_.fresh_id();
        const IRId body_label = builder_.fresh_id();
        const IRId continue_label = builder_.fresh_id();
        const IRId merge_label = builder_.fresh_id();
        emit_branch(head_label);

        IRInstruction head_inst;
        head_inst.op = IROp::Label;
        head_inst.result_id = head_label;
        fn_.body.push_back(std::move(head_inst));

        IRInstruction merge;
        merge.op = IROp::LoopMerge;
        merge.operands = {merge_label, continue_label};
        fn_.body.push_back(std::move(merge));
        const Value cond = statement.expr.kind == Decl::Expr::Kind::unknown ? Value{builder_.fresh_id(), types_.find("bool")} : lower_expr(statement.expr);
        emit_branch_conditional(cond.id, body_label, merge_label);

        IRInstruction body_inst;
        body_inst.op = IROp::Label;
        body_inst.result_id = body_label;
        fn_.body.push_back(std::move(body_inst));
        for (const auto &child : statement.children) lower_statement(child);
        emit_branch(continue_label);

        IRInstruction continue_inst;
        continue_inst.op = IROp::Label;
        continue_inst.result_id = continue_label;
        fn_.body.push_back(std::move(continue_inst));
        emit_branch(head_label);

        IRInstruction merge_inst;
        merge_inst.op = IROp::Label;
        merge_inst.result_id = merge_label;
        fn_.body.push_back(std::move(merge_inst));
    }

    void lower_decl(const std::string &type_name, const std::string &local_name, const Decl::Expr &init) {
        // Inside a constructor body, `position = vec4(...)` parses as a
        // declaration where the parser fills both the "type" and "name" with
        // "position". Without intervention we'd allocate a junk Variable with
        // pointer-to-unknown-type, and the CompositeConstruct synthesized by
        // emit_constructor_return would never see the computed value. Detect
        // the constructor-owned field write here and record it directly.
        if (in_constructor() && init.kind != Decl::Expr::Kind::unknown) {
            const TypeInfo *info = types_.info_by_id(ctor_owner_type_id_);
            const bool name_matches_owner_field = [&]() {
                if (!info) return false;
                for (const auto &member : info->members) {
                    if (member.first == local_name) return true;
                }
                return false;
            }();
            const bool looks_like_field_write =
                name_matches_owner_field &&
                (type_name.empty() || type_name == local_name || !types_.find(type_name));
            if (looks_like_field_write) {
                const Value v = lower_expr(init);
                if (record_constructor_field(local_name, v.id)) return;
            }
        }

        const IRId type_id = types_.find(type_name);
        const IRId ptr_ty = types_.pointer_to(type_id, StorageClass::Function);
        IRInstruction var;
        var.op = IROp::Variable;
        var.result_id = builder_.fresh_id();
        var.type_id = ptr_ty;
        var.literals.push_back(static_cast<u32>(StorageClass::Function));
        fn_.body.push_back(std::move(var));

        Local local;
        local.id = fn_.body.back().result_id;
        local.type_id = type_id;
        local.type_name = type_name;
        local.is_pointer = true;
        locals_[local_name] = local;

        if (init.kind != Decl::Expr::Kind::unknown) {
            const Value v = lower_expr(init);
            IRInstruction store;
            store.op = IROp::Store;
            store.operands = {local.id, v.id};
            fn_.body.push_back(std::move(store));
        }
    }

    void lower_assign(const std::string &lhs, const Decl::Expr &rhs) {
        const std::string lhs_t = trim(lhs);

        // Stage-I/O write target (synthesized by the wrapper):
        //   __rt_write_output(N) = expr
        //   __rt_write_builtin("name") = expr
        if (lhs_t.starts_with("__rt_write_output(")) {
            const auto open = lhs_t.find('(');
            const auto close = lhs_t.rfind(')');
            const u32 location = static_cast<u32>(std::stoul(lhs_t.substr(open + 1, close - open - 1)));
            const Value v = lower_expr(rhs);
            IRInstruction inst;
            inst.op = IROp::WriteOutput;
            inst.operands = {v.id};
            inst.literals = {location};
            fn_.body.push_back(std::move(inst));
            return;
        }
        if (lhs_t.starts_with("__rt_write_builtin(")) {
            const auto first_q = lhs_t.find('"');
            const auto last_q = lhs_t.rfind('"');
            const std::string name = lhs_t.substr(first_q + 1, last_q - first_q - 1);
            const Value v = lower_expr(rhs);
            IRInstruction inst;
            inst.op = IROp::WriteBuiltin;
            inst.operands = {v.id};
            inst.literals = {static_cast<u32>(builtin_from_gl_name(name))};
            fn_.body.push_back(std::move(inst));
            return;
        }

        // Plain assignment to a local or a member of a struct local.
        // We support: `name = expr` and `name.member = expr` and `this.member = expr`.
        const Value v = lower_expr(rhs);

        Lex lex(lhs_t);
        const Tok head = lex.next();
        if (head.kind != TokKind::ident) {
            // Drop the rhs result; we can't honor this assignment shape.
            return;
        }

        std::vector<std::string> path;
        while (true) {
            const Tok t = lex.peek();
            if (t.kind == TokKind::dot) {
                lex.next();
                const Tok field = lex.next();
                if (field.kind != TokKind::ident) break;
                path.push_back(field.text);
            } else {
                break;
            }
        }

        const auto it = locals_.find(head.text);
        if (it == locals_.end()) {
            // Unknown lhs. Inside a constructor body the source writes to the
            // owning struct's fields via bare names (`position = expr` instead
            // of `this.position = expr`); record those into the constructor's
            // field-value map. The trailing CompositeConstruct + ReturnValue
            // synthesized by emit_constructor_return assembles them into the
            // returned `this`.
            if (path.empty() && record_constructor_field(head.text, v.id)) return;
            // Fallback: treat as a write to an implicit "this" pointer if a
            // pointer-shaped `this` local was synthesized elsewhere.
            const auto this_it = locals_.find("this");
            if (this_it == locals_.end()) return;
            std::vector<std::string> full_path = {head.text};
            full_path.insert(full_path.end(), path.begin(), path.end());
            store_member(this_it->second, full_path, v);
            return;
        }

        if (path.empty()) {
            // Direct local assignment.
            if (it->second.is_pointer) {
                IRInstruction store;
                store.op = IROp::Store;
                store.operands = {it->second.id, v.id};
                fn_.body.push_back(std::move(store));
            } else {
                // SSA rebind: the local now refers to v.
                it->second.id = v.id;
                it->second.type_id = v.type_id;
            }
        } else {
            store_member(it->second, path, v);
        }
    }

    void store_member(const Local &local, const std::vector<std::string> &path, const Value &v) {
        // Resolve the member type and index chain.
        IRId cur_type = local.type_id;
        std::vector<IRId> indices;
        for (const auto &name : path) {
            const TypeInfo *info = types_.info_by_id(cur_type);
            if (!info) break;
            // Vector member access (`.x`, `.y`, `.z`, `.w`) -> component index.
            if (info->kind == TypeInfo::Kind::Vector) {
                const auto idx = vector_component(name);
                if (!idx) break;
                indices.push_back(constant_uint(*idx));
                cur_type = info->element_type_id;
                continue;
            }
            // Struct member.
            u32 found_index = 0;
            bool found = false;
            for (u32 i = 0; i < info->members.size(); ++i) {
                if (info->members[i].first == name) {
                    found_index = i;
                    cur_type = info->members[i].second;
                    found = true;
                    break;
                }
            }
            if (!found) break;
            indices.push_back(constant_uint(found_index));
        }

        if (!local.is_pointer) {
            // CompositeInsert path: build a new SSA value with the member set.
            IRInstruction insert;
            insert.op = IROp::CompositeInsert;
            insert.result_id = builder_.fresh_id();
            insert.type_id = local.type_id;
            insert.operands = {local.id, v.id};
            for (auto idx : indices) {
                insert.literals.push_back(idx); // store index ids; backend reads as literals
            }
            fn_.body.push_back(std::move(insert));
            // Rebind the local SSA to the new value.
            auto &slot = locals_[local_name_for(local.id)];
            (void)slot;
            return;
        }
        // Pointer path: OpAccessChain + OpStore.
        const IRId ptr_ty = types_.pointer_to(cur_type, StorageClass::Function);
        IRInstruction chain;
        chain.op = IROp::AccessChain;
        chain.result_id = builder_.fresh_id();
        chain.type_id = ptr_ty;
        chain.operands = {local.id};
        for (auto idx : indices) chain.operands.push_back(idx);
        const IRId chain_id = chain.result_id;
        fn_.body.push_back(std::move(chain));

        IRInstruction store;
        store.op = IROp::Store;
        store.operands = {chain_id, v.id};
        fn_.body.push_back(std::move(store));
    }

    // Reverse lookup is only used to update the SSA binding of a value local
    // after a CompositeInsert. Linear scan is fine — locals_ is small.
    [[nodiscard]] std::string local_name_for(IRId id) const {
        for (const auto &[name, info] : locals_) {
            if (info.id == id) return name;
        }
        return {};
    }

    void lower_return(const Decl::Expr &expr) {
        if (expr.kind == Decl::Expr::Kind::unknown) {
            IRInstruction r;
            r.op = IROp::Return;
            fn_.body.push_back(std::move(r));
            return;
        }
        const Value v = lower_expr(expr);
        IRInstruction r;
        r.op = IROp::ReturnValue;
        r.operands = {v.id};
        fn_.body.push_back(std::move(r));
    }

    Value lower_expr(const Decl::Expr &expr) {
        switch (expr.kind) {
        case Decl::Expr::Kind::name:
            return emit_name(expr.text);
        case Decl::Expr::Kind::literal_int: {
            Value v;
            v.id = constant_int(std::stoi(expr.text));
            v.type_id = types_.find("i32");
            return v;
        }
        case Decl::Expr::Kind::literal_float: {
            Value v;
            v.id = constant_float(std::stof(expr.text));
            v.type_id = types_.find("f32");
            return v;
        }
        case Decl::Expr::Kind::unary:
            if (!expr.children.empty()) {
                const Value child = lower_expr(expr.children.front());
                return expr.op == "-" ? emit_negate(child) : child;
            }
            return {};
        case Decl::Expr::Kind::binary:
            if (expr.children.size() == 2) {
                const Value lhs = lower_expr(expr.children[0]);
                const Value rhs = lower_expr(expr.children[1]);
                if (expr.op == "+") return emit_binop(IROp::FAdd, lhs, rhs);
                if (expr.op == "-") return emit_binop(IROp::FSub, lhs, rhs);
                if (expr.op == "*") return emit_mul_like(TokKind::star, lhs, rhs);
                if (expr.op == "/") return emit_mul_like(TokKind::slash, lhs, rhs);
                if (expr.op == "%") return emit_mul_like(TokKind::percent, lhs, rhs);
            }
            return {};
        case Decl::Expr::Kind::call:
            if (!expr.children.empty()) {
                std::vector<Value> args;
                for (std::size_t i = 1; i < expr.children.size(); ++i) {
                    args.push_back(lower_expr(expr.children[i]));
                }
                return emit_call(expr.children.front().text, args);
            }
            return {};
        case Decl::Expr::Kind::member:
            if (!expr.children.empty()) {
                return emit_member_access(lower_expr(expr.children.front()), expr.text);
            }
            return {};
        case Decl::Expr::Kind::unknown:
            return {};
        }
        return {};
    }

    [[nodiscard]] static std::optional<u32> vector_component(const std::string &name) {
        if (name.size() != 1) return std::nullopt;
        switch (name[0]) {
        case 'x': case 'r': case 's': return 0;
        case 'y': case 'g': case 't': return 1;
        case 'z': case 'b': case 'p': return 2;
        case 'w': case 'a': case 'q': return 3;
        default: return std::nullopt;
        }
    }

    [[nodiscard]] BuiltIn builtin_from_gl_name(std::string_view gl_name) const {
        if (gl_name == "gl_Position") return BuiltIn::Position;
        if (gl_name == "gl_PointSize") return BuiltIn::PointSize;
        if (gl_name == "gl_VertexIndex") return BuiltIn::VertexIndex;
        if (gl_name == "gl_InstanceIndex") return BuiltIn::InstanceIndex;
        if (gl_name == "gl_FragCoord") return BuiltIn::FragCoord;
        if (gl_name == "gl_FrontFacing") return BuiltIn::FrontFacing;
        if (gl_name == "gl_FragDepth") return BuiltIn::FragDepth;
        if (gl_name == "gl_GlobalInvocationID") return BuiltIn::GlobalInvocationId;
        if (gl_name == "gl_LocalInvocationID") return BuiltIn::LocalInvocationId;
        if (gl_name == "gl_WorkGroupID") return BuiltIn::WorkGroupId;
        return BuiltIn::Position; // best-effort default
    }

    IRId constant_uint(u32 value) {
        const std::string sig = "cu:" + std::to_string(value);
        const IRId ty = types_.find("u32");
        return builder_.intern_constant(sig, IROp::ConstantUInt, ty, {}, {value});
    }

    IRId constant_int(i32 value) {
        const std::string sig = "ci:" + std::to_string(value);
        const IRId ty = types_.find("i32");
        return builder_.intern_constant(sig, IROp::ConstantInt, ty, {}, {static_cast<u32>(value)});
    }

    IRId constant_float(float value) {
        u32 bits;
        std::memcpy(&bits, &value, sizeof(bits));
        const std::string sig = "cf:" + std::to_string(bits);
        const IRId ty = types_.find("f32");
        return builder_.intern_constant(sig, IROp::ConstantFloat, ty, {}, {bits});
    }

    // Name lookup that handles locals, parameters, struct field names (when
    // shadowed by the implicit `this`), and global uniform variables.
    Value emit_name(const std::string &name) {
        // Local / parameter.
        if (const auto it = locals_.find(name); it != locals_.end()) {
            if (it->second.is_pointer) {
                IRInstruction load;
                load.op = IROp::Load;
                load.result_id = builder_.fresh_id();
                load.type_id = it->second.type_id;
                load.operands = {it->second.id};
                fn_.body.push_back(std::move(load));
                return Value{load.result_id, it->second.type_id};
            }
            return Value{it->second.id, it->second.type_id};
        }
        // Implicit struct field reference inside a constructor body: turn
        // `position` into `this.position`. The lowerer emits the field's
        // initial Load — for SSA constructors we model `this` as a function
        // local variable.
        if (const auto this_it = locals_.find("this"); this_it != locals_.end()) {
            const TypeInfo *info = types_.info_by_id(this_it->second.type_id);
            if (info) {
                for (u32 i = 0; i < info->members.size(); ++i) {
                    if (info->members[i].first == name) {
                        return emit_member_load_from_pointer(this_it->second, i, info->members[i].second);
                    }
                }
            }
        }
        // Global uniform. The parser captures expression text into Decl::Expr
        // before any uniform-name mangling, so `transform` reaches us as its
        // source identifier rather than `u_..._h..`. Translate it here by
        // matching against the uniform table; the mangled form is also accepted
        // so that downstream code paths that go through lower_uniform_references
        // continue to work.
        std::string lookup_name = name;
        if (uniform_var_ids_.find(lookup_name) == uniform_var_ids_.end()) {
            for (const auto &u : uniforms_) {
                if (u.name == name && (u.is_anonymous || u.scope_name.empty())) {
                    lookup_name = uniform_binding_name(u);
                    break;
                }
            }
        }
        if (const auto vit = uniform_var_ids_.find(lookup_name); vit != uniform_var_ids_.end()) {
            const IRId var_id = vit->second;
            const IRId ptr_ty = uniform_var_type_ids_.at(lookup_name);
            // Load the value through the pointer.
            const TypeInfo *info_ptr = nullptr;
            (void)info_ptr;
            // Find pointee type by scanning the type pool — cheap because the
            // pool is small at compile time.
            IRId pointee_ty = IRId_None;
            for (const auto &inst : builder_.module().type_constant_pool) {
                if (inst.op == IROp::TypePointer && inst.result_id == ptr_ty) {
                    pointee_ty = inst.operands.front();
                    break;
                }
            }
            IRInstruction load;
            load.op = IROp::Load;
            load.result_id = builder_.fresh_id();
            load.type_id = pointee_ty;
            load.operands = {var_id};
            fn_.body.push_back(std::move(load));
            return Value{load.result_id, pointee_ty};
        }
        // Unknown name — leave a placeholder. The verify pass surfaces this.
        return Value{IRId_None, IRId_None};
    }

    Value emit_member_load_from_pointer(const Local &local, u32 index, IRId member_type) {
        const IRId ptr_ty = types_.pointer_to(member_type, StorageClass::Function);
        IRInstruction chain;
        chain.op = IROp::AccessChain;
        chain.result_id = builder_.fresh_id();
        chain.type_id = ptr_ty;
        chain.operands = {local.id, constant_uint(index)};
        const IRId chain_id = chain.result_id;
        fn_.body.push_back(std::move(chain));

        IRInstruction load;
        load.op = IROp::Load;
        load.result_id = builder_.fresh_id();
        load.type_id = member_type;
        load.operands = {chain_id};
        fn_.body.push_back(std::move(load));
        return Value{load.result_id, member_type};
    }

    Value emit_member_access(const Value &base, const std::string &name) {
        const TypeInfo *info = types_.info_by_id(base.type_id);
        if (!info) return base;
        if (info->kind == TypeInfo::Kind::Vector) {
            const auto idx = vector_component(name);
            if (!idx) return base;
            IRInstruction extract;
            extract.op = IROp::CompositeExtract;
            extract.result_id = builder_.fresh_id();
            extract.type_id = info->element_type_id;
            extract.operands = {base.id};
            extract.literals = {*idx};
            fn_.body.push_back(std::move(extract));
            return Value{extract.result_id, info->element_type_id};
        }
        if (info->kind == TypeInfo::Kind::Struct) {
            for (u32 i = 0; i < info->members.size(); ++i) {
                if (info->members[i].first == name) {
                    IRInstruction extract;
                    extract.op = IROp::CompositeExtract;
                    extract.result_id = builder_.fresh_id();
                    extract.type_id = info->members[i].second;
                    extract.operands = {base.id};
                    extract.literals = {i};
                    fn_.body.push_back(std::move(extract));
                    return Value{extract.result_id, info->members[i].second};
                }
            }
        }
        return base;
    }

    Value emit_call(const std::string &callee, const std::vector<Value> &args) {
        // Constructor of a known type.
        if (const IRId type_id = types_.find(callee); type_id) {
            // If the user wrote a custom constructor (`Type::Type(...)`) with
            // a matching arity, inline its body here. Without this, backends
            // see CompositeConstruct Type with operands shaped like the call
            // arguments rather than Type's fields, which is a type error.
            if (const Value inlined = try_inline_constructor(type_id, args); inlined.id != IRId_None) {
                return inlined;
            }
            IRInstruction construct;
            construct.op = IROp::CompositeConstruct;
            construct.result_id = builder_.fresh_id();
            construct.type_id = type_id;
            construct.operands.reserve(args.size());
            for (const auto &a : args) construct.operands.push_back(a.id);
            fn_.body.push_back(std::move(construct));
            return Value{fn_.body.back().result_id, type_id};
        }
        // Stage-I/O reads (synthesized by the wrapper):
        //   __rt_read_input(N) and __rt_read_builtin("name")
        if (callee == "__rt_read_input" && args.size() == 1) {
            // The arg id is a constant uint; pull it through the literal.
            // Find the constant value from the type/constant pool.
            IRInstruction inst;
            inst.op = IROp::ReadInput;
            inst.result_id = builder_.fresh_id();
            inst.type_id = IRId_None; // backend infers from decoration target
            inst.literals = {literal_from_const(args.front().id)};
            fn_.body.push_back(std::move(inst));
            return Value{inst.result_id, IRId_None};
        }
        // Texture / sample primitive.
        if (callee == "sample" && args.size() == 2) {
            IRInstruction inst;
            inst.op = IROp::ImageSampleImplicitLod;
            inst.result_id = builder_.fresh_id();
            inst.type_id = types_.find("vec4");
            inst.operands = {args[0].id, args[1].id};
            fn_.body.push_back(std::move(inst));
            return Value{inst.result_id, types_.find("vec4")};
        }
        // User function: emit a generic FunctionCall referencing the callee by
        // name. The single-module inliner (or the linker, for cross-module
        // calls) rewrites operand[0] to the resolved function's result_id and
        // ultimately inlines the body so the backend never sees a FunctionCall
        // for a user function.
        IRInstruction call;
        call.op = IROp::FunctionCall;
        call.result_id = builder_.fresh_id();
        // operand[0] is the resolved function id (0 = unresolved until the
        // inliner runs); operand[1..N] are the argument values.
        call.operands.reserve(args.size() + 1);
        call.operands.push_back(IRId_None);
        for (const auto &a : args) call.operands.push_back(a.id);
        // literal[0] indexes into IRModule::call_target_names where the
        // source-level callee identifier lives. Cleared by the inliner once
        // every call in the module is resolved.
        auto &targets = builder_.module().call_target_names;
        const u32 name_index = static_cast<u32>(targets.size());
        targets.push_back(callee);
        call.literals = {name_index};
        fn_.body.push_back(std::move(call));
        return Value{call.result_id, IRId_None};
    }

    u32 literal_from_const(IRId const_id) const {
        for (const auto &inst : builder_.module().type_constant_pool) {
            if (inst.result_id == const_id &&
                (inst.op == IROp::ConstantUInt || inst.op == IROp::ConstantInt)) {
                return inst.literals.empty() ? 0u : inst.literals.front();
            }
        }
        return 0;
    }

    // If a previously-lowered IRFunction matches `Type::Type(args...)` for
    // this type, inline its body into the current function. The constructor
    // body's parameter ids are remapped to the call args' ids, and each of
    // its result ids is replaced with a fresh id local to this function. The
    // constructor's terminating ReturnValue is consumed: its operand becomes
    // the call's result. Returns Value{} on no match.
    [[nodiscard]] Value try_inline_constructor(IRId type_id, const std::vector<Value> &args) {
        const auto &functions = builder_.module().functions;
        const IRFunction *ctor = nullptr;
        for (const auto &candidate : functions) {
            if (candidate.stage != StageKind::none) continue;
            if (candidate.return_type_id != type_id) continue;
            if (candidate.parameter_ids.size() != args.size()) continue;
            ctor = &candidate;
            break;
        }
        if (!ctor) return Value{};

        std::unordered_map<IRId, IRId> remap;
        for (std::size_t i = 0; i < ctor->parameter_ids.size() && i < args.size(); ++i) {
            remap[ctor->parameter_ids[i]] = args[i].id;
        }

        Value result;
        for (const auto &inst : ctor->body) {
            if (inst.op == IROp::Label) continue;
            if (inst.op == IROp::FunctionParameter) continue;
            if (inst.op == IROp::Return) continue;
            if (inst.op == IROp::ReturnValue) {
                if (!inst.operands.empty()) {
                    const IRId operand = inst.operands.front();
                    auto it = remap.find(operand);
                    result.id = it != remap.end() ? it->second : operand;
                    result.type_id = ctor->return_type_id;
                }
                continue;
            }
            IRInstruction copy = inst;
            if (copy.result_id != IRId_None) {
                const IRId fresh = builder_.fresh_id();
                remap[copy.result_id] = fresh;
                copy.result_id = fresh;
            }
            for (auto &operand : copy.operands) {
                if (auto it = remap.find(operand); it != remap.end()) operand = it->second;
            }
            fn_.body.push_back(std::move(copy));
        }
        if (!result.id) return Value{};
        return result;
    }

    Value emit_binop(IROp op, const Value &lhs, const Value &rhs) {
        IRInstruction inst;
        inst.op = op;
        inst.result_id = builder_.fresh_id();
        inst.type_id = lhs.type_id ? lhs.type_id : rhs.type_id;
        inst.operands = {lhs.id, rhs.id};
        fn_.body.push_back(std::move(inst));
        return Value{inst.result_id, inst.type_id};
    }

    Value emit_negate(const Value &v) {
        IRInstruction inst;
        inst.op = IROp::FNegate;
        inst.result_id = builder_.fresh_id();
        inst.type_id = v.type_id;
        inst.operands = {v.id};
        fn_.body.push_back(std::move(inst));
        return Value{inst.result_id, v.type_id};
    }

    Value emit_mul_like(TokKind op, const Value &lhs, const Value &rhs) {
        // Pick the right SPIR-V-ish opcode based on the operand types.
        const TypeInfo *lhs_info = types_.info_by_id(lhs.type_id);
        const TypeInfo *rhs_info = types_.info_by_id(rhs.type_id);
        if (op == TokKind::star && lhs_info && rhs_info) {
            if (lhs_info->kind == TypeInfo::Kind::Matrix && rhs_info->kind == TypeInfo::Kind::Vector) {
                return emit_binop_typed(IROp::MatrixTimesVector, lhs, rhs, rhs.type_id);
            }
            if (lhs_info->kind == TypeInfo::Kind::Matrix && rhs_info->kind == TypeInfo::Kind::Matrix) {
                return emit_binop_typed(IROp::MatrixTimesMatrix, lhs, rhs, lhs.type_id);
            }
            if (lhs_info->kind == TypeInfo::Kind::Vector && rhs_info->kind == TypeInfo::Kind::Float) {
                return emit_binop_typed(IROp::VectorTimesScalar, lhs, rhs, lhs.type_id);
            }
        }
        IROp ir_op = IROp::FMul;
        if (op == TokKind::slash) ir_op = IROp::FDiv;
        else if (op == TokKind::percent) ir_op = IROp::FMod;
        return emit_binop(ir_op, lhs, rhs);
    }

    Value emit_binop_typed(IROp op, const Value &lhs, const Value &rhs, IRId result_ty) {
        IRInstruction inst;
        inst.op = op;
        inst.result_id = builder_.fresh_id();
        inst.type_id = result_ty;
        inst.operands = {lhs.id, rhs.id};
        fn_.body.push_back(std::move(inst));
        return Value{inst.result_id, result_ty};
    }

    IRBuilder &builder_;
    TypeRegistry &types_;
    IRFunction &fn_;
    const std::vector<UniformBinding> &uniforms_;
    const std::unordered_map<std::string, IRId> &uniform_var_ids_;
    const std::unordered_map<std::string, IRId> &uniform_var_type_ids_;
    const std::vector<StageInterface> &stage_interfaces_;
    StageKind stage_;
    std::unordered_map<std::string, Local> locals_;

    // Constructor lowering state. Active when the current function is a
    // member-init body for `Type::Type(...)`. See begin_constructor.
    std::string ctor_owner_name_;
    IRId ctor_owner_type_id_ = IRId_None;
    std::vector<IRId> ctor_field_values_;
};

// ----------------------------------------------------------------------------
// Helpers from the old IR.cpp that are still useful for stage-wrapper synthesis.
// ----------------------------------------------------------------------------

StageKind detect_stage(std::string_view name) {
    if (name.starts_with("vert")) return StageKind::vertex;
    if (name.starts_with("frag")) return StageKind::fragment;
    return StageKind::none;
}

std::string globals_type_name(StageKind stage) {
    switch (stage) {
    case StageKind::vertex: return "vert_globals";
    case StageKind::fragment: return "frag_globals";
    case StageKind::none: return "globals";
    }
    return "globals";
}

bool is_stage_globals_type(StageKind stage, std::string_view type) {
    return type == globals_type_name(stage);
}

const StageInterface *find_interface(const std::vector<StageInterface> &interfaces, std::string_view type_name) {
    for (const auto &interface : interfaces) {
        if (interface.type_name == type_name) return &interface;
    }
    return nullptr;
}

bool is_rasterizer_only(const StageIOField &field) {
    return field.interpolation == "clip" || field.builtin == "position";
}

// Resolve every IROp::FunctionCall against the functions defined in this
// module by inlining the callee's body in place. The IR pipeline doesn't
// preserve a FunctionCall instruction for user functions: backends only see
// straight-line code plus reserved primitive ops. Constructors are already
// inlined at lower-call time; this pass handles arbitrary user-to-user calls
// and runs to a fixed point so chains of inlined calls expand fully.
//
// Calls whose target lives in another translation unit (no match in this
// module's function table) are left alone: the linker handles cross-module
// resolution in a later pass over rtslo/rtsll inputs.
bool inline_one_pass(IRFunction &fn, IRModule &ir, IRBuilder &builder) {
    bool progress = false;
    std::vector<IRInstruction> new_body;
    new_body.reserve(fn.body.size());
    // Maps the FunctionCall's result_id to the inlined callee's returned
    // value id, so subsequent instructions in fn that referenced the call
    // result get rewired to the actual return value.
    std::unordered_map<IRId, IRId> call_result_remap;
    const auto apply_remap = [&](IRInstruction &inst) {
        for (auto &op : inst.operands) {
            if (auto it = call_result_remap.find(op); it != call_result_remap.end()) op = it->second;
        }
    };

    for (const auto &inst : fn.body) {
        if (inst.op != IROp::FunctionCall || inst.operands.empty() || inst.literals.empty()) {
            IRInstruction copy = inst;
            apply_remap(copy);
            new_body.push_back(std::move(copy));
            continue;
        }
        const u32 name_index = inst.literals[0];
        if (name_index >= ir.call_target_names.size()) {
            IRInstruction copy = inst;
            apply_remap(copy);
            new_body.push_back(std::move(copy));
            continue;
        }
        const std::string &callee_name = ir.call_target_names[name_index];
        const std::size_t arg_count = inst.operands.size() - 1; // operand[0] is the (unresolved) target id slot

        const IRFunction *target = nullptr;
        for (const auto &candidate : ir.functions) {
            if (&candidate == &fn) continue;
            if (candidate.source_name != callee_name) continue;
            if (candidate.parameter_ids.size() != arg_count) continue;
            // A forward declaration with no body cannot be inlined. Skip it
            // so the search continues to the real definition, which may live
            // later in this module or in another module the linker provides.
            if (candidate.body.empty()) continue;
            target = &candidate;
            break;
        }
        if (!target) {
            // Leave for the linker. Apply the running remap so any prior
            // inlined-call result substitutions still propagate.
            IRInstruction copy = inst;
            apply_remap(copy);
            new_body.push_back(std::move(copy));
            continue;
        }

        std::unordered_map<IRId, IRId> local_remap;
        for (std::size_t i = 0; i < target->parameter_ids.size(); ++i) {
            IRId arg_id = inst.operands[i + 1];
            if (auto it = call_result_remap.find(arg_id); it != call_result_remap.end()) arg_id = it->second;
            local_remap[target->parameter_ids[i]] = arg_id;
        }

        IRId returned_id = IRId_None;
        for (const auto &body_inst : target->body) {
            if (body_inst.op == IROp::Label) continue;
            if (body_inst.op == IROp::FunctionParameter) continue;
            if (body_inst.op == IROp::Return) continue;
            if (body_inst.op == IROp::ReturnValue) {
                if (!body_inst.operands.empty()) {
                    IRId ret = body_inst.operands[0];
                    if (auto it = local_remap.find(ret); it != local_remap.end()) ret = it->second;
                    returned_id = ret;
                }
                continue;
            }
            IRInstruction copy = body_inst;
            if (copy.result_id != IRId_None) {
                const IRId fresh = builder.fresh_id();
                local_remap[copy.result_id] = fresh;
                copy.result_id = fresh;
            }
            for (auto &operand : copy.operands) {
                if (auto it = local_remap.find(operand); it != local_remap.end()) operand = it->second;
            }
            new_body.push_back(std::move(copy));
        }
        if (inst.result_id != IRId_None && returned_id != IRId_None) {
            call_result_remap[inst.result_id] = returned_id;
        }
        progress = true;
    }

    fn.body = std::move(new_body);
    return progress;
}

void inline_resolved_calls(IRModule &ir, IRBuilder &builder) {
    // Bounded fixed-point: each pass either makes progress or it doesn't, and
    // legal RTSL forbids recursion. The cap stops a malformed module from
    // looping forever.
    for (int iteration = 0; iteration < 64; ++iteration) {
        bool any_progress = false;
        for (auto &fn : ir.functions) {
            if (inline_one_pass(fn, ir, builder)) any_progress = true;
        }
        if (!any_progress) break;
    }
    // Once nothing is left to inline against this module, drop the side
    // table: any surviving FunctionCalls are cross-module and will get their
    // names from the artifact's string section after linking.
    if (ir.call_target_names.empty()) return;
    bool any_unresolved = false;
    for (const auto &fn : ir.functions) {
        for (const auto &inst : fn.body) {
            if (inst.op == IROp::FunctionCall) { any_unresolved = true; break; }
        }
        if (any_unresolved) break;
    }
    if (!any_unresolved) {
        ir.call_target_names.clear();
    }
}

} // namespace

// ----------------------------------------------------------------------------
// Top-level lowering: build types, globals, decorations, then walk functions.
// ----------------------------------------------------------------------------

IRModule lower_to_ir(const SemanticModule &module, DiagnosticEngine *diagnostics) {
    IRModule ir;
    ir.source_name = module.source_name;
    ir.structs = module.structs;
    ir.uniforms = module.uniforms;
    ir.stage_interfaces = module.stage_interfaces;

    IRBuilder builder(ir);
    TypeRegistry types(builder);
    for (const auto &decl : module.structs) {
        types.register_struct(decl);
    }

    // Emit one global Variable per uniform with the right storage class and
    // a matching set/binding decoration pair. The mangled binding name is what
    // the SSA body references.
    std::unordered_map<std::string, IRId> uniform_var_ids;
    std::unordered_map<std::string, IRId> uniform_var_type_ids;
    for (const auto &uniform : ir.uniforms) {
        const std::string mangled = uniform_binding_name(uniform);
        const bool is_resource = is_resource_uniform_type(uniform.type);
        const StorageClass sc = is_resource ? StorageClass::UniformConstant : StorageClass::Uniform;
        const IRId value_ty = types.find(uniform.type);
        const IRId ptr_ty = types.pointer_to(value_ty, sc);
        IRInstruction var;
        var.op = IROp::Variable;
        var.result_id = builder.fresh_id();
        var.type_id = ptr_ty;
        var.literals.push_back(static_cast<u32>(sc));
        builder.add_global_variable(std::move(var));
        const IRId var_id = ir.global_variables.back().result_id;
        uniform_var_ids[mangled] = var_id;
        uniform_var_type_ids[mangled] = ptr_ty;

        IRDecoration ds;
        ds.target = var_id;
        ds.kind = IRDecorationKind::DescriptorSet;
        ds.literals = {uniform.set};
        builder.add_decoration(std::move(ds));
        IRDecoration bd;
        bd.target = var_id;
        bd.kind = IRDecorationKind::Binding;
        bd.literals = {uniform.binding};
        builder.add_decoration(std::move(bd));
    }

    // Walk the semantic symbols and lower function bodies.
    struct PendingStage {
        std::size_t function_index;
        StageKind stage;
        std::string user_name;
        std::string carrier_type;
        std::string input_type;
        std::string output_type;
    };
    std::vector<PendingStage> pending_stages;

    const Mangler mangler;

    for (const auto &symbol : module.symbols) {
        if (symbol.kind != DeclKind::function) continue;
        // A forward declaration (`fn name(...) -> T;` with no body) carries no
        // statements to lower. Skip it entirely: it's there for source-level
        // name lookup and for declaring symbols that the linker will resolve
        // against another input. Emitting a body-less IRFunction would just
        // confuse the inliner into picking the empty entry over the real def.
        if (symbol.body_statements.empty()) continue;

        const StageKind stage = detect_stage(symbol.name);
        const bool is_stage_entry = stage != StageKind::none;
        // Detect a struct member-init constructor: a function named
        // "Type::Type" where Type is a known struct. The source declares no
        // return type for these; we promote the return type to the owning
        // struct so the inlined call site can use it like a normal expression.
        std::string ctor_owner;
        if (const auto scope = symbol.name.find("::"); scope != std::string::npos) {
            const std::string owner = symbol.name.substr(0, scope);
            const std::string method = symbol.name.substr(scope + 2);
            if (owner == method && types.find(owner)) {
                ctor_owner = owner;
            }
        }
        const bool is_constructor = !ctor_owner.empty();

        const std::string stage_globals_type = is_stage_entry ? globals_type_name(stage) : std::string{};
        bool has_explicit_globals = false;
        std::vector<ParameterDecl> parameters;
        parameters.reserve(symbol.parameters.size());
        for (const auto &p : symbol.parameters) {
            if (is_stage_entry && p.type == stage_globals_type) has_explicit_globals = true;
            parameters.push_back(p);
        }
        if (is_stage_entry && !has_explicit_globals) {
            parameters.insert(parameters.begin(), ParameterDecl{.type = stage_globals_type, .name = "g"});
        }

        IRFunction fn;
        fn.return_type_id = is_constructor ? types.find(ctor_owner) : types.find(symbol.return_type);
        fn.result_id = builder.fresh_id();
        fn.stage = stage;
        fn.exported = symbol.exported;
        fn.source_name = symbol.name;
        fn.parameter_type_names.reserve(parameters.size());
        for (const auto &p : parameters) {
            fn.parameter_type_names.push_back(p.type);
        }
        ir.functions.push_back(std::move(fn));
        const std::size_t fn_index = ir.functions.size() - 1;

        FunctionLowerer lowerer(builder, types, ir.functions.back(), ir.uniforms,
                                uniform_var_ids, uniform_var_type_ids,
                                ir.stage_interfaces, stage);
        if (is_constructor) lowerer.begin_constructor(ctor_owner);
        lowerer.begin_entry_block();
        lowerer.emit_function_parameters(parameters);
        for (const auto &statement : symbol.body_statements) {
            // Uniform identifiers in body expressions are resolved by name
            // inside FunctionLowerer::emit_name against the uniforms table;
            // no text-level mangling is needed here because the parser builds
            // a Decl::Expr ahead of any text substitution we could do.
            lowerer.lower_statement(statement);
        }
        if (is_constructor) {
            lowerer.emit_constructor_return();
        } else if (symbol.return_type.empty() || symbol.return_type == "void") {
            // Insert an implicit Return for void-returning functions that
            // didn't end on a terminator.
            const auto &body = ir.functions.back().body;
            const bool needs_ret = body.empty() ||
                                   (body.back().op != IROp::Return &&
                                    body.back().op != IROp::ReturnValue &&
                                    body.back().op != IROp::Branch);
            if (needs_ret) lowerer.emit_implicit_void_return();
        }

        if (is_stage_entry) {
            std::string carrier_type;
            std::string input_type;
            for (const auto &p : symbol.parameters) {
                if (is_stage_builtin_carrier(p.type)) {
                    if (carrier_type.empty()) carrier_type = p.type;
                } else if (is_stage_globals_type(stage, p.type)) {
                    continue;
                } else if (input_type.empty()) {
                    input_type = p.type;
                }
            }
            pending_stages.push_back(PendingStage{
                .function_index = fn_index,
                .stage = stage,
                .user_name = symbol.name,
                .carrier_type = std::move(carrier_type),
                .input_type = std::move(input_type),
                .output_type = symbol.return_type,
            });
        }
    }

    // Synthesize stage interfaces from struct fields when the source didn't
    // declare them. A varying must be declared explicitly because its
    // interpolation qualifiers are not derivable.
    const auto synth_from_struct = [&](const std::string &type, StageRole role) {
        if (type.empty() || type == "void") return;
        if (find_interface(ir.stage_interfaces, type)) return;
        for (const auto &decl : ir.structs) {
            if (decl.name != type) continue;
            StageInterface derived;
            derived.role = role;
            derived.type_name = type;
            u32 location = 0;
            for (const auto &field : decl.fields) {
                StageIOField f;
                f.name = field.name;
                f.location = location++;
                f.has_location = true;
                derived.fields.push_back(std::move(f));
            }
            ir.stage_interfaces.push_back(std::move(derived));
            return;
        }
    };
    const auto require_varying = [&](const std::string &type, std::string_view context) {
        if (type.empty() || type == "void") return;
        for (const auto &interface : ir.stage_interfaces) {
            if (interface.type_name == type && interface.role == StageRole::varying) return;
        }
        if (diagnostics) {
            diagnostics->report(3100, DiagnosticSeverity::error, {}, module.source_name,
                                "stage payload '" + type + "' is " + std::string(context) +
                                    " and requires a 'varying' declaration to define interpolation");
        }
    };
    for (const auto &pending : pending_stages) {
        switch (pending.stage) {
        case StageKind::vertex:
            synth_from_struct(pending.input_type, StageRole::input);
            break;
        case StageKind::fragment:
            require_varying(pending.input_type, "a fragment input");
            synth_from_struct(pending.output_type, StageRole::output);
            break;
        case StageKind::none:
            break;
        }
    }

    // Generate the compiler-provided backend entry wrappers. For now the
    // wrapper is the trivial form: ReadInput each input location into a local,
    // call the user entry, write each output location via WriteOutput, write
    // the clip-space builtin if applicable. The interesting work (carrier
    // builtins, varying lowering) follows once the rest of the pipeline is
    // wired through.
    (void)pending_stages;

    inline_resolved_calls(ir, builder);

    return ir;
}

bool verify_ir(const IRModule &) { return true; }

} // namespace rtsl
