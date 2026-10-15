#include "rutile/compiler.hpp"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <errno.h>
#include <limits.h>
#include <math.h>
#include <float.h>

#include <llvm-c/Core.h>
#include <llvm-c/Analysis.h>
#include <llvm-c/Target.h>
#include <llvm-c/TargetMachine.h>
#include <llvm-c/Transforms/PassBuilder.h>

_Static_assert(FLT_RADIX == 2 && FLT_MANT_DIG == 24 && DBL_MANT_DIG == 53 && sizeof(float) == 4 && sizeof(double) == 8, "LLVM f32/f64 require 32-bit and 64-bit binary floating-point host types.");

enum TypeKind {
	VOID_TYPE,
	BOOL_TYPE,
	NULL_TYPE,
	CHAR_TYPE,
	SIGNED_INTEGER_TYPE,
	UNSIGNED_INTEGER_TYPE,
	FLOAT_TYPE,
	POINTER_TYPE,
	REFERENCE_TYPE,
	ARRAY_TYPE,
	STRUCT_TYPE,
	UNION_TYPE,
	ENUM_TYPE,
	FUNCTION_TYPE,
};

enum TypeQualifier {
	MUTABLE_TYPE,
	CONST_TYPE,
};

struct Type {
	enum TypeKind kind;
	union {
		int bits;
		struct Type* element;
		struct Record* record;
		struct Function* function;
		struct {
			struct Type* element;
			uint64_t count;
		} array;
	} data;
	enum TypeQualifier qualifier;
};

enum NamespaceKind {
	UNRESOLVED_NAMESPACE,
	DECLARED_NAMESPACE,
	ASSOCIATED_NAMESPACE,
	ALIASED_NAMESPACE,
};

struct Namespace {
	const char* name;
	size_t length;
	struct Namespace* parent;
	struct Namespace* next;
	struct Record* record;
	enum NamespaceKind kind;
	const char* source_path;
	struct Namespace* canonical;
};

struct Token {
	int kind;
	const char* start;
	size_t length;
	int line;
	int column;
	const char* path;
	struct Namespace* owner;
};

struct Span {
	struct Token first;
	struct Token last;
};

struct Allocation {
	void* memory;
	struct Allocation* next;
};

struct Variable {
	struct TypeNode* syntax;
	struct Token name;
	struct Type type;
	LLVMValueRef slot;
	struct Variable* next;
	struct Variable* scope_next;
	unsigned scope_depth;
};

struct Expression;
struct Function;

struct Argument {
	struct Expression* expression;
	struct Expression* initializer;
	struct Statement* initialization;
	struct Variable* variable;
	struct Type passed_type;
	struct Argument* next;
};

struct InitializationContext {
	struct Variable* variable;
	struct Type type;
	struct Statement* body;
	struct Expression* initializer;
};

enum TokenKind {
	END = 256,
	NAME,
	INTEGER,
	DECIMAL,
	STRING,
	CHARACTER,
	TRUE,
	FALSE,
	FN,
	VAR,
	RETURN,
	IMPORT,
	AUTO,
	ARROW,
	ELLIPSIS,
	STRUCT,
	UNION,
	ENUM,
	SIZEOF,
	IF,
	ELSE,
	WHILE,
	FOR,
	SWITCH,
	CASE,
	DEFAULT,
	BREAK,
	CONTINUE,
	EQUAL,
	NOT_EQUAL,
	LESS_EQUAL,
	GREATER_EQUAL,
	LOGICAL_AND,
	LOGICAL_OR,
	SHIFT_LEFT,
	SHIFT_RIGHT,
	INCREMENT,
	DECREMENT,
	ADD_ASSIGN,
	SUB_ASSIGN,
	MUL_ASSIGN,
	DIV_ASSIGN,
	MOD_ASSIGN,
	AND_ASSIGN,
	OR_ASSIGN,
	XOR_ASSIGN,
	LEFT_ASSIGN,
	RIGHT_ASSIGN,
	COPY_ASSIGN,
	NAMESPACE,
	QUALIFY,
	VIRTUAL,
	OVERRIDE,
	FINAL,
	CONST,
	DISCARD,
	DELETE,
	NULLPTR,
};

enum NodeKind {
	INTEGER_EXPRESSION,
	BOOLEAN_EXPRESSION,
	NULLPTR_EXPRESSION,
	DECIMAL_EXPRESSION,
	STRING_EXPRESSION,
	NAME_EXPRESSION,
	FUNCTION_EXPRESSION,
	BOUND_FUNCTION_EXPRESSION,
	CALL_EXPRESSION,
	ACCESS_EXPRESSION,
	RELOCATE_EXPRESSION,
	ABANDON_EXPRESSION,
	CONSTRUCTION_EXPRESSION,
	NEGATE_EXPRESSION,
	BINARY_EXPRESSION,
	ASSIGNMENT_EXPRESSION,
	VALUE_INITIALIZATION_EXPRESSION,
	NOT_EXPRESSION,
	COMPLEMENT_EXPRESSION,
	PREFIX_UPDATE_EXPRESSION,
	POSTFIX_UPDATE_EXPRESSION,
	ADDRESS_EXPRESSION,
	REFERENCE_BINDING_EXPRESSION,
	REFERENCE_CAST_EXPRESSION,
	REFERENCE_INITIALIZATION_EXPRESSION,
	BASE_VALUE_EXPRESSION,
	BASE_POINTER_EXPRESSION,
	DEREFERENCE_EXPRESSION,
	INDEX_EXPRESSION,
	FIELD_EXPRESSION,
	POINTER_FIELD_EXPRESSION,
	ENUMERATOR_EXPRESSION,
	CAST_EXPRESSION,
	SIZEOF_TYPE_EXPRESSION,
	SIZEOF_VALUE_EXPRESSION,
	DECLARATION_STATEMENT,
	RETURN_STATEMENT,
	EXPRESSION_STATEMENT,
	EMPTY_STATEMENT,
	BLOCK_STATEMENT,
	IF_STATEMENT,
	WHILE_STATEMENT,
	FOR_STATEMENT,
	SWITCH_STATEMENT,
	BREAK_STATEMENT,
	CONTINUE_STATEMENT,
	FUNCTION_DECLARATION,
	FUNCTION_DEFINITION,
	FUNCTION_DELETED,
	TYPE_NODE,
};

enum TypeSyntaxKind {
	CONST_SYNTAX,
	BUILTIN_SYNTAX,
	POINTER_SYNTAX,
	REFERENCE_SYNTAX,
	ARRAY_SYNTAX,
	NAMED_SYNTAX,
	FUNCTION_SYNTAX,
};

enum ReceiverKind {
	BIND_RECEIVER,
	EVALUATE_RECEIVER,
};

enum BaseConversionKind {
	DERIVED_TO_BASE,
	BASE_TO_DERIVED,
};

struct TypeNode {
	enum TypeSyntaxKind kind;
	struct Span span;
	union {
		const struct BuiltinType* builtin;
		struct Function* function;
		struct {
			enum TypeKind kind;
			struct Token name;
		} named;
		struct TypeNode* element;
		struct {
			struct TypeNode* element;
			uint64_t count;
		} array;
	} data;
};

struct Expression {
	enum NodeKind kind;
	struct Span span;
	struct Type type;
	enum ValueUse {
		OBSERVED_VALUE,
		TRANSFERRED_VALUE,
	} use;
	struct Function* assignment_function;
	LLVMValueRef assignment_address;
	LLVMValueRef temporary_address;
	union {
		struct {
			struct Function* function;
			struct Argument* arguments;
			LLVMValueRef slot;
		} construction;
		uint64_t integer;
		double decimal;
		struct Enumerator* enumerator;
		struct Function* function;
		struct {
			struct Function* function;
			struct Expression* receiver;
			enum ReceiverKind kind;
			struct Record* qualified_owner;
		} bound_function;
		struct {
			struct Expression* value;
			struct BasePath* path;
			enum BaseConversionKind kind;
		} base;
		struct {
			struct TypeNode* syntax;
			struct Expression* value;
		} cast;
		struct {
			union {
				struct TypeNode* syntax;
				struct Expression* value;
			} operand;
			uint64_t size;
		} size;
		struct {
			struct Expression* object;
			struct Token name;
			struct Member* binding;
			struct BasePath* base_path;
			struct InitializationContext* initialization;
		} member;
		struct {
			unsigned char* bytes;
			size_t count;
		} string;
		struct {
			struct Token name;
			struct Variable* binding;
			struct InitializationContext* initialization;
		} variable;
		struct {
			struct Token name;
			struct Function* binding;
			struct Expression* callee;
			struct Expression* receiver_evaluation;
			struct Record* qualified_owner;
			struct Argument* arguments;
		} call;
		struct Expression* operand;
		struct {
			struct Expression* value;
			int operation;
		} update;
		struct {
			int operation;
			struct Expression* left;
			struct Expression* right;
		} binary;
	} data;
};

struct Statement {
	enum NodeKind kind;
	struct Span span;
	struct Statement* next;
	unsigned exits;
	struct LifetimeCleanup* cleanup;
	union {
		struct {
			struct Variable* variable;
			struct Expression* initializer;
			struct Statement* initialization;
			int operation;
		} declaration;
		struct Expression* expression;
		struct Statement* block;
		struct {
			struct Expression* condition;
			struct Statement* first;
			struct Statement* second;
		} branch;
		struct {
			struct Statement* initializer;
			struct Expression* condition;
			struct Expression* step;
			struct Statement* body;
		} loop;
		struct {
			struct Expression* value;
			struct Case* cases;
		} selection;
	} data;
};

struct Case {
	struct Span span;
	struct Expression* value;
	struct Statement* statements;
	uint64_t constant;
	unsigned exits;
	struct Case* next;
	struct LifetimeCleanup* cleanup;
};

struct ScopeBinding {
	struct Token name;
	struct Variable* variable;
	struct ScopeBinding* next;
};

struct Scope {
	struct ScopeBinding* bindings;
	struct Scope* parent;
	struct InitializationContext* initialization;
};

enum FlowKind {
	LOOP_FLOW,
	SWITCH_FLOW,
};

enum FlowExit {
	NORMAL_EXIT = 1,
	RETURN_EXIT = 2,
	BREAK_EXIT = 4,
	CONTINUE_EXIT = 8,
};

struct Flow {
	enum FlowKind kind;
	struct Flow* parent;
	LLVMBasicBlockRef break_target;
	LLVMBasicBlockRef continue_target;
};

enum RecordState {
	FORWARD_RECORD,
	DEFINED_RECORD,
	RESOLVING_RECORD,
	RESOLVED_RECORD,
};

struct Member {
	struct TypeNode* syntax;
	struct Type type;
	struct Token name;
	unsigned index;
	struct Record* owner;
	struct Member* next;
};

enum EnumValueState {
	UNRESOLVED_ENUMERATOR,
	RESOLVED_ENUMERATOR,
};

struct Enumerator {
	enum EnumValueState state;
	struct Token name;
	struct Expression* initializer;
	int64_t value;
	struct Record* owner;
	struct Enumerator* next;
};

enum BaseKind {
	NONVIRTUAL_BASE,
	VIRTUAL_BASE,
};

struct BaseRelation {
	struct TypeNode* syntax;
	struct Record* record;
	unsigned index;
	enum BaseKind kind;
	struct BaseRelation* next;
};

struct BasePath {
	struct BaseRelation* relation;
	struct BasePath* next;
};

enum BaseResolution {
	UNRESOLVED_BASES,
	RESOLVING_BASES,
	RESOLVED_BASES,
};

struct VirtualBase {
	struct Record* record;
	unsigned index;
	struct VirtualBase* next;
};

enum VirtualModification {
	ORDINARY_FUNCTION,
	VIRTUAL_FUNCTION,
	OVERRIDE_FUNCTION,
	FINAL_FUNCTION,
};

enum VirtualResolution {
	UNRESOLVED_VIRTUAL,
	RESOLVING_VIRTUAL,
	RESOLVED_VIRTUAL,
};

struct VirtualCallable {
	struct Function* function;
	unsigned index;
	struct VirtualCallable* next;
};

struct OverrideRelation {
	struct Function* function;
	struct OverrideRelation* next;
};

enum SubobjectKind {
	ROOT_SUBOBJECT,
	ORDINARY_SUBOBJECT,
	SHARED_SUBOBJECT,
};

struct SubobjectEdge {
	struct BaseRelation* relation;
	struct Subobject* target;
	struct SubobjectEdge* next;
};

struct Subobject {
	struct Record* record;
	enum SubobjectKind kind;
	uint64_t offset;
	struct BasePath* path;
	struct SubobjectEdge* edges;
	LLVMValueRef view;
	struct Subobject* next;
};

struct Record {
	enum TypeKind kind;
	enum RecordState state;
	struct Token name;
	struct Span span;
	struct Namespace* associated;
	struct BaseRelation* bases;
	struct VirtualBase* virtual_bases;
	struct VirtualCallable* callables;
	struct Subobject* subobjects;
	enum BaseResolution base_resolution;
	union {
		struct Member* members;
		struct Enumerator* enumerators;
	} data;
	LLVMTypeRef llvm_type;
	LLVMTypeRef llvm_body;
	struct Record* next;
};

struct TagSymbol {
	struct Record* record;
	struct TagSymbol* next;
};

enum ParameterArity {
	FIXED_PARAMETERS,
	VARIADIC_PARAMETERS,
};

enum FunctionRole {
	ORDINARY_ROLE,
	CONSTRUCTOR_ROLE,
	DESTRUCTOR_ROLE,
	ASSIGNMENT_ROLE,
};

enum GeneratedOperation {
	DEFAULT_CONSTRUCTION,
	COPY_CONSTRUCTION,
	VALUE_ASSIGNMENT,
};

struct Function {
	enum FunctionRole role;
	LLVMValueRef llvm_value;
	LLVMTypeRef llvm_type;
	struct TypeNode* result_syntax;
	enum NodeKind kind;
	struct Span span;
	struct Token name;
	struct Namespace* owner;
	struct Token local_name;
	struct Type result;
	struct Variable* parameters;
	struct Statement* statements;
	struct Record* interface;
	enum VirtualModification modification;
	enum VirtualResolution virtual_resolution;
	struct OverrideRelation* overrides;
	LLVMValueRef dispatch;
	enum ParameterArity arity;
	struct Function* next;
	struct LifetimeCleanup* cleanup;
};

enum LifetimeStatus {
	ALIVE_OBJECT,
	DEAD_OBJECT,
};

enum Reachability {
	UNREACHABLE_STATE,
	REACHABLE_STATE,
};

struct LifetimeObject {
	struct Variable* variable;
	enum LifetimeStatus status;
	struct LifetimeObject* next;
};

struct LifetimeState {
	enum Reachability reachability;
	struct LifetimeObject* objects;
};

struct LifetimeCleanup {
	struct Variable* variable;
	struct LifetimeCleanup* next;
};

struct LifetimeControl {
	enum FlowKind kind;
	struct LifetimeObject* boundary;
	struct LifetimeState breaks;
	struct LifetimeState continues;
	struct LifetimeControl* parent;
};

struct LifetimeAnalysis {
	struct Compiler* compiler;
	struct LifetimeState state;
	struct LifetimeControl* control;
};

struct FunctionSymbol {
	struct Function* function;
	struct FunctionSymbol* next;
};

struct DispatchThunk {
	struct Record* complete;
	struct Subobject* origin;
	struct VirtualCallable* callable;
	struct FinalOverrider* target;
	LLVMValueRef value;
	struct DispatchThunk* next;
};

struct Keyword {
	const char* text;
	int kind;
};

struct BuiltinType {
	const char* name;
	struct Type type;
};

enum OperatorKind {
	ARITHMETIC_OPERATOR,
	COMPARISON_OPERATOR,
	LOGICAL_OPERATOR,
	ASSIGNMENT_OPERATOR,
	BITWISE_OPERATOR,
	SHIFT_OPERATOR,
};

struct Operator {
	int token;
	int precedence;
	enum OperatorKind kind;
	int compound;
	LLVMValueRef (*integer_builder)(LLVMBuilderRef, LLVMValueRef, LLVMValueRef, const char*);
	LLVMValueRef (*float_builder)(LLVMBuilderRef, LLVMValueRef, LLVMValueRef, const char*);
};

enum CompilationMode {
	EXECUTABLE_COMPILATION,
	LIBRARY_COMPILATION,
};

struct Compiler {
	enum CompilationMode mode;
	FILE* diagnostics;
	FILE* (*open_file)(const char*, const char*);
	struct Namespace* namespaces;
	struct Namespace* namespace_scope;
	struct Allocation* allocations;
	struct Token* tokens;
	size_t token_count;
	size_t token_index;
	struct Token token;
	struct Function* functions;
	struct Record* records;
	struct Record* last_record;
	struct TagSymbol* tags;
	LLVMTargetDataRef layout;
	struct Function* last_function;
	struct Function* entry;
	struct FunctionSymbol* symbols;
	struct FunctionSymbol* last_symbol;
	struct DispatchThunk* thunks;
	struct DispatchThunk* last_thunk;
	struct Function* emitted_function;
	struct Function* emitted_signature;
	bool failed;
	LLVMContextRef llvm_context;
	LLVMModuleRef module;
	LLVMBuilderRef builder;
	LLVMTargetMachineRef machine;
	int optimization;
	int next_string;
	struct Temporary* temporaries;
};

struct Temporary {
	struct Type type;
	LLVMValueRef address;
	LLVMValueRef predicate;
	struct Temporary* next;
};

struct Lexer {
	struct Compiler* compiler;
	const char* path;
	const char* cursor;
	int line;
	int column;
};

const struct Keyword keywords[] = { { "nullptr", NULLPTR }, { "delete", DELETE }, { "discard", DISCARD }, { "const", CONST }, { "namespace", NAMESPACE }, { "fn", FN }, { "var", VAR }, { "return", RETURN }, { "import", IMPORT }, { "auto", AUTO }, { "struct", STRUCT }, { "union", UNION }, { "enum", ENUM }, { "sizeof", SIZEOF }, { "if", IF }, { "else", ELSE }, { "while", WHILE }, { "for", FOR }, { "switch", SWITCH }, { "case", CASE }, { "default", DEFAULT }, { "break", BREAK }, { "continue", CONTINUE }, { "true", TRUE }, { "false", FALSE }, { "virtual", VIRTUAL }, { "override", OVERRIDE }, { "final", FINAL } };

const struct BuiltinType builtin_types[] = { { "i08", { SIGNED_INTEGER_TYPE, { 8 } } }, { "i16", { SIGNED_INTEGER_TYPE, { 16 } } }, { "i32", { SIGNED_INTEGER_TYPE, { 32 } } }, { "i64", { SIGNED_INTEGER_TYPE, { 64 } } }, { "u08", { UNSIGNED_INTEGER_TYPE, { 8 } } }, { "u16", { UNSIGNED_INTEGER_TYPE, { 16 } } }, { "u32", { UNSIGNED_INTEGER_TYPE, { 32 } } }, { "u64", { UNSIGNED_INTEGER_TYPE, { 64 } } }, { "usize", { UNSIGNED_INTEGER_TYPE, { sizeof(void*) * CHAR_BIT } } }, { "f32", { FLOAT_TYPE, { 32 } } }, { "f64", { FLOAT_TYPE, { 64 } } }, { "char", { CHAR_TYPE, { 0 } } }, { "bool", { BOOL_TYPE, { 0 } } }, { "void", { VOID_TYPE, { 0 } } }, { "null", { NULL_TYPE, { 0 } } } };

const struct Operator operators[] = {
	{ '=', 1, ASSIGNMENT_OPERATOR, 0, NULL, NULL },
	{ COPY_ASSIGN, 1, ASSIGNMENT_OPERATOR, 0, NULL, NULL },
	{ ADD_ASSIGN, 1, ASSIGNMENT_OPERATOR, '+', NULL, NULL },
	{ SUB_ASSIGN, 1, ASSIGNMENT_OPERATOR, '-', NULL, NULL },
	{ MUL_ASSIGN, 1, ASSIGNMENT_OPERATOR, '*', NULL, NULL },
	{ DIV_ASSIGN, 1, ASSIGNMENT_OPERATOR, '/', NULL, NULL },
	{ MOD_ASSIGN, 1, ASSIGNMENT_OPERATOR, '%', NULL, NULL },
	{ AND_ASSIGN, 1, ASSIGNMENT_OPERATOR, '&', NULL, NULL },
	{ OR_ASSIGN, 1, ASSIGNMENT_OPERATOR, '|', NULL, NULL },
	{ XOR_ASSIGN, 1, ASSIGNMENT_OPERATOR, '^', NULL, NULL },
	{ LEFT_ASSIGN, 1, ASSIGNMENT_OPERATOR, SHIFT_LEFT, NULL, NULL },
	{ RIGHT_ASSIGN, 1, ASSIGNMENT_OPERATOR, SHIFT_RIGHT, NULL, NULL },
	{ LOGICAL_OR, 2, LOGICAL_OPERATOR, 0, NULL, NULL },
	{ LOGICAL_AND, 3, LOGICAL_OPERATOR, 0, NULL, NULL },
	{ '|', 4, BITWISE_OPERATOR, 0, LLVMBuildOr, NULL },
	{ '^', 5, BITWISE_OPERATOR, 0, LLVMBuildXor, NULL },
	{ '&', 6, BITWISE_OPERATOR, 0, LLVMBuildAnd, NULL },
	{ EQUAL, 7, COMPARISON_OPERATOR, 0, NULL, NULL },
	{ NOT_EQUAL, 7, COMPARISON_OPERATOR, 0, NULL, NULL },
	{ '<', 8, COMPARISON_OPERATOR, 0, NULL, NULL },
	{ '>', 8, COMPARISON_OPERATOR, 0, NULL, NULL },
	{ LESS_EQUAL, 8, COMPARISON_OPERATOR, 0, NULL, NULL },
	{ GREATER_EQUAL, 8, COMPARISON_OPERATOR, 0, NULL, NULL },
	{ SHIFT_LEFT, 9, SHIFT_OPERATOR, 0, LLVMBuildShl, NULL },
	{ SHIFT_RIGHT, 9, SHIFT_OPERATOR, 0, NULL, NULL },
	{ '+', 10, ARITHMETIC_OPERATOR, 0, LLVMBuildAdd, LLVMBuildFAdd },
	{ '-', 10, ARITHMETIC_OPERATOR, 0, LLVMBuildSub, LLVMBuildFSub },
	{ '*', 11, ARITHMETIC_OPERATOR, 0, LLVMBuildMul, LLVMBuildFMul },
	{ '/', 11, ARITHMETIC_OPERATOR, 0, NULL, LLVMBuildFDiv },
	{ '%', 11, ARITHMETIC_OPERATOR, 0, NULL, NULL },
};

struct Spelling {
	const char* text;
	int token;
};

const struct Spelling spellings[] = {
	{ "::", QUALIFY },
	{ ":=", COPY_ASSIGN },
	{ "<<=", LEFT_ASSIGN },
	{ ">>=", RIGHT_ASSIGN },
	{ "...", ELLIPSIS },
	{ "->", ARROW },
	{ "==", EQUAL },
	{ "!=", NOT_EQUAL },
	{ "<=", LESS_EQUAL },
	{ ">=", GREATER_EQUAL },
	{ "&&", LOGICAL_AND },
	{ "||", LOGICAL_OR },
	{ "<<", SHIFT_LEFT },
	{ ">>", SHIFT_RIGHT },
	{ "++", INCREMENT },
	{ "--", DECREMENT },
	{ "+=", ADD_ASSIGN },
	{ "-=", SUB_ASSIGN },
	{ "*=", MUL_ASSIGN },
	{ "/=", DIV_ASSIGN },
	{ "%=", MOD_ASSIGN },
	{ "&=", AND_ASSIGN },
	{ "|=", OR_ASSIGN },
	{ "^=", XOR_ASSIGN },
};

void diagnostic(struct Compiler* compiler, struct Token token, const char* format, ...) {
	if (compiler->failed) {
		return;
	}
	compiler->failed = true;
	fprintf(compiler->diagnostics, "%s:%d:%d: error: ", token.path ? token.path : "<source>", token.line, token.column);
	va_list arguments;
	va_start(arguments, format);
	vfprintf(compiler->diagnostics, format, arguments);
	va_end(arguments);
	fputc('\n', compiler->diagnostics);
}

void* allocate(struct Compiler* compiler, size_t size) {
	void* memory = calloc(1, size);
	struct Allocation* allocation = malloc(sizeof(struct Allocation));
	if (!memory || !allocation) {
		free(memory);
		free(allocation);
		diagnostic(compiler, compiler->token, "out of memory");
		return NULL;
	}
	allocation->memory = memory;
	allocation->next = compiler->allocations;
	compiler->allocations = allocation;
	return memory;
}

bool token_text(struct Token token, const char* text) {
	return token.length == strlen(text) && !memcmp(token.start, text, token.length);
}

bool name_equal(struct Token a, struct Token b) {
	return a.length == b.length && !memcmp(a.start, b.start, a.length);
}

bool type_equal(struct Type a, struct Type b) {
	if (a.kind != b.kind || a.qualifier != b.qualifier) {
		return false;
	}
	switch (a.kind) {
	case POINTER_TYPE:
	case REFERENCE_TYPE:
		return type_equal(*a.data.element, *b.data.element);
	case ARRAY_TYPE:
		return a.data.array.count == b.data.array.count && type_equal(*a.data.array.element, *b.data.array.element);
	case STRUCT_TYPE:
	case UNION_TYPE:
	case ENUM_TYPE:
		return a.data.record == b.data.record;
	case FUNCTION_TYPE: {
		struct Function* first = a.data.function;
		struct Function* second = b.data.function;
		if (!type_equal(first->result, second->result) || first->arity != second->arity) {
			return false;
		}
		struct Variable* left = first->parameters;
		struct Variable* right = second->parameters;
		while (left && right) {
			if (!type_equal(left->type, right->type)) {
				return false;
			}
			left = left->next;
			right = right->next;
		}
		return !left && !right;
	}
	default:
		return a.data.bits == b.data.bits;
	}
}

bool numeric_type(struct Type type) {
	return type.kind == BOOL_TYPE || type.kind == CHAR_TYPE || type.kind == SIGNED_INTEGER_TYPE || type.kind == UNSIGNED_INTEGER_TYPE || type.kind == FLOAT_TYPE || type.kind == ENUM_TYPE;
}

bool is_null_pointer_type(struct Type type) {
	return type.kind == POINTER_TYPE && type.data.element->kind == NULL_TYPE;
}

bool unsized_type(struct Type type) {
	return type.kind == VOID_TYPE || type.kind == NULL_TYPE;
}

bool implicit_pointer_conversion(struct Type source, struct Type target) {
	if (source.kind != POINTER_TYPE || target.kind != POINTER_TYPE) {
		return false;
	}
	if (is_null_pointer_type(source)) {
		return true;
	}
	return target.data.element->kind == VOID_TYPE && source.data.element->kind != FUNCTION_TYPE && (source.data.element->qualifier == MUTABLE_TYPE || target.data.element->qualifier == CONST_TYPE);
}

bool zero_extended_type(struct Type type) {
	return type.kind == UNSIGNED_INTEGER_TYPE || type.kind == CHAR_TYPE || type.kind == BOOL_TYPE;
}

struct Type pointer_type(struct Compiler* compiler, struct Type element) {
	struct Type pointer = { POINTER_TYPE, { 0 } };
	pointer.data.element = allocate(compiler, sizeof(struct Type));
	if (!pointer.data.element) {
		return (struct Type){ 0 };
	}
	*pointer.data.element = element;
	return pointer;
}

bool is_void(struct Type type) {
	return type.kind == VOID_TYPE;
}

bool identifier_start(char c) {
	return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_';
}

bool identifier_char(char c) {
	return identifier_start(c) || (c >= '0' && c <= '9');
}

const struct BuiltinType* find_builtin(struct Token token) {
	for (size_t i = 0; i < sizeof(builtin_types) / sizeof(builtin_types[0]); i++) {
		if (token_text(token, builtin_types[i].name)) {
			return &builtin_types[i];
		}
	}
	return NULL;
}

const struct Operator* find_operator(int token) {
	for (size_t i = 0; i < sizeof(operators) / sizeof(operators[0]); i++) {
		if (token == operators[i].token) {
			return &operators[i];
		}
	}
	return NULL;
}

void lexer_advance(struct Lexer* lexer) {
	if (*lexer->cursor == '\n') {
		lexer->line++;
		lexer->column = 1;
	}
	else {
		lexer->column++;
	}
	lexer->cursor++;
}

struct Token lexer_token(struct Lexer* lexer) {
	while (true) {
		char c = *lexer->cursor;
		if (c == ' ' || c == '\t' || c == '\r' || c == '\n') {
			lexer_advance(lexer);
			continue;
		}
		if (c == '/' && lexer->cursor[1] == '/') {
			while (*lexer->cursor && *lexer->cursor != '\n') {
				lexer_advance(lexer);
			}
			continue;
		}
		if (c == '/' && lexer->cursor[1] == '*') {
			struct Token comment = { 0, lexer->cursor, 0, lexer->line, lexer->column, lexer->path };
			lexer_advance(lexer);
			lexer_advance(lexer);
			while (*lexer->cursor && !(lexer->cursor[0] == '*' && lexer->cursor[1] == '/')) {
				lexer_advance(lexer);
			}
			if (!*lexer->cursor) {
				diagnostic(lexer->compiler, comment, "unterminated comment");
				break;
			}
			lexer_advance(lexer);
			lexer_advance(lexer);
			continue;
		}
		break;
	}
	struct Token token = { (unsigned char)*lexer->cursor, lexer->cursor, 0, lexer->line, lexer->column, lexer->path };
	if (!*lexer->cursor) {
		token.kind = END;
	}
	else if (identifier_start(*lexer->cursor)) {
		token.kind = NAME;
		while (identifier_char(*lexer->cursor)) {
			lexer_advance(lexer);
		}
	}
	else if (*lexer->cursor >= '0' && *lexer->cursor <= '9') {
		token.kind = INTEGER;
		while (*lexer->cursor >= '0' && *lexer->cursor <= '9') {
			lexer_advance(lexer);
		}
		if (*lexer->cursor == '.' && lexer->cursor[1] >= '0' && lexer->cursor[1] <= '9') {
			token.kind = DECIMAL;
			lexer_advance(lexer);
			while (*lexer->cursor >= '0' && *lexer->cursor <= '9') {
				lexer_advance(lexer);
			}
		}
	}
	else if (*lexer->cursor == '\'') {
		token.kind = CHARACTER;
		lexer_advance(lexer);
		if (!*lexer->cursor || *lexer->cursor == '\n' || *lexer->cursor == '\r' || *lexer->cursor == '\'') {
			diagnostic(lexer->compiler, token, "character literal requires exactly one byte");
		}
		else {
			if (*lexer->cursor == '\\') {
				lexer_advance(lexer);
			}
			if (*lexer->cursor && *lexer->cursor != '\n' && *lexer->cursor != '\r') {
				lexer_advance(lexer);
			}
			if (*lexer->cursor != '\'') {
				diagnostic(lexer->compiler, token, "character literal requires exactly one byte and a closing quote");
			}
			else {
				lexer_advance(lexer);
			}
		}
	}
	else if (*lexer->cursor == '"') {
		token.kind = STRING;
		lexer_advance(lexer);
		while (*lexer->cursor && *lexer->cursor != '"' && *lexer->cursor != '\n') {
			if (*lexer->cursor == '\\') {
				lexer_advance(lexer);
			}
			if (*lexer->cursor && *lexer->cursor != '\n') {
				lexer_advance(lexer);
			}
		}
		if (*lexer->cursor != '"') {
			diagnostic(lexer->compiler, token, "unterminated string literal");
		}
		else {
			lexer_advance(lexer);
		}
	}
	else {
		size_t width = 1;
		for (size_t i = 0; i < sizeof(spellings) / sizeof(spellings[0]); i++) {
			size_t length = strlen(spellings[i].text);
			if (!strncmp(lexer->cursor, spellings[i].text, length)) {
				token.kind = spellings[i].token;
				width = length;
				break;
			}
		}
		for (size_t i = 0; i < width; i++) {
			lexer_advance(lexer);
		}
	}
	token.length = (size_t)(lexer->cursor - token.start);
	for (size_t i = 0; i < sizeof(keywords) / sizeof(keywords[0]); i++) {
		if (token.kind == NAME && token_text(token, keywords[i].text)) {
			token.kind = keywords[i].kind;
			break;
		}
	}
	return token;
}

void tokenize(struct Compiler* compiler, const char* path, const char* bytes) {
	struct Lexer lexer = { compiler, path, bytes, 1, 1 };
	while (!compiler->failed) {
		struct Token token = lexer_token(&lexer);
		if (compiler->token_count >= SIZE_MAX / sizeof(struct Token) - 1) {
			diagnostic(compiler, token, "token allocation overflow");
			break;
		}
		struct Token* resized = realloc(compiler->tokens, (compiler->token_count + 1) * sizeof(struct Token));
		if (!resized) {
			diagnostic(compiler, token, "out of memory");
			break;
		}
		compiler->tokens = resized;
		compiler->tokens[compiler->token_count++] = token;
		if (token.kind == END) {
			break;
		}
	}
}

void next_token(struct Compiler* compiler) {
	if (compiler->token_index < compiler->token_count) {
		compiler->token = compiler->tokens[compiler->token_index++];
		compiler->token.owner = compiler->namespace_scope;
	}
}

struct Token previous_token(struct Compiler* compiler) {
	return compiler->tokens[compiler->token_index > 1 ? compiler->token_index - 2 : 0];
}

bool take(struct Compiler* compiler, int kind) {
	if (compiler->token.kind != kind) {
		return false;
	}
	next_token(compiler);
	return true;
}

bool expect(struct Compiler* compiler, int kind, const char* description) {
	if (take(compiler, kind)) {
		return true;
	}
	diagnostic(compiler, compiler->token, "expected %s", description);
	return false;
}

struct Token qualify_name(struct Compiler* compiler, struct Namespace* owner, struct Token name) {
	if (!owner) {
		name.owner = NULL;
		return name;
	}

	char* text = allocate(compiler, owner->length + name.length + 3);
	if (!text) {
		return name;
	}

	memcpy(text, owner->name, owner->length);
	memcpy(text + owner->length, "::", 2);
	memcpy(text + owner->length + 2, name.start, name.length);
	name.length += owner->length + 2;
	name.start = text;
	name.owner = NULL;
	return name;
}

struct Namespace* namespace_local(struct Compiler* compiler, struct Namespace* owner, struct Token name) {
	for (struct Namespace* space = compiler->namespaces; space; space = space->next) {
		if (space->parent != owner) {
			continue;
		}

		size_t prefix = owner ? owner->length + 2 : 0;
		if (space->length - prefix == name.length && !memcmp(space->name + prefix, name.start, name.length)) {
			return space;
		}
	}
	return NULL;
}

struct Namespace* namespace_ensure(struct Compiler* compiler, struct Namespace* owner, struct Token name) {
	struct Namespace* space = namespace_local(compiler, owner, name);
	if (space) {
		return space;
	}
	space = allocate(compiler, sizeof(struct Namespace));
	if (!space) {
		return NULL;
	}
	struct Token full_name = qualify_name(compiler, owner, name);
	space->name = full_name.start;
	space->length = full_name.length;
	space->parent = owner;
	space->source_path = name.path;
	space->next = compiler->namespaces;
	compiler->namespaces = space;
	return space;
}

struct QualifiedName {
	struct Namespace* owner;
	struct Token name;
};

enum MemberLookupKind {
	NO_MEMBER,
	FIELD_MEMBER,
	FUNCTION_MEMBER,
	NAMESPACE_MEMBER,
	ENUMERATOR_MEMBER,
	AMBIGUOUS_MEMBER,
};

struct MemberLookup {
	enum MemberLookupKind kind;
	union {
		struct Member* field;
		struct Function* function;
		struct Namespace* space;
		struct Enumerator* enumerator;
	} entity;
	struct BasePath* path;
};

void resolve_bases(struct Compiler* compiler, struct Record* record);

struct Token unqualified_token(struct Token name) {
	for (size_t index = 0; index + 1 < name.length; index++) {
		if (name.start[index] == ':' && name.start[index + 1] == ':') {
			name.start += index + 2;
			name.length -= index + 2;
			index = 0;
		}
	}
	return name;
}



struct MemberLookup record_local_member(struct Compiler* compiler, struct Record* record, struct Token name) {
	struct MemberLookup result = { 0 };
	for (struct Member* field = record->kind == ENUM_TYPE ? NULL : record->data.members; field; field = field->next) {
		if (name_equal(field->name, name)) {
			result.kind = FIELD_MEMBER;
			result.entity.field = field;
			return result;
		}
	}
	for (struct FunctionSymbol* symbol = compiler->symbols; symbol; symbol = symbol->next) {
		if (symbol->function->owner == record->associated && name_equal(symbol->function->local_name, name)) {
			result.kind = FUNCTION_MEMBER;
			result.entity.function = symbol->function;
			return result;
		}
	}
	for (struct Record* enumeration = compiler->records; enumeration; enumeration = enumeration->next) {
		if (enumeration->kind != ENUM_TYPE || enumeration->associated->parent != record->associated) {
			continue;
		}
		for (struct Enumerator* value = enumeration->data.enumerators; value; value = value->next) {
			if (name_equal(unqualified_token(value->name), name)) {
				result.kind = ENUMERATOR_MEMBER;
				result.entity.enumerator = value;
				return result;
			}
		}
	}
	struct Namespace* space = namespace_local(compiler, record->associated, name);
	if (space && space->kind != UNRESOLVED_NAMESPACE) {
		result.kind = NAMESPACE_MEMBER;
		result.entity.space = space->canonical ? space->canonical : space;
		return result;
	}

	return result;
}

struct MemberLookup record_member(struct Compiler* compiler, struct Record* record, struct Token name);

struct Namespace* namespace_child(struct Compiler* compiler, struct Namespace* owner, struct Token name) {
	while (owner && owner->canonical) {
		owner = owner->canonical;
	}
	if (!owner || !owner->record) {
		return namespace_local(compiler, owner, name);
	}
	struct MemberLookup member = record_member(compiler, owner->record, name);
	if (member.kind == AMBIGUOUS_MEMBER) {
		diagnostic(compiler, name, "ambiguous inherited name");
		return NULL;
	}
	return member.kind == NAMESPACE_MEMBER ? member.entity.space : NULL;
}

struct QualifiedName declaration_name(struct Compiler* compiler, struct Token name) {
	struct QualifiedName result = { name.owner, name };
	size_t begin = 0;
	for (size_t index = 0; index + 1 < name.length; index++) {
		if (name.start[index] != ':' || name.start[index + 1] != ':') {
			continue;
		}
		struct Token part = name;
		part.start += begin;
		part.length = index - begin;
		struct Namespace* space = NULL;
		if (!begin) {
			for (struct Namespace* owner = name.owner; ; owner = owner->parent) {
				space = namespace_local(compiler, owner, part);
				if (space || !owner) {
					break;
				}
			}
		}
		if (!space) {
			space = namespace_ensure(compiler, result.owner, part);
		}
		if (!space) {
			return result;
		}
		result.owner = space;
		begin = index + 2;
		index++;
	}
	result.name.start += begin;
	result.name.length -= begin;
	result.name.kind = NAME;
	result.name.owner = result.owner;
	return result;
}

struct QualifiedName lookup_name(struct Compiler* compiler, struct Token name) {
	while (name.owner && name.owner->canonical) {
		name.owner = name.owner->canonical;
	}
	struct QualifiedName result = { name.owner, name };
	size_t begin = 0;
	for (size_t index = 0; index + 1 < name.length; index++) {
		if (name.start[index] != ':' || name.start[index + 1] != ':') {
			continue;
		}
		struct Token part = name;
		part.start += begin;
		part.length = index - begin;
		struct Namespace* space = NULL;
		if (!begin) {
			for (struct Namespace* owner = name.owner; ; owner = owner->parent) {
				space = namespace_child(compiler, owner, part);
				if (space || !owner) {
					break;
				}
			}
		}
		else {
			space = namespace_child(compiler, result.owner, part);
		}
		if (!space) {
			diagnostic(compiler, name, "qualified name owner was not found");
			result.name.kind = END;
			return result;
		}
		result.owner = space;
		begin = index + 2;
		index++;
	}
	result.name.start += begin;
	result.name.length -= begin;
	result.name.kind = begin ? QUALIFY : NAME;
	result.name.owner = result.owner;
	return result;
}

struct Token parse_qualified_name(struct Compiler* compiler, bool* destructor) {
	if (destructor) {
		*destructor = take(compiler, '~');
	}
	struct Token name = compiler->token;
	if (!expect(compiler, NAME, "an identifier")) {
		return name;
	}

	while (take(compiler, QUALIFY)) {
		if (destructor && *destructor) {
			diagnostic(compiler, compiler->token, "destructor name must end the qualified name");
			return name;
		}
		if (destructor) {
			*destructor = take(compiler, '~');
		}
		name.kind = QUALIFY;
		struct Token part = compiler->token;
		if (!expect(compiler, NAME, "an identifier after '::'")) {
			return name;
		}

		char* text = allocate(compiler, name.length + part.length + 3);
		if (!text) {
			return name;
		}
		memcpy(text, name.start, name.length);
		memcpy(text + name.length, "::", 2);
		memcpy(text + name.length + 2, part.start, part.length);
		name.start = text;
		name.length += part.length + 2;
	}
	return name;
}

struct Token parse_name(struct Compiler* compiler) {
	return parse_qualified_name(compiler, NULL);
}

bool namespace_name_equal(struct Token declared, struct Namespace* owner, struct Token name);
bool namespace_declares(struct Compiler* compiler, struct Namespace* owner, struct Token name);

struct Token resolve_name(struct Compiler* compiler, struct Token name) {
	for (size_t index = 0; index + 1 < name.length; index++) {
		if (name.start[index] != ':' || name.start[index + 1] != ':') {
			continue;
		}

		struct Token first = name;
		first.length = index;
		struct Namespace* space = NULL;
		struct Record* enumeration = NULL;
		for (struct Namespace* owner = name.owner; ; owner = owner->parent) {
			space = namespace_local(compiler, owner, first);
			for (struct Record* record = compiler->records; !space && record; record = record->next) {
				if (record->kind == ENUM_TYPE && namespace_name_equal(record->name, owner, first)) {
					enumeration = record;
					break;
				}
			}
			if (space || enumeration || namespace_declares(compiler, owner, first) || !owner) {
				break;
			}
		}
		if (!space && !enumeration) {
			diagnostic(compiler, name, "unknown namespace '%.*s'", (int)first.length, first.start);
			return name;
		}

		struct Token rest = name;
		rest.start += index + 2;
		rest.length -= index + 2;
		if (enumeration) {
			struct Namespace enum_scope = { enumeration->name.start, enumeration->name.length, NULL, NULL };
			return qualify_name(compiler, &enum_scope, rest);
		}
		return qualify_name(compiler, space, rest);
	}
	return name;
}

bool namespace_name_equal(struct Token declared, struct Namespace* owner, struct Token name) {
	size_t prefix = owner ? owner->length + 2 : 0;
	if (declared.length != prefix + name.length) {
		return false;
	}
	if (owner && (memcmp(declared.start, owner->name, owner->length) || memcmp(declared.start + owner->length, "::", 2))) {
		return false;
	}
	return !memcmp(declared.start + prefix, name.start, name.length);
}

bool namespace_declares(struct Compiler* compiler, struct Namespace* owner, struct Token name) {
	for (struct Namespace* space = compiler->namespaces; space; space = space->next) {
		struct Token declared = { NAME, space->name, space->length, 1, 1, NULL, NULL };
		if (namespace_name_equal(declared, owner, name)) {
			return true;
		}
	}
	for (struct Function* function = compiler->functions; function; function = function->next) {
		if (namespace_name_equal(function->name, owner, name)) {
			return true;
		}
	}
	for (struct Record* record = compiler->records; record; record = record->next) {
		if (namespace_name_equal(record->name, owner, name)) {
			return true;
		}
		if (record->kind != ENUM_TYPE) {
			continue;
		}
		for (struct Enumerator* value = record->data.enumerators; value; value = value->next) {
			if (namespace_name_equal(value->name, owner, name)) {
				return true;
			}
		}
	}
	return false;
}

struct Token resolve_symbol(struct Compiler* compiler, struct Token name) {
	name = resolve_name(compiler, name);
	for (struct Namespace* owner = name.owner; owner; owner = owner->parent) {
		if (namespace_declares(compiler, owner, name)) {
			return qualify_name(compiler, owner, name);
		}
	}
	name.owner = NULL;
	return name;
}

struct Record* find_record(struct Compiler* compiler, struct Token name) {
	struct QualifiedName lookup = lookup_name(compiler, name);
	if (lookup.name.kind == END) {
		return NULL;
	}
	for (struct Namespace* owner = lookup.owner; ; owner = owner->parent) {
		struct Namespace* associated = namespace_child(compiler, owner, lookup.name);
		for (struct TagSymbol* tag = compiler->tags; tag; tag = tag->next) {
			if (associated && tag->record == associated->record) {
				return tag->record;
			}
		}
		if (!owner || lookup.name.kind == QUALIFY) {
			return NULL;
		}
	}
}

LLVMTypeRef llvm_type(struct Compiler* compiler, struct Type type);
void resolve_record(struct Compiler* compiler, struct Record* record);

struct TypeNode* parse_type(struct Compiler* compiler) {
	struct TypeNode* syntax = allocate(compiler, sizeof(struct TypeNode));
	if (!syntax) {
		return NULL;
	}
	syntax->span.first = compiler->token;
	if (take(compiler, CONST)) {
		syntax->kind = CONST_SYNTAX;
		syntax->data.element = parse_type(compiler);
		struct TypeNode* root = syntax->data.element;
		struct TypeNode* inner = root;
		while (inner && (inner->kind == POINTER_SYNTAX || inner->kind == REFERENCE_SYNTAX || inner->kind == ARRAY_SYNTAX)) {
			struct TypeNode** element = inner->kind == ARRAY_SYNTAX ? &inner->data.array.element : &inner->data.element;
			if ((*element)->kind != POINTER_SYNTAX && (*element)->kind != REFERENCE_SYNTAX && (*element)->kind != ARRAY_SYNTAX) {
				syntax->data.element = *element;
				*element = syntax;
				return root;
			}
			inner = *element;
		}
		return syntax;
	}
	if (take(compiler, '(')) {
		size_t closing = compiler->token_index - 1;
		size_t depth = 1;
		while (closing < compiler->token_count && depth) {
			int kind = compiler->tokens[closing].kind;
			if (kind == '(') {
				depth++;
			}
			else if (kind == ')') {
				depth--;
			}
			closing++;
		}
		if (depth || closing >= compiler->token_count || compiler->tokens[closing].kind != ARROW) {
			struct TypeNode* grouped = parse_type(compiler);
			expect(compiler, ')', "')' after grouped type");
			if (!grouped) {
				return NULL;
			}
			syntax = grouped;
			goto suffixes;
		}
		syntax->kind = FUNCTION_SYNTAX;
		struct Function* signature = allocate(compiler, sizeof(struct Function));
		syntax->data.function = signature;
		if (!signature) {
			return syntax;
		}
		struct Variable** tail = &signature->parameters;
		while (compiler->token.kind != ')' && !compiler->failed) {
			if (take(compiler, ELLIPSIS)) {
				signature->arity = VARIADIC_PARAMETERS;
				break;
			}
			struct Variable* parameter = allocate(compiler, sizeof(struct Variable));
			if (!parameter) {
				return syntax;
			}
			parameter->syntax = parse_type(compiler);
			if (compiler->token.kind == NAME) {
				parameter->name = compiler->token;
				next_token(compiler);
			}
			*tail = parameter;
			tail = &parameter->next;
			if (!take(compiler, ',')) {
				break;
			}
		}
		expect(compiler, ')', "')'");
		expect(compiler, ARROW, "'->' in function type");
		signature->result_syntax = parse_type(compiler);
	}
	else if (compiler->token.kind == STRUCT || compiler->token.kind == UNION || compiler->token.kind == ENUM) {
		syntax->kind = NAMED_SYNTAX;
		syntax->data.named.kind = compiler->token.kind == STRUCT ? STRUCT_TYPE : (compiler->token.kind == UNION ? UNION_TYPE : ENUM_TYPE);
		next_token(compiler);
		syntax->data.named.name = parse_name(compiler);
	}
	else {
		syntax->kind = BUILTIN_SYNTAX;
		syntax->data.builtin = find_builtin(compiler->token);
		if (!syntax->data.builtin) {
			if (compiler->token.kind == NAME) {
				syntax->kind = NAMED_SYNTAX;
				syntax->data.named.kind = VOID_TYPE;
				syntax->data.named.name = parse_name(compiler);
				goto suffixes;
			}
			else {
				diagnostic(compiler, compiler->token, "expected a type");
			}
			return syntax;
		}
		next_token(compiler);
	}

suffixes:
	while (compiler->token.kind == '*' || compiler->token.kind == '&' || compiler->token.kind == '[') {
		struct TypeNode* modifier = allocate(compiler, sizeof(struct TypeNode));
		if (!modifier) {
			return syntax;
		}
		modifier->span.first = syntax->span.first;
		if (compiler->token.kind == '*' || compiler->token.kind == '&') {
			modifier->kind = compiler->token.kind == '*' ? POINTER_SYNTAX : REFERENCE_SYNTAX;
			next_token(compiler);
			modifier->data.element = syntax;
		}
		else {
			next_token(compiler);
			modifier->kind = ARRAY_SYNTAX;
			modifier->data.array.element = syntax;
			struct Token length = compiler->token;
			if (!expect(compiler, INTEGER, "a positive integer array length")) {
				return modifier;
			}
			for (size_t i = 0; i < length.length; i++) {
				unsigned digit = (unsigned)(length.start[i] - '0');
				if (modifier->data.array.count > (UINT64_MAX - digit) / 10) {
					diagnostic(compiler, length, "array length is out of range");
					return modifier;
				}
				modifier->data.array.count = modifier->data.array.count * 10 + digit;
			}
			if (!modifier->data.array.count) {
				diagnostic(compiler, length, "array length must be positive");
			}
			expect(compiler, ']', "']'");
		}
		modifier->span.last = previous_token(compiler);
		syntax = modifier;
	}
	syntax->span.last = previous_token(compiler);
	return syntax;
}

struct Type resolve_type(struct Compiler* compiler, struct TypeNode* syntax) {
	struct Type type = { 0 };
	if (!syntax || compiler->failed) {
		return type;
	}
	switch (syntax->kind) {
	case CONST_SYNTAX:
		type = resolve_type(compiler, syntax->data.element);
		type.qualifier = CONST_TYPE;
		return type;
	case BUILTIN_SYNTAX:
		return syntax->data.builtin->type;
	case POINTER_SYNTAX:
		type = pointer_type(compiler, resolve_type(compiler, syntax->data.element));
		if (type.data.element && type.data.element->kind == REFERENCE_TYPE) {
			diagnostic(compiler, syntax->span.first, "pointer cannot point to a reference");
		}
		return type;
	case REFERENCE_SYNTAX:
		type = pointer_type(compiler, resolve_type(compiler, syntax->data.element));
		if (!type.data.element) {
			return type;
		}
		if (unsized_type(*type.data.element) || type.data.element->kind == REFERENCE_TYPE) {
			diagnostic(compiler, syntax->span.first, "reference requires a non-reference value type");
		}
		type.kind = REFERENCE_TYPE;
		return type;
	case FUNCTION_SYNTAX:
		type.kind = FUNCTION_TYPE;
		type.data.function = syntax->data.function;
		type.data.function->result = resolve_type(compiler, type.data.function->result_syntax);
		if (type.data.function->result.kind == NULL_TYPE) {
			diagnostic(compiler, syntax->span.first, "function cannot return the unsized null type");
		}
		for (struct Variable* parameter = type.data.function->parameters; parameter; parameter = parameter->next) {
			parameter->type = resolve_type(compiler, parameter->syntax);
			if (unsized_type(parameter->type) || parameter->type.kind == FUNCTION_TYPE) {
				diagnostic(compiler, parameter->syntax->span.first, "function parameter requires a value type");
			}
		}
		return type;
	case NAMED_SYNTAX: {
		struct Record* record = find_record(compiler, syntax->data.named.name);
		if (!record || (syntax->data.named.kind != VOID_TYPE && record->kind != syntax->data.named.kind)) {
			diagnostic(compiler, syntax->data.named.name, "unknown or mismatched type tag '%.*s'", (int)syntax->data.named.name.length, syntax->data.named.name.start);
			return type;
		}
		type.kind = record->kind;
		type.data.record = record;
		return type;
	}
	case ARRAY_SYNTAX:
		type.kind = ARRAY_TYPE;
		type.data.array.count = syntax->data.array.count;
		type.data.array.element = allocate(compiler, sizeof(struct Type));
		if (type.data.array.element) {
			*type.data.array.element = resolve_type(compiler, syntax->data.array.element);
			if (unsized_type(*type.data.array.element) || type.data.array.element->kind == REFERENCE_TYPE) {
				diagnostic(compiler, syntax->span.first, "array elements cannot have void or reference type");
			}
		}
		return type;
	}
	diagnostic(compiler, syntax->span.first, "invalid type syntax");
	return type;
}

void resolve_bases(struct Compiler* compiler, struct Record* record) {
	if (compiler->failed || record->base_resolution == RESOLVED_BASES) {
		return;
	}
	if (record->base_resolution == RESOLVING_BASES) {
		diagnostic(compiler, record->name, "cyclic inheritance");
		return;
	}
	record->base_resolution = RESOLVING_BASES;
	unsigned index = 0;
	for (struct BaseRelation* base = record->bases; base; base = base->next) {
		struct Type type = resolve_type(compiler, base->syntax);
		if (compiler->failed) {
			return;
		}
		if (type.kind != STRUCT_TYPE || type.data.record->state == FORWARD_RECORD) {
			diagnostic(compiler, base->syntax->span.first, "base must be a complete struct");
			return;
		}
		base->record = type.data.record;
		for (struct BaseRelation* previous = record->bases; previous != base; previous = previous->next) {
			if (previous->record == base->record) {
				diagnostic(compiler, base->syntax->span.first, "duplicate direct base");
				return;
			}
		}
		base->index = index++;
		resolve_bases(compiler, base->record);
		if (compiler->failed) {
			return;
		}
	}
	record->base_resolution = RESOLVED_BASES;
}

enum BaseSearchKind {
	NO_BASE_PATH,
	ONE_BASE_PATH,
	MULTIPLE_BASE_PATHS,
};

struct BaseSearch {
	enum BaseSearchKind kind;
	struct BasePath* path;
};

struct BasePath* append_base_path(struct Compiler* compiler, struct BasePath* path, struct BaseRelation* relation) {
	struct BasePath* result = allocate(compiler, sizeof(struct BasePath));
	if (!result) {
		return NULL;
	}
	if (!path) {
		result->relation = relation;
		return result;
	}
	result->relation = path->relation;
	result->next = append_base_path(compiler, path->next, relation);
	return result;
}

void build_subobject_edges(struct Compiler* compiler, struct Record* complete, struct Subobject* object) {
	struct SubobjectEdge** tail = &object->edges;
	for (struct BaseRelation* relation = object->record->bases; relation && !compiler->failed; relation = relation->next) {
		struct Subobject* target = NULL;
		if (relation->kind == VIRTUAL_BASE) {
			for (struct Subobject* existing = complete->subobjects; existing; existing = existing->next) {
				if (existing->kind == SHARED_SUBOBJECT && existing->record == relation->record) {
					target = existing;
					break;
				}
			}
		}
		struct SubobjectEdge* edge = allocate(compiler, sizeof(struct SubobjectEdge));
		if (!edge) {
			return;
		}
		edge->relation = relation;
		*tail = edge;
		tail = &edge->next;
		if (target) {
			edge->target = target;
			continue;
		}
		target = allocate(compiler, sizeof(struct Subobject));
		if (!target) {
			return;
		}
		target->record = relation->record;
		target->kind = relation->kind == VIRTUAL_BASE ? SHARED_SUBOBJECT : ORDINARY_SUBOBJECT;
		target->path = append_base_path(compiler, object->path, relation);
		struct Subobject** objects = &complete->subobjects;
		while (*objects) {
			objects = &(*objects)->next;
		}
		*objects = target;
		edge->target = target;
		if (relation->kind == VIRTUAL_BASE) {
			struct VirtualBase** bases = &complete->virtual_bases;
			unsigned index = 0;
			while (*bases) {
				bases = &(*bases)->next;
				index++;
			}
			struct VirtualBase* base = allocate(compiler, sizeof(struct VirtualBase));
			if (!base) {
				return;
			}
			base->record = relation->record;
			base->index = index;
			*bases = base;
		}
		build_subobject_edges(compiler, complete, target);
	}
}

void prepare_subobjects(struct Compiler* compiler, struct Record* record) {
	if (compiler->failed || record->subobjects) {
		return;
	}
	resolve_bases(compiler, record);
	if (compiler->failed) {
		return;
	}
	struct Subobject* root = allocate(compiler, sizeof(struct Subobject));
	if (!root) {
		return;
	}
	root->record = record;
	root->kind = ROOT_SUBOBJECT;
	record->subobjects = root;
	build_subobject_edges(compiler, record, root);
}

struct BaseSearch find_base_path(struct Compiler* compiler, struct Record* derived, struct Record* base) {
	prepare_subobjects(compiler, derived);
	struct BaseSearch result = { 0 };
	for (struct Subobject* object = derived->subobjects; object; object = object->next) {
		if (object->record != base) {
			continue;
		}
		if (result.kind != NO_BASE_PATH) {
			return (struct BaseSearch){ MULTIPLE_BASE_PATHS, NULL };
		}
		result.kind = ONE_BASE_PATH;
		result.path = object->path;
	}
	return result;
}

struct Subobject* path_subobject(struct Subobject* object, struct BasePath* path) {
	for (; object && path; path = path->next) {
		struct SubobjectEdge* edge = object->edges;
		while (edge && edge->relation != path->relation) {
			edge = edge->next;
		}
		object = edge ? edge->target : NULL;
	}
	return object;
}

bool subobject_contains(struct Subobject* derived, struct Subobject* base) {
	if (derived == base) {
		return true;
	}
	for (struct SubobjectEdge* edge = derived ? derived->edges : NULL; edge; edge = edge->next) {
		if (subobject_contains(edge->target, base)) {
			return true;
		}
	}
	return false;
}

struct LookupCandidate {
	struct MemberLookup member;
	struct Subobject* object;
	struct LookupCandidate* next;
};

void collect_member_candidates(struct Compiler* compiler, struct Subobject* object, struct Token name, struct LookupCandidate** candidates) {
	struct MemberLookup local = record_local_member(compiler, object->record, name);
	if (local.kind != NO_MEMBER) {
		struct LookupCandidate* candidate = allocate(compiler, sizeof(struct LookupCandidate));
		if (!candidate) {
			return;
		}
		candidate->member = local;
		candidate->member.path = object->path;
		candidate->object = object;
		candidate->next = *candidates;
		*candidates = candidate;
		return;
	}
	for (struct SubobjectEdge* edge = object->edges; edge && !compiler->failed; edge = edge->next) {
		collect_member_candidates(compiler, edge->target, name, candidates);
	}
}

bool same_member(struct MemberLookup first, struct MemberLookup second) {
	if (first.kind != second.kind) {
		return false;
	}
	switch (first.kind) {
	case FIELD_MEMBER:
		return first.entity.field == second.entity.field;
	case FUNCTION_MEMBER:
		return first.entity.function == second.entity.function;
	case NAMESPACE_MEMBER:
		return first.entity.space == second.entity.space;
	case ENUMERATOR_MEMBER:
		return first.entity.enumerator == second.entity.enumerator;
	default:
		return false;
	}
}

struct MemberLookup record_member(struct Compiler* compiler, struct Record* record, struct Token name) {
	struct MemberLookup result = record_local_member(compiler, record, name);
	if (result.kind != NO_MEMBER) {
		return result;
	}
	prepare_subobjects(compiler, record);
	if (compiler->failed || !record->subobjects) {
		return result;
	}
	struct LookupCandidate* candidates = NULL;
	collect_member_candidates(compiler, record->subobjects, name, &candidates);
	for (struct LookupCandidate* candidate = candidates; candidate; candidate = candidate->next) {
		struct LookupCandidate* dominant = candidates;
		while (dominant) {
			if (dominant->object != candidate->object && subobject_contains(dominant->object, candidate->object)) {
				break;
			}
			dominant = dominant->next;
		}
		if (dominant) {
			continue;
		}
		if (result.kind != NO_MEMBER && !same_member(result, candidate->member)) {
			result.kind = AMBIGUOUS_MEMBER;
			return result;
		}
		result = candidate->member;
	}
	return result;
}

struct BasePath* concatenate_base_path(struct Compiler* compiler, struct BasePath* first, struct BasePath* second) {
	if (!first) {
		return second;
	}
	struct BasePath* path = allocate(compiler, sizeof(struct BasePath));
	if (!path) {
		return NULL;
	}
	path->relation = first->relation;
	path->next = concatenate_base_path(compiler, first->next, second);
	return path;
}

struct MemberLookup qualified_record_member(struct Compiler* compiler, struct Record* record, struct Token name, struct Record** qualified_owner) {
	struct QualifiedName lookup = lookup_name(compiler, name);
	if (lookup.name.kind != QUALIFY) {
		struct MemberLookup member = record_member(compiler, record, name);
		if (member.kind == FIELD_MEMBER && member.entity.field->owner && find_base_path(compiler, record, member.entity.field->owner).kind == MULTIPLE_BASE_PATHS) {
			diagnostic(compiler, name, "ambiguous field base subobject");
			return (struct MemberLookup){ 0 };
		}
		return member;
	}
	struct Record* owner = lookup.owner ? lookup.owner->record : NULL;
	if (!owner) {
		diagnostic(compiler, name, "member qualifier must name a base struct");
		return (struct MemberLookup){ 0 };
	}
	struct BaseSearch selection = find_base_path(compiler, record, owner);
	struct MemberLookup member = record_member(compiler, owner, lookup.name);
	struct Variable* receiver = member.kind == FUNCTION_MEMBER ? member.entity.function->parameters : NULL;
	if (selection.kind == NO_BASE_PATH || (selection.kind == MULTIPLE_BASE_PATHS && (member.kind != FUNCTION_MEMBER || (receiver && token_text(receiver->name, "this"))))) {
		diagnostic(compiler, name, selection.kind == MULTIPLE_BASE_PATHS ? "ambiguous base qualifier" : "member qualifier is not a base of the object");
		return (struct MemberLookup){ 0 };
	}
	member.path = concatenate_base_path(compiler, selection.path, member.path);
	if (member.kind == FIELD_MEMBER && member.entity.field->owner && find_base_path(compiler, owner, member.entity.field->owner).kind == MULTIPLE_BASE_PATHS) {
		diagnostic(compiler, name, "ambiguous field base subobject");
		return (struct MemberLookup){ 0 };
	}
	*qualified_owner = owner;
	return member;
}

struct Expression* new_expression(struct Compiler* compiler, enum NodeKind kind, struct Token token) {
	struct Expression* expression = allocate(compiler, sizeof(struct Expression));
	if (expression) {
		expression->kind = kind;
		expression->span.first = token;
		expression->span.last = token;
	}
	return expression;
}

struct Expression* parse_expression(struct Compiler* compiler, int minimum);
struct Statement* parse_statement(struct Compiler* compiler);

bool known_type_name(struct Compiler* compiler, struct Token name) {
	if (find_builtin(name)) {
		return true;
	}
	for (size_t index = 0; index + 2 < compiler->token_count; index++) {
		int kind = compiler->tokens[index].kind;
		int following = compiler->tokens[index + 2].kind;
		if ((kind == STRUCT || kind == UNION || kind == ENUM) && compiler->tokens[index + 1].kind == NAME && (following == '{' || following == ';' || following == ':') && name_equal(name, compiler->tokens[index + 1])) {
			return true;
		}
	}
	return false;
}

bool scan_type(struct Compiler* compiler, size_t* position) {
	if (*position < compiler->token_count && compiler->tokens[*position].kind == CONST) {
		(*position)++;
		return scan_type(compiler, position);
	}
	size_t index = *position;
	if (index >= compiler->token_count) {
		return false;
	}
	int kind = compiler->tokens[index++].kind;
	if (kind == '(') {
		if (index + 1 < compiler->token_count && compiler->tokens[index].kind == ELLIPSIS && compiler->tokens[index + 1].kind == ')') {
			index += 2;
		}
		else if (index < compiler->token_count && compiler->tokens[index].kind == ')') {
			index++;
		}
		else {
			if (!scan_type(compiler, &index)) {
				return false;
			}
			if (index < compiler->token_count && compiler->tokens[index].kind == ')') {
				index++;
				if (index >= compiler->token_count || compiler->tokens[index].kind != ARROW) {
					goto suffixes;
				}
			}
			else {
				while (true) {
					if (index < compiler->token_count && compiler->tokens[index].kind == NAME) {
						index++;
					}
					if (index < compiler->token_count && compiler->tokens[index].kind == ')') {
						index++;
						break;
					}
					if (index >= compiler->token_count || compiler->tokens[index++].kind != ',') {
						return false;
					}
					if (index + 1 < compiler->token_count && compiler->tokens[index].kind == ELLIPSIS && compiler->tokens[index + 1].kind == ')') {
						index += 2;
						break;
					}
					if (!scan_type(compiler, &index)) {
						return false;
					}
				}
			}
		}
		if (index >= compiler->token_count || compiler->tokens[index++].kind != ARROW || !scan_type(compiler, &index)) {
			return false;
		}
	}
	else {
		bool tagged = kind == STRUCT || kind == UNION || kind == ENUM;
		if (tagged) {
			if (index >= compiler->token_count) {
				return false;
			}
			kind = compiler->tokens[index++].kind;
		}
		if (kind != NAME) {
			return false;
		}
		struct Token name = compiler->tokens[index - 1];
		while (index < compiler->token_count && compiler->tokens[index].kind == QUALIFY) {
			index++;
			if (index >= compiler->token_count || compiler->tokens[index].kind != NAME) {
				return false;
			}
			name = compiler->tokens[index++];
		}
		if (!tagged && !known_type_name(compiler, name)) {
			return false;
		}
	}

suffixes:
	while (index < compiler->token_count) {
		if (compiler->tokens[index].kind == '*' || compiler->tokens[index].kind == '&') {
			index++;
			continue;
		}
		if (compiler->tokens[index].kind != '[') {
			break;
		}
		index++;
		if (index + 1 >= compiler->token_count || compiler->tokens[index].kind != INTEGER || compiler->tokens[index + 1].kind != ']') {
			return false;
		}
		index += 2;
	}
	*position = index;
	return true;
}

bool type_start(struct Compiler* compiler) {
	size_t index = compiler->token_index - 1;
	return scan_type(compiler, &index);
}

struct Expression* parse_prefix(struct Compiler* compiler) {
	struct Token token = compiler->token;
	struct Expression* expression = NULL;
	switch (token.kind) {
	case NULLPTR:
		expression = new_expression(compiler, NULLPTR_EXPRESSION, token);
		if (!expression) {
			return NULL;
		}
		expression->type = pointer_type(compiler, (struct Type){ NULL_TYPE, { 0 } });
		next_token(compiler);
		break;
	case TRUE:
	case FALSE:
		expression = new_expression(compiler, BOOLEAN_EXPRESSION, token);
		if (!expression) {
			return NULL;
		}

		expression->data.integer = token.kind == TRUE;
		expression->type = (struct Type){ BOOL_TYPE, { 0 } };
		next_token(compiler);
		break;
	case CHARACTER: {
		expression = new_expression(compiler, INTEGER_EXPRESSION, token);
		if (!expression) {
			return NULL;
		}
		unsigned char byte = (unsigned char)token.start[1];
		if (byte == '\\') {
			byte = (unsigned char)token.start[2];
			switch (byte) {
			case '0':
				byte = 0;
				break;
			case 'n':
				byte = '\n';
				break;
			case 'r':
				byte = '\r';
				break;
			case 't':
				byte = '\t';
				break;
			case '\\':
			case '"':
			case '\'':
				break;
			default:
				diagnostic(compiler, token, "unsupported character escape");
				break;
			}
		}
		expression->data.integer = byte;
		next_token(compiler);
		break;
	}
	case INTEGER:
		expression = new_expression(compiler, INTEGER_EXPRESSION, token);
		if (!expression) {
			return NULL;
		}
		for (size_t i = 0; i < token.length; i++) {
			unsigned digit = (unsigned)(token.start[i] - '0');
			if (expression->data.integer > (UINT64_MAX - digit) / 10) {
				diagnostic(compiler, token, "integer literal exceeds 64-bit magnitude");
				break;
			}
			expression->data.integer = expression->data.integer * 10 + digit;
		}
		next_token(compiler);
		break;
	case DECIMAL:
		expression = new_expression(compiler, DECIMAL_EXPRESSION, token);
		next_token(compiler);
		break;
	case STRING:
		expression = new_expression(compiler, STRING_EXPRESSION, token);
		if (!expression) {
			return NULL;
		}
		expression->data.string.bytes = allocate(compiler, token.length);
		if (!expression->data.string.bytes) {
			return NULL;
		}
		for (size_t i = 1; i + 1 < token.length; i++) {
			unsigned char byte = (unsigned char)token.start[i];
			if (byte == '\\') {
				byte = (unsigned char)token.start[++i];
				switch (byte) {
				case '0':
					byte = 0;
					break;
				case 'n':
					byte = '\n';
					break;
				case 'r':
					byte = '\r';
					break;
				case 't':
					byte = '\t';
					break;
				case '\\':
				case '"':
					break;
				default:
					diagnostic(compiler, token, "unsupported string escape");
					break;
				}
			}
			expression->data.string.bytes[expression->data.string.count++] = byte;
		}
		next_token(compiler);
		break;
	case '!':
	case '~':
		next_token(compiler);
		expression = new_expression(compiler, token.kind == '!' ? NOT_EXPRESSION : COMPLEMENT_EXPRESSION, token);
		if (!expression) {
			return NULL;
		}
		expression->data.operand = parse_prefix(compiler);
		break;
	case INCREMENT:
	case DECREMENT:
		next_token(compiler);
		expression = new_expression(compiler, PREFIX_UPDATE_EXPRESSION, token);
		if (!expression) {
			return NULL;
		}
		expression->data.update.operation = token.kind;
		expression->data.update.value = parse_prefix(compiler);
		break;
	case DISCARD:
	case '@':
	case '$':
	case '&':
	case '*':
		next_token(compiler);
		expression = new_expression(compiler, token.kind == DISCARD ? ABANDON_EXPRESSION : token.kind == '@' ? ACCESS_EXPRESSION : token.kind == '$' ? RELOCATE_EXPRESSION : token.kind == '&' ? ADDRESS_EXPRESSION : DEREFERENCE_EXPRESSION, token);
		if (!expression) {
			return NULL;
		}
		expression->data.operand = parse_prefix(compiler);
		break;
	case '-':
		next_token(compiler);
		expression = new_expression(compiler, NEGATE_EXPRESSION, token);
		if (!expression) {
			return NULL;
		}
		expression->data.operand = parse_prefix(compiler);
		break;
	case '(':
		next_token(compiler);
		size_t type_end = compiler->token_index - 1;
		if (scan_type(compiler, &type_end) && type_end < compiler->token_count && compiler->tokens[type_end].kind == ')') {
			expression = new_expression(compiler, CAST_EXPRESSION, token);
			if (!expression) {
				return NULL;
			}
			expression->data.cast.syntax = parse_type(compiler);
			expect(compiler, ')', "')'");
			expression->data.cast.value = parse_prefix(compiler);
		}
		else {
			expression = parse_expression(compiler, 0);
			expect(compiler, ')', "')'");
		}
		break;
	case SIZEOF:
		next_token(compiler);
		if (take(compiler, '(')) {
			if (type_start(compiler)) {
				expression = new_expression(compiler, SIZEOF_TYPE_EXPRESSION, token);
				if (!expression) {
					return NULL;
				}
				expression->data.size.operand.syntax = parse_type(compiler);
			}
			else {
				expression = new_expression(compiler, SIZEOF_VALUE_EXPRESSION, token);
				if (!expression) {
					return NULL;
				}
				expression->data.size.operand.value = parse_expression(compiler, 0);
			}
			expect(compiler, ')', "')'");
		}
		else {
			expression = new_expression(compiler, SIZEOF_VALUE_EXPRESSION, token);
			if (!expression) {
				return NULL;
			}
			expression->data.size.operand.value = parse_prefix(compiler);
		}
		break;
	case NAME:
		token = parse_name(compiler);
		expression = new_expression(compiler, NAME_EXPRESSION, token);
		if (expression) {
			expression->data.variable.name = token;
		}
		break;
	case AUTO:
		diagnostic(compiler, token, "'auto' is not supported; use 'var' for typed inference");
		break;
	default:
		diagnostic(compiler, token, "expected an expression");
		break;
	}
	while (expression && !compiler->failed) {
		if (take(compiler, '(')) {
			struct Expression* call = new_expression(compiler, CALL_EXPRESSION, expression->span.first);
			if (!call) {
				return NULL;
			}
			call->data.call.callee = expression;
			struct Argument** tail = &call->data.call.arguments;
			while (compiler->token.kind != ')' && !compiler->failed) {
				struct Argument* argument = allocate(compiler, sizeof(struct Argument));
				if (!argument) {
					return NULL;
				}
				if (compiler->token.kind == '{') {
					argument->initialization = parse_statement(compiler);
				}
				else {
					argument->expression = parse_expression(compiler, 0);
				}
				*tail = argument;
				tail = &argument->next;
				if (!take(compiler, ',')) {
					break;
				}
			}
			expect(compiler, ')', "')'");
			expression = call;
		}
		else if (take(compiler, '[')) {
			struct Expression* index = new_expression(compiler, INDEX_EXPRESSION, expression->span.first);
			if (!index) {
				return NULL;
			}
			index->data.binary.left = expression;
			index->data.binary.right = parse_expression(compiler, 0);
			expect(compiler, ']', "']'");
			index->span.last = previous_token(compiler);
			expression = index;
		}
		else if (compiler->token.kind == '.' || compiler->token.kind == ARROW) {
			int kind = compiler->token.kind;
			next_token(compiler);
			struct Expression* field = new_expression(compiler, kind == '.' ? FIELD_EXPRESSION : POINTER_FIELD_EXPRESSION, expression->span.first);
			if (!field) {
				return NULL;
			}
			field->data.member.object = expression;
			field->data.member.name = parse_name(compiler);
			field->span.last = previous_token(compiler);
			expression = field;
		}
		else if (compiler->token.kind == INCREMENT || compiler->token.kind == DECREMENT) {
			int operation = compiler->token.kind;
			next_token(compiler);
			struct Expression* update = new_expression(compiler, POSTFIX_UPDATE_EXPRESSION, expression->span.first);
			if (!update) {
				return NULL;
			}
			update->data.update.operation = operation;
			update->data.update.value = expression;
			expression = update;
		}
		else {
			break;
		}
	}
	if (expression) {
		expression->span.last = previous_token(compiler);
	}
	return expression;
}

struct Expression* parse_expression(struct Compiler* compiler, int minimum) {
	struct Expression* left = parse_prefix(compiler);
	while (!compiler->failed) {
		const struct Operator* operation = find_operator(compiler->token.kind);
		if (!operation || operation->precedence < minimum) {
			break;
		}
		struct Token token = compiler->token;
		next_token(compiler);
		struct Expression* right = parse_expression(compiler, operation->precedence + (operation->kind == ASSIGNMENT_OPERATOR ? 0 : 1));
		struct Expression* binary = new_expression(compiler, operation->kind == ASSIGNMENT_OPERATOR ? ASSIGNMENT_EXPRESSION : BINARY_EXPRESSION, token);
		if (!binary) {
			return NULL;
		}
		binary->data.binary.operation = operation->token;
		binary->data.binary.left = left;
		binary->data.binary.right = right;
		binary->span.first = left->span.first;
		binary->span.last = previous_token(compiler);
		left = binary;
	}
	return left;
}

bool declaration_start(struct Compiler* compiler) {
	if (compiler->token.kind == VAR || compiler->token.kind == '[' || compiler->token.kind == STRUCT || compiler->token.kind == UNION || compiler->token.kind == ENUM) {
		return true;
	}
	size_t index = compiler->token_index - 1;
	if (compiler->token.kind == '(') {
		size_t closing = index + 1;
		size_t depth = 1;
		while (closing < compiler->token_count && depth) {
			int kind = compiler->tokens[closing++].kind;
			if (kind == '(') {
				depth++;
			}
			else if (kind == ')') {
				depth--;
			}
		}
		if (!depth && closing < compiler->token_count && compiler->tokens[closing].kind == NAME) {
			return false;
		}
	}
	return scan_type(compiler, &index) && index < compiler->token_count && compiler->tokens[index].kind == NAME;
}

struct Statement* parse_statement(struct Compiler* compiler);

struct Statement* parse_block(struct Compiler* compiler) {
	struct Statement* first = NULL;
	struct Statement** tail = &first;
	expect(compiler, '{', "'{'");
	while (compiler->token.kind != '}' && !compiler->failed) {
		if (compiler->token.kind == END) {
			diagnostic(compiler, compiler->token, "unterminated block");
			break;
		}
		struct Statement* statement = parse_statement(compiler);
		if (!statement) {
			break;
		}
		*tail = statement;
		tail = &statement->next;
	}
	expect(compiler, '}', "'}'");
	return first;
}

struct Statement* parse_statement(struct Compiler* compiler) {
	struct Token token = compiler->token;
	struct Statement* statement = allocate(compiler, sizeof(struct Statement));
	if (!statement) {
		return NULL;
	}
	statement->span.first = token;
	switch (token.kind) {
	case '{':
		statement->kind = BLOCK_STATEMENT;
		statement->data.block = parse_block(compiler);
		break;
	case ';':
		next_token(compiler);
		statement->kind = EMPTY_STATEMENT;
		break;
	case IF:
		next_token(compiler);
		statement->kind = IF_STATEMENT;
		expect(compiler, '(', "'('");
		statement->data.branch.condition = parse_expression(compiler, 0);
		expect(compiler, ')', "')'");
		statement->data.branch.first = parse_statement(compiler);
		if (take(compiler, ELSE)) {
			statement->data.branch.second = parse_statement(compiler);
		}
		break;
	case WHILE:
		next_token(compiler);
		statement->kind = WHILE_STATEMENT;
		expect(compiler, '(', "'('");
		statement->data.loop.condition = parse_expression(compiler, 0);
		expect(compiler, ')', "')'");
		statement->data.loop.body = parse_statement(compiler);
		break;
	case FOR:
		next_token(compiler);
		statement->kind = FOR_STATEMENT;
		expect(compiler, '(', "'('");
		statement->data.loop.initializer = parse_statement(compiler);
		if (statement->data.loop.initializer && statement->data.loop.initializer->kind != EMPTY_STATEMENT && statement->data.loop.initializer->kind != DECLARATION_STATEMENT && statement->data.loop.initializer->kind != EXPRESSION_STATEMENT) {
			diagnostic(compiler, token, "invalid for initializer");
		}
		if (compiler->token.kind != ';') {
			statement->data.loop.condition = parse_expression(compiler, 0);
		}
		expect(compiler, ';', "';'");
		if (compiler->token.kind != ')') {
			statement->data.loop.step = parse_expression(compiler, 0);
		}
		expect(compiler, ')', "')'");
		statement->data.loop.body = parse_statement(compiler);
		break;
	case SWITCH: {
		next_token(compiler);
		statement->kind = SWITCH_STATEMENT;
		expect(compiler, '(', "'('");
		statement->data.selection.value = parse_expression(compiler, 0);
		expect(compiler, ')', "')'");
		expect(compiler, '{', "'{'");
		struct Case** tail = &statement->data.selection.cases;
		while (compiler->token.kind != '}' && !compiler->failed) {
			struct Case* item = allocate(compiler, sizeof(struct Case));
			if (!item) {
				break;
			}
			item->span.first = compiler->token;
			if (take(compiler, CASE)) {
				item->value = parse_expression(compiler, 0);
			}
			else {
				expect(compiler, DEFAULT, "case or default");
			}
			expect(compiler, ':', "':'");
			struct Statement** body_tail = &item->statements;
			while (compiler->token.kind != CASE && compiler->token.kind != DEFAULT && compiler->token.kind != '}' && !compiler->failed) {
				struct Statement* child = parse_statement(compiler);
				if (!child) {
					break;
				}
				*body_tail = child;
				body_tail = &child->next;
			}
			item->span.last = previous_token(compiler);
			*tail = item;
			tail = &item->next;
		}
		expect(compiler, '}', "'}'");
		break;
	}
	case BREAK:
	case CONTINUE:
		next_token(compiler);
		statement->kind = token.kind == BREAK ? BREAK_STATEMENT : CONTINUE_STATEMENT;
		expect(compiler, ';', "';'");
		break;
	default:
		if (take(compiler, RETURN)) {
			statement->kind = RETURN_STATEMENT;
			if (compiler->token.kind != ';') {
				statement->data.expression = parse_expression(compiler, 0);
			}
		}
		else {
			if (declaration_start(compiler)) {
				statement->kind = DECLARATION_STATEMENT;
				struct Variable* variable = allocate(compiler, sizeof(struct Variable));
				if (!variable) {
					return NULL;
				}
				statement->data.declaration.variable = variable;
				if (!take(compiler, VAR)) {
					variable->syntax = parse_type(compiler);
				}
				variable->name = compiler->token;
				expect(compiler, NAME, "variable name");
				if (compiler->token.kind == '=' || compiler->token.kind == COPY_ASSIGN) {
					statement->data.declaration.operation = compiler->token.kind;
					next_token(compiler);
					statement->data.declaration.initializer = parse_expression(compiler, 0);
				}
				else if (compiler->token.kind == '{') {
					statement->data.declaration.initialization = parse_statement(compiler);
				}
				else if (!variable->syntax) {
					diagnostic(compiler, variable->name, "inferred variables require an initializer");
				}
			}

			else {
				statement->kind = EXPRESSION_STATEMENT;
				statement->data.expression = parse_expression(compiler, 0);
			}
		}
		expect(compiler, ';', "';'");
		break;
	}
	statement->span.last = previous_token(compiler);
	return statement;
}

void parse_declarations(struct Compiler* compiler);
void parse_namespace(struct Compiler* compiler);

void parse_function(struct Compiler* compiler, struct Record* receiver) {
	struct Token first = compiler->token;
	if (!expect(compiler, FN, "'fn' (imports and global declarations are not supported yet)")) {
		return;
	}
	struct Function* function = allocate(compiler, sizeof(struct Function));
	if (!function) {
		return;
	}
	function->kind = FUNCTION_DECLARATION;
	function->span.first = first;
	struct Namespace* previous_scope = compiler->namespace_scope;
	bool destructor;
	struct QualifiedName declared = declaration_name(compiler, parse_qualified_name(compiler, &destructor));
	struct Record* special_owner = declared.owner ? declared.owner->record : NULL;
	if (destructor) {
		function->role = DESTRUCTOR_ROLE;
		if (!special_owner || !name_equal(declared.name, unqualified_token(special_owner->name))) {
			diagnostic(compiler, declared.name, "destructor name must match its owning type");
			return;
		}
	}
	else if (special_owner && name_equal(declared.name, unqualified_token(special_owner->name))) {
		function->role = CONSTRUCTOR_ROLE;
	}
	else if (token_text(declared.name, "operator") && take(compiler, '=')) {
		function->role = ASSIGNMENT_ROLE;
		if (!special_owner) {
			diagnostic(compiler, declared.name, "assignment operator requires an owning type");
			return;
		}
	}
	function->owner = declared.owner;
	function->local_name = declared.name;
	function->name = qualify_name(compiler, declared.owner, declared.name);
	compiler->namespace_scope = declared.owner;
	expect(compiler, '(', "'('");
	struct Variable** parameter_tail = &function->parameters;
	while (compiler->token.kind != ')' && !compiler->failed) {
		if (take(compiler, ELLIPSIS)) {
			function->arity = VARIADIC_PARAMETERS;
			break;
		}
		struct Variable* parameter = allocate(compiler, sizeof(struct Variable));
		if (!parameter) {
			break;
		}
		parameter->syntax = parse_type(compiler);
		parameter->name = compiler->token;
		expect(compiler, NAME, "parameter name");
		*parameter_tail = parameter;
		parameter_tail = &parameter->next;
		if (!take(compiler, ',')) {
			break;
		}
	}
	expect(compiler, ')', "')'");
	if (take(compiler, ARROW)) {
		function->result_syntax = parse_type(compiler);
	}
	function->interface = receiver;
	if ((function->role == CONSTRUCTOR_ROLE || function->role == DESTRUCTOR_ROLE) && function->result_syntax) {
		diagnostic(compiler, function->name, "constructors and destructors have no return type");
	}
	if (take(compiler, ':')) {
		if (take(compiler, DELETE)) {
			function->kind = FUNCTION_DELETED;
		}
		else if (take(compiler, VIRTUAL)) {
			function->modification = VIRTUAL_FUNCTION;
		}
		else if (take(compiler, OVERRIDE)) {
			function->modification = OVERRIDE_FUNCTION;
		}
		else if (take(compiler, FINAL)) {
			function->modification = FINAL_FUNCTION;
		}
		else {
			diagnostic(compiler, compiler->token, "expected delete, virtual, override, or final modifier");
		}
	}
	if (function->modification != ORDINARY_FUNCTION) {
		if (!receiver || receiver->kind != STRUCT_TYPE) {
			diagnostic(compiler, function->name, "virtual relationships must originate in a struct interface");
			return;
		}
		struct VirtualCallable* callable = allocate(compiler, sizeof(struct VirtualCallable));
		if (!callable) {
			return;
		}
		callable->function = function;
		struct VirtualCallable** tail = &receiver->callables;
		while (*tail) {
			tail = &(*tail)->next;
		}
		*tail = callable;
	}
	if (function->kind == FUNCTION_DELETED) {
		expect(compiler, ';', "';' after deleted function");
	}
	else {
		function->kind = take(compiler, ';') ? FUNCTION_DECLARATION : FUNCTION_DEFINITION;
	}
	if (function->kind == FUNCTION_DEFINITION) {
		expect(compiler, '{', "function body or ';'");
		struct Statement** statement_tail = &function->statements;
		while (compiler->token.kind != '}' && !compiler->failed) {
			if (compiler->token.kind == END) {
				diagnostic(compiler, compiler->token, "unterminated function body");
				break;
			}
			struct Statement* statement = parse_statement(compiler);
			if (!statement) {
				break;
			}
			*statement_tail = statement;
			statement_tail = &statement->next;
		}
		expect(compiler, '}', "'}'");
		expect(compiler, ';', "';' after function definition");
	}
	function->span.last = previous_token(compiler);
	if (compiler->last_function) {
		compiler->last_function->next = function;
	}
	else {
		compiler->functions = function;
	}
	compiler->last_function = function;
	compiler->namespace_scope = previous_scope;
	compiler->token.owner = previous_scope;
}

void parse_record(struct Compiler* compiler) {
	struct Token first = compiler->token;
	enum TypeKind kind = first.kind == STRUCT ? STRUCT_TYPE : (first.kind == UNION ? UNION_TYPE : ENUM_TYPE);
	next_token(compiler);
	struct Namespace* previous_scope = compiler->namespace_scope;
	struct QualifiedName declared = declaration_name(compiler, parse_name(compiler));
	struct Namespace* associated = namespace_ensure(compiler, declared.owner, declared.name);
	if (!associated) {
		return;
	}
	struct Record* record = associated->record;
	if (record && record->kind != kind) {
		diagnostic(compiler, declared.name, "conflicting type tags");
		return;
	}
	if (!record) {
		record = allocate(compiler, sizeof(struct Record));
		if (!record) {
			return;
		}
		record->kind = kind;
		record->span.first = first;
		record->name = qualify_name(compiler, declared.owner, declared.name);
		record->associated = associated;
		associated->record = record;
		associated->kind = ASSOCIATED_NAMESPACE;
		if (compiler->last_record) {
			compiler->last_record->next = record;
		}
		else {
			compiler->records = record;
		}
		compiler->last_record = record;
	}
	if (take(compiler, ':')) {
		if (kind != STRUCT_TYPE) {
			diagnostic(compiler, declared.name, "only structs can inherit");
			return;
		}
		struct BaseRelation** tail = &record->bases;
		while (!compiler->failed) {
			struct BaseRelation* base = allocate(compiler, sizeof(struct BaseRelation));
			if (!base) {
				return;
			}
			base->kind = take(compiler, VIRTUAL) ? VIRTUAL_BASE : NONVIRTUAL_BASE;
			base->syntax = parse_type(compiler);
			*tail = base;
			tail = &base->next;
			if (!take(compiler, ',')) {
				break;
			}
		}
		if (compiler->token.kind != '{') {
			diagnostic(compiler, declared.name, "base relationships require a struct definition");
			return;
		}
	}
	if (take(compiler, '{')) {
		if (record->state != FORWARD_RECORD) {
			diagnostic(compiler, declared.name, "duplicate type definition");
			return;
		}
		record->state = DEFINED_RECORD;
		if (record->kind == ENUM_TYPE) {
			struct Enumerator** tail = &record->data.enumerators;
			while (compiler->token.kind != '}' && !compiler->failed) {
				struct Enumerator* value = allocate(compiler, sizeof(struct Enumerator));
				if (!value) {
					return;
				}
				value->name = qualify_name(compiler, compiler->namespace_scope, compiler->token);
				value->owner = record;
				expect(compiler, NAME, "an enumerator name");
				if (take(compiler, '=')) {
					value->initializer = parse_expression(compiler, 0);
				}
				*tail = value;
				tail = &value->next;
				if (!take(compiler, ',')) {
					break;
				}
			}
		}
		else {
			compiler->namespace_scope = associated;
			compiler->token.owner = associated;
			struct Member** tail = &record->data.members;
			while (compiler->token.kind != '}' && !compiler->failed) {
				if (compiler->token.kind == FN) {
					parse_function(compiler, record);
					continue;
				}
				if (compiler->token.kind == NAMESPACE) {
					parse_namespace(compiler);
					continue;
				}
				if (compiler->token.kind == STRUCT || compiler->token.kind == UNION || compiler->token.kind == ENUM) {
					parse_record(compiler);
					continue;
				}
				struct Member* member = allocate(compiler, sizeof(struct Member));
				if (!member) {
					return;
				}
				member->syntax = parse_type(compiler);
				member->name = compiler->token;
				expect(compiler, NAME, "a field name");
				expect(compiler, ';', "';'");
				*tail = member;
				tail = &member->next;
			}
		}
		compiler->namespace_scope = previous_scope;
		expect(compiler, '}', "'}'");
	}
	expect(compiler, ';', "';'");
	record->span.last = previous_token(compiler);
}

void parse_declarations(struct Compiler* compiler);

void parse_namespace(struct Compiler* compiler) {
	next_token(compiler);
	struct Namespace* previous = compiler->namespace_scope;
	struct Namespace* owner = previous;
	while (!compiler->failed) {
		struct Token name = compiler->token;
		if (!expect(compiler, NAME, "a namespace name")) {
			return;
		}
		struct Namespace* space = namespace_ensure(compiler, owner, name);
		if (!space) {
			return;
		}
		if (space->kind == UNRESOLVED_NAMESPACE) {
			space->kind = DECLARED_NAMESPACE;
		}
		owner = space;
		if (!take(compiler, QUALIFY)) {
			break;
		}
	}
	compiler->namespace_scope = owner;
	expect(compiler, '{', "'{' after namespace name");
	parse_declarations(compiler);
	compiler->namespace_scope = previous;
	expect(compiler, '}', "'}' after namespace declarations");
	expect(compiler, ';', "';' after namespace declaration");
}

void parse_declarations(struct Compiler* compiler) {
	while (compiler->token.kind != END && compiler->token.kind != '}' && !compiler->failed) {
		if (compiler->token.kind == NAMESPACE) {
			parse_namespace(compiler);
			continue;
		}
		if (compiler->token.kind == STRUCT || compiler->token.kind == UNION || compiler->token.kind == ENUM) {
			parse_record(compiler);
			continue;
		}
		parse_function(compiler, NULL);
	}
}

void parse_functions(struct Compiler* compiler, size_t first_token) {
	compiler->namespace_scope = NULL;
	compiler->token_index = first_token;
	next_token(compiler);
	parse_declarations(compiler);
	if (!compiler->failed && compiler->token.kind != END) {
		diagnostic(compiler, compiler->token, "unexpected '}' at global scope");
	}
}

struct Function* find_function(struct Compiler* compiler, struct Token name) {
	struct QualifiedName lookup = lookup_name(compiler, name);
	if (lookup.name.kind == END) {
		return NULL;
	}
	for (struct Namespace* owner = lookup.owner; ; owner = owner->parent) {
		if (owner && owner->record) {
			struct MemberLookup member = record_member(compiler, owner->record, lookup.name);
			if (member.kind == AMBIGUOUS_MEMBER) {
				diagnostic(compiler, name, "ambiguous inherited function");
				return NULL;
			}
			if (member.kind != NO_MEMBER) {
				return member.kind == FUNCTION_MEMBER ? member.entity.function : NULL;
			}
		}
		for (struct FunctionSymbol* symbol = compiler->symbols; symbol; symbol = symbol->next) {
			if (symbol->function->role == ORDINARY_ROLE && symbol->function->owner == owner && name_equal(symbol->function->local_name, lookup.name)) {
				return symbol->function;
			}
		}
		if (!owner || lookup.name.kind == QUALIFY) {
			return NULL;
		}
	}
}

struct Variable* find_variable(struct Variable* scope, struct Token name) {
	for (struct Variable* variable = scope; variable; variable = variable->scope_next) {
		if (name_equal(name, variable->name)) {
			return variable;
		}
	}
	return NULL;
}

struct Enumerator* find_enumerator(struct Compiler* compiler, struct Token name) {
	struct QualifiedName lookup = lookup_name(compiler, name);
	if (lookup.name.kind == END) {
		return NULL;
	}
	for (struct Namespace* owner = lookup.owner; ; owner = owner->parent) {
		if (owner && owner->record && owner->record->kind != ENUM_TYPE) {
			struct MemberLookup member = record_member(compiler, owner->record, lookup.name);
			if (member.kind == AMBIGUOUS_MEMBER) {
				diagnostic(compiler, name, "ambiguous inherited enumerator");
				return NULL;
			}
			if (member.kind != NO_MEMBER) {
				return member.kind == ENUMERATOR_MEMBER ? member.entity.enumerator : NULL;
			}
		}
		for (struct Record* enumeration = compiler->records; enumeration; enumeration = enumeration->next) {
			if (enumeration->kind != ENUM_TYPE || (enumeration->associated != owner && enumeration->associated->parent != owner)) {
				continue;
			}
			for (struct Enumerator* value = enumeration->data.enumerators; value; value = value->next) {
				if (name_equal(unqualified_token(value->name), lookup.name)) {
					return value;
				}
			}
		}
		if (!owner || lookup.name.kind == QUALIFY) {
			return NULL;
		}
	}
}

void register_tags(struct Compiler* compiler) {
	for (struct Record* record = compiler->records; record && !compiler->failed; record = record->next) {
		struct Record* previous = find_record(compiler, record->name);
		if (previous) {
			if (previous->kind != record->kind) {
				diagnostic(compiler, record->name, "conflicting type tags");
			}
			else if (record->state == DEFINED_RECORD) {
				if (previous->state != FORWARD_RECORD) {
					diagnostic(compiler, record->name, "duplicate type definition");
				}
				else {
					previous->state = DEFINED_RECORD;
					previous->data = record->data;
					previous->span = record->span;
				}
			}
			continue;
		}
		struct TagSymbol* symbol = allocate(compiler, sizeof(struct TagSymbol));
		if (!symbol) {
			return;
		}
		symbol->record = record;
		symbol->next = compiler->tags;
		compiler->tags = symbol;
		if (record->kind != ENUM_TYPE) {
			char* name = allocate(compiler, record->name.length + 9);
			if (!name) {
				return;
			}
			memcpy(name, "rt.type.", 8);
			memcpy(name + 8, record->name.start, record->name.length);
			record->llvm_type = LLVMStructCreateNamed(compiler->llvm_context, name);
		}
		else {
			record->llvm_type = LLVMInt32TypeInContext(compiler->llvm_context);
		}
	}
}

void complete_type(struct Compiler* compiler, struct Type type, struct Token token) {
	if (compiler->failed) {
		return;
	}
	switch (type.kind) {
	case VOID_TYPE:
		diagnostic(compiler, token, "void is not an object type");
		break;
	case NULL_TYPE:
		diagnostic(compiler, token, "null is not an object type; use a pointer");
		break;
	case FUNCTION_TYPE:
		diagnostic(compiler, token, "function signatures are not object types; use a pointer");
		break;
	case ARRAY_TYPE: {
		complete_type(compiler, *type.data.array.element, token);
		if (compiler->failed) {
			break;
		}
		uint64_t size = LLVMABISizeOfType(compiler->layout, llvm_type(compiler, *type.data.array.element));
		if (size && type.data.array.count > SIZE_MAX / size) {
			diagnostic(compiler, token, "array size exceeds the target address space");
		}
		break;
	}
	case STRUCT_TYPE:
	case UNION_TYPE:
	case ENUM_TYPE:
		resolve_record(compiler, type.data.record);
		break;
	default:
		break;
	}
}

int numeric_bits(struct Type type) {
	return type.kind == BOOL_TYPE ? 1 : type.kind == CHAR_TYPE ? 8 : type.kind == ENUM_TYPE ? 32 : type.data.bits;
}

bool integer_constant(struct Expression* expression, uint64_t* magnitude, bool* negative);

bool enum_integer(struct Compiler* compiler, struct Expression* expression, int64_t* result) {
	uint64_t magnitude;
	bool negative;
	if (integer_constant(expression, &magnitude, &negative)) {
		if (magnitude > (uint64_t)INT32_MAX + (negative ? 1 : 0)) {
			diagnostic(compiler, expression->span.first, "enumerator value is out of range");
			return false;
		}
		*result = negative ? -(int64_t)magnitude : (int64_t)magnitude;
		return true;
	}
	if (expression->kind == NAME_EXPRESSION) {
		struct Enumerator* value = find_enumerator(compiler, expression->data.variable.name);
		if (value && value->state == RESOLVED_ENUMERATOR) {
			*result = value->value;
			return true;
		}
	}
	if (expression->kind == BINARY_EXPRESSION) {
		int64_t left, right, value;
		if (!enum_integer(compiler, expression->data.binary.left, &left) || !enum_integer(compiler, expression->data.binary.right, &right)) {
			return false;
		}
		switch (expression->data.binary.operation) {
		case '+':
			value = left + right;
			break;
		case '-':
			value = left - right;
			break;
		case '*':
			value = left * right;
			break;
		case '/':
		case '%':
			if (!right) {
				diagnostic(compiler, expression->span.first, "division by zero in constant expression");
				return false;
			}
			value = expression->data.binary.operation == '/' ? left / right : left % right;
			break;
		case '&':
			value = left & right;
			break;
		case '|':
			value = left | right;
			break;
		case '^':
			value = left ^ right;
			break;
		default:
			diagnostic(compiler, expression->span.first, "unsupported constant operator");
			return false;
		}
		if (value < INT32_MIN || value > INT32_MAX) {
			diagnostic(compiler, expression->span.first, "integer constant is out of range");
			return false;
		}
		*result = value;
		return true;
	}
	diagnostic(compiler, expression->span.first, "enumerator initializer must be an integer constant");
	return false;
}

void resolve_record(struct Compiler* compiler, struct Record* record) {
	if (compiler->failed || record->state == RESOLVED_RECORD) {
		return;
	}
	if (record->state == FORWARD_RECORD) {
		diagnostic(compiler, record->name, "type has no complete definition");
		return;
	}
	if (record->state == RESOLVING_RECORD) {
		diagnostic(compiler, record->name, "type contains itself by value");
		return;
	}
	record->state = RESOLVING_RECORD;
	if (record->kind == ENUM_TYPE) {
		int64_t next = 0;
		for (struct Enumerator* value = record->data.enumerators; value && !compiler->failed; value = value->next) {
			value->owner = record;
			for (struct Enumerator* earlier = record->data.enumerators; earlier != value; earlier = earlier->next) {
				if (name_equal(earlier->name, value->name)) {
					diagnostic(compiler, value->name, "duplicate enumerator");
					return;
				}
			}
			if (value->initializer && !enum_integer(compiler, value->initializer, &next)) {
				return;
			}
			if (next < INT32_MIN || next > INT32_MAX) {
				diagnostic(compiler, value->name, "enumerator value is out of range");
				return;
			}
			value->value = next++;
			value->state = RESOLVED_ENUMERATOR;
		}
		record->state = RESOLVED_RECORD;
		return;
	}
	prepare_subobjects(compiler, record);
	if (compiler->failed) {
		return;
	}
	record->llvm_body = record->llvm_type;
	if (record->virtual_bases) {
		record->llvm_body = LLVMStructCreateNamed(compiler->llvm_context, "rt.body");
	}
	unsigned count = record->virtual_bases || record->callables ? 1 : 0;
	uint64_t maximum_size = 0;
	unsigned maximum_alignment = 0;
	struct Member* storage = NULL;
	for (struct BaseRelation* base = record->bases; base; base = base->next) {
		resolve_record(compiler, base->record);
		if (compiler->failed) {
			return;
		}
		if (base->kind == NONVIRTUAL_BASE) {
			base->index = count++;
		}
		else {
			for (struct VirtualBase* shared = record->virtual_bases; shared; shared = shared->next) {
				if (shared->record == base->record) {
					base->index = shared->index;
					break;
				}
			}
		}
	}
	for (struct Member* member = record->data.members; member && !compiler->failed; member = member->next) {
		for (struct Function* function = compiler->functions; function; function = function->next) {
			if (function->owner == record->associated && name_equal(function->local_name, member->name)) {
				diagnostic(compiler, member->name, "field conflicts with an associated function");
				return;
			}
		}
		if (namespace_local(compiler, record->associated, member->name)) {
			diagnostic(compiler, member->name, "field conflicts with an associated declaration");
			return;
		}
		for (struct Member* previous = record->data.members; previous != member; previous = previous->next) {
			if (name_equal(previous->name, member->name)) {
				diagnostic(compiler, member->name, "duplicate field");
				return;
			}
		}
		member->type = resolve_type(compiler, member->syntax);
		if (record->kind == UNION_TYPE && member->type.kind == REFERENCE_TYPE) {
			diagnostic(compiler, member->name, "union cannot contain a reference field");
			return;
		}
		complete_type(compiler, member->type, member->name);
		if (compiler->failed) {
			return;
		}
		member->owner = record;
		member->index = count++;
		LLVMTypeRef type = llvm_type(compiler, member->type);
		uint64_t size = LLVMABISizeOfType(compiler->layout, type);
		unsigned alignment = LLVMABIAlignmentOfType(compiler->layout, type);
		if (size > maximum_size) {
			maximum_size = size;
		}
		if (alignment > maximum_alignment) {
			maximum_alignment = alignment;
			storage = member;
		}
	}
	if (!count) {
		LLVMTypeRef byte = LLVMInt8TypeInContext(compiler->llvm_context);
		LLVMStructSetBody(record->llvm_body, &byte, 1, false);
		record->state = RESOLVED_RECORD;
		return;
	}
	LLVMTypeRef* members = allocate(compiler, (count + 1) * sizeof(LLVMTypeRef));
	if (!members) {
		return;
	}
	if (record->kind == STRUCT_TYPE) {
		unsigned index = 0;
		if (record->virtual_bases || record->callables) {
			members[index++] = LLVMPointerTypeInContext(compiler->llvm_context, 0);
		}
		for (struct BaseRelation* base = record->bases; base; base = base->next) {
			if (base->kind == NONVIRTUAL_BASE) {
				members[index++] = base->record->llvm_body;
			}
		}
		for (struct Member* member = record->data.members; member; member = member->next) {
			members[index++] = llvm_type(compiler, member->type);
		}
		LLVMStructSetBody(record->llvm_body, members, count, false);
		if (record->virtual_bases) {
			unsigned complete_count = 1;
			for (struct VirtualBase* shared = record->virtual_bases; shared; shared = shared->next) {
				complete_count++;
			}
			LLVMTypeRef* complete = allocate(compiler, complete_count * sizeof(LLVMTypeRef));
			if (!complete) {
				return;
			}
			complete[0] = record->llvm_body;
			for (struct VirtualBase* shared = record->virtual_bases; shared; shared = shared->next) {
				resolve_record(compiler, shared->record);
				complete[shared->index + 1] = shared->record->llvm_body;
			}
			LLVMStructSetBody(record->llvm_type, complete, complete_count, false);
		}
	}
	else {
		if (maximum_size > SIZE_MAX - (maximum_alignment - 1)) {
			diagnostic(compiler, record->name, "union size exceeds the target address space");
			return;
		}
		uint64_t size = ((maximum_size + maximum_alignment - 1) / maximum_alignment) * maximum_alignment;
		members[0] = llvm_type(compiler, storage->type);
		uint64_t first_size = LLVMABISizeOfType(compiler->layout, members[0]);
		if (size > first_size) {
			members[1] = LLVMArrayType2(LLVMInt8TypeInContext(compiler->llvm_context), size - first_size);
		}
		LLVMStructSetBody(record->llvm_type, members, size > first_size ? 2 : 1, false);
	}
	record->state = RESOLVED_RECORD;
}

bool integer_constant(struct Expression* expression, uint64_t* magnitude, bool* negative);

void register_functions(struct Compiler* compiler) {
	for (struct Function* function = compiler->functions; function && !compiler->failed; function = function->next) {
		function->result = resolve_type(compiler, function->result_syntax);
		if (!is_void(function->result)) {
			complete_type(compiler, function->result, function->name);
		}
		struct Variable* scope = NULL;
		for (struct Variable* parameter = function->parameters; parameter; parameter = parameter->next) {
			parameter->type = resolve_type(compiler, parameter->syntax);
			complete_type(compiler, parameter->type, parameter->name);
			if (unsized_type(parameter->type)) {
				diagnostic(compiler, parameter->name, "parameter cannot have an unsized type");
			}
			if (find_variable(scope, parameter->name)) {
				diagnostic(compiler, parameter->name, "duplicate parameter");
			}
			parameter->scope_next = scope;
			scope = parameter;
		}
		if (function->role == DESTRUCTOR_ROLE && (!function->parameters || function->parameters->next || function->arity != FIXED_PARAMETERS || !token_text(function->parameters->name, "this") || function->parameters->type.kind != REFERENCE_TYPE || function->parameters->type.data.element->qualifier != MUTABLE_TYPE)) {
			diagnostic(compiler, function->name, "destructor requires one reference receiver named this");
			return;
		}
		struct Record* interface = function->interface;
		for (struct Function* declaration = compiler->functions; !interface && declaration; declaration = declaration->next) {
			if (declaration->interface && declaration->owner == function->owner && name_equal(declaration->local_name, function->local_name)) {
				interface = declaration->interface;
			}
		}
		if ((interface || function->role != ORDINARY_ROLE) && (!function->parameters || !token_text(function->parameters->name, "this"))) {
			diagnostic(compiler, function->name, "struct interface function requires an owning reference receiver named this");
			return;
		}
		if (function->owner && function->owner->record && function->parameters && token_text(function->parameters->name, "this")) {
			struct Type receiver = function->parameters->type;
			struct Record* owner = function->owner->record;
			if (receiver.kind != REFERENCE_TYPE || receiver.data.element->kind != owner->kind || receiver.data.element->data.record != owner) {
				diagnostic(compiler, function->parameters->name, "associated receiver must reference its owning type");
				return;
			}
		}
		struct Function* previous = NULL;
		for (struct FunctionSymbol* symbol = compiler->symbols; symbol; symbol = symbol->next) {
			if (symbol->function->owner == function->owner && symbol->function->role == function->role && name_equal(symbol->function->local_name, function->local_name)) {
				if (function->role == CONSTRUCTOR_ROLE || function->role == ASSIGNMENT_ROLE) {
					struct Variable* left = symbol->function->parameters;
					struct Variable* right = function->parameters;
					while (left && right && type_equal(left->type, right->type)) {
						left = left->next;
						right = right->next;
					}
					if (left || right) {
						continue;
					}
				}
				previous = symbol->function;
				break;
			}
		}
		if (previous) {
			if (previous->modification != ORDINARY_FUNCTION && function->modification != ORDINARY_FUNCTION && previous->modification != function->modification) {
				diagnostic(compiler, function->name, "conflicting virtual relationships");
				return;
			}
			if (function->modification != ORDINARY_FUNCTION) {
				previous->modification = function->modification;
			}
			if (function->interface) {
				previous->interface = function->interface;
			}
			bool compatible = type_equal(previous->result, function->result) && previous->arity == function->arity;
			struct Variable* a = previous->parameters;
			struct Variable* b = function->parameters;
			while (a && b && compatible) {
				compatible = type_equal(a->type, b->type);
				a = a->next;
				b = b->next;
			}
			if (!compatible || a || b) {
				diagnostic(compiler, function->name, "conflicting function declarations");
			}
			else if (function->kind != FUNCTION_DECLARATION) {
				if (previous->kind != FUNCTION_DECLARATION) {
					diagnostic(compiler, function->name, "duplicate function definition");
				}
				else {
					previous->kind = function->kind;
					previous->span = function->span;
					previous->parameters = function->parameters;
					previous->statements = function->statements;
				}
			}
			continue;
		}
		struct FunctionSymbol* symbol = allocate(compiler, sizeof(struct FunctionSymbol));
		if (!symbol) {
			break;
		}
		symbol->function = function;
		if (compiler->last_symbol) {
			compiler->last_symbol->next = symbol;
		}
		else {
			compiler->symbols = symbol;
		}
		compiler->last_symbol = symbol;
	}
	for (struct FunctionSymbol* symbol = compiler->symbols; symbol && !compiler->failed; symbol = symbol->next) {
		struct Function* function = symbol->function;
		if (function->kind != FUNCTION_DECLARATION) {
			continue;
		}
		struct Namespace* associated = function->owner;
		while (associated && !associated->record) {
			associated = associated->parent;
		}
		if (associated) {
			continue;
		}
		if (function->result.kind == STRUCT_TYPE || function->result.kind == UNION_TYPE || function->result.kind == ARRAY_TYPE) {
			diagnostic(compiler, function->name, "C bindings returning aggregates by value are not supported");
		}
		for (struct Variable* parameter = function->parameters; parameter; parameter = parameter->next) {
			if (parameter->type.kind == STRUCT_TYPE || parameter->type.kind == UNION_TYPE || parameter->type.kind == ARRAY_TYPE) {
				diagnostic(compiler, parameter->name, "C bindings taking aggregates by value are not supported");
			}
		}
	}
}

bool virtual_parameters_equal(struct Function* first, struct Function* second) {
	if (first->arity != second->arity || !first->parameters || !second->parameters || !token_text(first->parameters->name, "this") || !token_text(second->parameters->name, "this")) {
		return false;
	}
	struct Variable* a = first->parameters->next;
	struct Variable* b = second->parameters->next;
	while (a && b) {
		if (!type_equal(a->type, b->type)) {
			return false;
		}
		a = a->next;
		b = b->next;
	}
	return !a && !b;
}

bool covariant_result(struct Compiler* compiler, struct Type derived, struct Type base) {
	if (type_equal(derived, base)) {
		return true;
	}
	if (derived.kind != base.kind || (derived.kind != POINTER_TYPE && derived.kind != REFERENCE_TYPE) || derived.data.element->kind != STRUCT_TYPE || base.data.element->kind != STRUCT_TYPE) {
		return false;
	}
	return find_base_path(compiler, derived.data.element->data.record, base.data.element->data.record).kind == ONE_BASE_PATH;
}

void resolve_virtual_function(struct Compiler* compiler, struct Function* function) {
	if (compiler->failed || function->virtual_resolution == RESOLVED_VIRTUAL) {
		return;
	}
	if (function->virtual_resolution == RESOLVING_VIRTUAL) {
		diagnostic(compiler, function->name, "cyclic virtual relationship");
		return;
	}
	function->virtual_resolution = RESOLVING_VIRTUAL;
	struct Record* owner = function->owner ? function->owner->record : NULL;
	if (!owner || owner->kind != STRUCT_TYPE) {
		function->virtual_resolution = RESOLVED_VIRTUAL;
		return;
	}
	if (function->modification != ORDINARY_FUNCTION && (!function->interface || !function->parameters || !token_text(function->parameters->name, "this"))) {
		diagnostic(compiler, function->name, "virtual operation requires a struct interface receiver");
		return;
	}
	for (struct FunctionSymbol* symbol = compiler->symbols; symbol && !compiler->failed; symbol = symbol->next) {
		struct Function* base = symbol->function;
		struct Record* base_owner = base->owner ? base->owner->record : NULL;
		if (!base_owner || base_owner == owner || !name_equal(base->local_name, function->local_name) || !virtual_parameters_equal(function, base) || find_base_path(compiler, owner, base_owner).kind == NO_BASE_PATH) {
			continue;
		}
		resolve_virtual_function(compiler, base);
		if (base->modification == ORDINARY_FUNCTION) {
			continue;
		}
		if (function->modification == ORDINARY_FUNCTION || function->modification == VIRTUAL_FUNCTION) {
			diagnostic(compiler, function->name, "matching inherited virtual operation requires explicit override or final");
			return;
		}
		if (base->modification == FINAL_FUNCTION) {
			diagnostic(compiler, function->name, "cannot override a final operation");
			return;
		}
		if (!covariant_result(compiler, function->result, base->result)) {
			diagnostic(compiler, function->name, "override result is not signature-compatible or covariant");
			return;
		}
		struct OverrideRelation* relation = allocate(compiler, sizeof(struct OverrideRelation));
		if (!relation) {
			return;
		}
		relation->function = base;
		relation->next = function->overrides;
		function->overrides = relation;
	}
	if ((function->modification == OVERRIDE_FUNCTION || function->modification == FINAL_FUNCTION) && !function->overrides) {
		diagnostic(compiler, function->name, "override requires a matching inherited virtual operation");
		return;
	}
	function->virtual_resolution = RESOLVED_VIRTUAL;
}

bool function_overrides(struct Function* implementation, struct Function* declaration) {
	if (implementation == declaration) {
		return true;
	}
	for (struct OverrideRelation* relation = implementation->overrides; relation; relation = relation->next) {
		if (function_overrides(relation->function, declaration)) {
			return true;
		}
	}
	return false;
}

struct FinalOverrider {
	struct Function* function;
	struct Subobject* object;
};

struct FinalOverrider final_overrider(struct Compiler* compiler, struct Record* complete, struct Subobject* origin, struct Function* declaration) {
	struct FinalOverrider result = { 0 };
	for (struct Subobject* object = complete->subobjects; object; object = object->next) {
		if (!subobject_contains(object, origin)) {
			continue;
		}
		for (struct VirtualCallable* callable = object->record->callables; callable; callable = callable->next) {
			if (!function_overrides(callable->function, declaration)) {
				continue;
			}
			struct Subobject* dominant = complete->subobjects;
			while (dominant) {
				if (dominant != object && subobject_contains(dominant, object)) {
					struct VirtualCallable* candidate = dominant->record->callables;
					while (candidate && !function_overrides(candidate->function, declaration)) {
						candidate = candidate->next;
					}
					if (candidate) {
						break;
					}
				}
				dominant = dominant->next;
			}
			if (dominant) {
				continue;
			}
			if (result.object && (result.object != object || result.function != callable->function)) {
				diagnostic(compiler, complete->name, "virtual operation has no unique final overrider");
				return result;
			}
			result.function = callable->function;
			result.object = object;
		}
	}
	return result;
}

void resolve_virtual_functions(struct Compiler* compiler) {
	for (struct Record* record = compiler->records; record; record = record->next) {
		struct VirtualCallable** entry = &record->callables;
		unsigned index = 0;
		while (*entry) {
			struct VirtualCallable* callable = *entry;
			for (struct FunctionSymbol* symbol = compiler->symbols; symbol; symbol = symbol->next) {
				if (symbol->function->owner == callable->function->owner && name_equal(symbol->function->local_name, callable->function->local_name)) {
					callable->function = symbol->function;
					break;
				}
			}
			struct VirtualCallable* previous = record->callables;
			while (previous != callable && previous->function != callable->function) {
				previous = previous->next;
			}
			if (previous != callable) {
				*entry = callable->next;
				continue;
			}
			callable->index = index++;
			entry = &callable->next;
		}
	}
	for (struct FunctionSymbol* symbol = compiler->symbols; symbol && !compiler->failed; symbol = symbol->next) {
		resolve_virtual_function(compiler, symbol->function);
	}
	for (struct Record* record = compiler->records; record && !compiler->failed; record = record->next) {
		for (struct Subobject* object = record->subobjects; object && !compiler->failed; object = object->next) {
			for (struct VirtualCallable* callable = object->record->callables; callable && !compiler->failed; callable = callable->next) {
				final_overrider(compiler, record, object, callable->function);
			}
		}
	}
}

struct ScopeBinding* scope_local(struct Scope* scope, struct Token name) {
	for (struct ScopeBinding* binding = scope->bindings; binding; binding = binding->next) {
		if (name_equal(binding->name, name)) {
			return binding;
		}
	}
	return NULL;
}

struct ScopeBinding* scope_binding(struct Scope* scope, struct Token name) {
	for (; scope; scope = scope->parent) {
		struct ScopeBinding* binding = scope_local(scope, name);
		if (binding) {
			return binding;
		}
	}
	return NULL;
}

void scope_remove(struct Scope* scope, struct Variable* variable) {
	for (; scope; scope = scope->parent) {
		for (struct ScopeBinding* binding = scope->bindings; binding; binding = binding->next) {
			if (binding->variable == variable) {
				binding->variable = NULL;
				return;
			}
		}
	}
}

void scope_declare(struct Compiler* compiler, struct Scope* scope, struct Variable* variable) {
	struct ScopeBinding* binding = scope_local(scope, variable->name);
	if (!binding) {
		binding = allocate(compiler, sizeof(struct ScopeBinding));
		if (!binding) {
			return;
		}
		binding->name = variable->name;
		binding->next = scope->bindings;
		scope->bindings = binding;
	}

	binding->variable = variable;
}

struct Scope* scope_snapshot(struct Compiler* compiler, struct Scope* scope) {
	if (!scope) {
		return NULL;
	}
	struct Scope* snapshot = allocate(compiler, sizeof(struct Scope));
	if (!snapshot) {
		return NULL;
	}
	snapshot->parent = scope_snapshot(compiler, scope->parent);
	snapshot->initialization = scope->initialization;
	struct ScopeBinding** next = &snapshot->bindings;
	for (struct ScopeBinding* binding = scope->bindings; binding; binding = binding->next) {
		struct ScopeBinding* copy = allocate(compiler, sizeof(struct ScopeBinding));
		if (!copy) {
			return NULL;
		}
		copy->name = binding->name;
		copy->variable = binding->variable;
		*next = copy;
		next = &copy->next;
	}
	return snapshot;
}

void scope_merge(struct Scope* scope, struct Scope* snapshot) {
	for (; scope && snapshot; scope = scope->parent, snapshot = snapshot->parent) {
		for (struct ScopeBinding* binding = scope->bindings; binding; binding = binding->next) {
			struct ScopeBinding* branch = scope_local(snapshot, binding->name);
			if (branch && !branch->variable) {
				binding->variable = NULL;
			}
		}
	}
}

bool borrowed_storage(struct Expression* expression) {
	if (!expression) {
		return false;
	}

	switch (expression->kind) {
	case DEREFERENCE_EXPRESSION:
	case REFERENCE_CAST_EXPRESSION:
	case BASE_VALUE_EXPRESSION:
	case POINTER_FIELD_EXPRESSION:
	case FIELD_EXPRESSION:
	case INDEX_EXPRESSION:
		return true;
	case NAME_EXPRESSION:
		return expression->data.variable.binding && expression->data.variable.binding->type.kind == REFERENCE_TYPE;
	case CALL_EXPRESSION:
		return expression->data.call.binding && expression->data.call.binding->result.kind == REFERENCE_TYPE;
	case CAST_EXPRESSION:
		return borrowed_storage(expression->data.cast.value);
	default:
		return false;
	}
}

void move_expression(struct Compiler* compiler, struct Expression* expression, struct Scope* scope) {
	if (!expression) {
		return;
	}
	if (borrowed_storage(expression)) {
		diagnostic(compiler, expression->span.first, "cannot move borrowed storage; use ':=' to copy");
		return;
	}

	while (expression->kind == CAST_EXPRESSION) {
		expression = expression->data.cast.value;
	}
	if (expression->kind != NAME_EXPRESSION || !expression->data.variable.binding) {
		return;
	}

}

unsigned analyze_statements(struct Compiler* compiler, struct Function* function, struct Statement* statements, struct Scope* scope, struct Flow* flow);
void prepare_reference_fields(struct Compiler* compiler, struct Variable* variable, struct Statement* initialization);
struct Function* select_special_function(struct Compiler* compiler, struct Record* record, enum FunctionRole role, struct Argument* arguments, struct Token token);

struct Function* receiver_function(struct Compiler* compiler, struct Type object, struct Token name) {
	struct Record* qualified_owner = NULL;
	struct MemberLookup member = qualified_record_member(compiler, object.data.record, name, &qualified_owner);
	if (member.kind == AMBIGUOUS_MEMBER) {
		diagnostic(compiler, name, "ambiguous inherited member");
		return NULL;
	}
	if (member.kind != NO_MEMBER) {
		return member.kind == FUNCTION_MEMBER ? member.entity.function : NULL;
	}

	struct Function* function = find_function(compiler, name);
	struct Variable* receiver = function ? function->parameters : NULL;
	if (!receiver || !token_text(receiver->name, "this") || receiver->type.kind != REFERENCE_TYPE || receiver->type.data.element->kind != STRUCT_TYPE || object.kind != STRUCT_TYPE) {
		return NULL;
	}
	struct BaseSearch path = find_base_path(compiler, object.data.record, receiver->type.data.element->data.record);
	if (path.kind != ONE_BASE_PATH) {
		if (path.kind == MULTIPLE_BASE_PATHS) {
			diagnostic(compiler, name, "ambiguous receiver base");
		}
		return NULL;
	}
	return function;
}

void bind_expression(struct Compiler* compiler, struct Expression* expression, struct Scope* scope) {
	if (!expression || compiler->failed) {
		return;
	}
	switch (expression->kind) {
	case NAME_EXPRESSION: {
		if (expression->data.variable.name.kind == QUALIFY) {
			struct Token qualifier = expression->data.variable.name;
			for (size_t index = 0; index < qualifier.length; index++) {
				if (qualifier.start[index] == ':') {
					qualifier.length = index;
					break;
				}
			}
			if (scope_binding(scope, qualifier)) {
				diagnostic(compiler, qualifier, "identifier is not a namespace");
				break;
			}
		}
		struct ScopeBinding* binding = expression->data.variable.name.kind == QUALIFY ? NULL : scope_binding(scope, expression->data.variable.name);
		if (binding && !binding->variable) {
			diagnostic(compiler, expression->span.first, "undeclared variable '%.*s'", (int)binding->name.length, binding->name.start);
			break;
		}
		expression->data.variable.binding = binding ? binding->variable : NULL;
		if (!expression->data.variable.binding) {
			struct Token name = expression->data.variable.name;
			struct Token receiver_name = name;
			receiver_name.start = "this";
			receiver_name.length = 4;
			struct ScopeBinding* receiver_binding = name.kind == QUALIFY ? NULL : scope_binding(scope, receiver_name);
			if (receiver_binding && receiver_binding->variable && receiver_binding->variable->type.kind == REFERENCE_TYPE) {
				struct Type object = *receiver_binding->variable->type.data.element;
				if (object.kind == STRUCT_TYPE || object.kind == UNION_TYPE) {
					struct MemberLookup member = record_member(compiler, object.data.record, name);
					struct Member* field = member.kind == FIELD_MEMBER ? member.entity.field : NULL;
					struct Function* method = receiver_function(compiler, object, name);
					if (field || (method && method->owner && method->owner->record && method->parameters && token_text(method->parameters->name, "this"))) {
						struct Expression* receiver = new_expression(compiler, NAME_EXPRESSION, receiver_name);
						if (!receiver) {
							break;
						}
						receiver->data.variable.name = receiver_name;
						receiver->data.variable.binding = receiver_binding->variable;
						receiver->type = object;
						if (field) {
							expression->kind = FIELD_EXPRESSION;
							expression->data.member.object = receiver;
							expression->data.member.name = name;
							expression->data.member.binding = field;
							expression->data.member.base_path = member.path;
							expression->data.member.initialization = NULL;
							expression->type = field->type.kind == REFERENCE_TYPE ? *field->type.data.element : field->type;
							if (field->type.kind != REFERENCE_TYPE && object.qualifier == CONST_TYPE) {
								expression->type.qualifier = CONST_TYPE;
							}
						}
						else {
							expression->kind = BOUND_FUNCTION_EXPRESSION;
							expression->data.bound_function.function = method;
							expression->data.bound_function.receiver = receiver;
							expression->data.bound_function.kind = BIND_RECEIVER;
							expression->data.bound_function.qualified_owner = NULL;
							expression->type = (struct Type){ FUNCTION_TYPE, { .function = method } };
						}
						break;
					}
				}
			}
			struct Function* function = find_function(compiler, expression->data.variable.name);
			if (function) {
				expression->kind = FUNCTION_EXPRESSION;
				expression->data.function = function;
				expression->type = (struct Type){ FUNCTION_TYPE, { .function = function } };
				break;
			}
			struct Enumerator* value = find_enumerator(compiler, expression->data.variable.name);
			if (!value) {
				diagnostic(compiler, expression->span.first, "undeclared variable '%.*s'", (int)expression->data.variable.name.length, expression->data.variable.name.start);
			}
			else {
				expression->kind = ENUMERATOR_EXPRESSION;
				expression->data.enumerator = value;
				expression->type = (struct Type){ ENUM_TYPE, { .record = value->owner } };
			}
		}
		else {
			expression->type = expression->data.variable.binding->type;
			if (expression->type.kind == REFERENCE_TYPE) {
				expression->type = *expression->type.data.element;
			}
			for (struct Scope* context = scope; context; context = context->parent) {
				if (context->initialization && context->initialization->variable == expression->data.variable.binding) {
					expression->data.variable.initialization = context->initialization;
					break;
				}
			}
		}
		break;
	}
	case BASE_VALUE_EXPRESSION:
	case BASE_POINTER_EXPRESSION:
		bind_expression(compiler, expression->data.base.value, scope);
		break;
	case CALL_EXPRESSION:
		if (expression->data.call.callee->kind == NAME_EXPRESSION) {
			struct Token name = expression->data.call.callee->data.variable.name;
			const struct BuiltinType* builtin = find_builtin(name);
			struct Record* record = builtin ? NULL : find_record(compiler, name);
			if (builtin || record) {
				struct Argument* arguments = expression->data.call.arguments;
				expression->kind = CONSTRUCTION_EXPRESSION;
				expression->type = builtin ? builtin->type : (struct Type){ record->kind, { .record = record } };
				expression->data.construction.function = NULL;
				expression->data.construction.arguments = arguments;
				expression->data.construction.slot = NULL;
				for (struct Argument* argument = arguments; argument; argument = argument->next) {
					if (argument->initialization) {
						diagnostic(compiler, expression->span.first, "constructor arguments require expressions");
						break;
					}
					bind_expression(compiler, argument->expression, scope);
				}
				break;
			}
		}
		if (expression->data.call.callee->kind == NAME_EXPRESSION) {
			struct Token name = expression->data.call.callee->data.variable.name;
			if (unqualified_token(name).length != name.length) {
				struct QualifiedName qualified = lookup_name(compiler, name);
				expression->data.call.qualified_owner = qualified.owner ? qualified.owner->record : NULL;
			}
		}
		bind_expression(compiler, expression->data.call.callee, scope);
		if (compiler->failed) {
			break;
		}
		struct Argument* receiver_argument = NULL;
		if (expression->data.call.callee->kind == BOUND_FUNCTION_EXPRESSION) {
			struct Expression* callee = expression->data.call.callee;
			struct Function* function = callee->data.bound_function.function;
			struct Expression* receiver = callee->data.bound_function.receiver;
			expression->data.call.qualified_owner = callee->data.bound_function.qualified_owner;
			if (callee->data.bound_function.kind == BIND_RECEIVER) {
				receiver_argument = allocate(compiler, sizeof(struct Argument));
				if (!receiver_argument) {
					break;
				}
				receiver_argument->expression = receiver;
				receiver_argument->next = expression->data.call.arguments;
				expression->data.call.arguments = receiver_argument;
			}
			else {
				expression->data.call.receiver_evaluation = receiver;
			}
			callee->kind = FUNCTION_EXPRESSION;
			callee->data.function = function;
		}
		struct Type callable = expression->data.call.callee->type;
		if (callable.kind == POINTER_TYPE) {
			callable = *callable.data.element;
		}
		if (callable.kind != FUNCTION_TYPE) {
			diagnostic(compiler, expression->span.first, "expression is not callable");
			break;
		}
		expression->data.call.binding = callable.data.function;
		expression->type = expression->data.call.binding->result;
		if (expression->type.kind == REFERENCE_TYPE) {
			expression->type = *expression->type.data.element;
		}
		struct Variable* parameter = callable.data.function->parameters;
		for (struct Argument* argument = expression->data.call.arguments; argument; argument = argument->next) {
			if (argument->initialization) {
				if (!parameter || !parameter->name.length) {
					diagnostic(compiler, expression->span.first, "argument block requires a named parameter");
					break;
				}
				argument->variable = allocate(compiler, sizeof(struct Variable));
				if (!argument->variable) {
					break;
				}
				argument->variable->name = parameter->name;
				argument->variable->type = parameter->type;
				struct Scope argument_scope = { NULL, scope };
				struct InitializationContext* argument_context = allocate(compiler, sizeof(struct InitializationContext));
				if (!argument_context) {
					break;
				}
				argument_context->variable = argument->variable;
				argument_context->type = argument->variable->type;
				argument_context->body = argument->initialization;
				argument_scope.initialization = argument_context;
				if (parameter->type.kind != REFERENCE_TYPE) {
					struct Statement* first = argument->initialization->data.block;
					struct Expression* binding = first && first->kind == EXPRESSION_STATEMENT ? first->data.expression : NULL;
					if (binding && binding->kind == ASSIGNMENT_EXPRESSION && (binding->data.binary.operation == '=' || binding->data.binary.operation == COPY_ASSIGN) && binding->data.binary.left->kind == NAME_EXPRESSION && name_equal(binding->data.binary.left->data.variable.name, parameter->name)) {
						argument->initializer = binding->data.binary.right;
						argument_context->initializer = argument->initializer;
						argument->initialization->data.block = first->next;
						bind_expression(compiler, argument->initializer, scope);
					}
				}
				scope_declare(compiler, &argument_scope, argument->variable);
				argument->expression = new_expression(compiler, NAME_EXPRESSION, parameter->name);
				if (!argument->expression) {
					break;
				}
				argument->expression->data.variable.name = parameter->name;
				bind_expression(compiler, argument->expression, &argument_scope);
				if (!argument->initializer) {
					prepare_reference_fields(compiler, argument->variable, argument->initialization);
				}
				unsigned exits = analyze_statements(compiler, callable.data.function, argument->initialization->data.block, &argument_scope, NULL);
				if (exits != NORMAL_EXIT) {
					diagnostic(compiler, expression->span.first, "argument block must yield its parameter normally");
				}
				struct ScopeBinding* result = scope_binding(&argument_scope, parameter->name);
				if (!result || result->variable != argument->variable) {
					diagnostic(compiler, expression->span.first, "argument block moved its result parameter");
				}
			}
			else {
				if (argument != receiver_argument) {
					bind_expression(compiler, argument->expression, scope);
				}
			}
			if (parameter) {
				parameter = parameter->next;
			}
		}
		break;
	case CAST_EXPRESSION:
		bind_expression(compiler, expression->data.cast.value, scope);
		expression->type = resolve_type(compiler, expression->data.cast.syntax);
		if (expression->type.kind == REFERENCE_TYPE) {
			struct Expression* referent = expression->data.cast.value;
			expression->type = *expression->type.data.element;
			expression->kind = REFERENCE_CAST_EXPRESSION;
			expression->data.operand = referent;
		}
		break;
	case SIZEOF_TYPE_EXPRESSION:
	case SIZEOF_VALUE_EXPRESSION: {
		struct Type type;
		if (expression->kind == SIZEOF_TYPE_EXPRESSION) {
			type = resolve_type(compiler, expression->data.size.operand.syntax);
		}
		else {
			bind_expression(compiler, expression->data.size.operand.value, scope);
			type = expression->data.size.operand.value->type;
		}
		complete_type(compiler, type, expression->span.first);
		if (type.kind == REFERENCE_TYPE) {
			type = *type.data.element;
			complete_type(compiler, type, expression->span.first);
		}
		if (!compiler->failed) {
			expression->data.size.size = LLVMABISizeOfType(compiler->layout, llvm_type(compiler, type));
		}
		expression->type = (struct Type){ UNSIGNED_INTEGER_TYPE, { sizeof(void*) * CHAR_BIT } };
		break;
	}
	case FIELD_EXPRESSION:
	case POINTER_FIELD_EXPRESSION: {
		bind_expression(compiler, expression->data.member.object, scope);
		if (compiler->failed) {
			break;
		}
		struct Type object = expression->data.member.object->type;
		if (expression->kind == POINTER_FIELD_EXPRESSION) {
			if (object.kind != POINTER_TYPE) {
				diagnostic(compiler, expression->span.first, "'->' requires a pointer to a record");
				break;
			}
			object = *object.data.element;
		}
		if (object.kind != STRUCT_TYPE && object.kind != UNION_TYPE) {
			diagnostic(compiler, expression->span.first, "field access requires a struct or union");
			break;
		}
		struct Record* qualified_owner = NULL;
		struct MemberLookup member = qualified_record_member(compiler, object.data.record, expression->data.member.name, &qualified_owner);
		if (member.kind == AMBIGUOUS_MEMBER) {
			diagnostic(compiler, expression->data.member.name, "ambiguous inherited member");
			break;
		}
		if (member.kind == FIELD_MEMBER) {
			expression->data.member.binding = member.entity.field;
			expression->data.member.base_path = member.path;
			expression->type = member.entity.field->type;
			if (expression->type.kind == REFERENCE_TYPE) {
				expression->type = *expression->type.data.element;
			}
			else if (object.qualifier == CONST_TYPE) {
				expression->type.qualifier = CONST_TYPE;
			}
		}
		if (!expression->data.member.binding) {
			struct Function* function = receiver_function(compiler, object, expression->data.member.name);
			if (!function) {
				diagnostic(compiler, expression->data.member.name, "unknown field or associated function '%.*s'", (int)expression->data.member.name.length, expression->data.member.name.start);
				break;
			}
			struct Expression* receiver = expression->data.member.object;
			enum ReceiverKind receiver_kind = EVALUATE_RECEIVER;
			if (function->parameters && token_text(function->parameters->name, "this")) {
				receiver_kind = BIND_RECEIVER;
				if (expression->kind == POINTER_FIELD_EXPRESSION) {
					struct Expression* pointee = new_expression(compiler, DEREFERENCE_EXPRESSION, receiver->span.first);
					if (!pointee) {
						break;
					}
					pointee->data.operand = receiver;
					pointee->type = object;
					receiver = pointee;
				}
				if (qualified_owner && qualified_owner != object.data.record) {
					struct BaseSearch selection = find_base_path(compiler, object.data.record, qualified_owner);
					struct Expression* projection = new_expression(compiler, BASE_VALUE_EXPRESSION, receiver->span.first);
					if (!projection) {
						break;
					}
					projection->data.base.value = receiver;
					projection->data.base.path = selection.path;
					projection->type = (struct Type){ STRUCT_TYPE, { .record = qualified_owner } };
					receiver = projection;
				}
			}
			expression->kind = BOUND_FUNCTION_EXPRESSION;
			expression->data.bound_function.function = function;
			expression->data.bound_function.receiver = receiver;
			expression->data.bound_function.kind = receiver_kind;
			expression->data.bound_function.qualified_owner = qualified_owner;
			expression->type = (struct Type){ FUNCTION_TYPE, { .function = function } };
			break;
		}
		struct Expression* root = expression;
		while (root->kind == FIELD_EXPRESSION || root->kind == INDEX_EXPRESSION || root->kind == BASE_VALUE_EXPRESSION || root->kind == REFERENCE_CAST_EXPRESSION) {
			if (root->kind == FIELD_EXPRESSION) {
				root = root->data.member.object;
			}
			else if (root->kind == INDEX_EXPRESSION) {
				root = root->data.binary.left;
			}
			else {
				root = root->kind == BASE_VALUE_EXPRESSION ? root->data.base.value : root->data.operand;
			}
		}
		for (struct Scope* context = scope; context; context = context->parent) {
			if (context->initialization && root->kind == NAME_EXPRESSION && context->initialization->variable == root->data.variable.binding) {
				expression->data.member.initialization = context->initialization;
				break;
			}
		}
		break;
	}
	case ENUMERATOR_EXPRESSION:
		break;
	case ADDRESS_EXPRESSION:
		bind_expression(compiler, expression->data.operand, scope);
		expression->type = pointer_type(compiler, expression->data.operand->type);
		break;
	case DEREFERENCE_EXPRESSION:
		bind_expression(compiler, expression->data.operand, scope);
		if (expression->data.operand->type.kind != POINTER_TYPE) {
			diagnostic(compiler, expression->span.first, "dereference requires a pointer");
			break;
		}
		expression->type = *expression->data.operand->type.data.element;
		break;
	case INDEX_EXPRESSION: {
		bind_expression(compiler, expression->data.binary.left, scope);
		bind_expression(compiler, expression->data.binary.right, scope);
		struct Type base = expression->data.binary.left->type;
		if (base.kind == ARRAY_TYPE) {
			expression->type = *base.data.array.element;
		}
		else if (base.kind == POINTER_TYPE) {
			expression->type = *base.data.element;
		}
		else {
			diagnostic(compiler, expression->span.first, "indexing requires an array or pointer");
		}
		if (base.kind == ARRAY_TYPE && base.qualifier == CONST_TYPE) {
			expression->type.qualifier = CONST_TYPE;
		}
		break;
	}
	case PREFIX_UPDATE_EXPRESSION:
	case POSTFIX_UPDATE_EXPRESSION:
		bind_expression(compiler, expression->data.update.value, scope);
		expression->type = expression->data.update.value->type;
		break;
	case NOT_EXPRESSION:
		bind_expression(compiler, expression->data.operand, scope);
		expression->type = (struct Type){ BOOL_TYPE, { 0 } };
		break;
	case ACCESS_EXPRESSION: {
		struct Expression* operand = expression->data.operand;
		bool member_call = operand->kind == CALL_EXPRESSION && (operand->data.call.callee->kind == FIELD_EXPRESSION || operand->data.call.callee->kind == POINTER_FIELD_EXPRESSION);
		bind_expression(compiler, operand, scope);
		if (compiler->failed) {
			break;
		}
		if (member_call && operand->data.call.callee->kind == FUNCTION_EXPRESSION && operand->data.call.binding->parameters && token_text(operand->data.call.binding->parameters->name, "this")) {
			struct Argument* argument = operand->data.call.arguments;
			struct Expression* receiver = new_expression(compiler, ACCESS_EXPRESSION, expression->span.first);
			if (!receiver) {
				break;
			}
			receiver->data.operand = argument->expression;
			receiver->type = argument->expression->type;
			argument->expression = receiver;
			*expression = *operand;
			break;
		}
		expression->type = operand->type;
		break;
	}
	case RELOCATE_EXPRESSION:
	case ABANDON_EXPRESSION:
		bind_expression(compiler, expression->data.operand, scope);
		expression->type = expression->data.operand->type;
		if (expression->kind == RELOCATE_EXPRESSION) {
			move_expression(compiler, expression->data.operand, scope);
		}
		if (expression->kind == ABANDON_EXPRESSION) {
			expression->type = (struct Type){ VOID_TYPE, { 0 } };
		}
		break;
	case COMPLEMENT_EXPRESSION:
	case NEGATE_EXPRESSION:
		bind_expression(compiler, expression->data.operand, scope);
		expression->type = expression->data.operand->type;
		break;
	case VALUE_INITIALIZATION_EXPRESSION:
	case ASSIGNMENT_EXPRESSION:
	case BINARY_EXPRESSION:
		bind_expression(compiler, expression->data.binary.left, scope);
		bind_expression(compiler, expression->data.binary.right, scope);
		expression->type = !is_void(expression->data.binary.left->type) ? expression->data.binary.left->type : expression->data.binary.right->type;
		if (expression->kind == ASSIGNMENT_EXPRESSION && (expression->type.kind == STRUCT_TYPE || expression->type.kind == UNION_TYPE)) {
			struct Expression copy = { 0 };
			copy.kind = CONSTRUCTION_EXPRESSION;
			copy.type = expression->type;
			struct Argument source = { 0 };
			source.expression = expression->data.binary.operation == COPY_ASSIGN ? &copy : expression->data.binary.right;
			expression->assignment_function = select_special_function(compiler, expression->type.data.record, ASSIGNMENT_ROLE, &source, expression->span.first);
			if (expression->assignment_function) {
				expression->type = expression->assignment_function->result.kind == REFERENCE_TYPE ? *expression->assignment_function->result.data.element : expression->assignment_function->result;
			}
		}
		if (expression->kind == VALUE_INITIALIZATION_EXPRESSION) {
			expression->type = (struct Type){ VOID_TYPE, { 0 } };
		}
		if (expression->kind == BINARY_EXPRESSION) {
			const struct Operator* operation = find_operator(expression->data.binary.operation);
			if (operation->token == '-' && expression->data.binary.left->type.kind == POINTER_TYPE && expression->data.binary.right->type.kind == POINTER_TYPE) {
				expression->type = (struct Type){ SIGNED_INTEGER_TYPE, { sizeof(void*) * CHAR_BIT } };
			}
			if (operation->kind == COMPARISON_OPERATOR || operation->kind == LOGICAL_OPERATOR) {
				expression->type = (struct Type){ BOOL_TYPE, { 0 } };
			}
		}
		break;
	case REFERENCE_INITIALIZATION_EXPRESSION:
		bind_expression(compiler, expression->data.binary.left, scope);
		bind_expression(compiler, expression->data.binary.right, scope);
		if (compiler->failed) {
			break;
		}
		break;
	case STRING_EXPRESSION:
		expression->type = pointer_type(compiler, (struct Type){ CHAR_TYPE, { 0 } });
		break;
	case BOOLEAN_EXPRESSION:
	case NULLPTR_EXPRESSION:
	case INTEGER_EXPRESSION:
	case DECIMAL_EXPRESSION:
		break;
	default:
		diagnostic(compiler, expression->span.first, "invalid expression node");
		break;
	}
}

bool integer_constant(struct Expression* expression, uint64_t* magnitude, bool* negative) {
	if (expression->kind == INTEGER_EXPRESSION) {
		*magnitude = expression->data.integer;
		*negative = false;
		return true;
	}
	if (expression->kind == NEGATE_EXPRESSION && integer_constant(expression->data.operand, magnitude, negative)) {
		*negative = !*negative && *magnitude;
		return true;
	}
	return false;
}

bool decimal_constant(struct Expression* expression, struct Token* token, bool* negative) {
	if (expression->kind == DECIMAL_EXPRESSION) {
		*token = expression->span.first;
		*negative = false;
		return true;
	}
	if (expression->kind == NEGATE_EXPRESSION && decimal_constant(expression->data.operand, token, negative)) {
		*negative = !*negative;
		return true;
	}
	return false;
}

double decimal_value(struct Expression* expression) {
	return expression->kind == NEGATE_EXPRESSION ? -decimal_value(expression->data.operand) : expression->data.decimal;
}

void check_decimal(struct Compiler* compiler, struct Expression* expression, struct Type* expected, struct Token token, bool negative) {
	if (!expected) {
		diagnostic(compiler, expression->span.first, "decimal literal has no type context");
		return;
	}
	if (!numeric_type(*expected) || expected->kind != FLOAT_TYPE) {
		diagnostic(compiler, expression->span.first, "decimal literal requires a floating-point type");
		return;
	}
	char* text = allocate(compiler, token.length + 1);
	if (!text) {
		return;
	}
	memcpy(text, token.start, token.length);
	double value = numeric_bits(*expected) == 32 ? (double)strtof(text, NULL) : strtod(text, NULL);
	if (!isfinite(value)) {
		diagnostic(compiler, expression->span.first, "decimal literal is out of range for its type");
		return;
	}
	struct Expression* literal = expression;
	while (literal->kind == NEGATE_EXPRESSION) {
		literal->type = *expected;
		literal = literal->data.operand;
	}
	literal->type = *expected;
	literal->data.decimal = value;
	(void)negative;
	expression->type = *expected;
}

bool zero_pointer_constant(struct Expression* expression) {
	uint64_t magnitude;
	bool negative;
	return expression->kind == INTEGER_EXPRESSION && expression->span.first.kind == INTEGER && integer_constant(expression, &magnitude, &negative) && !magnitude;
}

void check_integer(struct Compiler* compiler, struct Expression* expression, struct Type* expected, uint64_t magnitude, bool negative) {
	if (!expected) {
		diagnostic(compiler, expression->span.first, "integer literal has no type context");
		return;
	}
	if (expected->kind == POINTER_TYPE && zero_pointer_constant(expression)) {
		expression->type = *expected;
		return;
	}
	if (!numeric_type(*expected) || expected->kind == FLOAT_TYPE) {
		diagnostic(compiler, expression->span.first, "integer literal requires an integer type");
		return;
	}
	if (expected->kind == BOOL_TYPE) {
		expression->type = *expected;
		return;
	}

	uint64_t limit = UINT64_MAX;
	if (!zero_extended_type(*expected)) {
		limit = (UINT64_C(1) << (numeric_bits(*expected) - 1)) - (negative ? 0 : 1);
	}
	else if (numeric_bits(*expected) < 64) {
		limit = (UINT64_C(1) << numeric_bits(*expected)) - 1;
	}
	if ((negative && zero_extended_type(*expected)) || magnitude > limit) {
		diagnostic(compiler, expression->span.first, "integer literal is out of range for its type");
	}
	expression->type = *expected;
}

bool lvalue_expression(struct Expression* expression) {
	if (expression->kind == ASSIGNMENT_EXPRESSION) {
		return !expression->assignment_function || expression->assignment_function->result.kind == REFERENCE_TYPE;
	}
	if (expression->kind == ACCESS_EXPRESSION) {
		return lvalue_expression(expression->data.operand);
	}
	if (expression->kind == BASE_VALUE_EXPRESSION) {
		return lvalue_expression(expression->data.base.value);
	}
	return expression->kind == REFERENCE_CAST_EXPRESSION || (expression->kind == CALL_EXPRESSION && expression->data.call.binding->result.kind == REFERENCE_TYPE) || expression->kind == NAME_EXPRESSION || expression->kind == DEREFERENCE_EXPRESSION || (expression->kind == INDEX_EXPRESSION && (expression->data.binary.left->type.kind == POINTER_TYPE || lvalue_expression(expression->data.binary.left))) || expression->kind == POINTER_FIELD_EXPRESSION || (expression->kind == FIELD_EXPRESSION && lvalue_expression(expression->data.member.object));
}

bool borrowed_value(struct Expression* expression) {
	if (expression->kind == FIELD_EXPRESSION || expression->kind == POINTER_FIELD_EXPRESSION || expression->kind == INDEX_EXPRESSION || expression->kind == DEREFERENCE_EXPRESSION || expression->kind == REFERENCE_CAST_EXPRESSION || expression->kind == REFERENCE_BINDING_EXPRESSION || expression->kind == BASE_VALUE_EXPRESSION) {
		return true;
	}
	if (expression->kind == CAST_EXPRESSION && !expression->data.cast.syntax) {
		return borrowed_value(expression->data.cast.value);
	}
	if (expression->kind == BASE_POINTER_EXPRESSION) {
		return borrowed_value(expression->data.base.value);
	}
	return lvalue_expression(expression);
}

bool addressable_expression(struct Expression* expression) {
	return lvalue_expression(expression) || expression->kind == FIELD_EXPRESSION || expression->kind == POINTER_FIELD_EXPRESSION || expression->kind == INDEX_EXPRESSION || expression->kind == DEREFERENCE_EXPRESSION || expression->kind == BASE_VALUE_EXPRESSION || expression->kind == REFERENCE_CAST_EXPRESSION;
}

bool expression_mutable_access(struct Expression* expression) {
	if (!expression || expression->type.qualifier == CONST_TYPE) {
		return false;
	}
	if (expression->kind == ACCESS_EXPRESSION) {
		return true;
	}
	if (expression->kind == FIELD_EXPRESSION || expression->kind == POINTER_FIELD_EXPRESSION) {
		return expression_mutable_access(expression->data.member.object);
	}
	if (expression->kind == INDEX_EXPRESSION) {
		return expression_mutable_access(expression->data.binary.left);
	}
	if (expression->kind == BASE_VALUE_EXPRESSION) {
		return expression_mutable_access(expression->data.base.value);
	}
	if (expression->kind == REFERENCE_CAST_EXPRESSION) {
		return expression_mutable_access(expression->data.operand);
	}
	return false;
}

struct Expression* storage_expression(struct Expression* expression) {
	while (expression && expression->kind == ACCESS_EXPRESSION) {
		expression = expression->data.operand;
	}
	return expression;
}

bool type_value_equal(struct Type first, struct Type second) {
	first.qualifier = MUTABLE_TYPE;
	second.qualifier = MUTABLE_TYPE;
	return type_equal(first, second);
}

void require_owned_value(struct Compiler* compiler, struct Expression* expression) {
	if (expression && borrowed_value(expression)) {
		diagnostic(compiler, expression->span.first, "owned value requires an rvalue; use '$' to relocate or construct a value");
	}
	if (expression) {
		expression->use = TRANSFERRED_VALUE;
	}
}

void construct_value(struct Compiler* compiler, struct Expression* expression, struct Type type) {
	if (!expression) {
		return;
	}
	struct Expression* source = allocate(compiler, sizeof(struct Expression));
	struct Argument* argument = allocate(compiler, sizeof(struct Argument));
	if (!source || !argument) {
		return;
	}
	*source = *expression;
	source->use = OBSERVED_VALUE;
	argument->expression = source;
	expression->kind = CONSTRUCTION_EXPRESSION;
	expression->type = type;
	expression->data.construction.function = NULL;
	expression->data.construction.arguments = argument;
	expression->data.construction.slot = NULL;
}

bool special_argument_matches(struct Compiler* compiler, struct Type parameter, struct Expression* argument) {
	struct Type value = parameter.kind == REFERENCE_TYPE ? *parameter.data.element : parameter;
	if (!type_value_equal(value, argument->type) && !(is_void(argument->type) && numeric_type(value)) && !implicit_pointer_conversion(argument->type, value) && !(value.kind == POINTER_TYPE && zero_pointer_constant(argument))) {
		return false;
	}
	if (parameter.kind != REFERENCE_TYPE) {
		return !borrowed_value(argument);
	}
	return value.qualifier == CONST_TYPE || expression_mutable_access(argument);
}

struct Function* select_special_function(struct Compiler* compiler, struct Record* record, enum FunctionRole role, struct Argument* arguments, struct Token token) {
	struct Function* selected = NULL;
	unsigned best = UINT_MAX;
	for (struct FunctionSymbol* symbol = compiler->symbols; symbol; symbol = symbol->next) {
		struct Function* function = symbol->function;
		if (function->role != role || !function->owner || function->owner->record != record) {
			continue;
		}
		struct Variable* parameter = function->parameters ? function->parameters->next : NULL;
		struct Argument* argument = arguments;
		unsigned rank = 0;
		while (parameter && argument && special_argument_matches(compiler, parameter->type, argument->expression)) {
			struct Type value = parameter->type.kind == REFERENCE_TYPE ? *parameter->type.data.element : parameter->type;
			if (!type_value_equal(value, argument->expression->type) && (implicit_pointer_conversion(argument->expression->type, value) || (value.kind == POINTER_TYPE && zero_pointer_constant(argument->expression)))) {
				rank += 2;
			}
			if (parameter->type.kind == REFERENCE_TYPE && !lvalue_expression(argument->expression)) {
				rank++;
			}
			parameter = parameter->next;
			argument = argument->next;
		}
		if (parameter || argument || rank > best) {
			continue;
		}
		if (selected && rank == best) {
			diagnostic(compiler, token, "ambiguous special function invocation");
			return NULL;
		}
		selected = function;
		best = rank;
	}
	if (selected && selected->kind == FUNCTION_DELETED) {
		diagnostic(compiler, token, "selected function is deleted");
	}
	return selected;
}

bool contains_reference_fields(struct Type type);
bool requires_union_selection(struct Type type);

bool generated_operation_trivial(struct Compiler* compiler, struct Type type, enum GeneratedOperation operation) {
	if (type.kind == ARRAY_TYPE) {
		return generated_operation_trivial(compiler, *type.data.array.element, operation);
	}
	if (type.kind != STRUCT_TYPE && type.kind != UNION_TYPE) {
		return true;
	}
	struct Record* record = type.data.record;
	if (record->callables || record->virtual_bases) {
		return false;
	}
	struct Expression value = { 0 };
	value.kind = operation == VALUE_ASSIGNMENT ? CONSTRUCTION_EXPRESSION : NAME_EXPRESSION;
	value.type = type;
	struct Argument argument = { 0 };
	argument.expression = &value;
	if (select_special_function(compiler, record, operation == VALUE_ASSIGNMENT ? ASSIGNMENT_ROLE : CONSTRUCTOR_ROLE, operation == DEFAULT_CONSTRUCTION ? NULL : &argument, record->name)) {
		return false;
	}
	for (struct BaseRelation* base = record->bases; base; base = base->next) {
		if (!generated_operation_trivial(compiler, (struct Type){ STRUCT_TYPE, { .record = base->record } }, operation)) {
			return false;
		}
	}
	for (struct Member* field = record->data.members; field; field = field->next) {
		if (!generated_operation_trivial(compiler, field->type, operation)) {
			return false;
		}
	}
	return true;
}

bool const_element_type(struct Type type) {
	if (type.qualifier == CONST_TYPE) {
		return true;
	}
	return type.kind == ARRAY_TYPE && const_element_type(*type.data.array.element);
}

void check_generated_operation(struct Compiler* compiler, struct Type type, enum GeneratedOperation operation, struct Token token);

void check_constructor_subobjects(struct Compiler* compiler, struct Record* record, struct Token token) {
	for (struct BaseRelation* base = record->bases; base && !compiler->failed; base = base->next) {
		check_generated_operation(compiler, (struct Type){ STRUCT_TYPE, { .record = base->record } }, DEFAULT_CONSTRUCTION, token);
	}
	for (struct Member* field = record->kind == STRUCT_TYPE ? record->data.members : NULL; field && !compiler->failed; field = field->next) {
		check_generated_operation(compiler, field->type, DEFAULT_CONSTRUCTION, token);
	}
}

void check_generated_operation(struct Compiler* compiler, struct Type type, enum GeneratedOperation operation, struct Token token) {
	if (compiler->failed) {
		return;
	}
	if (operation == VALUE_ASSIGNMENT && (type.qualifier == CONST_TYPE || type.kind == REFERENCE_TYPE)) {
		diagnostic(compiler, token, "generated assignment cannot assign an immutable or reference component");
		return;
	}
	if (type.kind == REFERENCE_TYPE && operation == DEFAULT_CONSTRUCTION) {
		diagnostic(compiler, token, "generated default construction cannot bind a reference");
		return;
	}
	if (type.kind == ARRAY_TYPE) {
		check_generated_operation(compiler, *type.data.array.element, operation, token);
		return;
	}
	if (type.kind != STRUCT_TYPE && type.kind != UNION_TYPE) {
		return;
	}
	struct Expression value = { 0 };
	value.kind = operation == VALUE_ASSIGNMENT ? CONSTRUCTION_EXPRESSION : NAME_EXPRESSION;
	value.type = type;
	struct Argument argument = { 0 };
	argument.expression = &value;
	struct Function* function = select_special_function(compiler, type.data.record, operation == VALUE_ASSIGNMENT ? ASSIGNMENT_ROLE : CONSTRUCTOR_ROLE, operation == DEFAULT_CONSTRUCTION ? NULL : &argument, token);
	if (function) {
		if (operation != VALUE_ASSIGNMENT && contains_reference_fields(type)) {
			diagnostic(compiler, token, "constructor does not provide a binding for its reference fields");
		}
		if (operation != VALUE_ASSIGNMENT) {
			check_constructor_subobjects(compiler, type.data.record, token);
		}
		return;
	}
	if (operation == DEFAULT_CONSTRUCTION) {
		for (struct FunctionSymbol* symbol = compiler->symbols; symbol; symbol = symbol->next) {
			struct Function* constructor = symbol->function;
			if (constructor->role == CONSTRUCTOR_ROLE && constructor->owner == type.data.record->associated) {
				diagnostic(compiler, token, "user-declared constructor suppresses implicit default construction");
				return;
			}
		}
	}
	if (type.kind == UNION_TYPE) {
		if (operation == DEFAULT_CONSTRUCTION && type.data.record->data.members) {
			struct Member* member = type.data.record->data.members;
			while (member && const_element_type(member->type)) {
				member = member->next;
			}
			if (!member) {
				diagnostic(compiler, token, "implicit union default constructor is deleted because every member is const-qualified");
				return;
			}
		}
		if (operation == DEFAULT_CONSTRUCTION && (contains_reference_fields(type) || requires_union_selection(type))) {
			diagnostic(compiler, token, "generated union construction requires an explicit member initialization");
			return;
		}
		for (struct Member* member = type.data.record->data.members; member; member = member->next) {
			if (!generated_operation_trivial(compiler, member->type, operation)) {
				diagnostic(compiler, token, "generated union operation cannot select a nontrivial member");
				return;
			}
			check_generated_operation(compiler, member->type, operation, token);
		}
		return;
	}
	for (struct BaseRelation* base = type.data.record->bases; base; base = base->next) {
		check_generated_operation(compiler, (struct Type){ STRUCT_TYPE, { .record = base->record } }, operation, token);
	}
	for (struct Member* member = type.data.record->data.members; member; member = member->next) {
		check_generated_operation(compiler, member->type, operation, token);
	}
}

enum BaseConversionRequest {
	IMPLICIT_BASE_CONVERSION,
	EXPLICIT_BASE_CONVERSION,
};

bool convert_base_expression(struct Compiler* compiler, struct Expression* expression, struct Type target, enum BaseConversionRequest request) {
	struct Type source = expression->type;
	if (type_equal(source, target)) {
		return true;
	}
	enum NodeKind kind = BASE_VALUE_EXPRESSION;
	struct Type source_object = source;
	struct Type target_object = target;
	if (source.kind == POINTER_TYPE && target.kind == POINTER_TYPE) {
		kind = BASE_POINTER_EXPRESSION;
		source_object = *source.data.element;
		target_object = *target.data.element;
	}
	if (source_object.kind != STRUCT_TYPE || target_object.kind != STRUCT_TYPE) {
		return false;
	}
	struct BaseSearch selection = find_base_path(compiler, source_object.data.record, target_object.data.record);
	enum BaseConversionKind direction = DERIVED_TO_BASE;
	if (selection.kind == NO_BASE_PATH && request == EXPLICIT_BASE_CONVERSION) {
		selection = find_base_path(compiler, target_object.data.record, source_object.data.record);
		direction = BASE_TO_DERIVED;
	}
	if (selection.kind == MULTIPLE_BASE_PATHS) {
		diagnostic(compiler, expression->span.first, "ambiguous base conversion");
		return false;
	}
	if (selection.kind != ONE_BASE_PATH) {
		return false;
	}
	if (direction == BASE_TO_DERIVED) {
		for (struct BasePath* step = selection.path; step; step = step->next) {
			if (step->relation->kind == VIRTUAL_BASE) {
				diagnostic(compiler, expression->span.first, "cannot downcast through a virtual base");
				return false;
			}
		}
	}
	struct Expression* value = allocate(compiler, sizeof(struct Expression));
	if (!value) {
		return false;
	}
	*value = *expression;
	expression->kind = kind;
	expression->data.base.value = value;
	expression->data.base.path = selection.path;
	expression->data.base.kind = direction;
	expression->type = target;
	return true;
}

bool contains_reference_fields(struct Type type);
bool type_has_views(struct Type type);
bool initialization_path_equal(struct Compiler* compiler, struct Expression* left, struct Expression* right);
bool initialization_contains_path(struct Compiler* compiler, struct Expression* expression, struct Expression* path);

bool owned_union_initialization(struct Expression* expression) {
	if (expression->kind != FIELD_EXPRESSION || !expression->data.member.binding || expression->data.member.binding->owner->kind != UNION_TYPE || !expression->data.member.initialization) {
		return false;
	}
	struct InitializationContext* context = expression->data.member.initialization;
	if (context->type.kind == REFERENCE_TYPE) {
		return false;
	}
	struct Expression* root = expression;
	while (root->kind != NAME_EXPRESSION) {
		if (root->kind == FIELD_EXPRESSION) {
			root = root->data.member.object;
		}
		else if (root->kind == INDEX_EXPRESSION && root->data.binary.left->type.kind == ARRAY_TYPE) {
			root = root->data.binary.left;
		}
		else if (root->kind == BASE_VALUE_EXPRESSION) {
			root = root->data.base.value;
		}
		else {
			return false;
		}
	}
	return root->data.variable.binding == context->variable;
}

void validate_reference_field_read(struct Compiler* compiler, struct Expression* expression) {
	struct InitializationContext* context = expression->data.member.initialization;
	if (!context || expression->data.member.binding->type.kind != REFERENCE_TYPE) {
		return;
	}
	struct Expression* root = expression;
	while (root->kind == FIELD_EXPRESSION || root->kind == INDEX_EXPRESSION || root->kind == BASE_VALUE_EXPRESSION || root->kind == REFERENCE_CAST_EXPRESSION) {
		if (root->kind == FIELD_EXPRESSION) {
			root = root->data.member.object;
		}
		else if (root->kind == INDEX_EXPRESSION) {
			root = root->data.binary.left;
		}
		else {
			root = root->kind == BASE_VALUE_EXPRESSION ? root->data.base.value : root->data.operand;
		}
		if (root->kind == FIELD_EXPRESSION && root->data.member.binding->type.kind == REFERENCE_TYPE) {
			return;
		}
	}
	if (root->kind != NAME_EXPRESSION || root->data.variable.binding != context->variable) {
		return;
	}
	if (context->type.kind == REFERENCE_TYPE) {
		return;
	}
	if (context->initializer) {
		return;
	}
	for (struct Statement* statement = context->body->data.block; statement; statement = statement->next) {
		if (statement->kind != EXPRESSION_STATEMENT) {
			continue;
		}
		struct Expression* binding = statement->data.expression;
		if (binding->kind == REFERENCE_INITIALIZATION_EXPRESSION && binding->type.kind == REFERENCE_TYPE && initialization_path_equal(compiler, binding->data.binary.left, expression)) {
			return;
		}
		if (binding->kind == VALUE_INITIALIZATION_EXPRESSION && !is_void(binding->type) && initialization_contains_path(compiler, expression, binding->data.binary.left)) {
			return;
		}
	}

	diagnostic(compiler, expression->span.first, "reference field used before binding");
}

void check_expression(struct Compiler* compiler, struct Expression* expression, struct Type* expected) {
	if (!expression || compiler->failed) {
		return;
	}
	if (expected && expected->kind == REFERENCE_TYPE) {
		if (expected->data.element->qualifier != CONST_TYPE && !expression_mutable_access(expression) && expression->kind != FUNCTION_EXPRESSION) {
			diagnostic(compiler, expression->span.first, "mutable reference binding requires explicit '@' access");
			return;
		}
		convert_base_expression(compiler, expression, *expected->data.element, IMPLICIT_BASE_CONVERSION);
		check_expression(compiler, expression, expected->data.element);
		if (compiler->failed) {
			return;
		}
		if (!lvalue_expression(expression) && expression->kind != FUNCTION_EXPRESSION && expected->data.element->qualifier != CONST_TYPE) {
			diagnostic(compiler, expression->span.first, "reference binding requires an existing object");
			return;
		}

		struct Expression* referent = allocate(compiler, sizeof(struct Expression));
		if (!referent) {
			return;
		}
		*referent = *expression;
		expression->kind = REFERENCE_BINDING_EXPRESSION;
		expression->data.operand = referent;
		expression->type = *expected;
		return;
	}

	uint64_t magnitude;
	bool negative;
	if (integer_constant(expression, &magnitude, &negative)) {
		check_integer(compiler, expression, expected, magnitude, negative);
		return;
	}
	struct Token decimal;
	if (decimal_constant(expression, &decimal, &negative)) {
		struct Type context = expected && expected->kind == BOOL_TYPE ? (struct Type){ FLOAT_TYPE, { 64 } } : expected ? *expected : (struct Type){ 0 };
		check_decimal(compiler, expression, expected ? &context : NULL, decimal, negative);
		if (!expected || expected->kind != BOOL_TYPE || compiler->failed) {
			return;
		}
		goto contextual_conversion;
	}
	switch (expression->kind) {
	case ACCESS_EXPRESSION:
		check_expression(compiler, expression->data.operand, NULL);
		if (!lvalue_expression(expression->data.operand) || expression->data.operand->type.qualifier == CONST_TYPE) {
			diagnostic(compiler, expression->span.first, "'@' requires mutable storage");
		}
		break;
	case RELOCATE_EXPRESSION:
		check_expression(compiler, expression->data.operand, NULL);
		expression->type = expression->data.operand->type;
		if (expression->data.operand->kind != NAME_EXPRESSION || expression->data.operand->data.variable.binding->type.kind == REFERENCE_TYPE) {
			diagnostic(compiler, expression->span.first, "relocation requires an owned local binding");
		}
		break;
	case ABANDON_EXPRESSION:
		if (expression->data.operand->kind != NAME_EXPRESSION || !expression->data.operand->data.variable.binding || expression->data.operand->data.variable.binding->type.kind == REFERENCE_TYPE) {
			diagnostic(compiler, expression->span.first, "discard requires an owned local identifier");
		}
		expression->type = (struct Type){ VOID_TYPE, { 0 } };
		break;
	case CONSTRUCTION_EXPRESSION: {
		struct Argument* argument = expression->data.construction.arguments;
		if (unsized_type(expression->type) || expression->type.kind == FUNCTION_TYPE || expression->type.kind == REFERENCE_TYPE) {
			diagnostic(compiler, expression->span.first, "construction requires an object type");
			break;
		}
		if (expression->type.kind == STRUCT_TYPE || expression->type.kind == UNION_TYPE) {
			complete_type(compiler, expression->type, expression->span.first);
			expression->data.construction.function = select_special_function(compiler, expression->type.data.record, CONSTRUCTOR_ROLE, argument, expression->span.first);
		}
		struct Function* function = expression->data.construction.function;
		if (function) {
			check_constructor_subobjects(compiler, expression->type.data.record, expression->span.first);
			if (compiler->failed) {
				break;
			}
			if (contains_reference_fields(expression->type)) {
				diagnostic(compiler, expression->span.first, "constructor does not provide a binding for its reference fields");
				break;
			}
			struct Variable* parameter = function->parameters->next;
			for (; argument && parameter; argument = argument->next, parameter = parameter->next) {
				check_expression(compiler, argument->expression, &parameter->type);
				if (parameter->type.kind != REFERENCE_TYPE) {
					require_owned_value(compiler, argument->expression);
				}
				argument->passed_type = parameter->type;
			}
			break;
		}
		check_generated_operation(compiler, expression->type, argument ? COPY_CONSTRUCTION : DEFAULT_CONSTRUCTION, expression->span.first);
		if (argument) {
			if (argument->next) {
				diagnostic(compiler, expression->span.first, "no matching constructor");
				break;
			}
			check_expression(compiler, argument->expression, &expression->type);
		}
		break;
	}
	case BASE_VALUE_EXPRESSION:
	case BASE_POINTER_EXPRESSION:
		check_expression(compiler, expression->data.base.value, NULL);
		break;
	case REFERENCE_INITIALIZATION_EXPRESSION: {
		struct Expression* target = storage_expression(expression->data.binary.left);
		if (!expression_mutable_access(expression->data.binary.left)) {
			diagnostic(compiler, target->span.first, "reference initialization target requires explicit '@' access");
			break;
		}
		if (target->kind != NAME_EXPRESSION) {
			check_expression(compiler, target->data.member.object, NULL);
		}
		struct Type reference = target->kind == NAME_EXPRESSION ? target->data.variable.binding->type : target->data.member.binding->type;
		check_expression(compiler, expression->data.binary.right, &reference);
		if (!compiler->failed) {
			expression->type = reference;
		}
		break;
	}
	case VALUE_INITIALIZATION_EXPRESSION:
	case ASSIGNMENT_EXPRESSION: {
		struct Expression* left = expression->data.binary.left;
		check_expression(compiler, left, NULL);
		struct Expression* right = expression->data.binary.right;
		if (expression->data.binary.operation == COPY_ASSIGN) {
			construct_value(compiler, right, left->type);
		}
		if (expression->kind == ASSIGNMENT_EXPRESSION && (left->type.kind == STRUCT_TYPE || left->type.kind == UNION_TYPE)) {
			struct Argument argument = { 0 };
			argument.expression = right;
			struct Function* function = select_special_function(compiler, left->type.data.record, ASSIGNMENT_ROLE, &argument, expression->span.first);
			if (function) {
				if (!expression_mutable_access(left)) {
					diagnostic(compiler, left->span.first, "assignment target requires explicit '@' access");
					break;
				}
				require_owned_value(compiler, right);
				if (function->parameters->next->type.kind != REFERENCE_TYPE) {
					check_expression(compiler, right, &function->parameters->next->type);
					require_owned_value(compiler, right);
				}
				else {
					right->use = OBSERVED_VALUE;
					check_expression(compiler, right, &function->parameters->next->type);
				}
				expression->assignment_function = function;
				expression->type = function->result.kind == REFERENCE_TYPE ? *function->result.data.element : function->result;
				break;
			}
		}
		if (owned_union_initialization(storage_expression(left)) && !contains_reference_fields(left->type) && !find_operator(expression->data.binary.operation)->compound) {
			expression->kind = VALUE_INITIALIZATION_EXPRESSION;
		}
		if (!expression_mutable_access(left)) {
			diagnostic(compiler, left->span.first, "assignment target requires explicit '@' access");
			break;
		}
		if (!lvalue_expression(left) || left->type.kind == ARRAY_TYPE) {
			diagnostic(compiler, expression->span.first, "assignment requires an assignable non-array expression");
			break;
		}
		const struct Operator* operation = find_operator(expression->data.binary.operation);
		struct Type right_type = left->type;
		if (operation->compound && left->type.kind == POINTER_TYPE) {
			if (operation->compound != '+' && operation->compound != '-') {
				diagnostic(compiler, expression->span.first, "unsupported pointer assignment operator");
				break;
			}
			complete_type(compiler, *left->type.data.element, expression->span.first);
			right_type = expression->data.binary.right->type;
			if (is_void(right_type)) {
				right_type = (struct Type){ SIGNED_INTEGER_TYPE, { sizeof(void*) * CHAR_BIT } };
			}
		}
		else if (operation->compound && (!numeric_type(left->type) || (left->type.kind == FLOAT_TYPE && (operation->compound == '%' || operation->compound == '&' || operation->compound == '|' || operation->compound == '^' || operation->compound == SHIFT_LEFT || operation->compound == SHIFT_RIGHT)))) {
			diagnostic(compiler, expression->span.first, "compound assignment requires suitable numeric operands");
			break;
		}
		check_expression(compiler, expression->data.binary.right, &right_type);
		if (!operation->compound) {
			require_owned_value(compiler, expression->data.binary.right);
			if (expression->kind == ASSIGNMENT_EXPRESSION) {
				check_generated_operation(compiler, left->type, VALUE_ASSIGNMENT, expression->span.first);
			}
		}
		if (left->type.kind == POINTER_TYPE && operation->compound && right_type.kind != SIGNED_INTEGER_TYPE && right_type.kind != UNSIGNED_INTEGER_TYPE && right_type.kind != CHAR_TYPE) {
			diagnostic(compiler, expression->span.first, "pointer arithmetic requires an integer offset");
		}
		expression->type = left->type;
		break;
	}
	case NOT_EXPRESSION: {
		struct Type context = expression->data.operand->type;
		if (is_void(context)) {
			context = (struct Type){ SIGNED_INTEGER_TYPE, { 32 } };
		}
		check_expression(compiler, expression->data.operand, &context);
		if (!numeric_type(context) && context.kind != POINTER_TYPE && context.kind != ENUM_TYPE) {
			diagnostic(compiler, expression->span.first, "logical operation requires a scalar operand");
		}
		expression->type = (struct Type){ BOOL_TYPE, { 0 } };
		break;
	}
	case COMPLEMENT_EXPRESSION:
		check_expression(compiler, expression->data.operand, expected);
		expression->type = expression->data.operand->type;
		if (expression->type.kind != SIGNED_INTEGER_TYPE && expression->type.kind != UNSIGNED_INTEGER_TYPE && expression->type.kind != CHAR_TYPE && expression->type.kind != BOOL_TYPE && expression->type.kind != ENUM_TYPE) {
			diagnostic(compiler, expression->span.first, "bitwise complement requires an integer");
		}
		break;
	case PREFIX_UPDATE_EXPRESSION:
	case POSTFIX_UPDATE_EXPRESSION:
		check_expression(compiler, expression->data.update.value, NULL);
		if (!expression_mutable_access(expression->data.update.value)) {
			diagnostic(compiler, expression->span.first, "update target requires explicit '@' access");
		}
		expression->type = expression->data.update.value->type;
		if (!lvalue_expression(expression->data.update.value) || (!numeric_type(expression->type) && expression->type.kind != POINTER_TYPE)) {
			diagnostic(compiler, expression->span.first, "increment or decrement requires an assignable scalar");
		}
		if (expression->type.kind == POINTER_TYPE) {
			complete_type(compiler, *expression->type.data.element, expression->span.first);
		}
		break;
	case BINARY_EXPRESSION: {
		const struct Operator* operation = find_operator(expression->data.binary.operation);
		struct Expression* left = expression->data.binary.left;
		struct Expression* right = expression->data.binary.right;
		struct Type type = !is_void(left->type) ? left->type : right->type;
		if (operation->kind == COMPARISON_OPERATOR && is_null_pointer_type(left->type) && right->type.kind == POINTER_TYPE) {
			type = right->type;
		}
		if ((operation->kind == COMPARISON_OPERATOR || operation->token == '-') && left->type.kind == POINTER_TYPE && right->type.kind == POINTER_TYPE && left->type.data.element->kind == STRUCT_TYPE && right->type.data.element->kind == STRUCT_TYPE) {
			struct BaseSearch common = find_base_path(compiler, left->type.data.element->data.record, right->type.data.element->data.record);
			if (common.kind == MULTIPLE_BASE_PATHS) {
				diagnostic(compiler, expression->span.first, "ambiguous pointer base conversion");
				break;
			}
			if (common.kind == ONE_BASE_PATH) {
				type = right->type;
			}
		}
		if (is_void(type) && expected && operation->kind != COMPARISON_OPERATOR && operation->kind != LOGICAL_OPERATOR) {
			type = *expected;
		}
		if (operation->kind == LOGICAL_OPERATOR) {
			struct Type first = left->type, second = right->type;
			if (is_void(first)) {
				first = (struct Type){ SIGNED_INTEGER_TYPE, { 32 } };
			}
			if (is_void(second)) {
				second = (struct Type){ SIGNED_INTEGER_TYPE, { 32 } };
			}
			check_expression(compiler, left, &first);
			check_expression(compiler, right, &second);
			if ((!numeric_type(first) && first.kind != POINTER_TYPE && first.kind != ENUM_TYPE) || (!numeric_type(second) && second.kind != POINTER_TYPE && second.kind != ENUM_TYPE)) {
				diagnostic(compiler, expression->span.first, "logical operations require scalar operands");
			}
			expression->type = (struct Type){ BOOL_TYPE, { 0 } };
			break;
		}
		if (type.kind == POINTER_TYPE && (operation->token == '+' || operation->token == '-')) {
			if (left->type.kind != POINTER_TYPE) {
				diagnostic(compiler, expression->span.first, "pointer arithmetic requires a pointer on the left");
				break;
			}
			complete_type(compiler, *type.data.element, expression->span.first);
			check_expression(compiler, left, &type);
			if (right->type.kind == POINTER_TYPE && operation->token == '-') {
				check_expression(compiler, right, &type);
				expression->type = (struct Type){ SIGNED_INTEGER_TYPE, { sizeof(void*) * CHAR_BIT } };
			}
			else {
				struct Type offset = right->type;
				if (is_void(offset)) {
					offset = (struct Type){ SIGNED_INTEGER_TYPE, { sizeof(void*) * CHAR_BIT } };
				}
				if (offset.kind != SIGNED_INTEGER_TYPE && offset.kind != UNSIGNED_INTEGER_TYPE && offset.kind != CHAR_TYPE) {
					diagnostic(compiler, expression->span.first, "pointer offset requires integer type");
					break;
				}
				check_expression(compiler, right, &offset);
				expression->type = type;
			}
			break;
		}
		if (is_void(type) && operation->kind == COMPARISON_OPERATOR) {
			type = (struct Type){ SIGNED_INTEGER_TYPE, { 32 } };
		}
		if (!numeric_type(type) && type.kind != ENUM_TYPE && !(type.kind == POINTER_TYPE && operation->kind == COMPARISON_OPERATOR)) {
			diagnostic(compiler, expression->span.first, "operation requires a numeric type context or typed operands");
			break;
		}
		if ((operation->kind == BITWISE_OPERATOR || operation->kind == SHIFT_OPERATOR || operation->token == '%') && type.kind == FLOAT_TYPE) {
			diagnostic(compiler, expression->span.first, "operator requires integer operands");
			break;
		}
		check_expression(compiler, left, &type);
		check_expression(compiler, right, &type);
		expression->type = operation->kind == COMPARISON_OPERATOR ? (struct Type){ BOOL_TYPE, { 0 } } : type;
		break;
	}
	case CAST_EXPRESSION: {
		struct Expression* value = expression->data.cast.value;
		struct Type source = value->type;
		struct Type target = expression->type;
		if (is_void(source)) {
			source = target;
		}
		check_expression(compiler, value, &source);
		if (source.kind == POINTER_TYPE && target.kind == POINTER_TYPE) {
			convert_base_expression(compiler, value, target, EXPLICIT_BASE_CONVERSION);
			source = value->type;
		}
		if ((source.kind == FLOAT_TYPE && target.kind == POINTER_TYPE) || (source.kind == POINTER_TYPE && target.kind == FLOAT_TYPE)) {
			diagnostic(compiler, expression->span.first, "cannot cast between floating-point and pointer types");
		}
		if (!is_void(target) && ((!numeric_type(source) && source.kind != POINTER_TYPE) || (!numeric_type(target) && target.kind != POINTER_TYPE))) {
			diagnostic(compiler, expression->span.first, "cast requires scalar types");
		}
		break;
	}
	case REFERENCE_CAST_EXPRESSION:
		convert_base_expression(compiler, expression->data.operand, expression->type, EXPLICIT_BASE_CONVERSION);
		check_expression(compiler, expression->data.operand, &expression->type);
		if (!lvalue_expression(expression->data.operand) && expression->data.operand->kind != FUNCTION_EXPRESSION) {
			diagnostic(compiler, expression->span.first, "reference cast requires an existing object of the referent type");
		}
		break;
	case SIZEOF_VALUE_EXPRESSION:
		check_expression(compiler, expression->data.size.operand.value, NULL);
		break;
	case SIZEOF_TYPE_EXPRESSION:
		break;
	case FIELD_EXPRESSION:
	case POINTER_FIELD_EXPRESSION:
		check_expression(compiler, expression->data.member.object, NULL);
		validate_reference_field_read(compiler, expression);
		break;
	case ENUMERATOR_EXPRESSION:
		break;
	case ADDRESS_EXPRESSION:
		check_expression(compiler, expression->data.operand, NULL);
		if (!lvalue_expression(expression->data.operand) && expression->data.operand->kind != FUNCTION_EXPRESSION) {
			diagnostic(compiler, expression->span.first, "address-of requires an assignable expression");
		}
		break;
	case DEREFERENCE_EXPRESSION:
		check_expression(compiler, expression->data.operand, NULL);
		if (expression->type.kind != FUNCTION_TYPE) {
			complete_type(compiler, expression->type, expression->span.first);
		}
		break;
	case INDEX_EXPRESSION: {
		check_expression(compiler, expression->data.binary.left, NULL);
		complete_type(compiler, expression->type, expression->span.first);
		struct Type index = expression->data.binary.right->type;
		if (is_void(index)) {
			index = (struct Type){ UNSIGNED_INTEGER_TYPE, { sizeof(void*) * CHAR_BIT } };
		}
		if (index.kind != SIGNED_INTEGER_TYPE && index.kind != UNSIGNED_INTEGER_TYPE && index.kind != CHAR_TYPE && index.kind != BOOL_TYPE && index.kind != ENUM_TYPE) {
			diagnostic(compiler, expression->span.first, "index must have integer type");
			break;
		}
		check_expression(compiler, expression->data.binary.right, &index);
		break;
	}
	case NEGATE_EXPRESSION:
		check_expression(compiler, expression->data.operand, expected);
		expression->type = expression->data.operand->type;
		if (!numeric_type(expression->type) || numeric_bits(expression->type) <= 1 || expression->type.kind == UNSIGNED_INTEGER_TYPE) {
			diagnostic(compiler, expression->span.first, "unary minus requires a signed integer or floating-point operand");
		}
		break;
	case CALL_EXPRESSION: {
		check_expression(compiler, expression->data.call.callee, NULL);
		check_expression(compiler, expression->data.call.receiver_evaluation, NULL);
		struct Variable* parameter = expression->data.call.binding->parameters;
		for (struct Argument* argument = expression->data.call.arguments; argument && !compiler->failed; argument = argument->next) {
			if (parameter) {
				check_expression(compiler, argument->initializer, &parameter->type);
				check_expression(compiler, argument->expression, &parameter->type);
				if (parameter->type.kind != REFERENCE_TYPE && !argument->initialization) {
					require_owned_value(compiler, argument->expression);
				}
				argument->passed_type = parameter->type;
				parameter = parameter->next;
			}
			else if (expression->data.call.binding->arity == VARIADIC_PARAMETERS) {
				check_expression(compiler, argument->expression, NULL);
				require_owned_value(compiler, argument->expression);
				argument->passed_type = argument->expression->type;
				if (argument->passed_type.kind == STRUCT_TYPE || argument->passed_type.kind == UNION_TYPE || argument->passed_type.kind == ARRAY_TYPE) {
					diagnostic(compiler, argument->expression->span.first, "C variadic aggregate arguments are not supported");
				}
				if (is_void(argument->passed_type)) {
					diagnostic(compiler, argument->expression->span.first, "variadic argument cannot be void");
				}
				if (numeric_type(argument->passed_type) && argument->passed_type.kind == FLOAT_TYPE && numeric_bits(argument->passed_type) == 32) {
					argument->passed_type = (struct Type){ FLOAT_TYPE, { 64 } };
				}
				else if (numeric_type(argument->passed_type) && argument->passed_type.kind != FLOAT_TYPE && numeric_bits(argument->passed_type) < 32) {
					argument->passed_type = (struct Type){ SIGNED_INTEGER_TYPE, { 32 } };
				}
			}
			else {
				diagnostic(compiler, expression->span.first, "too many function arguments");
			}
		}
		if (parameter) {
			diagnostic(compiler, expression->span.first, "incorrect function argument count");
		}
		break;
	}
	case BOOLEAN_EXPRESSION:
	case NULLPTR_EXPRESSION:
	case STRING_EXPRESSION:
	case NAME_EXPRESSION:
	case FUNCTION_EXPRESSION:
		if (expression->kind == FUNCTION_EXPRESSION && expression->data.function->kind == FUNCTION_DELETED) {
			diagnostic(compiler, expression->span.first, "use of deleted function");
			break;
		}
		if (expression->kind == NAME_EXPRESSION && expression->data.variable.initialization && expression->data.variable.initialization->type.kind == REFERENCE_TYPE) {
			struct InitializationContext* context = expression->data.variable.initialization;
			struct Statement* statement = context->body->data.block;
			while (statement) {
				struct Expression* binding = statement->kind == EXPRESSION_STATEMENT ? statement->data.expression : NULL;
				if (binding && binding->kind == REFERENCE_INITIALIZATION_EXPRESSION && binding->type.kind == REFERENCE_TYPE && binding->data.binary.left->kind == NAME_EXPRESSION && binding->data.binary.left->data.variable.binding == expression->data.variable.binding) {
					break;
				}
				statement = statement->next;
			}
			if (!statement) {
				diagnostic(compiler, expression->span.first, "reference used before binding");
			}
		}
		break;
	case BOUND_FUNCTION_EXPRESSION:
		diagnostic(compiler, expression->span.first, "receiver-bound function must be called; take the qualified function address instead");
		break;
	default:
		diagnostic(compiler, expression->span.first, "invalid expression node");
		break;
	}
contextual_conversion:
	if (expected && !type_value_equal(expression->type, *expected) && implicit_pointer_conversion(expression->type, *expected)) {
		struct Expression* source = allocate(compiler, sizeof(struct Expression));
		if (!source) {
			return;
		}
		*source = *expression;
		expression->kind = CAST_EXPRESSION;
		expression->data.cast.value = source;
		expression->type = *expected;
	}
	if (expected && expected->kind == POINTER_TYPE && !type_equal(expression->type, *expected)) {
		convert_base_expression(compiler, expression, *expected, IMPLICIT_BASE_CONVERSION);
	}
	if (expected && !type_equal(expression->type, *expected) && ((expected->kind == BOOL_TYPE && (numeric_type(expression->type) || expression->type.kind == POINTER_TYPE)) || (expression->type.kind == BOOL_TYPE && numeric_type(*expected)))) {
		struct Expression* source = allocate(compiler, sizeof(struct Expression));
		if (!source) {
			return;
		}

		*source = *expression;
		expression->kind = CAST_EXPRESSION;
		expression->data.cast.value = source;
		expression->type = *expected;
	}

	if (expected && !type_value_equal(expression->type, *expected)) {
		diagnostic(compiler, expression->span.first, "expression type does not match its context");
	}
}

uint64_t integer_mask(struct Type type) {
	int bits = numeric_bits(type);
	return bits == 64 ? UINT64_MAX : (UINT64_C(1) << bits) - 1;
}

int64_t signed_constant(uint64_t value, int bits) {
	uint64_t sign = UINT64_C(1) << (bits - 1);
	uint64_t mask = bits == 64 ? UINT64_MAX : (UINT64_C(1) << bits) - 1;
	value &= mask;
	if (!(value & sign)) {
		return (int64_t)value;
	}
	return -1 - (int64_t)((~value) & mask);
}

bool case_integer(struct Compiler* compiler, struct Expression* expression, struct Type context, uint64_t* result) {
	uint64_t mask = integer_mask(context);
	uint64_t left, right;
	switch (expression->kind) {
	case BOOLEAN_EXPRESSION:
		*result = expression->data.integer;
		return true;
	case INTEGER_EXPRESSION:
		*result = expression->data.integer & mask;
		return true;
	case NAME_EXPRESSION: {
		struct Enumerator* value = find_enumerator(compiler, expression->data.variable.name);
		if (value && value->state == RESOLVED_ENUMERATOR) {
			*result = (uint64_t)value->value & mask;
			return true;
		}
		break;
	}
	case NEGATE_EXPRESSION:
		if (!case_integer(compiler, expression->data.operand, context, &left)) {
			return false;
		}
		*result = (0 - left) & mask;
		return true;
	case COMPLEMENT_EXPRESSION:
		if (!case_integer(compiler, expression->data.operand, context, &left)) {
			return false;
		}
		*result = ~left & mask;
		return true;
	case CAST_EXPRESSION: {
		struct Type type = expression->data.cast.syntax ? resolve_type(compiler, expression->data.cast.syntax) : expression->type;
		if (type.kind != SIGNED_INTEGER_TYPE && type.kind != UNSIGNED_INTEGER_TYPE && type.kind != CHAR_TYPE && type.kind != BOOL_TYPE && type.kind != ENUM_TYPE) {
			break;
		}
		if (!case_integer(compiler, expression->data.cast.value, type, &left)) {
			return false;
		}
		*result = (zero_extended_type(type) ? left : (uint64_t)signed_constant(left, numeric_bits(type))) & mask;
		return true;
	}
	case SIZEOF_TYPE_EXPRESSION: {
		struct Type type = resolve_type(compiler, expression->data.size.operand.syntax);
		complete_type(compiler, type, expression->span.first);
		if (compiler->failed) {
			return false;
		}
		*result = LLVMABISizeOfType(compiler->layout, llvm_type(compiler, type)) & mask;
		return true;
	}
	case BINARY_EXPRESSION:
		if (!case_integer(compiler, expression->data.binary.left, context, &left) || !case_integer(compiler, expression->data.binary.right, context, &right)) {
			return false;
		}
		switch (expression->data.binary.operation) {
		case '+':
			*result = (left + right) & mask;
			return true;
		case '-':
			*result = (left - right) & mask;
			return true;
		case '*':
			*result = (left * right) & mask;
			return true;
		case '&':
			*result = left & right;
			return true;
		case '|':
			*result = left | right;
			return true;
		case '^':
			*result = left ^ right;
			return true;
		case SHIFT_LEFT:
		case SHIFT_RIGHT:
			if (right >= (uint64_t)numeric_bits(context)) {
				diagnostic(compiler, expression->span.first, "shift count is out of range in constant expression");
				return false;
			}
			if (expression->data.binary.operation == SHIFT_LEFT) {
				*result = (left << right) & mask;
			}
			else if (zero_extended_type(context) || !(left & (UINT64_C(1) << (numeric_bits(context) - 1)))) {
				*result = left >> right;
			}
			else {
				*result = ((left >> right) | (mask ^ (mask >> right))) & mask;
			}
			return true;
		case '/':
		case '%':
			if (!right) {
				diagnostic(compiler, expression->span.first, "division by zero in constant expression");
				return false;
			}
			if (zero_extended_type(context)) {
				*result = expression->data.binary.operation == '/' ? left / right : left % right;
			}
			else {
				int64_t first = signed_constant(left, numeric_bits(context)), second = signed_constant(right, numeric_bits(context));
				if (first == INT64_MIN && second == -1) {
					diagnostic(compiler, expression->span.first, "division overflows in constant expression");
					return false;
				}
				*result = (uint64_t)(expression->data.binary.operation == '/' ? first / second : first % second) & mask;
			}
			return true;
		default:
			break;
		}
		break;
	default:
		break;
	}
	diagnostic(compiler, expression->span.first, "case requires an integer constant expression");
	return false;
}

void check_condition(struct Compiler* compiler, struct Expression* expression, struct Scope* scope) {
	bind_expression(compiler, expression, scope);
	struct Type type = expression->type;
	if (is_void(type)) {
		type = (struct Type){ SIGNED_INTEGER_TYPE, { 32 } };
	}
	check_expression(compiler, expression, &type);
	if (!numeric_type(type) && type.kind != ENUM_TYPE && type.kind != POINTER_TYPE) {
		diagnostic(compiler, expression->span.first, "condition requires a scalar expression");
	}
}

unsigned analyze_statements(struct Compiler* compiler, struct Function* function, struct Statement* statements, struct Scope* scope, struct Flow* flow);

enum StorageStepKind {
	BASE_STORAGE_STEP,
	FIELD_STORAGE_STEP,
	INDEX_STORAGE_STEP,
};

struct StorageStep {
	enum StorageStepKind kind;
	union {
		struct BaseRelation* base;
		struct Member* field;
		uint64_t index;
	} data;
	struct StorageStep* next;
};

struct StoragePath {
	struct Expression* root;
	struct StorageStep* steps;
};

void storage_step_append(struct Compiler* compiler, struct StoragePath* path, struct StorageStep step) {
	struct StorageStep* stored = allocate(compiler, sizeof(struct StorageStep));
	if (!stored) {
		return;
	}
	*stored = step;
	struct StorageStep** tail = &path->steps;
	while (*tail) {
		tail = &(*tail)->next;
	}
	*tail = stored;
}

struct StoragePath initialization_storage_path(struct Compiler* compiler, struct Expression* expression) {
	struct StoragePath path = { 0 };
	if (!expression) {
		return path;
	}
	if (expression->kind == ACCESS_EXPRESSION) {
		return initialization_storage_path(compiler, expression->data.operand);
	}
	if (expression->kind == NAME_EXPRESSION) {
		path.root = expression;
		return path;
	}
	if (expression->kind == REFERENCE_CAST_EXPRESSION) {
		return initialization_storage_path(compiler, expression->data.operand);
	}
	struct BasePath* bases = NULL;
	if (expression->kind == BASE_VALUE_EXPRESSION && expression->data.base.kind == DERIVED_TO_BASE) {
		path = initialization_storage_path(compiler, expression->data.base.value);
		bases = expression->data.base.path;
	}
	else if (expression->kind == FIELD_EXPRESSION && expression->data.member.binding) {
		path = initialization_storage_path(compiler, expression->data.member.object);
		bases = expression->data.member.base_path;
	}
	else if (expression->kind == INDEX_EXPRESSION) {
		path = initialization_storage_path(compiler, expression->data.binary.left);
		uint64_t index;
		if (!case_integer(compiler, expression->data.binary.right, (struct Type){ UNSIGNED_INTEGER_TYPE, { sizeof(void*) * CHAR_BIT } }, &index)) {
			path.root = NULL;
			return path;
		}
		storage_step_append(compiler, &path, (struct StorageStep){ INDEX_STORAGE_STEP, { .index = index }, NULL });
		return path;
	}
	else {
		return path;
	}
	for (; bases; bases = bases->next) {
		storage_step_append(compiler, &path, (struct StorageStep){ BASE_STORAGE_STEP, { .base = bases->relation }, NULL });
	}
	if (expression->kind == FIELD_EXPRESSION) {
		storage_step_append(compiler, &path, (struct StorageStep){ FIELD_STORAGE_STEP, { .field = expression->data.member.binding }, NULL });
	}
	return path;
}

bool initialization_path_equal(struct Compiler* compiler, struct Expression* left, struct Expression* right) {
	struct StoragePath first = initialization_storage_path(compiler, left);
	struct StoragePath second = initialization_storage_path(compiler, right);
	if (!first.root || !second.root || !name_equal(first.root->data.variable.name, second.root->data.variable.name)) {
		return false;
	}
	if (first.root->data.variable.binding && second.root->data.variable.binding && first.root->data.variable.binding != second.root->data.variable.binding) {
		return false;
	}
	struct StorageStep* a = first.steps;
	struct StorageStep* b = second.steps;
	struct Type storage_type = first.root->data.variable.binding ? first.root->data.variable.binding->type : first.root->type;
	for (; a && b; a = a->next, b = b->next) {
		if (a->kind != b->kind) {
			return false;
		}
		switch (a->kind) {
		case BASE_STORAGE_STEP:
			if (storage_type.kind != STRUCT_TYPE) {
				return false;
			}
			struct Subobject* first_object = storage_type.data.record->subobjects;
			struct Subobject* second_object = first_object;
			while (a && a->kind == BASE_STORAGE_STEP) {
				struct BasePath step = { a->data.base, NULL };
				first_object = path_subobject(first_object, &step);
				if (!a->next || a->next->kind != BASE_STORAGE_STEP) {
					break;
				}
				a = a->next;
			}
			while (b && b->kind == BASE_STORAGE_STEP) {
				struct BasePath step = { b->data.base, NULL };
				second_object = path_subobject(second_object, &step);
				if (!b->next || b->next->kind != BASE_STORAGE_STEP) {
					break;
				}
				b = b->next;
			}
			if (!first_object || first_object != second_object) {
				return false;
			}
			storage_type = (struct Type){ STRUCT_TYPE, { .record = first_object->record } };
			break;
		case FIELD_STORAGE_STEP:
			if (a->data.field != b->data.field) {
				return false;
			}
			storage_type = a->data.field->type;
			break;
		case INDEX_STORAGE_STEP:
			if (storage_type.kind == ARRAY_TYPE) {
				storage_type = *storage_type.data.array.element;
			}
			if (a->data.index != b->data.index) {
				return false;
			}
			break;
		}
	}
	return !a && !b;
}

bool normalize_initialization_storage(struct Compiler* compiler, struct Expression* expression, struct Variable* owner) {
	if (!expression) {
		return false;
	}
	if (expression->kind == ACCESS_EXPRESSION) {
		if (!normalize_initialization_storage(compiler, expression->data.operand, owner)) {
			return false;
		}
		expression->type = expression->data.operand->type;
		return true;
	}
	if (expression->kind == NAME_EXPRESSION) {
		if (!name_equal(expression->data.variable.name, owner->name)) {
			return false;
		}
		expression->data.variable.binding = owner;
		expression->type = owner->type.kind == REFERENCE_TYPE ? *owner->type.data.element : owner->type;
		return true;
	}
	if (expression->kind == CAST_EXPRESSION) {
		struct Expression* value = expression->data.cast.value;
		if (!normalize_initialization_storage(compiler, value, owner)) {
			return false;
		}
		struct Type target = resolve_type(compiler, expression->data.cast.syntax);
		if (target.kind != REFERENCE_TYPE || target.data.element->kind != STRUCT_TYPE) {
			return false;
		}
		if (!convert_base_expression(compiler, value, *target.data.element, EXPLICIT_BASE_CONVERSION)) {
			return false;
		}
		*expression = *value;
		return true;
	}
	if (expression->kind == INDEX_EXPRESSION) {
		if (!normalize_initialization_storage(compiler, expression->data.binary.left, owner) || expression->data.binary.left->type.kind != ARRAY_TYPE) {
			return false;
		}
		expression->type = *expression->data.binary.left->type.data.array.element;
		return true;
	}
	if (expression->kind != FIELD_EXPRESSION || !normalize_initialization_storage(compiler, expression->data.member.object, owner)) {
		return false;
	}
	struct Type object = expression->data.member.object->type;
	if (object.kind != STRUCT_TYPE && object.kind != UNION_TYPE) {
		return false;
	}
	struct Record* qualified_owner = NULL;
	struct MemberLookup member = qualified_record_member(compiler, object.data.record, expression->data.member.name, &qualified_owner);
	if (member.kind == AMBIGUOUS_MEMBER) {
		diagnostic(compiler, expression->span.first, "ambiguous inherited field initialization");
		return false;
	}
	if (member.kind != FIELD_MEMBER) {
		return false;
	}
	expression->data.member.binding = member.entity.field;
	expression->data.member.base_path = member.path;
	expression->type = member.entity.field->type.kind == REFERENCE_TYPE ? *member.entity.field->type.data.element : member.entity.field->type;
	return true;
}

bool contains_reference_fields(struct Type type) {
	if (type.kind == REFERENCE_TYPE) {
		return true;
	}
	if (type.kind == ARRAY_TYPE) {
		return contains_reference_fields(*type.data.array.element);
	}
	if (type.kind == STRUCT_TYPE || type.kind == UNION_TYPE) {
		for (struct BaseRelation* base = type.data.record->bases; base; base = base->next) {
			if (contains_reference_fields((struct Type){ STRUCT_TYPE, { .record = base->record } })) {
				return true;
			}
		}
		for (struct Member* member = type.data.record->data.members; member; member = member->next) {
			if (contains_reference_fields(member->type)) {
				return true;
			}
		}
	}
	return false;
}

bool initialization_contains_path(struct Compiler* compiler, struct Expression* expression, struct Expression* path) {
	while (expression) {
		if (initialization_path_equal(compiler, expression, path)) {
			return true;
		}
		if (expression->kind == FIELD_EXPRESSION) {
			expression = expression->data.member.object;
		}
		else if (expression->kind == ACCESS_EXPRESSION) {
			expression = expression->data.operand;
		}
		else if (expression->kind == INDEX_EXPRESSION) {
			expression = expression->data.binary.left;
		}
		else if (expression->kind == BASE_VALUE_EXPRESSION) {
			expression = expression->data.base.value;
		}
		else {
			break;
		}
	}
	return false;
}

struct Member* initialized_union_member(struct Compiler* compiler, struct Expression* path, struct Statement* initialization) {
	for (struct Statement* statement = initialization ? initialization->data.block : NULL; statement; statement = statement->next) {
		struct Expression* expression = statement->kind == EXPRESSION_STATEMENT ? statement->data.expression : NULL;
		if (!expression || (expression->kind != ASSIGNMENT_EXPRESSION && expression->kind != VALUE_INITIALIZATION_EXPRESSION && expression->kind != REFERENCE_INITIALIZATION_EXPRESSION)) {
			continue;
		}
		struct Expression* target = expression->data.binary.left;
		while (target) {
			if (target->kind == ACCESS_EXPRESSION) {
				target = target->data.operand;
				continue;
			}
			if (target->kind == FIELD_EXPRESSION) {
				if (initialization_path_equal(compiler, target->data.member.object, path)) {
					return target->data.member.binding;
				}
				target = target->data.member.object;
			}
			else if (target->kind == INDEX_EXPRESSION) {
				target = target->data.binary.left;
			}
			else if (target->kind == BASE_VALUE_EXPRESSION) {
				target = target->data.base.value;
			}
			else {
				break;
			}
		}
	}
	return NULL;
}

bool prepare_whole_reference_path(struct Compiler* compiler, struct Expression* path, struct Statement* initialization) {
	for (struct Statement* statement = initialization ? initialization->data.block : NULL; statement; statement = statement->next) {
		struct Expression* expression = statement->kind == EXPRESSION_STATEMENT ? statement->data.expression : NULL;
		if (expression && expression->kind == ASSIGNMENT_EXPRESSION && !find_operator(expression->data.binary.operation)->compound && initialization_path_equal(compiler, expression->data.binary.left, path)) {
			expression->kind = VALUE_INITIALIZATION_EXPRESSION;
			return true;
		}
	}
	return false;
}

struct InitializedSubobject {
	struct Subobject* object;
	struct InitializedSubobject* next;
};

void prepare_reference_path(struct Compiler* compiler, struct Type type, struct Expression* path, struct Statement* initialization) {
	path->type = type;
	if (compiler->failed || !contains_reference_fields(type)) {
		return;
	}
	if (type.kind == STRUCT_TYPE || type.kind == UNION_TYPE) {
		if (prepare_whole_reference_path(compiler, path, initialization)) {
			return;
		}
	}
	if (type.kind == ARRAY_TYPE) {
		for (uint64_t index = 0; index < type.data.array.count; index++) {
			struct Expression* element = new_expression(compiler, INDEX_EXPRESSION, path->span.first);
			struct Expression* subscript = new_expression(compiler, INTEGER_EXPRESSION, path->span.first);
			if (!element || !subscript) {
				return;
			}
			subscript->data.integer = index;
			element->data.binary.left = path;
			element->data.binary.right = subscript;
			prepare_reference_path(compiler, *type.data.array.element, element, initialization);
		}
		return;
	}
	if (type.kind == STRUCT_TYPE || type.kind == UNION_TYPE) {
		struct Member* selected = type.kind == UNION_TYPE ? initialized_union_member(compiler, path, initialization) : NULL;
		struct InitializedSubobject* initialized = NULL;
		for (struct Subobject* object = type.data.record->subobjects; object; object = object->next) {
			if (!object->path) {
				continue;
			}
			struct Expression projection = { 0 };
			projection.kind = BASE_VALUE_EXPRESSION;
			projection.span = path->span;
			projection.data.base.value = path;
			projection.data.base.path = object->path;
			projection.type = (struct Type){ STRUCT_TYPE, { .record = object->record } };
			if (!prepare_whole_reference_path(compiler, &projection, initialization)) {
				continue;
			}
			struct InitializedSubobject* complete = allocate(compiler, sizeof(struct InitializedSubobject));
			if (!complete) {
				return;
			}
			complete->object = object;
			complete->next = initialized;
			initialized = complete;
		}
		for (struct Subobject* object = type.data.record->subobjects; object; object = object->next) {
			struct InitializedSubobject* complete = initialized;
			while (complete && !subobject_contains(complete->object, object)) {
				complete = complete->next;
			}
			if (complete) {
				continue;
			}
			struct Expression* projection = path;
			if (object->path) {
				projection = new_expression(compiler, BASE_VALUE_EXPRESSION, path->span.first);
				if (!projection) {
					return;
				}
				projection->data.base.value = path;
				projection->data.base.path = object->path;
				projection->type = (struct Type){ STRUCT_TYPE, { .record = object->record } };
			}
			for (struct Member* member = object->record->data.members; member; member = member->next) {
				if (type.kind == UNION_TYPE && member != selected) {
					continue;
				}
				struct Expression* field = new_expression(compiler, FIELD_EXPRESSION, member->name);
				if (!field) {
					return;
				}
				field->data.member.object = projection;
				field->data.member.name = member->name;
				field->data.member.binding = member;
				prepare_reference_path(compiler, member->type, field, initialization);
			}
		}
		return;
	}
	if (type.kind != REFERENCE_TYPE) {
		return;
	}

	struct Expression* binding = NULL;
	for (struct Statement* statement = initialization ? initialization->data.block : NULL; statement; statement = statement->next) {
		if (statement->kind != EXPRESSION_STATEMENT) {
			continue;
		}
		struct Expression* expression = statement->data.expression;
		if ((expression->kind != ASSIGNMENT_EXPRESSION && expression->kind != REFERENCE_INITIALIZATION_EXPRESSION) || expression->data.binary.operation != '=') {
			continue;
		}
		if (initialization_path_equal(compiler, expression->data.binary.left, path)) {
			binding = expression;
			break;
		}
	}
	if (!binding) {
		diagnostic(compiler, path->span.first, "reference field must be bound in the initialization block");
		return;
	}
	if (initialization_path_equal(compiler, binding->data.binary.right, path)) {
		diagnostic(compiler, path->span.first, "reference cannot bind to itself");
		return;
	}

	binding->kind = REFERENCE_INITIALIZATION_EXPRESSION;
}

void prepare_reference_fields(struct Compiler* compiler, struct Variable* variable, struct Statement* initialization) {
	for (struct Statement* statement = initialization ? initialization->data.block : NULL; statement; statement = statement->next) {
		struct Expression* expression = statement->kind == EXPRESSION_STATEMENT ? statement->data.expression : NULL;
		if (expression && expression->kind == ASSIGNMENT_EXPRESSION) {
			normalize_initialization_storage(compiler, expression->data.binary.left, variable);
		}
	}
	struct Expression* path = new_expression(compiler, NAME_EXPRESSION, variable->name);
	if (!path) {
		return;
	}
	path->data.variable.name = variable->name;
	path->data.variable.binding = variable;
	prepare_reference_path(compiler, variable->type, path, initialization);
}

bool requires_union_selection(struct Type type) {
	if (type.kind == ARRAY_TYPE) {
		return requires_union_selection(*type.data.array.element);
	}
	if (type.kind != STRUCT_TYPE && type.kind != UNION_TYPE) {
		return false;
	}
	if (type.kind == UNION_TYPE && (type_has_views(type) || contains_reference_fields(type))) {
		return true;
	}
	for (struct Subobject* object = type.data.record->subobjects; object; object = object->next) {
		for (struct Member* field = object->record->data.members; field; field = field->next) {
			if (requires_union_selection(field->type)) {
				return true;
			}
		}
	}
	return false;
}

unsigned analyze_statement(struct Compiler* compiler, struct Function* function, struct Statement* statement, struct Scope* scope, struct Flow* flow) {
	unsigned exits = NORMAL_EXIT;
	switch (statement->kind) {
	case EMPTY_STATEMENT:
		break;
	case BLOCK_STATEMENT: {
		struct Scope child = { NULL, scope };
		exits = analyze_statements(compiler, function, statement->data.block, &child, flow);
		break;
	}
	case DECLARATION_STATEMENT: {
		struct Variable* variable = statement->data.declaration.variable;
		variable->type = resolve_type(compiler, variable->syntax);
		if (variable->syntax) {
			complete_type(compiler, variable->type, variable->name);
		}
		struct ScopeBinding* existing = scope_local(scope, variable->name);
		variable->scope_depth = 0;
		for (struct Scope* owner = scope; owner; owner = owner->parent) {
			variable->scope_depth++;
		}
		if (variable->type.kind != REFERENCE_TYPE && contains_reference_fields(variable->type) && statement->data.declaration.initialization) {
			struct Statement* body = statement->data.declaration.initialization;
			struct Statement* first = body->data.block;
			struct Expression* binding = first && first->kind == EXPRESSION_STATEMENT ? first->data.expression : NULL;
			if (binding && binding->kind == ASSIGNMENT_EXPRESSION && (binding->data.binary.operation == '=' || binding->data.binary.operation == COPY_ASSIGN) && binding->data.binary.left->kind == NAME_EXPRESSION && name_equal(binding->data.binary.left->data.variable.name, variable->name)) {
				statement->data.declaration.initializer = binding->data.binary.right;
				statement->data.declaration.operation = binding->data.binary.operation;
				body->data.block = first->next;
			}
		}
		if (variable->type.kind == REFERENCE_TYPE && !statement->data.declaration.initializer && !statement->data.declaration.initialization) {
			diagnostic(compiler, variable->name, "reference must be bound when declared");
			break;
		}
		if (variable->type.kind != REFERENCE_TYPE) {
			scope_declare(compiler, scope, variable);
		}
		bind_expression(compiler, statement->data.declaration.initializer, scope);
		if (variable->type.kind != REFERENCE_TYPE) {
			if (statement->data.declaration.operation == COPY_ASSIGN) {
				construct_value(compiler, statement->data.declaration.initializer, variable->syntax ? variable->type : statement->data.declaration.initializer->type);
			}
		}
		else if (statement->data.declaration.initializer && !lvalue_expression(statement->data.declaration.initializer)) {
			diagnostic(compiler, variable->name, "local references cannot bind temporaries");
		}
		check_expression(compiler, statement->data.declaration.initializer, variable->syntax ? &variable->type : NULL);
		if (variable->type.kind != REFERENCE_TYPE) {
			require_owned_value(compiler, statement->data.declaration.initializer);
		}
		if (variable->type.kind == REFERENCE_TYPE) {
			scope_declare(compiler, scope, variable);
		}
		if (!statement->data.declaration.initializer && !statement->data.declaration.initialization && variable->type.kind != REFERENCE_TYPE) {
			struct Expression* construction = new_expression(compiler, CONSTRUCTION_EXPRESSION, variable->name);
			if (construction) {
				construction->type = variable->type;
				construction->use = TRANSFERRED_VALUE;
				statement->data.declaration.initializer = construction;
				check_expression(compiler, construction, &variable->type);
			}
		}
		if (!variable->syntax && statement->data.declaration.initializer) {
			variable->type = statement->data.declaration.initializer->type;
		}
		if (unsized_type(variable->type)) {
			diagnostic(compiler, variable->name, "variable cannot have an unsized type");
		}
		if (variable->type.kind == FUNCTION_TYPE) {
			diagnostic(compiler, variable->name, "function entities require explicit address-of to obtain a pointer");
		}
		if (!statement->data.declaration.initializer) {
			if (!statement->data.declaration.initialization && requires_union_selection(variable->type)) {
				diagnostic(compiler, variable->name, "union initialization must explicitly select a member requiring runtime views");
				break;
			}
			prepare_reference_fields(compiler, variable, statement->data.declaration.initialization);
		}
		if (statement->data.declaration.initialization) {
			struct InitializationContext* context = allocate(compiler, sizeof(struct InitializationContext));
			if (!context) {
				break;
			}
			*context = (struct InitializationContext){ variable, variable->type, statement->data.declaration.initialization, statement->data.declaration.initializer };
			struct Scope initialization_scope = { NULL, scope, context };
			exits = analyze_statement(compiler, function, statement->data.declaration.initialization, &initialization_scope, flow);
		}
		break;
	}
	case EXPRESSION_STATEMENT:
		bind_expression(compiler, statement->data.expression, scope);
		check_expression(compiler, statement->data.expression, NULL);
		break;
	case RETURN_STATEMENT:
		if (!statement->data.expression) {
			if (!is_void(function->result)) {
				diagnostic(compiler, statement->span.first, "non-void function must return a value");
			}
		}
		else if (is_void(function->result)) {
			diagnostic(compiler, statement->span.first, "void function cannot return a value");
		}
		else {
			bind_expression(compiler, statement->data.expression, scope);
			struct Expression* value = statement->data.expression;
			if (function->result.kind != REFERENCE_TYPE && value->kind == NAME_EXPRESSION && value->data.variable.binding && value->data.variable.binding->scope_depth && value->data.variable.binding->type.kind != REFERENCE_TYPE) {
				struct Expression* relocation = new_expression(compiler, RELOCATE_EXPRESSION, value->span.first);
				if (!relocation) {
					break;
				}
				relocation->data.operand = value;
				relocation->type = value->type;
				statement->data.expression = relocation;
			}
			check_expression(compiler, statement->data.expression, &function->result);
			if (function->result.kind != REFERENCE_TYPE) {
				require_owned_value(compiler, statement->data.expression);
			}
		}
		exits = RETURN_EXIT;
		break;
	case IF_STATEMENT: {
		check_condition(compiler, statement->data.branch.condition, scope);
		struct Scope* first_snapshot = scope_snapshot(compiler, scope);
		struct Scope* second_snapshot = scope_snapshot(compiler, scope);
		if (compiler->failed) {
			break;
		}
		struct Scope first = { NULL, first_snapshot }, second = { NULL, second_snapshot };
		exits = analyze_statement(compiler, function, statement->data.branch.first, &first, flow);
		exits |= statement->data.branch.second ? analyze_statement(compiler, function, statement->data.branch.second, &second, flow) : NORMAL_EXIT;
		scope_merge(scope, first_snapshot);
		scope_merge(scope, second_snapshot);
		break;
	}
	case WHILE_STATEMENT:
	case FOR_STATEMENT: {
		struct Scope child = { NULL, scope };
		struct Flow loop = { LOOP_FLOW, flow, NULL, NULL };
		if (statement->data.loop.initializer) {
			analyze_statement(compiler, function, statement->data.loop.initializer, &child, &loop);
		}
		if (statement->data.loop.condition) {
			check_condition(compiler, statement->data.loop.condition, &child);
		}
		struct Scope body = { NULL, &child };
		unsigned body_exits = analyze_statement(compiler, function, statement->data.loop.body, &body, &loop);
		if (statement->data.loop.step) {
			bind_expression(compiler, statement->data.loop.step, &child);
			check_expression(compiler, statement->data.loop.step, NULL);
		}
		uint64_t magnitude = 0;
		bool negative = false;
		unsigned infinite = !statement->data.loop.condition || (integer_constant(statement->data.loop.condition, &magnitude, &negative) && magnitude);
		exits = (body_exits & RETURN_EXIT) | (!infinite || (body_exits & BREAK_EXIT) ? NORMAL_EXIT : 0);
		break;
	}
	case BREAK_STATEMENT:
		if (!flow) {
			diagnostic(compiler, statement->span.first, "break requires a loop or switch");
		}
		exits = BREAK_EXIT;
		break;
	case CONTINUE_STATEMENT: {
		struct Flow* loop = flow;
		while (loop && loop->kind != LOOP_FLOW) {
			loop = loop->parent;
		}
		if (!loop) {
			diagnostic(compiler, statement->span.first, "continue requires a loop");
		}
		exits = CONTINUE_EXIT;
		break;
	}
	case SWITCH_STATEMENT: {
		bind_expression(compiler, statement->data.selection.value, scope);
		struct Type type = statement->data.selection.value->type;
		if (is_void(type)) {
			type = (struct Type){ SIGNED_INTEGER_TYPE, { 32 } };
		}
		check_expression(compiler, statement->data.selection.value, &type);
		if (type.kind != SIGNED_INTEGER_TYPE && type.kind != UNSIGNED_INTEGER_TYPE && type.kind != CHAR_TYPE && type.kind != BOOL_TYPE && type.kind != ENUM_TYPE) {
			diagnostic(compiler, statement->span.first, "switch requires an integer or enum");
			break;
		}
		struct Scope child = { NULL, scope };
		struct Flow selection = { SWITCH_FLOW, flow, NULL, NULL };
		struct Scope* removals = scope_snapshot(compiler, scope);
		struct Case* default_case = NULL;
		for (struct Case* item = statement->data.selection.cases; item && !compiler->failed; item = item->next) {
			if (item->value) {
				if (!case_integer(compiler, item->value, type, &item->constant)) {
					break;
				}
				for (struct Case* previous = statement->data.selection.cases; previous != item; previous = previous->next) {
					if (previous->value && previous->constant == item->constant) {
						diagnostic(compiler, item->span.first, "duplicate switch case");
					}
				}
			}
			else {
				if (default_case) {
					diagnostic(compiler, item->span.first, "duplicate default case");
				}
				default_case = item;
			}
			struct Scope* snapshot = scope_snapshot(compiler, scope);
			if (compiler->failed) {
				break;
			}
			child.parent = snapshot;
			item->exits = analyze_statements(compiler, function, item->statements, &child, &selection);
			scope_merge(removals, snapshot);
		}
		scope_merge(scope, removals);
		exits = default_case ? 0 : NORMAL_EXIT;
		// Cases can fall through into every following case.
		for (struct Case* item = statement->data.selection.cases; item; item = item->next) {
			unsigned branch_exits = item->exits;
			for (struct Case* later = item->next; later && (branch_exits & NORMAL_EXIT); later = later->next) {
				branch_exits = (branch_exits & ~NORMAL_EXIT) | later->exits;
			}
			exits |= branch_exits;
		}
		if (exits & BREAK_EXIT) {
			exits = (exits & ~BREAK_EXIT) | NORMAL_EXIT;
		}
		break;
	}
	default:
		diagnostic(compiler, statement->span.first, "invalid statement node");
		break;
	}
	statement->exits = exits;
	return exits;
}

unsigned analyze_statements(struct Compiler* compiler, struct Function* function, struct Statement* statements, struct Scope* scope, struct Flow* flow) {
	unsigned exits = NORMAL_EXIT;
	for (struct Statement* statement = statements; statement && !compiler->failed; statement = statement->next) {
		if (statement->kind == EMPTY_STATEMENT) {
			continue;
		}
		if (!(exits & NORMAL_EXIT)) {
			diagnostic(compiler, statement->span.first, "unreachable statement");
			break;
		}
		exits = (exits & ~NORMAL_EXIT) | analyze_statement(compiler, function, statement, scope, flow);
	}
	return exits;
}

struct LifetimeObject* lifetime_find(struct LifetimeState* state, struct Variable* variable) {
	for (struct LifetimeObject* object = state->objects; object; object = object->next) {
		if (object->variable == variable) {
			return object;
		}
	}
	return NULL;
}

struct LifetimeState lifetime_clone(struct Compiler* compiler, struct LifetimeState* state) {
	struct LifetimeState copy = { state->reachability, NULL };
	struct LifetimeObject** tail = &copy.objects;
	for (struct LifetimeObject* object = state->objects; object; object = object->next) {
		struct LifetimeObject* item = allocate(compiler, sizeof(struct LifetimeObject));
		if (!item) {
			break;
		}
		item->variable = object->variable;
		item->status = object->status;
		*tail = item;
		tail = &item->next;
	}
	return copy;
}

void lifetime_merge(struct Compiler* compiler, struct LifetimeState* destination, struct LifetimeState* incoming, struct Token token) {
	if (incoming->reachability == UNREACHABLE_STATE) {
		return;
	}
	if (destination->reachability == UNREACHABLE_STATE) {
		*destination = lifetime_clone(compiler, incoming);
		return;
	}
	for (struct LifetimeObject* object = destination->objects; object; object = object->next) {
		struct LifetimeObject* other = lifetime_find(incoming, object->variable);
		if ((!other && object->status != DEAD_OBJECT) || (other && object->status != other->status)) {
			diagnostic(compiler, token, "inconsistent object lifetimes at reachable control-flow merge");
			return;
		}
	}
	for (struct LifetimeObject* object = incoming->objects; object; object = object->next) {
		if (object->status != DEAD_OBJECT && !lifetime_find(destination, object->variable)) {
			diagnostic(compiler, token, "control flow bypasses a live object declaration");
			return;
		}
	}
}

void lifetime_enter(struct LifetimeAnalysis* analysis, struct Variable* variable) {
	for (struct LifetimeObject* object = analysis->state.objects; object; object = object->next) {
		if (object->status != DEAD_OBJECT && object->variable->scope_depth == variable->scope_depth && name_equal(object->variable->name, variable->name)) {
			diagnostic(analysis->compiler, variable->name, "duplicate live variable declaration");
			return;
		}
	}
	struct LifetimeObject* object = allocate(analysis->compiler, sizeof(struct LifetimeObject));
	if (!object) {
		return;
	}
	object->variable = variable;
	object->status = ALIVE_OBJECT;
	object->next = analysis->state.objects;
	analysis->state.objects = object;
}

void lifetime_trim(struct LifetimeState* state, struct LifetimeObject* boundary) {
	while (state->objects && (!boundary || state->objects->variable != boundary->variable)) {
		state->objects = state->objects->next;
	}
}

struct LifetimeCleanup* lifetime_cleanups(struct LifetimeAnalysis* analysis, struct LifetimeObject* boundary) {
	struct LifetimeCleanup* result = NULL;
	struct LifetimeCleanup** tail = &result;
	for (struct LifetimeObject* object = analysis->state.objects; object && (!boundary || object->variable != boundary->variable); object = object->next) {
		if (object->status != ALIVE_OBJECT || object->variable->type.kind == REFERENCE_TYPE) {
			continue;
		}
		struct LifetimeCleanup* cleanup = allocate(analysis->compiler, sizeof(struct LifetimeCleanup));
		if (!cleanup) {
			break;
		}
		cleanup->variable = object->variable;
		*tail = cleanup;
		tail = &cleanup->next;
	}
	return result;
}

int lifetime_truth(struct Expression* expression) {
	if (!expression) {
		return 1;
	}
	uint64_t magnitude;
	bool negative;
	if (integer_constant(expression, &magnitude, &negative)) {
		return !!magnitude;
	}
	if (expression->kind == BOOLEAN_EXPRESSION) {
		return !!expression->data.integer;
	}
	if (expression->kind == NULLPTR_EXPRESSION) {
		return 0;
	}
	if (expression->kind == NOT_EXPRESSION) {
		int truth = lifetime_truth(expression->data.operand);
		return truth < 0 ? -1 : !truth;
	}
	if (expression->kind == CAST_EXPRESSION) {
		return lifetime_truth(expression->data.cast.value);
	}
	return -1;
}

void lifetime_expression(struct LifetimeAnalysis* analysis, struct Expression* expression);
void lifetime_statement(struct LifetimeAnalysis* analysis, struct Statement* statement);

void lifetime_statements(struct LifetimeAnalysis* analysis, struct Statement* statements) {
	for (struct Statement* statement = statements; statement && !analysis->compiler->failed; statement = statement->next) {
		lifetime_statement(analysis, statement);
	}
}

void lifetime_expression(struct LifetimeAnalysis* analysis, struct Expression* expression) {
	if (!expression || analysis->state.reachability == UNREACHABLE_STATE || analysis->compiler->failed) {
		return;
	}
	switch (expression->kind) {
	case NAME_EXPRESSION: {
		struct LifetimeObject* object = lifetime_find(&analysis->state, expression->data.variable.binding);
		if (object && object->status == DEAD_OBJECT) {
			diagnostic(analysis->compiler, expression->span.first, "object was relocated; declare a new binding before using it");
		}
		break;
	}
	case RELOCATE_EXPRESSION: {
		lifetime_expression(analysis, expression->data.operand);
		struct Variable* variable = expression->data.operand->kind == NAME_EXPRESSION ? expression->data.operand->data.variable.binding : NULL;
		struct LifetimeObject* object = lifetime_find(&analysis->state, variable);
		if (!object || variable->type.kind == REFERENCE_TYPE) {
			diagnostic(analysis->compiler, expression->span.first, "relocation requires an owned local binding");
			return;
		}
		object->status = DEAD_OBJECT;
		break;
	}
	case ABANDON_EXPRESSION: {
		lifetime_expression(analysis, expression->data.operand);
		struct LifetimeObject* object = lifetime_find(&analysis->state, expression->data.operand->data.variable.binding);
		if (object) {
			object->status = DEAD_OBJECT;
		}
		break;
	}
	case ACCESS_EXPRESSION:
	case NEGATE_EXPRESSION:
	case NOT_EXPRESSION:
	case COMPLEMENT_EXPRESSION:
	case ADDRESS_EXPRESSION:
	case DEREFERENCE_EXPRESSION:
	case REFERENCE_CAST_EXPRESSION:
	case REFERENCE_BINDING_EXPRESSION:
		lifetime_expression(analysis, expression->data.operand);
		break;
	case CAST_EXPRESSION:
		lifetime_expression(analysis, expression->data.cast.value);
		break;
	case BASE_VALUE_EXPRESSION:
	case BASE_POINTER_EXPRESSION:
		lifetime_expression(analysis, expression->data.base.value);
		break;
	case PREFIX_UPDATE_EXPRESSION:
	case POSTFIX_UPDATE_EXPRESSION:
		lifetime_expression(analysis, expression->data.update.value);
		break;
	case FIELD_EXPRESSION:
	case POINTER_FIELD_EXPRESSION:
		lifetime_expression(analysis, expression->data.member.object);
		break;
	case BOUND_FUNCTION_EXPRESSION:
		lifetime_expression(analysis, expression->data.bound_function.receiver);
		break;
	case ASSIGNMENT_EXPRESSION:
	case VALUE_INITIALIZATION_EXPRESSION:
	case REFERENCE_INITIALIZATION_EXPRESSION:
	case INDEX_EXPRESSION:
	case BINARY_EXPRESSION: {
		lifetime_expression(analysis, expression->data.binary.left);
		int operation = expression->data.binary.operation;
		if (expression->kind == BINARY_EXPRESSION && (operation == LOGICAL_AND || operation == LOGICAL_OR)) {
			int truth = lifetime_truth(expression->data.binary.left);
			if ((operation == LOGICAL_AND && !truth) || (operation == LOGICAL_OR && truth == 1)) {
				break;
			}
			struct LifetimeState skipped = lifetime_clone(analysis->compiler, &analysis->state);
			lifetime_expression(analysis, expression->data.binary.right);
			if (truth < 0) {
				lifetime_merge(analysis->compiler, &analysis->state, &skipped, expression->span.first);
			}
			break;
		}
		lifetime_expression(analysis, expression->data.binary.right);
		break;
	}
	case CALL_EXPRESSION:
		lifetime_expression(analysis, expression->data.call.callee);
		lifetime_expression(analysis, expression->data.call.receiver_evaluation);
		for (struct Argument* argument = expression->data.call.arguments; argument; argument = argument->next) {
			lifetime_expression(analysis, argument->initializer);
			if (argument->variable) {
				struct LifetimeObject* boundary = analysis->state.objects;
				lifetime_enter(analysis, argument->variable);
				lifetime_statement(analysis, argument->initialization);
				lifetime_trim(&analysis->state, boundary);
			}
			else {
				lifetime_expression(analysis, argument->expression);
			}
		}
		break;
	case CONSTRUCTION_EXPRESSION:
		for (struct Argument* argument = expression->data.construction.arguments; argument; argument = argument->next) {
			lifetime_expression(analysis, argument->expression);
		}
		break;
	default:
		break;
	}
}

void lifetime_nested_statement(struct LifetimeAnalysis* analysis, struct Statement* statement) {
	if (!statement) {
		return;
	}
	struct LifetimeObject* boundary = analysis->state.objects;
	lifetime_statement(analysis, statement);
	if (analysis->state.reachability == REACHABLE_STATE && statement->kind != BLOCK_STATEMENT) {
		statement->cleanup = lifetime_cleanups(analysis, boundary);
	}
	lifetime_trim(&analysis->state, boundary);
}

void lifetime_loop(struct LifetimeAnalysis* analysis, struct Statement* statement) {
	struct LifetimeObject* boundary = analysis->state.objects;
	lifetime_statement(analysis, statement->data.loop.initializer);
	struct LifetimeState entry = lifetime_clone(analysis->compiler, &analysis->state);
	lifetime_expression(analysis, statement->data.loop.condition);
	int truth = lifetime_truth(statement->data.loop.condition);
	struct LifetimeState exits = { 0 };
	if (truth != 1) {
		exits = lifetime_clone(analysis->compiler, &analysis->state);
	}
	struct LifetimeControl control = { LOOP_FLOW, analysis->state.objects, { 0 }, { 0 }, analysis->control };
	analysis->control = &control;
	if (!truth) {
		analysis->state.reachability = UNREACHABLE_STATE;
	}
	lifetime_nested_statement(analysis, statement->data.loop.body);
	lifetime_merge(analysis->compiler, &analysis->state, &control.continues, statement->span.first);
	lifetime_expression(analysis, statement->data.loop.step);
	lifetime_merge(analysis->compiler, &entry, &analysis->state, statement->span.first);
	lifetime_merge(analysis->compiler, &exits, &control.breaks, statement->span.first);
	analysis->state = exits;
	analysis->control = control.parent;
	if (analysis->state.reachability == REACHABLE_STATE) {
		statement->cleanup = lifetime_cleanups(analysis, boundary);
	}
	lifetime_trim(&analysis->state, boundary);
}

void lifetime_statement(struct LifetimeAnalysis* analysis, struct Statement* statement) {
	if (!statement || analysis->state.reachability == UNREACHABLE_STATE || analysis->compiler->failed) {
		return;
	}
	switch (statement->kind) {
	case BLOCK_STATEMENT: {
		struct LifetimeObject* boundary = analysis->state.objects;
		lifetime_statements(analysis, statement->data.block);
		if (analysis->state.reachability == REACHABLE_STATE) {
			statement->cleanup = lifetime_cleanups(analysis, boundary);
		}
		lifetime_trim(&analysis->state, boundary);
		break;
	}
	case DECLARATION_STATEMENT:
		lifetime_expression(analysis, statement->data.declaration.initializer);
		lifetime_enter(analysis, statement->data.declaration.variable);
		lifetime_statement(analysis, statement->data.declaration.initialization);
		break;
	case EXPRESSION_STATEMENT:
		lifetime_expression(analysis, statement->data.expression);
		break;
	case RETURN_STATEMENT:
		lifetime_expression(analysis, statement->data.expression);
		statement->cleanup = lifetime_cleanups(analysis, NULL);
		analysis->state.reachability = UNREACHABLE_STATE;
		break;
	case IF_STATEMENT: {
		lifetime_expression(analysis, statement->data.branch.condition);
		int truth = lifetime_truth(statement->data.branch.condition);
		struct LifetimeState incoming = lifetime_clone(analysis->compiler, &analysis->state);
		if (!truth) {
			analysis->state.reachability = UNREACHABLE_STATE;
		}
		lifetime_nested_statement(analysis, statement->data.branch.first);
		struct LifetimeState first = analysis->state;
		analysis->state = incoming;
		if (truth == 1) {
			analysis->state.reachability = UNREACHABLE_STATE;
		}
		lifetime_nested_statement(analysis, statement->data.branch.second);
		lifetime_merge(analysis->compiler, &analysis->state, &first, statement->span.first);
		break;
	}
	case WHILE_STATEMENT:
	case FOR_STATEMENT:
		lifetime_loop(analysis, statement);
		break;
	case BREAK_STATEMENT:
	case CONTINUE_STATEMENT: {
		struct LifetimeControl* control = analysis->control;
		while (control && statement->kind == CONTINUE_STATEMENT && control->kind != LOOP_FLOW) {
			control = control->parent;
		}
		if (!control) {
			break;
		}
		statement->cleanup = lifetime_cleanups(analysis, control->boundary);
		lifetime_trim(&analysis->state, control->boundary);
		lifetime_merge(analysis->compiler, statement->kind == BREAK_STATEMENT ? &control->breaks : &control->continues, &analysis->state, statement->span.first);
		analysis->state.reachability = UNREACHABLE_STATE;
		break;
	}
	case SWITCH_STATEMENT: {
		lifetime_expression(analysis, statement->data.selection.value);
		struct LifetimeState dispatch = lifetime_clone(analysis->compiler, &analysis->state);
		struct LifetimeControl control = { SWITCH_FLOW, analysis->state.objects, { 0 }, { 0 }, analysis->control };
		analysis->control = &control;
		analysis->state.reachability = UNREACHABLE_STATE;
		unsigned defaults = 0;
		struct Case* last = NULL;
		for (struct Case* item = statement->data.selection.cases; item; item = item->next) {
			lifetime_merge(analysis->compiler, &analysis->state, &dispatch, item->span.first);
			defaults += !item->value;
			lifetime_statements(analysis, item->statements);
			last = item;
		}
		if (last && analysis->state.reachability == REACHABLE_STATE) {
			last->cleanup = lifetime_cleanups(analysis, control.boundary);
		}
		lifetime_trim(&analysis->state, control.boundary);
		lifetime_merge(analysis->compiler, &analysis->state, &control.breaks, statement->span.first);
		if (!defaults) {
			lifetime_merge(analysis->compiler, &analysis->state, &dispatch, statement->span.first);
		}
		analysis->control = control.parent;
		break;
	}
	default:
		break;
	}
	statement->exits = (statement->exits & ~NORMAL_EXIT) | (analysis->state.reachability == REACHABLE_STATE ? NORMAL_EXIT : 0);
}

void analyze_function(struct Compiler* compiler, struct Function* function) {
	if (function->kind != FUNCTION_DEFINITION) {
		return;
	}
	struct Scope scope = { NULL, NULL };
	for (struct Variable* parameter = function->parameters; parameter; parameter = parameter->next) {
		parameter->scope_depth = 1;
		scope_declare(compiler, &scope, parameter);
	}
	unsigned exits = analyze_statements(compiler, function, function->statements, &scope, NULL);
	struct LifetimeAnalysis analysis = { compiler, { REACHABLE_STATE, NULL }, NULL };
	if (!compiler->failed) {
		for (struct Variable* parameter = function->parameters; parameter; parameter = parameter->next) {
			lifetime_enter(&analysis, parameter);
		}
		lifetime_statements(&analysis, function->statements);
		if (analysis.state.reachability == REACHABLE_STATE) {
			function->cleanup = lifetime_cleanups(&analysis, NULL);
		}
	}
	if (analysis.state.reachability == REACHABLE_STATE && !is_void(function->result)) {
		diagnostic(compiler, function->name, "non-void function must return a value");
	}
}

void check_enumerator_names(struct Compiler* compiler) {
	for (struct TagSymbol* tag = compiler->tags; tag && !compiler->failed; tag = tag->next) {
		if (tag->record->kind != ENUM_TYPE) {
			continue;
		}
		for (struct Enumerator* value = tag->record->data.enumerators; value; value = value->next) {
			for (struct TagSymbol* previous = compiler->tags; previous != tag; previous = previous->next) {
				if (previous->record->kind != ENUM_TYPE) {
					continue;
				}
				for (struct Enumerator* earlier = previous->record->data.enumerators; earlier; earlier = earlier->next) {
					if (name_equal(earlier->name, value->name)) {
						diagnostic(compiler, value->name, "duplicate enumerator '%.*s'", (int)value->name.length, value->name.start);
					}
				}
			}
		}
	}
}

void check_namespace_names(struct Compiler* compiler) {
	for (struct Function* function = compiler->functions; function && !compiler->failed; function = function->next) {
		if (function->role != ORDINARY_ROLE) {
			continue;
		}
		if (find_builtin(function->name)) {
			diagnostic(compiler, function->name, "function conflicts with a built-in type");
			return;
		}
		for (struct Record* record = compiler->records; record; record = record->next) {
			if (name_equal(function->name, record->name)) {
				diagnostic(compiler, function->name, "function conflicts with a type");
				return;
			}
			if (record->kind != ENUM_TYPE) {
				continue;
			}
			for (struct Enumerator* value = record->data.enumerators; value; value = value->next) {
				if (name_equal(function->name, value->name)) {
					diagnostic(compiler, function->name, "function conflicts with an enumerator");
					return;
				}
			}
		}
	}
	for (struct Record* record = compiler->records; record && !compiler->failed; record = record->next) {
		if (find_builtin(record->name)) {
			diagnostic(compiler, record->name, "type conflicts with a built-in type");
			return;
		}
		if (record->kind != ENUM_TYPE) {
			continue;
		}
		for (struct Enumerator* value = record->data.enumerators; value; value = value->next) {
			if (find_builtin(value->name)) {
				diagnostic(compiler, value->name, "enumerator conflicts with a built-in type");
				return;
			}
			for (struct Record* type = compiler->records; type; type = type->next) {
				if (name_equal(value->name, type->name)) {
					diagnostic(compiler, value->name, "enumerator conflicts with a type");
					return;
				}
			}
		}
	}
	for (struct Namespace* space = compiler->namespaces; space && !compiler->failed; space = space->next) {
		struct Token name = { NAME, space->name, space->length, 1, 1, space->source_path, NULL };
		if (space->kind == UNRESOLVED_NAMESPACE) {
			diagnostic(compiler, name, "qualified declaration owner was not declared");
			return;
		}
		if (find_builtin(name)) {
			diagnostic(compiler, name, "namespace conflicts with a built-in type");
			return;
		}
		for (struct Record* record = compiler->records; record; record = record->next) {
			if (name_equal(name, record->name) && space->record != record) {
				diagnostic(compiler, record->name, "type conflicts with a namespace");
				return;
			}
		}
		for (struct Function* function = compiler->functions; function; function = function->next) {
			if (name_equal(name, function->name)) {
				diagnostic(compiler, function->name, "function conflicts with a namespace");
				return;
			}
		}
		for (struct Record* record = compiler->records; record; record = record->next) {
			if (record->kind != ENUM_TYPE) {
				continue;
			}
			for (struct Enumerator* value = record->data.enumerators; value; value = value->next) {
				if (name_equal(name, value->name)) {
					diagnostic(compiler, value->name, "enumerator conflicts with a namespace");
					return;
				}
			}
		}
	}
}

void resolve_namespace_prefix(struct Compiler* compiler, struct Namespace* space) {
	if (!space || space->kind != UNRESOLVED_NAMESPACE || compiler->failed) {
		return;
	}
	resolve_namespace_prefix(compiler, space->parent);
	struct Namespace* parent = space->parent;
	while (parent && parent->canonical) {
		parent = parent->canonical;
	}
	struct Token name = { NAME, space->name, space->length, 1, 1, space->source_path, NULL };
	name = unqualified_token(name);
	struct Namespace* canonical = namespace_child(compiler, parent, name);
	if (canonical && canonical != space && canonical->kind != UNRESOLVED_NAMESPACE) {
		space->canonical = canonical;
		space->kind = ALIASED_NAMESPACE;
	}
}

void analyze(struct Compiler* compiler) {
	register_tags(compiler);
	for (struct TagSymbol* tag = compiler->tags; tag && !compiler->failed; tag = tag->next) {
		resolve_bases(compiler, tag->record);
	}
	for (struct Namespace* space = compiler->namespaces; space && !compiler->failed; space = space->next) {
		resolve_namespace_prefix(compiler, space);
	}
	for (struct Function* function = compiler->functions; function && !compiler->failed; function = function->next) {
		if (function->owner && function->owner->canonical) {
			function->owner = function->owner->canonical;
			function->name = qualify_name(compiler, function->owner, function->local_name);
		}
	}
	check_namespace_names(compiler);
	check_enumerator_names(compiler);
	for (struct TagSymbol* tag = compiler->tags; tag && !compiler->failed; tag = tag->next) {
		if (tag->record->state != FORWARD_RECORD) {
			resolve_record(compiler, tag->record);
		}
	}
	register_functions(compiler);
	resolve_virtual_functions(compiler);
	for (struct FunctionSymbol* symbol = compiler->symbols; symbol && !compiler->failed; symbol = symbol->next) {
		analyze_function(compiler, symbol->function);
	}
}

void select_entry(struct Compiler* compiler, const char* name) {
	struct Token token = { NAME, name, strlen(name), 1, 1, NULL };
	struct Function* function = find_function(compiler, token);
	if (!function) {
		diagnostic(compiler, compiler->functions ? compiler->functions->name : compiler->token, "entry function '%s' was not found", name);
		return;
	}
	if (function->kind != FUNCTION_DEFINITION) {
		diagnostic(compiler, function->name, "entry function '%s' has no definition", name);
		return;
	}
	struct Type result = { SIGNED_INTEGER_TYPE, { 32 } };
	if (!is_void(function->result) && !type_equal(function->result, result)) {
		diagnostic(compiler, function->name, "entry function must return i32 or omit its return type");
		return;
	}
	struct Variable* first = function->parameters;
	struct Type arguments = pointer_type(compiler, pointer_type(compiler, (struct Type){ CHAR_TYPE, { 0 } }));
	if (function->arity == VARIADIC_PARAMETERS || (first && (!first->next || first->next->next || !type_equal(first->type, result) || !type_equal(first->next->type, arguments)))) {
		diagnostic(compiler, function->name, "entry function must have no parameters or (i32, char**) parameters");
		return;
	}
	compiler->entry = function;
}

LLVMTypeRef llvm_type(struct Compiler* compiler, struct Type type) {
	if (type.kind == FUNCTION_TYPE) {
		struct Function* signature = type.data.function;
		unsigned count = 0;
		for (struct Variable* parameter = signature->parameters; parameter; parameter = parameter->next) {
			count++;
		}
		LLVMTypeRef* parameters = count ? allocate(compiler, count * sizeof(LLVMTypeRef)) : NULL;
		if (count && !parameters) {
			return NULL;
		}
		unsigned index = 0;
		for (struct Variable* parameter = signature->parameters; parameter; parameter = parameter->next) {
			parameters[index++] = llvm_type(compiler, parameter->type);
		}
		return LLVMFunctionType(llvm_type(compiler, signature->result), parameters, count, signature->arity == VARIADIC_PARAMETERS);
	}
	switch (type.kind) {
	case VOID_TYPE:
	case NULL_TYPE:
		return LLVMVoidTypeInContext(compiler->llvm_context);
	case POINTER_TYPE:
	case REFERENCE_TYPE:
		return LLVMPointerTypeInContext(compiler->llvm_context, 0);
	case STRUCT_TYPE:
	case UNION_TYPE:
	case ENUM_TYPE:
		return type.data.record->llvm_type;
	case ARRAY_TYPE:
		return LLVMArrayType2(llvm_type(compiler, *type.data.array.element), type.data.array.count);
	case FLOAT_TYPE:
		return type.data.bits == 32 ? LLVMFloatTypeInContext(compiler->llvm_context) : LLVMDoubleTypeInContext(compiler->llvm_context);
	case BOOL_TYPE:
	case CHAR_TYPE:
	case SIGNED_INTEGER_TYPE:
	case UNSIGNED_INTEGER_TYPE:
		return LLVMIntTypeInContext(compiler->llvm_context, (unsigned)numeric_bits(type));
	}
	return LLVMVoidTypeInContext(compiler->llvm_context);
}

LLVMAttributeRef abi_extension(struct Compiler* compiler, struct Type type) {
	if (!numeric_type(type) || type.kind == FLOAT_TYPE || !numeric_bits(type) || numeric_bits(type) >= 32) {
		return NULL;
	}
	const char* name = zero_extended_type(type) ? "zeroext" : "signext";
	return LLVMCreateEnumAttribute(compiler->llvm_context, LLVMGetEnumAttributeKindForName(name, strlen(name)), 0);
}

char* function_symbol(struct Compiler* compiler, struct Function* function) {
	struct Namespace* associated = function->owner;
	while (associated && !associated->record) {
		associated = associated->parent;
	}
	const char* prefix = compiler->mode == EXECUTABLE_COMPILATION && (function->kind == FUNCTION_DEFINITION || associated || token_text(function->name, "main")) ? "rt.function." : "";
	struct Token symbol_name = function->name;
	if (function->kind == FUNCTION_DECLARATION && !associated) {
		for (size_t index = 0; index + 1 < symbol_name.length; index++) {
			if (symbol_name.start[index] == ':' && symbol_name.start[index + 1] == ':') {
				symbol_name.start += index + 2;
				symbol_name.length -= index + 2;
				index = 0;
			}
		}
	}
	char suffix[64] = { 0 };
	if (function->role != ORDINARY_ROLE) {
		unsigned ordinal = 0;
		for (struct FunctionSymbol* symbol = compiler->symbols; symbol && symbol->function != function; symbol = symbol->next) {
			ordinal++;
		}
		snprintf(suffix, sizeof(suffix), ".%d.%u", function->role, ordinal);
	}
	size_t length = strlen(prefix) + symbol_name.length + strlen(suffix);
	char* name = allocate(compiler, length + 1);
	if (!name) {
		return NULL;
	}
	memcpy(name, prefix, strlen(prefix));
	memcpy(name + strlen(prefix), symbol_name.start, symbol_name.length);
	memcpy(name + strlen(prefix) + symbol_name.length, suffix, strlen(suffix));
	return name;
}

void declare_llvm_functions(struct Compiler* compiler) {
	for (struct FunctionSymbol* symbol = compiler->symbols; symbol && !compiler->failed; symbol = symbol->next) {
		struct Function* function = symbol->function;
		if (function->kind == FUNCTION_DELETED) {
			continue;
		}
		unsigned count = 0;
		for (struct Variable* parameter = function->parameters; parameter; parameter = parameter->next) {
			if (count == UINT_MAX) {
				diagnostic(compiler, function->name, "too many parameters for LLVM");
				return;
			}
			count++;
		}
		LLVMTypeRef* types = count ? allocate(compiler, count * sizeof(LLVMTypeRef)) : NULL;
		if (count && !types) {
			return;
		}
		unsigned index = 0;
		for (struct Variable* parameter = function->parameters; parameter; parameter = parameter->next) {
			types[index++] = llvm_type(compiler, parameter->type);
		}
		function->llvm_type = LLVMFunctionType(llvm_type(compiler, function->result), types, count, function->arity == VARIADIC_PARAMETERS);
		char* name = function_symbol(compiler, function);
		if (!name) {
			return;
		}
		LLVMValueRef existing = LLVMGetNamedFunction(compiler->module, name);
		if (existing) {
			for (struct FunctionSymbol* previous = compiler->symbols; previous != symbol; previous = previous->next) {
				struct Function* declaration = previous->function;
				if (declaration->llvm_value != existing) {
					continue;
				}

				bool compatible = declaration->arity == function->arity && type_equal(declaration->result, function->result);
				struct Variable* first = declaration->parameters;
				struct Variable* second = function->parameters;
				while (first && second && compatible) {
					compatible = type_equal(first->type, second->type);
					first = first->next;
					second = second->next;
				}
				if (!compatible || first || second) {
					diagnostic(compiler, function->name, "conflicting declarations for C symbol '%s'", name);
					return;
				}
				break;
			}
		}
		function->llvm_value = existing ? existing : LLVMAddFunction(compiler->module, name, function->llvm_type);
		LLVMSetFunctionCallConv(function->llvm_value, LLVMCCallConv);
		LLVMAttributeRef extension = abi_extension(compiler, function->result);
		if (extension) {
			LLVMAddAttributeAtIndex(function->llvm_value, LLVMAttributeReturnIndex, extension);
		}
		index = 1;
		for (struct Variable* parameter = function->parameters; parameter; parameter = parameter->next) {
			extension = abi_extension(compiler, parameter->type);
			if (extension) {
				LLVMAddAttributeAtIndex(function->llvm_value, index, extension);
			}
			index++;
		}
	}
}

LLVMValueRef emit_expression(struct Compiler* compiler, struct Expression* expression);
LLVMValueRef function_address(struct Compiler* compiler, struct Function* function);
void emit_statements(struct Compiler* compiler, struct Statement* statements, struct Flow* flow);
void emit_install_views(struct Compiler* compiler, struct Type type, LLVMValueRef address);
void emit_store_value(struct Compiler* compiler, struct Type type, LLVMValueRef address, LLVMValueRef value);

LLVMValueRef entry_alloca(struct Compiler* compiler, struct Type type) {
	LLVMValueRef function = LLVMGetBasicBlockParent(LLVMGetInsertBlock(compiler->builder));
	LLVMBasicBlockRef entry = LLVMGetEntryBasicBlock(function);
	LLVMBuilderRef builder = LLVMCreateBuilderInContext(compiler->llvm_context);
	LLVMValueRef first = LLVMGetFirstInstruction(entry);
	if (first) {
		LLVMPositionBuilderBefore(builder, first);
	}
	else {
		LLVMPositionBuilderAtEnd(builder, entry);
	}
	LLVMValueRef slot = LLVMBuildAlloca(builder, llvm_type(compiler, type), "storage");
	LLVMDisposeBuilder(builder);
	return slot;
}

LLVMValueRef pointer_index(struct Compiler* compiler, LLVMValueRef value, struct Type type) {
	LLVMTypeRef target = LLVMIntTypeInContext(compiler->llvm_context, LLVMPointerSize(compiler->layout) * CHAR_BIT);
	unsigned width = LLVMGetIntTypeWidth(LLVMTypeOf(value));
	unsigned target_width = LLVMGetIntTypeWidth(target);
	if (width < target_width) {
		return zero_extended_type(type) ? LLVMBuildZExt(compiler->builder, value, target, "index") : LLVMBuildSExt(compiler->builder, value, target, "index");
	}
	if (width > target_width) {
		return LLVMBuildTrunc(compiler->builder, value, target, "index");
	}
	return value;
}

LLVMValueRef emit_call(struct Compiler* compiler, struct Expression* expression) {
	if (expression->data.call.receiver_evaluation && !emit_expression(compiler, expression->data.call.receiver_evaluation)) {
		return NULL;
	}

	unsigned count = 0;
	for (struct Argument* argument = expression->data.call.arguments; argument; argument = argument->next) {
		if (count == UINT_MAX) {
			diagnostic(compiler, expression->span.first, "too many arguments for LLVM");
			return NULL;
		}
		count++;
	}
	LLVMValueRef* values = count ? allocate(compiler, count * sizeof(LLVMValueRef)) : NULL;
	if (count && !values) {
		return NULL;
	}
	unsigned index = 0;
	for (struct Argument* argument = expression->data.call.arguments; argument; argument = argument->next) {
		LLVMValueRef value;
		if (argument->variable) {
			argument->variable->slot = entry_alloca(compiler, argument->variable->type);
			if (argument->variable->type.kind != REFERENCE_TYPE) {
				LLVMValueRef initial = argument->initializer ? emit_expression(compiler, argument->initializer) : LLVMConstNull(llvm_type(compiler, argument->variable->type));
				if (argument->initializer || !argument->initialization) {
					LLVMBuildStore(compiler->builder, initial, argument->variable->slot);
				}
				if (argument->initializer || !argument->initialization || argument->initialization->data.block) {
					emit_install_views(compiler, argument->variable->type, argument->variable->slot);
				}
			}
			emit_statements(compiler, argument->initialization->data.block, NULL);
			value = emit_expression(compiler, argument->expression);
		}
		else {
			value = emit_expression(compiler, argument->expression);
		}
		if (!value) {
			return NULL;
		}
		if (!type_equal(argument->expression->type, argument->passed_type)) {
			LLVMTypeRef target = llvm_type(compiler, argument->passed_type);
			if (argument->expression->type.kind == FLOAT_TYPE) {
				value = LLVMBuildFPExt(compiler->builder, value, target, "promoted");
			}
			else if (zero_extended_type(argument->expression->type)) {
				value = LLVMBuildZExt(compiler->builder, value, target, "promoted");
			}
			else {
				value = LLVMBuildSExt(compiler->builder, value, target, "promoted");
			}
		}
		values[index++] = value;
	}
	struct Function* function = expression->data.call.binding;
	LLVMTypeRef signature = llvm_type(compiler, (struct Type){ FUNCTION_TYPE, { .function = function } });
	LLVMValueRef callee = expression->data.call.qualified_owner && expression->data.call.callee->kind == FUNCTION_EXPRESSION ? function->llvm_value : emit_expression(compiler, expression->data.call.callee);
	LLVMValueRef call = LLVMBuildCall2(compiler->builder, signature, callee, values, count, is_void(function->result) ? "" : "call");
	LLVMSetInstructionCallConv(call, LLVMCCallConv);
	LLVMAttributeRef extension = abi_extension(compiler, function->result);
	if (extension) {
		LLVMAddCallSiteAttribute(call, LLVMAttributeReturnIndex, extension);
	}
	index = 1;
	for (struct Argument* argument = expression->data.call.arguments; argument; argument = argument->next) {
		extension = abi_extension(compiler, argument->passed_type);
		if (extension) {
			LLVMAddCallSiteAttribute(call, index, extension);
		}
		index++;
	}
	return call;
}

LLVMValueRef emit_address(struct Compiler* compiler, struct Expression* expression);
LLVMValueRef emit_direct_call(struct Compiler* compiler, struct Function* function, LLVMValueRef* values, unsigned count);

LLVMValueRef emit_base_address(struct Compiler* compiler, LLVMValueRef address, struct Record* derived, struct BasePath* path) {
	for (; path; path = path->next) {
		if (path->relation->kind == NONVIRTUAL_BASE) {
			address = LLVMBuildStructGEP2(compiler->builder, derived->llvm_body, address, path->relation->index, "base");
		}
		else {
			LLVMTypeRef integer = LLVMInt64TypeInContext(compiler->llvm_context);
			LLVMValueRef header = LLVMBuildStructGEP2(compiler->builder, derived->llvm_body, address, 0, "view_slot");
			LLVMValueRef view = LLVMBuildLoad2(compiler->builder, LLVMPointerTypeInContext(compiler->llvm_context, 0), header, "view");
			LLVMValueRef index = LLVMConstInt(integer, path->relation->index, false);
			LLVMValueRef entry = LLVMBuildGEP2(compiler->builder, integer, view, &index, 1, "view_entry");
			LLVMValueRef offset = LLVMBuildLoad2(compiler->builder, integer, entry, "virtual_offset");
			address = LLVMBuildGEP2(compiler->builder, LLVMInt8TypeInContext(compiler->llvm_context), address, &offset, 1, "virtual_base");
		}
		derived = path->relation->record;
	}
	return address;
}

LLVMValueRef emit_inverse_base_address(struct Compiler* compiler, LLVMValueRef address, struct Record* derived, struct BasePath* path) {
	uint64_t offset = 0;
	for (; path; path = path->next) {
		offset += LLVMOffsetOfElement(compiler->layout, derived->llvm_body, path->relation->index);
		derived = path->relation->record;
	}
	LLVMValueRef index = LLVMConstInt(LLVMIntTypeInContext(compiler->llvm_context, LLVMPointerSize(compiler->layout) * CHAR_BIT), 0 - offset, true);
	return LLVMBuildGEP2(compiler->builder, LLVMInt8TypeInContext(compiler->llvm_context), address, &index, 1, "derived");
}

uint64_t subobject_offset(struct Compiler* compiler, struct Record* complete, struct BasePath* path) {
	struct Record* record = complete;
	uint64_t offset = 0;
	for (; path; path = path->next) {
		struct BaseRelation* relation = path->relation;
		if (relation->kind == NONVIRTUAL_BASE) {
			offset += LLVMOffsetOfElement(compiler->layout, record->llvm_body, relation->index);
		}
		else {
			for (struct VirtualBase* base = complete->virtual_bases; base; base = base->next) {
				if (base->record == relation->record) {
					offset = LLVMOffsetOfElement(compiler->layout, complete->llvm_type, base->index + 1);
					break;
				}
			}
		}
		record = relation->record;
	}
	return offset;
}

bool type_has_views(struct Type type) {
	if (type.kind == ARRAY_TYPE) {
		return type_has_views(*type.data.array.element);
	}
	if (type.kind != STRUCT_TYPE && type.kind != UNION_TYPE) {
		return false;
	}
	if (type.data.record->virtual_bases || type.data.record->callables) {
		return true;
	}
	for (struct Subobject* object = type.data.record->subobjects; object; object = object->next) {
		if (object->record->callables) {
			return true;
		}
		for (struct Member* field = object->record->data.members; field; field = field->next) {
			if (type_has_views(field->type)) {
				return true;
			}
		}
	}
	return false;
}

LLVMValueRef emit_array_element(struct Compiler* compiler, struct Type type, LLVMValueRef address, uint64_t index) {
	LLVMTypeRef integer = LLVMInt64TypeInContext(compiler->llvm_context);
	LLVMValueRef indices[] = { LLVMConstNull(integer), LLVMConstInt(integer, index, false) };
	return LLVMBuildGEP2(compiler->builder, llvm_type(compiler, type), address, indices, 2, "element");
}

unsigned virtual_base_count(struct Record* record) {
	unsigned count = 0;
	for (struct VirtualBase* base = record->virtual_bases; base; base = base->next) {
		count++;
	}
	return count;
}

unsigned virtual_callable_count(struct Record* record) {
	unsigned count = 0;
	for (struct VirtualCallable* callable = record->callables; callable; callable = callable->next) {
		count++;
	}
	return count;
}

LLVMTypeRef runtime_view_type(struct Compiler* compiler, struct Record* record) {
	LLVMTypeRef fields[] = { LLVMArrayType2(LLVMInt64TypeInContext(compiler->llvm_context), virtual_base_count(record)), LLVMArrayType2(LLVMPointerTypeInContext(compiler->llvm_context, 0), virtual_callable_count(record)) };
	return LLVMStructTypeInContext(compiler->llvm_context, fields, 2, false);
}

void apply_signature_attributes(struct Compiler* compiler, LLVMValueRef value, struct Function* function, bool call) {
	LLVMAttributeRef extension = abi_extension(compiler, function->result);
	if (extension) {
		if (call) {
			LLVMAddCallSiteAttribute(value, LLVMAttributeReturnIndex, extension);
		}
		else {
			LLVMAddAttributeAtIndex(value, LLVMAttributeReturnIndex, extension);
		}
	}
	unsigned index = 1;
	for (struct Variable* parameter = function->parameters; parameter; parameter = parameter->next) {
		extension = abi_extension(compiler, parameter->type);
		if (extension && call) {
			LLVMAddCallSiteAttribute(value, index, extension);
		}
		else if (extension) {
			LLVMAddAttributeAtIndex(value, index, extension);
		}
		index++;
	}
}

LLVMValueRef function_address(struct Compiler* compiler, struct Function* function) {
	if (function->modification == ORDINARY_FUNCTION) {
		return function->llvm_value;
	}
	if (!function->dispatch) {
		function->dispatch = LLVMAddFunction(compiler->module, "rt.dispatch", function->llvm_type);
		LLVMSetLinkage(function->dispatch, LLVMPrivateLinkage);
		LLVMSetFunctionCallConv(function->dispatch, LLVMGetFunctionCallConv(function->llvm_value));
		apply_signature_attributes(compiler, function->dispatch, function, false);
		LLVMAddAttributeAtIndex(function->dispatch, LLVMAttributeFunctionIndex, LLVMCreateStringAttribute(compiler->llvm_context, "thunk", 5, "", 0));
	}
	return function->dispatch;
}

LLVMValueRef request_dispatch_thunk(struct Compiler* compiler, struct Record* complete, struct Subobject* origin, struct VirtualCallable* callable) {
	for (struct DispatchThunk* thunk = compiler->thunks; thunk; thunk = thunk->next) {
		if (thunk->complete == complete && thunk->origin == origin && thunk->callable == callable) {
			return thunk->value;
		}
	}
	struct DispatchThunk* thunk = allocate(compiler, sizeof(struct DispatchThunk));
	struct FinalOverrider* target = allocate(compiler, sizeof(struct FinalOverrider));
	if (!thunk || !target) {
		return NULL;
	}
	*target = final_overrider(compiler, complete, origin, callable->function);
	if (compiler->failed || !target->function) {
		return NULL;
	}
	thunk->complete = complete;
	thunk->origin = origin;
	thunk->callable = callable;
	thunk->target = target;
	thunk->value = LLVMAddFunction(compiler->module, "rt.virtual", callable->function->llvm_type);
	LLVMSetLinkage(thunk->value, LLVMPrivateLinkage);
	LLVMSetFunctionCallConv(thunk->value, LLVMGetFunctionCallConv(callable->function->llvm_value));
	apply_signature_attributes(compiler, thunk->value, callable->function, false);
	LLVMAddAttributeAtIndex(thunk->value, LLVMAttributeFunctionIndex, LLVMCreateStringAttribute(compiler->llvm_context, "thunk", 5, "", 0));
	if (compiler->last_thunk) {
		compiler->last_thunk->next = thunk;
	}
	else {
		compiler->thunks = thunk;
	}
	compiler->last_thunk = thunk;
	return thunk->value;
}

LLVMValueRef emit_covariant_result(struct Compiler* compiler, LLVMValueRef value, struct Type source, struct Type target) {
	if (type_equal(source, target)) {
		return value;
	}
	struct BaseSearch selection = find_base_path(compiler, source.data.element->data.record, target.data.element->data.record);
	if (source.kind == REFERENCE_TYPE) {
		return emit_base_address(compiler, value, source.data.element->data.record, selection.path);
	}
	LLVMValueRef null = LLVMConstNull(llvm_type(compiler, target));
	LLVMValueRef empty = LLVMBuildICmp(compiler->builder, LLVMIntEQ, value, null, "null_result");
	LLVMBasicBlockRef previous = LLVMGetInsertBlock(compiler->builder);
	LLVMValueRef function = LLVMGetBasicBlockParent(previous);
	LLVMBasicBlockRef adjust = LLVMAppendBasicBlockInContext(compiler->llvm_context, function, "result.adjust");
	LLVMBasicBlockRef merge = LLVMAppendBasicBlockInContext(compiler->llvm_context, function, "result.end");
	LLVMBuildCondBr(compiler->builder, empty, merge, adjust);
	LLVMPositionBuilderAtEnd(compiler->builder, adjust);
	LLVMValueRef result = emit_base_address(compiler, value, source.data.element->data.record, selection.path);
	LLVMBuildBr(compiler->builder, merge);
	LLVMPositionBuilderAtEnd(compiler->builder, merge);
	LLVMValueRef phi = LLVMBuildPhi(compiler->builder, llvm_type(compiler, target), "covariant_result");
	LLVMValueRef values[] = { null, result };
	LLVMBasicBlockRef blocks[] = { previous, adjust };
	LLVMAddIncoming(phi, values, blocks, 2);
	return phi;
}

void emit_install_views(struct Compiler* compiler, struct Type type, LLVMValueRef address) {
	if (!type_has_views(type) || type.kind == UNION_TYPE) {
		return;
	}
	if (type.kind == ARRAY_TYPE) {
		for (uint64_t i = 0; i < type.data.array.count; i++) {
			emit_install_views(compiler, *type.data.array.element, emit_array_element(compiler, type, address, i));
		}
		return;
	}
	struct Record* complete = type.data.record;
	for (struct Subobject* object = complete->subobjects; object; object = object->next) {
		object->offset = subobject_offset(compiler, complete, object->path);
		LLVMValueRef offset = LLVMConstInt(LLVMInt64TypeInContext(compiler->llvm_context), object->offset, false);
		LLVMValueRef slot = LLVMBuildGEP2(compiler->builder, LLVMInt8TypeInContext(compiler->llvm_context), address, &offset, 1, "subobject");
		if (object->record->virtual_bases || object->record->callables) {
			if (!object->view) {
				object->view = LLVMAddGlobal(compiler->module, runtime_view_type(compiler, object->record), "rt.view");
				LLVMSetGlobalConstant(object->view, true);
				LLVMSetLinkage(object->view, LLVMPrivateLinkage);
			}
			LLVMBuildStore(compiler->builder, object->view, slot);
		}
		for (struct Member* field = object->record->data.members; field; field = field->next) {
			LLVMValueRef member = LLVMBuildStructGEP2(compiler->builder, object->record->llvm_body, slot, field->index, "field");
			emit_install_views(compiler, field->type, member);
		}
	}
}

void define_runtime_views(struct Compiler* compiler) {
	for (struct Record* complete = compiler->records; complete; complete = complete->next) {
		for (struct Subobject* object = complete->subobjects; object; object = object->next) {
			if (!object->view || LLVMGetInitializer(object->view)) {
				continue;
			}
			unsigned count = 0;
			for (struct VirtualBase* base = object->record->virtual_bases; base; base = base->next) {
				count++;
			}
			LLVMValueRef* offsets = count ? allocate(compiler, count * sizeof(LLVMValueRef)) : NULL;
			if (count && !offsets) {
				return;
			}
			for (struct VirtualBase* base = object->record->virtual_bases; base; base = base->next) {
				for (struct Subobject* target = complete->subobjects; target; target = target->next) {
					if (target->kind == SHARED_SUBOBJECT && target->record == base->record) {
						uint64_t offset = subobject_offset(compiler, complete, target->path) - object->offset;
						offsets[base->index] = LLVMConstInt(LLVMInt64TypeInContext(compiler->llvm_context), offset, true);
						break;
					}
				}
			}
			unsigned functions_count = virtual_callable_count(object->record);
			LLVMValueRef* functions = functions_count ? allocate(compiler, functions_count * sizeof(LLVMValueRef)) : NULL;
			if (functions_count && !functions) {
				return;
			}
			for (struct VirtualCallable* callable = object->record->callables; callable && !compiler->failed; callable = callable->next) {
				functions[callable->index] = request_dispatch_thunk(compiler, complete, object, callable);
			}
			if (compiler->failed) {
				return;
			}
			LLVMValueRef fields[] = { LLVMConstArray2(LLVMInt64TypeInContext(compiler->llvm_context), offsets, count), LLVMConstArray2(LLVMPointerTypeInContext(compiler->llvm_context, 0), functions, functions_count) };
			LLVMSetInitializer(object->view, LLVMConstStructInContext(compiler->llvm_context, fields, 2, false));
		}
	}
}

void emit_payload_copy(struct Compiler* compiler, struct Type type, LLVMValueRef destination, LLVMValueRef source) {
	if (!type_has_views(type) || type.kind == UNION_TYPE) {
		LLVMBuildStore(compiler->builder, LLVMBuildLoad2(compiler->builder, llvm_type(compiler, type), source, "payload"), destination);
		return;
	}
	if (type.kind == ARRAY_TYPE) {
		for (uint64_t i = 0; i < type.data.array.count; i++) {
			emit_payload_copy(compiler, *type.data.array.element, emit_array_element(compiler, type, destination, i), emit_array_element(compiler, type, source, i));
		}
		return;
	}
	for (struct Subobject* object = type.data.record->subobjects; object; object = object->next) {
		LLVMValueRef to = emit_base_address(compiler, destination, type.data.record, object->path);
		LLVMValueRef from = emit_base_address(compiler, source, type.data.record, object->path);
		for (struct Member* field = object->record->data.members; field; field = field->next) {
			LLVMValueRef to_field = LLVMBuildStructGEP2(compiler->builder, object->record->llvm_body, to, field->index, "field");
			LLVMValueRef from_field = LLVMBuildStructGEP2(compiler->builder, object->record->llvm_body, from, field->index, "field");
			emit_payload_copy(compiler, field->type, to_field, from_field);
		}
	}
}

LLVMValueRef emit_read_value(struct Compiler* compiler, struct Type type, LLVMValueRef address) {
	if (!type_has_views(type) || type.kind == UNION_TYPE) {
		return LLVMBuildLoad2(compiler->builder, llvm_type(compiler, type), address, "value");
	}
	LLVMValueRef temporary = entry_alloca(compiler, type);
	emit_install_views(compiler, type, temporary);
	emit_payload_copy(compiler, type, temporary, address);
	return LLVMBuildLoad2(compiler->builder, llvm_type(compiler, type), temporary, "value");
}

void emit_store_value(struct Compiler* compiler, struct Type type, LLVMValueRef address, LLVMValueRef value) {
	if (!type_has_views(type) || type.kind == UNION_TYPE) {
		LLVMBuildStore(compiler->builder, value, address);
		return;
	}
	LLVMValueRef temporary = entry_alloca(compiler, type);
	LLVMBuildStore(compiler->builder, value, temporary);
	emit_payload_copy(compiler, type, address, temporary);
}

LLVMValueRef emit_materialized_address(struct Compiler* compiler, struct Expression* expression) {
	if (addressable_expression(expression)) {
		return emit_address(compiler, expression);
	}
	LLVMValueRef value = emit_expression(compiler, expression);
	if (!value) {
		return NULL;
	}
	if (expression->temporary_address) {
		return expression->temporary_address;
	}
	LLVMValueRef address = entry_alloca(compiler, expression->type);
	LLVMBuildStore(compiler->builder, value, address);
	return address;
}

LLVMValueRef emit_field_slot(struct Compiler* compiler, struct Expression* expression) {

	struct Expression* object = expression->data.member.object;
	struct Type type = object->type;
	LLVMValueRef address;
	if (expression->kind == POINTER_FIELD_EXPRESSION) {
		address = emit_expression(compiler, object);
		type = *type.data.element;
	}
	else {
		address = emit_materialized_address(compiler, object);
	}
	if (!address) {
		return NULL;
	}
	address = emit_base_address(compiler, address, type.data.record, expression->data.member.base_path);
	struct Member* field = expression->data.member.binding;
	return field->owner->kind == UNION_TYPE ? address : LLVMBuildStructGEP2(compiler->builder, field->owner->llvm_body, address, field->index, "field");
}

LLVMValueRef emit_address(struct Compiler* compiler, struct Expression* expression) {
	switch (expression->kind) {
	case ASSIGNMENT_EXPRESSION:
		emit_expression(compiler, expression);
		return expression->assignment_address;
	case ACCESS_EXPRESSION:
		return emit_address(compiler, expression->data.operand);
	case FIELD_EXPRESSION:
	case POINTER_FIELD_EXPRESSION: {
		LLVMValueRef slot = emit_field_slot(compiler, expression);
		if (slot && expression->data.member.binding->type.kind == REFERENCE_TYPE) {
			return LLVMBuildLoad2(compiler->builder, llvm_type(compiler, expression->data.member.binding->type), slot, "field_referent");
		}
		return slot;
	}
	case REFERENCE_CAST_EXPRESSION:
		return expression->data.operand->kind == FUNCTION_EXPRESSION ? function_address(compiler, expression->data.operand->data.function) : emit_address(compiler, expression->data.operand);
	case BASE_VALUE_EXPRESSION: {
		struct Expression* value = expression->data.base.value;
		LLVMValueRef address = emit_materialized_address(compiler, value);
		if (expression->data.base.kind == BASE_TO_DERIVED) {
			return emit_inverse_base_address(compiler, address, expression->type.data.record, expression->data.base.path);
		}
		return emit_base_address(compiler, address, value->type.data.record, expression->data.base.path);
	}
	case CALL_EXPRESSION:
		return emit_call(compiler, expression);
	case NAME_EXPRESSION:
		if (expression->data.variable.binding->type.kind == REFERENCE_TYPE) {
			return LLVMBuildLoad2(compiler->builder, llvm_type(compiler, expression->data.variable.binding->type), expression->data.variable.binding->slot, "referent");
		}
		return expression->data.variable.binding->slot;
	case DEREFERENCE_EXPRESSION:
		return emit_expression(compiler, expression->data.operand);
	case INDEX_EXPRESSION: {
		struct Expression* base = expression->data.binary.left;
		LLVMValueRef index = emit_expression(compiler, expression->data.binary.right);
		if (!index) {
			return NULL;
		}
		index = pointer_index(compiler, index, expression->data.binary.right->type);
		if (base->type.kind == ARRAY_TYPE) {
			LLVMValueRef address = emit_materialized_address(compiler, base);
			LLVMValueRef indices[] = { LLVMConstInt(LLVMInt32TypeInContext(compiler->llvm_context), 0, false), index };
			return address ? LLVMBuildGEP2(compiler->builder, llvm_type(compiler, base->type), address, indices, 2, "element") : NULL;
		}
		LLVMValueRef address = emit_expression(compiler, base);
		return address ? LLVMBuildGEP2(compiler->builder, llvm_type(compiler, *base->type.data.element), address, &index, 1, "element") : NULL;
	}
	default:
		diagnostic(compiler, expression->span.first, "expression has no address");
		return NULL;
	}
}

LLVMValueRef emit_truth(struct Compiler* compiler, LLVMValueRef value, struct Type type) {
	LLVMValueRef zero = LLVMConstNull(llvm_type(compiler, type));
	return type.kind == FLOAT_TYPE ? LLVMBuildFCmp(compiler->builder, LLVMRealUNE, value, zero, "truth") : LLVMBuildICmp(compiler->builder, LLVMIntNE, value, zero, "truth");
}

LLVMValueRef emit_binary(struct Compiler* compiler, int token, LLVMValueRef left, LLVMValueRef right, struct Type left_type, struct Type right_type) {
	const struct Operator* operation = find_operator(token);
	if (left_type.kind == POINTER_TYPE && (token == '+' || token == '-')) {
		if (right_type.kind == POINTER_TYPE) {
			return LLVMBuildPtrDiff2(compiler->builder, llvm_type(compiler, *left_type.data.element), left, right, "difference");
		}
		right = pointer_index(compiler, right, right_type);
		if (token == '-') {
			right = LLVMBuildNeg(compiler->builder, right, "offset");
		}
		return LLVMBuildGEP2(compiler->builder, llvm_type(compiler, *left_type.data.element), left, &right, 1, "offset");
	}
	if (operation->kind == COMPARISON_OPERATOR) {
		LLVMValueRef value;
		if (left_type.kind == FLOAT_TYPE) {
			LLVMRealPredicate predicate = LLVMRealOEQ;
			switch (token) {
			case NOT_EQUAL:
				predicate = LLVMRealUNE;
				break;
			case '<':
				predicate = LLVMRealOLT;
				break;
			case '>':
				predicate = LLVMRealOGT;
				break;
			case LESS_EQUAL:
				predicate = LLVMRealOLE;
				break;
			case GREATER_EQUAL:
				predicate = LLVMRealOGE;
				break;
			default:
				break;
			}
			value = LLVMBuildFCmp(compiler->builder, predicate, left, right, "compare");
		}
		else {
			LLVMIntPredicate predicate = LLVMIntEQ;
			unsigned unsigned_type = zero_extended_type(left_type) || left_type.kind == POINTER_TYPE;
			switch (token) {
			case NOT_EQUAL:
				predicate = LLVMIntNE;
				break;
			case '<':
				predicate = unsigned_type ? LLVMIntULT : LLVMIntSLT;
				break;
			case '>':
				predicate = unsigned_type ? LLVMIntUGT : LLVMIntSGT;
				break;
			case LESS_EQUAL:
				predicate = unsigned_type ? LLVMIntULE : LLVMIntSLE;
				break;
			case GREATER_EQUAL:
				predicate = unsigned_type ? LLVMIntUGE : LLVMIntSGE;
				break;
			default:
				break;
			}
			value = LLVMBuildICmp(compiler->builder, predicate, left, right, "compare");
		}
		return value;
	}
	if (left_type.kind == FLOAT_TYPE) {
		return operation->float_builder(compiler->builder, left, right, "binary");
	}
	if (token == '/') {
		return zero_extended_type(left_type) ? LLVMBuildUDiv(compiler->builder, left, right, "binary") : LLVMBuildSDiv(compiler->builder, left, right, "binary");
	}
	if (token == '%') {
		return zero_extended_type(left_type) ? LLVMBuildURem(compiler->builder, left, right, "binary") : LLVMBuildSRem(compiler->builder, left, right, "binary");
	}
	if (token == SHIFT_RIGHT) {
		return zero_extended_type(left_type) ? LLVMBuildLShr(compiler->builder, left, right, "binary") : LLVMBuildAShr(compiler->builder, left, right, "binary");
	}
	return operation->integer_builder(compiler->builder, left, right, "binary");
}

struct Function* record_destructor(struct Compiler* compiler, struct Record* record) {
	for (struct FunctionSymbol* symbol = compiler->symbols; symbol; symbol = symbol->next) {
		if (symbol->function->role == DESTRUCTOR_ROLE && symbol->function->owner && symbol->function->owner->record == record) {
			return symbol->function;
		}
	}
	return NULL;
}

bool type_requires_destruction(struct Compiler* compiler, struct Type type) {
	if (type.kind == ARRAY_TYPE) {
		return type_requires_destruction(compiler, *type.data.array.element);
	}
	if (type.kind != STRUCT_TYPE && type.kind != UNION_TYPE) {
		return false;
	}
	if (record_destructor(compiler, type.data.record)) {
		return true;
	}
	for (struct BaseRelation* base = type.data.record->bases; base; base = base->next) {
		if (type_requires_destruction(compiler, (struct Type){ STRUCT_TYPE, { .record = base->record } })) {
			return true;
		}
	}
	for (struct Member* member = type.data.record->data.members; member; member = member->next) {
		if (type_requires_destruction(compiler, member->type)) {
			return true;
		}
	}
	return false;
}

LLVMValueRef emit_direct_call(struct Compiler* compiler, struct Function* function, LLVMValueRef* values, unsigned count) {
	LLVMValueRef call = LLVMBuildCall2(compiler->builder, function->llvm_type, function->llvm_value, values, count, is_void(function->result) ? "" : "special_call");
	LLVMSetInstructionCallConv(call, LLVMGetFunctionCallConv(function->llvm_value));
	apply_signature_attributes(compiler, call, function, true);
	return call;
}

void emit_destroy_type(struct Compiler* compiler, struct Type type, LLVMValueRef address);

void emit_destroy_members(struct Compiler* compiler, struct Member* member, LLVMValueRef address) {
	if (!member) {
		return;
	}
	emit_destroy_members(compiler, member->next, address);
	LLVMValueRef field = member->owner->kind == UNION_TYPE ? address : LLVMBuildStructGEP2(compiler->builder, member->owner->llvm_body, address, member->index, "destroy_field");
	emit_destroy_type(compiler, member->type, field);
}

void emit_destroy_body(struct Compiler* compiler, struct Record* record, LLVMValueRef address);

void emit_destroy_bases(struct Compiler* compiler, struct BaseRelation* base, struct Record* owner, LLVMValueRef address) {
	if (!base) {
		return;
	}
	emit_destroy_bases(compiler, base->next, owner, address);
	if (base->kind == NONVIRTUAL_BASE) {
		LLVMValueRef storage = LLVMBuildStructGEP2(compiler->builder, owner->llvm_body, address, base->index, "destroy_base");
		emit_destroy_body(compiler, base->record, storage);
	}
}

void emit_destroy_body(struct Compiler* compiler, struct Record* record, LLVMValueRef address) {
	struct Function* destructor = record_destructor(compiler, record);
	if (destructor) {
		if (destructor->kind == FUNCTION_DELETED) {
			diagnostic(compiler, destructor->name, "use of deleted destructor");
			return;
		}
		emit_direct_call(compiler, destructor, &address, 1);
	}
	if (record->kind == STRUCT_TYPE) {
		emit_destroy_members(compiler, record->data.members, address);
		emit_destroy_bases(compiler, record->bases, record, address);
	}
}

void emit_destroy_virtual_bases(struct Compiler* compiler, struct VirtualBase* base, struct Record* owner, LLVMValueRef address) {
	if (!base) {
		return;
	}
	emit_destroy_virtual_bases(compiler, base->next, owner, address);
	LLVMValueRef storage = LLVMBuildStructGEP2(compiler->builder, owner->llvm_type, address, base->index + 1, "destroy_virtual_base");
	emit_destroy_body(compiler, base->record, storage);
}

void emit_destroy_type(struct Compiler* compiler, struct Type type, LLVMValueRef address) {
	if (compiler->failed || !type_requires_destruction(compiler, type)) {
		return;
	}
	if (type.kind == ARRAY_TYPE) {
		for (uint64_t index = type.data.array.count; index; index--) {
			emit_destroy_type(compiler, *type.data.array.element, emit_array_element(compiler, type, address, index - 1));
		}
		return;
	}
	if (type.kind != STRUCT_TYPE && type.kind != UNION_TYPE) {
		return;
	}
	if (type.kind == UNION_TYPE && !record_destructor(compiler, type.data.record)) {
		diagnostic(compiler, type.data.record->name, "implicit union destructor is deleted because a member has nontrivial destruction");
		return;
	}
	emit_destroy_body(compiler, type.data.record, address);
	emit_destroy_virtual_bases(compiler, type.data.record->virtual_bases, type.data.record, address);
}

void emit_cleanups(struct Compiler* compiler, struct LifetimeCleanup* cleanup) {
	for (; cleanup; cleanup = cleanup->next) {
		emit_destroy_type(compiler, cleanup->variable->type, cleanup->variable->slot);
	}
}

void emit_temporary(struct Compiler* compiler, struct Expression* expression, LLVMValueRef address) {
	if (expression->use == TRANSFERRED_VALUE || (expression->type.kind != STRUCT_TYPE && expression->type.kind != UNION_TYPE && expression->type.kind != ARRAY_TYPE)) {
		return;
	}
	struct Temporary* temporary = allocate(compiler, sizeof(struct Temporary));
	if (!temporary) {
		return;
	}
	temporary->type = expression->type;
	temporary->address = address;
	temporary->next = compiler->temporaries;
	compiler->temporaries = temporary;
}

void emit_full_expression_end(struct Compiler* compiler) {
	while (compiler->temporaries) {
		struct Temporary* temporary = compiler->temporaries;
		compiler->temporaries = temporary->next;
		if (!temporary->predicate) {
			emit_destroy_type(compiler, temporary->type, temporary->address);
			continue;
		}
		LLVMValueRef function = LLVMGetBasicBlockParent(LLVMGetInsertBlock(compiler->builder));
		LLVMBasicBlockRef destroy = LLVMAppendBasicBlockInContext(compiler->llvm_context, function, "temporary.destroy");
		LLVMBasicBlockRef end = LLVMAppendBasicBlockInContext(compiler->llvm_context, function, "temporary.end");
		LLVMBuildCondBr(compiler->builder, temporary->predicate, destroy, end);
		LLVMPositionBuilderAtEnd(compiler->builder, destroy);
		emit_destroy_type(compiler, temporary->type, temporary->address);
		LLVMBuildBr(compiler->builder, end);
		LLVMPositionBuilderAtEnd(compiler->builder, end);
	}
}

void emit_construct_storage(struct Compiler* compiler, struct Type type, LLVMValueRef destination, LLVMValueRef source);
void emit_construct_body(struct Compiler* compiler, struct Record* record, LLVMValueRef destination, LLVMValueRef source);

void emit_construct_subobjects(struct Compiler* compiler, struct Record* record, LLVMValueRef destination, LLVMValueRef source) {
	for (struct BaseRelation* base = record->bases; base; base = base->next) {
		if (base->kind != NONVIRTUAL_BASE) {
			continue;
		}
		LLVMValueRef target = LLVMBuildStructGEP2(compiler->builder, record->llvm_body, destination, base->index, "construct_base");
		LLVMValueRef origin = source ? LLVMBuildStructGEP2(compiler->builder, record->llvm_body, source, base->index, "copy_base") : NULL;
		emit_construct_body(compiler, base->record, target, origin);
	}
	for (struct Member* field = record->kind == STRUCT_TYPE ? record->data.members : NULL; field; field = field->next) {
		LLVMValueRef target = LLVMBuildStructGEP2(compiler->builder, record->llvm_body, destination, field->index, "construct_field");
		LLVMValueRef origin = source ? LLVMBuildStructGEP2(compiler->builder, record->llvm_body, source, field->index, "copy_field") : NULL;
		emit_construct_storage(compiler, field->type, target, origin);
	}
}

void emit_construct_body(struct Compiler* compiler, struct Record* record, LLVMValueRef destination, LLVMValueRef source) {
	struct Expression value = { 0 };
	value.kind = NAME_EXPRESSION;
	value.type = (struct Type){ record->kind, { .record = record } };
	struct Argument argument = { 0 };
	argument.expression = &value;
	struct Function* constructor = select_special_function(compiler, record, CONSTRUCTOR_ROLE, source ? &argument : NULL, record->name);
	emit_construct_subobjects(compiler, record, destination, constructor ? NULL : source);
	if (constructor) {
		LLVMValueRef values[] = { destination, source };
		emit_direct_call(compiler, constructor, values, source ? 2 : 1);
	}
}

void emit_construct_virtual_subobjects(struct Compiler* compiler, struct Record* record, LLVMValueRef destination, LLVMValueRef source) {
	for (struct VirtualBase* base = record->virtual_bases; base; base = base->next) {
		LLVMValueRef target = LLVMBuildStructGEP2(compiler->builder, record->llvm_type, destination, base->index + 1, "construct_virtual_base");
		LLVMValueRef origin = source ? emit_base_address(compiler, source, record, find_base_path(compiler, record, base->record).path) : NULL;
		emit_construct_body(compiler, base->record, target, origin);
	}
}

void emit_construct_storage(struct Compiler* compiler, struct Type type, LLVMValueRef destination, LLVMValueRef source) {
	if (type.kind == ARRAY_TYPE) {
		for (uint64_t index = 0; index < type.data.array.count; index++) {
			emit_construct_storage(compiler, *type.data.array.element, emit_array_element(compiler, type, destination, index), source ? emit_array_element(compiler, type, source, index) : NULL);
		}
		return;
	}
	if (type.kind != STRUCT_TYPE && type.kind != UNION_TYPE) {
		LLVMValueRef value = source ? LLVMBuildLoad2(compiler->builder, llvm_type(compiler, type), source, "copy_value") : LLVMConstNull(llvm_type(compiler, type));
		LLVMBuildStore(compiler->builder, value, destination);
		return;
	}
	struct Record* record = type.data.record;
	struct Expression value = { 0 };
	value.kind = NAME_EXPRESSION;
	value.type = type;
	struct Argument argument = { 0 };
	argument.expression = &value;
	struct Function* constructor = select_special_function(compiler, record, CONSTRUCTOR_ROLE, source ? &argument : NULL, record->name);
	if (!source) {
		LLVMBuildStore(compiler->builder, LLVMConstNull(llvm_type(compiler, type)), destination);
	}
	emit_install_views(compiler, type, destination);
	if (constructor) {
		emit_construct_virtual_subobjects(compiler, record, destination, NULL);
		emit_construct_subobjects(compiler, record, destination, NULL);
		LLVMValueRef values[] = { destination, source };
		emit_direct_call(compiler, constructor, values, source ? 2 : 1);
		return;
	}
	if (record->kind == UNION_TYPE) {
		if (source) {
			emit_payload_copy(compiler, type, destination, source);
		}
		return;
	}
	emit_construct_virtual_subobjects(compiler, record, destination, source);
	emit_construct_body(compiler, record, destination, source);
}

struct AssignedSubobject {
	struct Subobject* object;
	struct AssignedSubobject* next;
};

void emit_assign_storage(struct Compiler* compiler, struct Type type, LLVMValueRef destination, LLVMValueRef source);

LLVMValueRef emit_assignment_result_discard(struct Compiler* compiler, struct Function* function, LLVMValueRef result) {
	if (function->result.kind != STRUCT_TYPE && function->result.kind != UNION_TYPE && function->result.kind != ARRAY_TYPE) {
		return NULL;
	}
	struct Expression value = { 0 };
	value.type = function->result;
	LLVMValueRef storage = entry_alloca(compiler, value.type);
	LLVMBuildStore(compiler->builder, result, storage);
	emit_temporary(compiler, &value, storage);
	return storage;
}

void emit_assign_subobject(struct Compiler* compiler, struct Record* complete, struct Subobject* object, LLVMValueRef destination, LLVMValueRef source, struct AssignedSubobject** assigned) {
	for (struct AssignedSubobject* previous = *assigned; previous; previous = previous->next) {
		if (previous->object == object) {
			return;
		}
	}
	struct AssignedSubobject* entry = allocate(compiler, sizeof(struct AssignedSubobject));
	if (!entry) {
		return;
	}
	entry->object = object;
	entry->next = *assigned;
	*assigned = entry;
	struct Type type = { STRUCT_TYPE, { .record = object->record } };
	struct Expression value = { 0 };
	value.kind = CONSTRUCTION_EXPRESSION;
	value.type = type;
	struct Argument argument = { 0 };
	argument.expression = &value;
	struct Function* function = select_special_function(compiler, object->record, ASSIGNMENT_ROLE, &argument, object->record->name);
	LLVMValueRef target = emit_base_address(compiler, destination, complete, object->path);
	LLVMValueRef origin = emit_base_address(compiler, source, complete, object->path);
	if (function) {
		if (object->record->virtual_bases && object != complete->subobjects) {
			diagnostic(compiler, function->name, "generated assignment requires an explicit operator for overlapping virtual-base ownership");
			return;
		}
		struct Type parameter = function->parameters->next->type;
		LLVMValueRef values[] = { target, parameter.kind == REFERENCE_TYPE ? origin : emit_read_value(compiler, type, origin) };
		LLVMValueRef result = emit_direct_call(compiler, function, values, 2);
		emit_assignment_result_discard(compiler, function, result);
		if (parameter.kind == REFERENCE_TYPE) {
			emit_destroy_type(compiler, type, origin);
		}
		return;
	}
	for (struct SubobjectEdge* edge = object->edges; edge; edge = edge->next) {
		emit_assign_subobject(compiler, complete, edge->target, destination, source, assigned);
	}
	for (struct Member* field = object->record->data.members; field; field = field->next) {
		LLVMValueRef target_field = LLVMBuildStructGEP2(compiler->builder, object->record->llvm_body, target, field->index, "assign_field");
		LLVMValueRef source_field = LLVMBuildStructGEP2(compiler->builder, object->record->llvm_body, origin, field->index, "owned_field");
		emit_assign_storage(compiler, field->type, target_field, source_field);
	}
}

void emit_assign_storage(struct Compiler* compiler, struct Type type, LLVMValueRef destination, LLVMValueRef source) {
	if (type.kind == ARRAY_TYPE) {
		for (uint64_t index = 0; index < type.data.array.count; index++) {
			emit_assign_storage(compiler, *type.data.array.element, emit_array_element(compiler, type, destination, index), emit_array_element(compiler, type, source, index));
		}
		return;
	}
	if (type.kind == STRUCT_TYPE) {
		struct AssignedSubobject* assigned = NULL;
		emit_assign_subobject(compiler, type.data.record, type.data.record->subobjects, destination, source, &assigned);
		return;
	}
	if (type.kind == UNION_TYPE) {
		struct Expression value = { 0 };
		value.kind = CONSTRUCTION_EXPRESSION;
		value.type = type;
		struct Argument argument = { 0 };
		argument.expression = &value;
		struct Function* function = select_special_function(compiler, type.data.record, ASSIGNMENT_ROLE, &argument, type.data.record->name);
		if (function) {
			struct Type parameter = function->parameters->next->type;
			LLVMValueRef values[] = { destination, parameter.kind == REFERENCE_TYPE ? source : emit_read_value(compiler, type, source) };
			emit_assignment_result_discard(compiler, function, emit_direct_call(compiler, function, values, 2));
			if (parameter.kind == REFERENCE_TYPE) {
				emit_destroy_type(compiler, type, source);
			}
			return;
		}
	}
	LLVMBuildStore(compiler->builder, LLVMBuildLoad2(compiler->builder, llvm_type(compiler, type), source, "owned_component"), destination);
}

LLVMValueRef emit_expression(struct Compiler* compiler, struct Expression* expression) {
	expression->temporary_address = NULL;
	uint64_t magnitude;
	bool negative;
	struct Token decimal;
	if (integer_constant(expression, &magnitude, &negative)) {
		return expression->type.kind == POINTER_TYPE ? LLVMConstNull(llvm_type(compiler, expression->type)) : LLVMConstInt(llvm_type(compiler, expression->type), expression->type.kind == BOOL_TYPE ? !!magnitude : negative ? UINT64_C(0) - magnitude : magnitude, false);
	}
	if (decimal_constant(expression, &decimal, &negative)) {
		return LLVMConstReal(llvm_type(compiler, expression->type), decimal_value(expression));
	}
	switch (expression->kind) {
	case ACCESS_EXPRESSION:
		return emit_expression(compiler, expression->data.operand);
	case ABANDON_EXPRESSION:
		return NULL;
	case RELOCATE_EXPRESSION: {
		LLVMValueRef address = emit_address(compiler, expression->data.operand);
		expression->temporary_address = address;
		emit_temporary(compiler, expression, address);
		return emit_read_value(compiler, expression->type, address);
	}
	case CONSTRUCTION_EXPRESSION: {
		LLVMValueRef address = entry_alloca(compiler, expression->type);
		expression->data.construction.slot = address;
		expression->temporary_address = address;
		emit_install_views(compiler, expression->type, address);
		struct Function* constructor = expression->data.construction.function;
		struct Argument* argument = expression->data.construction.arguments;
		if (constructor) {
			unsigned count = 1;
			for (struct Argument* item = argument; item; item = item->next) {
				count++;
			}
			LLVMValueRef* values = allocate(compiler, count * sizeof(LLVMValueRef));
			if (!values) {
				return NULL;
			}
			values[0] = address;
			unsigned index = 1;
			for (; argument; argument = argument->next) {
				values[index++] = emit_expression(compiler, argument->expression);
			}
			LLVMBuildStore(compiler->builder, LLVMConstNull(llvm_type(compiler, expression->type)), address);
			emit_install_views(compiler, expression->type, address);
			emit_construct_virtual_subobjects(compiler, expression->type.data.record, address, NULL);
			emit_construct_subobjects(compiler, expression->type.data.record, address, NULL);
			emit_direct_call(compiler, constructor, values, count);
		}
		else {
			LLVMValueRef source = NULL;
			if (argument) {
				struct Expression* value = argument->expression;
				source = emit_materialized_address(compiler, value);
			}
			emit_construct_storage(compiler, expression->type, address, source);
			emit_install_views(compiler, expression->type, address);
		}
		emit_temporary(compiler, expression, address);
		return emit_read_value(compiler, expression->type, address);
	}
	case BOOLEAN_EXPRESSION:
		return LLVMConstInt(llvm_type(compiler, expression->type), expression->data.integer, false);
	case NULLPTR_EXPRESSION:
		return LLVMConstNull(llvm_type(compiler, expression->type));
	case STRING_EXPRESSION: {
		if (expression->data.string.count > UINT_MAX) {
			diagnostic(compiler, expression->span.first, "string literal exceeds LLVM's supported length");
			return NULL;
		}
		LLVMValueRef bytes = LLVMConstStringInContext(compiler->llvm_context, (const char*)expression->data.string.bytes, (unsigned)expression->data.string.count, true);
		char name[64];
		snprintf(name, sizeof(name), ".string.%d", compiler->next_string++);
		LLVMValueRef global = LLVMAddGlobal(compiler->module, LLVMTypeOf(bytes), name);
		LLVMSetInitializer(global, bytes);
		LLVMSetGlobalConstant(global, true);
		LLVMSetLinkage(global, LLVMPrivateLinkage);
		return global;
	}
	case SIZEOF_TYPE_EXPRESSION:
	case SIZEOF_VALUE_EXPRESSION:
		return LLVMConstInt(llvm_type(compiler, expression->type), expression->data.size.size, false);
	case CAST_EXPRESSION: {
		LLVMValueRef value = emit_expression(compiler, expression->data.cast.value);
		if (!value) {
			return NULL;
		}
		struct Type source = expression->data.cast.value->type;
		struct Type target = expression->type;
		if (is_void(target)) {
			return NULL;
		}
		if (target.kind == BOOL_TYPE) {
			return emit_truth(compiler, value, source);
		}

		LLVMTypeRef result = llvm_type(compiler, target);
		if (source.kind == POINTER_TYPE && target.kind == POINTER_TYPE) {
			return value;
		}
		if (source.kind == POINTER_TYPE) {
			return LLVMBuildPtrToInt(compiler->builder, value, result, "cast");
		}
		if (target.kind == POINTER_TYPE) {
			return LLVMBuildIntToPtr(compiler->builder, value, result, "cast");
		}
		if (source.kind == FLOAT_TYPE && target.kind == FLOAT_TYPE) {
			return LLVMBuildFPCast(compiler->builder, value, result, "cast");
		}
		if (source.kind == FLOAT_TYPE) {
			return zero_extended_type(target) ? LLVMBuildFPToUI(compiler->builder, value, result, "cast") : LLVMBuildFPToSI(compiler->builder, value, result, "cast");
		}
		if (target.kind == FLOAT_TYPE) {
			return zero_extended_type(source) ? LLVMBuildUIToFP(compiler->builder, value, result, "cast") : LLVMBuildSIToFP(compiler->builder, value, result, "cast");
		}
		return LLVMBuildIntCast2(compiler->builder, value, result, !zero_extended_type(source), "cast");
	}
	case ENUMERATOR_EXPRESSION:
		return LLVMConstInt(llvm_type(compiler, expression->type), (uint64_t)expression->data.enumerator->value, true);
	case FIELD_EXPRESSION:
	case POINTER_FIELD_EXPRESSION: {
		LLVMValueRef address = emit_address(compiler, expression);
		if (expression->type.kind == FUNCTION_TYPE) {
			return address;
		}
		return address ? emit_read_value(compiler, expression->type, address) : NULL;
	}
	case ADDRESS_EXPRESSION:
		return expression->data.operand->kind == FUNCTION_EXPRESSION ? function_address(compiler, expression->data.operand->data.function) : emit_address(compiler, expression->data.operand);
	case FUNCTION_EXPRESSION:
		return function_address(compiler, expression->data.function);
	case DEREFERENCE_EXPRESSION:
		if (expression->type.kind == FUNCTION_TYPE) {
			return emit_expression(compiler, expression->data.operand);
		}
	case INDEX_EXPRESSION: {
		LLVMValueRef address = emit_address(compiler, expression);
		return address ? emit_read_value(compiler, expression->type, address) : NULL;
	}
	case NAME_EXPRESSION:
		if (expression->type.kind == FUNCTION_TYPE) {
			return emit_address(compiler, expression);
		}
		return emit_read_value(compiler, expression->type, emit_address(compiler, expression));
	case REFERENCE_CAST_EXPRESSION:
		if (expression->type.kind == FUNCTION_TYPE) {
			return emit_address(compiler, expression);
		}
		return emit_read_value(compiler, expression->type, emit_address(compiler, expression));
	case BASE_VALUE_EXPRESSION:
		return emit_read_value(compiler, expression->type, emit_address(compiler, expression));
	case BASE_POINTER_EXPRESSION: {
		struct Expression* source = expression->data.base.value;
		LLVMValueRef value = emit_expression(compiler, source);
		LLVMValueRef null = LLVMConstNull(llvm_type(compiler, expression->type));
		LLVMValueRef empty = LLVMBuildICmp(compiler->builder, LLVMIntEQ, value, null, "null_base");
		LLVMBasicBlockRef previous = LLVMGetInsertBlock(compiler->builder);
		LLVMValueRef function = LLVMGetBasicBlockParent(previous);
		LLVMBasicBlockRef convert = LLVMAppendBasicBlockInContext(compiler->llvm_context, function, "base.convert");
		LLVMBasicBlockRef merge = LLVMAppendBasicBlockInContext(compiler->llvm_context, function, "base.end");
		LLVMBuildCondBr(compiler->builder, empty, merge, convert);
		LLVMPositionBuilderAtEnd(compiler->builder, convert);
		LLVMValueRef adjusted = expression->data.base.kind == BASE_TO_DERIVED ? emit_inverse_base_address(compiler, value, expression->type.data.element->data.record, expression->data.base.path) : emit_base_address(compiler, value, source->type.data.element->data.record, expression->data.base.path);
		LLVMBuildBr(compiler->builder, merge);
		LLVMPositionBuilderAtEnd(compiler->builder, merge);
		LLVMValueRef result = LLVMBuildPhi(compiler->builder, llvm_type(compiler, expression->type), "base_pointer");
		LLVMValueRef values[] = { null, adjusted };
		LLVMBasicBlockRef blocks[] = { previous, convert };
		LLVMAddIncoming(result, values, blocks, 2);
		return result;
	}
	case REFERENCE_INITIALIZATION_EXPRESSION: {
		LLVMValueRef value = emit_expression(compiler, expression->data.binary.right);
		struct Expression* target = storage_expression(expression->data.binary.left);
		LLVMValueRef slot = target->kind == NAME_EXPRESSION ? target->data.variable.binding->slot : emit_field_slot(compiler, target);
		LLVMBuildStore(compiler->builder, value, slot);
		return value;
	}
	case REFERENCE_BINDING_EXPRESSION:
		if (expression->data.operand->kind == FUNCTION_EXPRESSION) {
			return function_address(compiler, expression->data.operand->data.function);
		}
		return emit_materialized_address(compiler, expression->data.operand);
	case NEGATE_EXPRESSION: {
		LLVMValueRef operand = emit_expression(compiler, expression->data.operand);
		if (!operand) {
			return NULL;
		}
		return expression->type.kind == FLOAT_TYPE ? LLVMBuildFNeg(compiler->builder, operand, "negative") : LLVMBuildNeg(compiler->builder, operand, "negative");
	}
	case VALUE_INITIALIZATION_EXPRESSION:
	case ASSIGNMENT_EXPRESSION: {
		struct Expression* target = expression->data.binary.left;
		LLVMValueRef address = emit_address(compiler, target);
		expression->assignment_address = address;
		if (expression->assignment_function) {
			LLVMValueRef values[] = { address, emit_expression(compiler, expression->data.binary.right) };
			LLVMValueRef result = emit_direct_call(compiler, expression->assignment_function, values, 2);
			expression->assignment_address = expression->assignment_function->result.kind == REFERENCE_TYPE ? result : NULL;
			if (expression->use != TRANSFERRED_VALUE) {
				expression->temporary_address = emit_assignment_result_discard(compiler, expression->assignment_function, result);
			}
			return expression->assignment_function->result.kind == REFERENCE_TYPE ? emit_read_value(compiler, expression->type, result) : result;
		}
		const struct Operator* operation = find_operator(expression->data.binary.operation);
		LLVMValueRef left = operation->compound ? LLVMBuildLoad2(compiler->builder, llvm_type(compiler, target->type), address, "previous") : NULL;
		LLVMValueRef value = emit_expression(compiler, expression->data.binary.right);
		if (operation->compound) {
			value = emit_binary(compiler, operation->compound, left, value, target->type, expression->data.binary.right->type);
		}
		if (expression->kind == VALUE_INITIALIZATION_EXPRESSION && target->kind != BASE_VALUE_EXPRESSION) {
			emit_install_views(compiler, target->type, address);
		}
		if (expression->kind == ASSIGNMENT_EXPRESSION && (target->type.kind == STRUCT_TYPE || target->type.kind == UNION_TYPE)) {
			LLVMValueRef source = entry_alloca(compiler, target->type);
			LLVMBuildStore(compiler->builder, value, source);
			emit_install_views(compiler, target->type, source);
			emit_assign_storage(compiler, target->type, address, source);
			return emit_read_value(compiler, target->type, address);
		}
		emit_store_value(compiler, target->type, address, value);
		return type_has_views(target->type) ? emit_read_value(compiler, target->type, address) : value;
	}
	case NOT_EXPRESSION: {
		LLVMValueRef value = emit_truth(compiler, emit_expression(compiler, expression->data.operand), expression->data.operand->type);
		value = LLVMBuildNot(compiler->builder, value, "not");
		return value;
	}
	case COMPLEMENT_EXPRESSION:
		return LLVMBuildNot(compiler->builder, emit_expression(compiler, expression->data.operand), "complement");
	case PREFIX_UPDATE_EXPRESSION:
	case POSTFIX_UPDATE_EXPRESSION: {
		struct Expression* target = expression->data.update.value;
		LLVMValueRef address = emit_address(compiler, target);
		LLVMValueRef previous = LLVMBuildLoad2(compiler->builder, llvm_type(compiler, target->type), address, "previous");
		struct Type step_type = target->type.kind == POINTER_TYPE ? (struct Type){ SIGNED_INTEGER_TYPE, { sizeof(void*) * CHAR_BIT } } : target->type;
		LLVMValueRef one = step_type.kind == FLOAT_TYPE ? LLVMConstReal(llvm_type(compiler, step_type), 1.0) : LLVMConstInt(llvm_type(compiler, step_type), 1, false);
		LLVMValueRef value = emit_binary(compiler, expression->data.update.operation == INCREMENT ? '+' : '-', previous, one, target->type, step_type);
		LLVMBuildStore(compiler->builder, value, address);
		return expression->kind == PREFIX_UPDATE_EXPRESSION ? value : previous;
	}
	case BINARY_EXPRESSION: {
		const struct Operator* operation = find_operator(expression->data.binary.operation);
		LLVMValueRef left = emit_expression(compiler, expression->data.binary.left);
		if (operation->kind == LOGICAL_OPERATOR) {
			LLVMValueRef truth = emit_truth(compiler, left, expression->data.binary.left->type);
			LLVMBasicBlockRef previous = LLVMGetInsertBlock(compiler->builder);
			LLVMValueRef function = LLVMGetBasicBlockParent(previous);
			LLVMBasicBlockRef right_block = LLVMAppendBasicBlockInContext(compiler->llvm_context, function, "logical.right");
			LLVMBasicBlockRef merge = LLVMAppendBasicBlockInContext(compiler->llvm_context, function, "logical.end");
			if (operation->token == LOGICAL_AND) {
				LLVMBuildCondBr(compiler->builder, truth, right_block, merge);
			}
			else {
				LLVMBuildCondBr(compiler->builder, truth, merge, right_block);
			}
			LLVMPositionBuilderAtEnd(compiler->builder, right_block);
			struct Temporary* boundary = compiler->temporaries;
			LLVMValueRef right = emit_truth(compiler, emit_expression(compiler, expression->data.binary.right), expression->data.binary.right->type);
			LLVMBasicBlockRef right_end = LLVMGetInsertBlock(compiler->builder);
			LLVMBuildBr(compiler->builder, merge);
			LLVMPositionBuilderAtEnd(compiler->builder, merge);
			LLVMValueRef phi = LLVMBuildPhi(compiler->builder, LLVMInt1TypeInContext(compiler->llvm_context), "logical");
			LLVMValueRef values[] = { LLVMConstInt(LLVMInt1TypeInContext(compiler->llvm_context), operation->token == LOGICAL_OR, false), right };
			LLVMBasicBlockRef blocks[] = { previous, right_end };
			LLVMAddIncoming(phi, values, blocks, 2);
			for (struct Temporary* temporary = compiler->temporaries; temporary && temporary != boundary; temporary = temporary->next) {
				LLVMValueRef predicate = LLVMBuildPhi(compiler->builder, LLVMInt1TypeInContext(compiler->llvm_context), "temporary_active");
				LLVMValueRef active[] = { LLVMConstInt(LLVMInt1TypeInContext(compiler->llvm_context), 0, false), temporary->predicate ? temporary->predicate : LLVMConstInt(LLVMInt1TypeInContext(compiler->llvm_context), 1, false) };
				LLVMAddIncoming(predicate, active, blocks, 2);
				temporary->predicate = predicate;
			}
			return phi;
		}
		LLVMValueRef right = emit_expression(compiler, expression->data.binary.right);
		return emit_binary(compiler, operation->token, left, right, expression->data.binary.left->type, expression->data.binary.right->type);
	}
	case CALL_EXPRESSION: {
		LLVMValueRef call = emit_call(compiler, expression);
		if (call && expression->data.call.binding->result.kind == REFERENCE_TYPE && expression->type.kind != FUNCTION_TYPE) {
			return emit_read_value(compiler, expression->type, call);
		}
		if (call && (expression->type.kind == STRUCT_TYPE || expression->type.kind == UNION_TYPE || expression->type.kind == ARRAY_TYPE)) {
			LLVMValueRef slot = entry_alloca(compiler, expression->type);
			LLVMBuildStore(compiler->builder, call, slot);
			expression->temporary_address = slot;
			emit_temporary(compiler, expression, slot);
		}
		return call;
	}
	default:
		diagnostic(compiler, expression->span.first, "invalid expression in LLVM generation");
		return NULL;
	}
}

void emit_statements(struct Compiler* compiler, struct Statement* statements, struct Flow* flow);

void branch_open(struct Compiler* compiler, LLVMBasicBlockRef target) {
	if (!LLVMGetBasicBlockTerminator(LLVMGetInsertBlock(compiler->builder))) {
		LLVMBuildBr(compiler->builder, target);
	}
}

void emit_statement(struct Compiler* compiler, struct Statement* statement, struct Flow* flow) {
	LLVMValueRef function = LLVMGetBasicBlockParent(LLVMGetInsertBlock(compiler->builder));
	switch (statement->kind) {
	case EMPTY_STATEMENT:
		break;
	case BLOCK_STATEMENT:
		emit_statements(compiler, statement->data.block, flow);
		break;
	case DECLARATION_STATEMENT: {
		struct Variable* variable = statement->data.declaration.variable;
		variable->slot = entry_alloca(compiler, variable->type);
		if (statement->data.declaration.initializer) {
			LLVMValueRef value = emit_expression(compiler, statement->data.declaration.initializer);
			emit_install_views(compiler, variable->type, variable->slot);
			emit_store_value(compiler, variable->type, variable->slot, value);
		}
		else if (!statement->data.declaration.initialization) {
			LLVMBuildStore(compiler->builder, LLVMConstNull(llvm_type(compiler, variable->type)), variable->slot);
			emit_install_views(compiler, variable->type, variable->slot);
		}
		if (statement->data.declaration.initialization) {
			if (statement->data.declaration.initialization->data.block) {
				emit_install_views(compiler, variable->type, variable->slot);
			}
			emit_statement(compiler, statement->data.declaration.initialization, flow);
		}
		emit_full_expression_end(compiler);
		break;
	}
	case EXPRESSION_STATEMENT:
		emit_expression(compiler, statement->data.expression);
		emit_full_expression_end(compiler);
		break;
	case RETURN_STATEMENT:
		if (statement->data.expression) {
			LLVMValueRef value = emit_expression(compiler, statement->data.expression);
			if (compiler->emitted_function && compiler->emitted_signature) {
				value = emit_covariant_result(compiler, value, compiler->emitted_function->result, compiler->emitted_signature->result);
			}
			emit_full_expression_end(compiler);
			emit_cleanups(compiler, statement->cleanup);
			LLVMBuildRet(compiler->builder, value);
		}
		else {
			emit_cleanups(compiler, statement->cleanup);
			LLVMBuildRetVoid(compiler->builder);
		}
		break;
	case BREAK_STATEMENT:
		emit_cleanups(compiler, statement->cleanup);
		LLVMBuildBr(compiler->builder, flow->break_target);
		break;
	case CONTINUE_STATEMENT:
		while (flow->kind != LOOP_FLOW) {
			flow = flow->parent;
		}
		emit_cleanups(compiler, statement->cleanup);
		LLVMBuildBr(compiler->builder, flow->continue_target);
		break;
	case IF_STATEMENT: {
		LLVMValueRef condition = emit_truth(compiler, emit_expression(compiler, statement->data.branch.condition), statement->data.branch.condition->type);
		emit_full_expression_end(compiler);
		LLVMBasicBlockRef first = LLVMAppendBasicBlockInContext(compiler->llvm_context, function, "if.then");
		LLVMBasicBlockRef second = LLVMAppendBasicBlockInContext(compiler->llvm_context, function, "if.else");
		LLVMBasicBlockRef end = LLVMAppendBasicBlockInContext(compiler->llvm_context, function, "if.end");
		LLVMBuildCondBr(compiler->builder, condition, first, second);
		LLVMPositionBuilderAtEnd(compiler->builder, first);
		emit_statement(compiler, statement->data.branch.first, flow);
		branch_open(compiler, end);
		LLVMPositionBuilderAtEnd(compiler->builder, second);
		if (statement->data.branch.second) {
			emit_statement(compiler, statement->data.branch.second, flow);
		}
		branch_open(compiler, end);
		LLVMPositionBuilderAtEnd(compiler->builder, end);
		if (!(statement->exits & NORMAL_EXIT)) {
			LLVMBuildUnreachable(compiler->builder);
		}
		break;
	}
	case WHILE_STATEMENT:
	case FOR_STATEMENT: {
		if (statement->data.loop.initializer) {
			emit_statement(compiler, statement->data.loop.initializer, flow);
		}
		LLVMBasicBlockRef condition_block = LLVMAppendBasicBlockInContext(compiler->llvm_context, function, "loop.condition");
		LLVMBasicBlockRef body = LLVMAppendBasicBlockInContext(compiler->llvm_context, function, "loop.body");
		LLVMBasicBlockRef step = LLVMAppendBasicBlockInContext(compiler->llvm_context, function, "loop.step");
		LLVMBasicBlockRef end = LLVMAppendBasicBlockInContext(compiler->llvm_context, function, "loop.end");
		struct Flow loop = { LOOP_FLOW, flow, end, step };
		branch_open(compiler, condition_block);
		LLVMPositionBuilderAtEnd(compiler->builder, condition_block);
		if (statement->data.loop.condition) {
			LLVMValueRef condition = emit_truth(compiler, emit_expression(compiler, statement->data.loop.condition), statement->data.loop.condition->type);
			emit_full_expression_end(compiler);
			LLVMBuildCondBr(compiler->builder, condition, body, end);
		}
		else {
			LLVMBuildBr(compiler->builder, body);
		}
		LLVMPositionBuilderAtEnd(compiler->builder, body);
		emit_statement(compiler, statement->data.loop.body, &loop);
		branch_open(compiler, step);
		LLVMPositionBuilderAtEnd(compiler->builder, step);
		if (statement->data.loop.step) {
			emit_expression(compiler, statement->data.loop.step);
			emit_full_expression_end(compiler);
		}
		branch_open(compiler, condition_block);
		LLVMPositionBuilderAtEnd(compiler->builder, end);
		if (!(statement->exits & NORMAL_EXIT)) {
			LLVMBuildUnreachable(compiler->builder);
		}
		break;
	}
	case SWITCH_STATEMENT: {
		LLVMValueRef value = emit_expression(compiler, statement->data.selection.value);
		emit_full_expression_end(compiler);
		LLVMBasicBlockRef end = LLVMAppendBasicBlockInContext(compiler->llvm_context, function, "switch.end");
		struct Flow selection = { SWITCH_FLOW, flow, end, NULL };
		unsigned count = 0;
		for (struct Case* item = statement->data.selection.cases; item; item = item->next) {
			count++;
		}
		LLVMBasicBlockRef* blocks = count ? allocate(compiler, count * sizeof(LLVMBasicBlockRef)) : NULL;
		if (count && !blocks) {
			return;
		}
		LLVMBasicBlockRef default_block = end;
		unsigned index = 0;
		for (struct Case* item = statement->data.selection.cases; item; item = item->next) {
			blocks[index] = LLVMAppendBasicBlockInContext(compiler->llvm_context, function, "switch.case");
			if (!item->value) {
				default_block = blocks[index];
			}
			index++;
		}
		LLVMValueRef instruction = LLVMBuildSwitch(compiler->builder, value, default_block, count);
		index = 0;
		for (struct Case* item = statement->data.selection.cases; item; item = item->next) {
			if (item->value) {
				LLVMAddCase(instruction, LLVMConstInt(LLVMTypeOf(value), item->constant, true), blocks[index]);
			}
			LLVMPositionBuilderAtEnd(compiler->builder, blocks[index]);
			emit_statements(compiler, item->statements, &selection);
			if (!LLVMGetBasicBlockTerminator(LLVMGetInsertBlock(compiler->builder))) {
				emit_cleanups(compiler, item->cleanup);
			}
			branch_open(compiler, item->next ? blocks[index + 1] : end);
			index++;
		}
		LLVMPositionBuilderAtEnd(compiler->builder, end);
		if (!(statement->exits & NORMAL_EXIT)) {
			LLVMBuildUnreachable(compiler->builder);
		}
		break;
	}
	default:
		diagnostic(compiler, statement->span.first, "invalid statement in LLVM generation");
		break;
	}
	if (statement->kind != RETURN_STATEMENT && statement->kind != BREAK_STATEMENT && statement->kind != CONTINUE_STATEMENT && !LLVMGetBasicBlockTerminator(LLVMGetInsertBlock(compiler->builder))) {
		emit_cleanups(compiler, statement->cleanup);
	}
}

void emit_statements(struct Compiler* compiler, struct Statement* statements, struct Flow* flow) {
	for (struct Statement* statement = statements; statement && !compiler->failed; statement = statement->next) {
		if (LLVMGetBasicBlockTerminator(LLVMGetInsertBlock(compiler->builder))) {
			break;
		}
		emit_statement(compiler, statement, flow);
	}
}

void emit_function_body(struct Compiler* compiler, struct Function* function, struct Function* signature, LLVMValueRef target, uint64_t receiver_offset) {
	if (function->kind != FUNCTION_DEFINITION) {
		return;
	}
	compiler->emitted_function = function;
	compiler->emitted_signature = signature;
	LLVMBasicBlockRef block = LLVMAppendBasicBlockInContext(compiler->llvm_context, target, "entry");
	LLVMPositionBuilderAtEnd(compiler->builder, block);
	unsigned index = 0;
	for (struct Variable* parameter = function->parameters; parameter; parameter = parameter->next) {
		LLVMValueRef value = LLVMGetParam(target, index);
		if (!index && receiver_offset) {
			LLVMValueRef offset = LLVMConstInt(LLVMInt64TypeInContext(compiler->llvm_context), receiver_offset, true);
			value = LLVMBuildGEP2(compiler->builder, LLVMInt8TypeInContext(compiler->llvm_context), value, &offset, 1, "receiver");
		}
		index++;
		parameter->slot = entry_alloca(compiler, parameter->type);
		emit_install_views(compiler, parameter->type, parameter->slot);
		emit_store_value(compiler, parameter->type, parameter->slot, value);
	}
	emit_statements(compiler, function->statements, NULL);
	if (!compiler->failed && !LLVMGetBasicBlockTerminator(LLVMGetInsertBlock(compiler->builder))) {
		emit_cleanups(compiler, function->cleanup);
		LLVMBuildRetVoid(compiler->builder);
	}
	compiler->emitted_function = NULL;
	compiler->emitted_signature = NULL;
}

void emit_function(struct Compiler* compiler, struct Function* function) {
	emit_function_body(compiler, function, function, function->llvm_value, 0);
}

unsigned function_parameter_count(struct Function* function) {
	unsigned count = 0;
	for (struct Variable* parameter = function->parameters; parameter; parameter = parameter->next) {
		count++;
	}
	return count;
}

void emit_dispatch_wrapper(struct Compiler* compiler, struct Function* function) {
	LLVMBasicBlockRef entry = LLVMAppendBasicBlockInContext(compiler->llvm_context, function->dispatch, "entry");
	LLVMPositionBuilderAtEnd(compiler->builder, entry);
	struct Record* owner = function->interface;
	struct VirtualCallable* callable = owner->callables;
	while (callable && callable->function != function) {
		callable = callable->next;
	}
	if (!callable) {
		diagnostic(compiler, function->name, "virtual callable has no owning interface slot");
		return;
	}
	unsigned count = function_parameter_count(function);
	LLVMValueRef* arguments = allocate(compiler, count * sizeof(LLVMValueRef));
	if (!arguments) {
		return;
	}
	for (unsigned index = 0; index < count; index++) {
		arguments[index] = LLVMGetParam(function->dispatch, index);
	}
	LLVMTypeRef pointer = LLVMPointerTypeInContext(compiler->llvm_context, 0);
	LLVMValueRef header = LLVMBuildStructGEP2(compiler->builder, owner->llvm_body, arguments[0], 0, "view_slot");
	LLVMValueRef view = LLVMBuildLoad2(compiler->builder, pointer, header, "view");
	LLVMValueRef functions = LLVMBuildStructGEP2(compiler->builder, runtime_view_type(compiler, owner), view, 1, "functions");
	LLVMTypeRef array = LLVMArrayType2(pointer, virtual_callable_count(owner));
	LLVMTypeRef integer = LLVMInt64TypeInContext(compiler->llvm_context);
	LLVMValueRef indices[] = { LLVMConstNull(integer), LLVMConstInt(integer, callable->index, false) };
	LLVMValueRef slot = LLVMBuildGEP2(compiler->builder, array, functions, indices, 2, "callable_slot");
	LLVMValueRef callee = LLVMBuildLoad2(compiler->builder, pointer, slot, "implementation");
	LLVMValueRef call = LLVMBuildCall2(compiler->builder, function->llvm_type, callee, arguments, count, is_void(function->result) ? "" : "result");
	LLVMSetInstructionCallConv(call, LLVMGetFunctionCallConv(function->llvm_value));
	apply_signature_attributes(compiler, call, function, true);
	if (function->arity == VARIADIC_PARAMETERS) {
		LLVMSetTailCallKind(call, LLVMTailCallKindMustTail);
	}
	if (is_void(function->result)) {
		LLVMBuildRetVoid(compiler->builder);
		return;
	}
	LLVMBuildRet(compiler->builder, call);
}

bool covariant_result_needs_adjustment(struct Compiler* compiler, struct Type source, struct Type target) {
	if (type_equal(source, target)) {
		return false;
	}
	struct BaseSearch selection = find_base_path(compiler, source.data.element->data.record, target.data.element->data.record);
	struct Record* record = source.data.element->data.record;
	uint64_t offset = 0;
	for (struct BasePath* path = selection.path; path; path = path->next) {
		if (path->relation->kind == VIRTUAL_BASE) {
			return true;
		}
		offset += LLVMOffsetOfElement(compiler->layout, record->llvm_body, path->relation->index);
		record = path->relation->record;
	}
	return offset != 0;
}

void emit_dispatch_thunk(struct Compiler* compiler, struct DispatchThunk* thunk) {
	struct Function* signature = thunk->callable->function;
	struct Function* implementation = thunk->target->function;
	uint64_t offset = subobject_offset(compiler, thunk->complete, thunk->target->object->path) - subobject_offset(compiler, thunk->complete, thunk->origin->path);
	if (signature->arity == VARIADIC_PARAMETERS && covariant_result_needs_adjustment(compiler, implementation->result, signature->result)) {
		if (implementation->kind != FUNCTION_DEFINITION) {
			diagnostic(compiler, implementation->name, "covariant variadic thunk requires the implementation body");
			return;
		}
		emit_function_body(compiler, implementation, signature, thunk->value, offset);
		return;
	}
	LLVMBasicBlockRef entry = LLVMAppendBasicBlockInContext(compiler->llvm_context, thunk->value, "entry");
	LLVMPositionBuilderAtEnd(compiler->builder, entry);
	unsigned count = function_parameter_count(signature);
	LLVMValueRef* arguments = allocate(compiler, count * sizeof(LLVMValueRef));
	if (!arguments) {
		return;
	}
	for (unsigned index = 0; index < count; index++) {
		arguments[index] = LLVMGetParam(thunk->value, index);
	}
	if (offset) {
		LLVMValueRef adjustment = LLVMConstInt(LLVMInt64TypeInContext(compiler->llvm_context), offset, true);
		arguments[0] = LLVMBuildGEP2(compiler->builder, LLVMInt8TypeInContext(compiler->llvm_context), arguments[0], &adjustment, 1, "receiver");
	}
	LLVMValueRef call = LLVMBuildCall2(compiler->builder, implementation->llvm_type, implementation->llvm_value, arguments, count, is_void(implementation->result) ? "" : "result");
	LLVMSetInstructionCallConv(call, LLVMGetFunctionCallConv(implementation->llvm_value));
	apply_signature_attributes(compiler, call, implementation, true);
	if (signature->arity == VARIADIC_PARAMETERS) {
		LLVMSetTailCallKind(call, LLVMTailCallKindMustTail);
	}
	if (is_void(signature->result)) {
		LLVMBuildRetVoid(compiler->builder);
		return;
	}
	LLVMValueRef result = signature->arity == VARIADIC_PARAMETERS ? call : emit_covariant_result(compiler, call, implementation->result, signature->result);
	LLVMBuildRet(compiler->builder, result);
}

void emit_runtime_dispatch(struct Compiler* compiler) {
	define_runtime_views(compiler);
	while (true) {
		if (compiler->failed) {
			break;
		}
		struct DispatchThunk* thunk = compiler->thunks;
		while (thunk && LLVMCountBasicBlocks(thunk->value)) {
			thunk = thunk->next;
		}
		if (thunk) {
			emit_dispatch_thunk(compiler, thunk);
			define_runtime_views(compiler);
			continue;
		}
		struct FunctionSymbol* symbol = compiler->symbols;
		while (symbol && (!symbol->function->dispatch || LLVMCountBasicBlocks(symbol->function->dispatch))) {
			symbol = symbol->next;
		}
		if (!symbol) {
			break;
		}
		emit_dispatch_wrapper(compiler, symbol->function);
	}
}

void emit_entry(struct Compiler* compiler) {
	LLVMTypeRef parameters[] = { LLVMInt32TypeInContext(compiler->llvm_context), LLVMPointerTypeInContext(compiler->llvm_context, 0) };
	LLVMTypeRef signature = LLVMFunctionType(parameters[0], parameters, 2, false);
	LLVMValueRef main = LLVMAddFunction(compiler->module, "main", signature);
	LLVMSetFunctionCallConv(main, LLVMCCallConv);
	LLVMPositionBuilderAtEnd(compiler->builder, LLVMAppendBasicBlockInContext(compiler->llvm_context, main, "entry"));
	LLVMValueRef arguments[] = { LLVMGetParam(main, 0), LLVMGetParam(main, 1) };
	LLVMValueRef call = LLVMBuildCall2(compiler->builder, compiler->entry->llvm_type, compiler->entry->llvm_value, arguments, compiler->entry->parameters ? 2 : 0, is_void(compiler->entry->result) ? "" : "result");
	LLVMSetInstructionCallConv(call, LLVMCCallConv);
	LLVMBuildRet(compiler->builder, is_void(compiler->entry->result) ? LLVMConstInt(parameters[0], 0, false) : call);
}

void initialize_backend(struct Compiler* compiler) {
#if defined(RUTILE_LLVM_VERSION_MAJOR) && defined(RUTILE_LLVM_VERSION_MINOR) && defined(RUTILE_LLVM_VERSION_PATCH)
	unsigned major;
	unsigned minor;
	unsigned patch;
	LLVMGetVersion(&major, &minor, &patch);
	if (major != RUTILE_LLVM_VERSION_MAJOR || minor != RUTILE_LLVM_VERSION_MINOR || patch != RUTILE_LLVM_VERSION_PATCH) {
		diagnostic(compiler, compiler->functions ? compiler->functions->name : compiler->token, "LLVM runtime version mismatch: expected %d.%d.%d, loaded %u.%u.%u", RUTILE_LLVM_VERSION_MAJOR, RUTILE_LLVM_VERSION_MINOR, RUTILE_LLVM_VERSION_PATCH, major, minor, patch);
		return;
	}
#endif
	if (LLVMInitializeNativeTarget() || LLVMInitializeNativeAsmPrinter()) {
		diagnostic(compiler, compiler->functions ? compiler->functions->name : compiler->token, "LLVM native target initialization failed");
		return;
	}
	compiler->llvm_context = LLVMContextCreate();
	compiler->module = LLVMModuleCreateWithNameInContext("rutile", compiler->llvm_context);
	compiler->builder = LLVMCreateBuilderInContext(compiler->llvm_context);
	char* triple = LLVMGetDefaultTargetTriple();
	char* error = NULL;
	LLVMTargetRef target = NULL;
	if (LLVMGetTargetFromTriple(triple, &target, &error)) {
		diagnostic(compiler, compiler->functions ? compiler->functions->name : compiler->token, "LLVM target is unavailable: %s", error ? error : "unknown target");
		if (error) {
			LLVMDisposeMessage(error);
		}
		LLVMDisposeMessage(triple);
		return;
	}
	const LLVMCodeGenOptLevel levels[] = { LLVMCodeGenLevelNone, LLVMCodeGenLevelLess, LLVMCodeGenLevelDefault, LLVMCodeGenLevelAggressive };
	compiler->machine = LLVMCreateTargetMachine(target, triple, "generic", "", levels[compiler->optimization], LLVMRelocPIC, LLVMCodeModelDefault);
	if (!compiler->machine) {
		diagnostic(compiler, compiler->functions ? compiler->functions->name : compiler->token, "LLVM could not create a native target machine");
		LLVMDisposeMessage(triple);
		return;
	}
	LLVMSetTarget(compiler->module, triple);
	LLVMDisposeMessage(triple);
	compiler->layout = LLVMCreateTargetDataLayout(compiler->machine);
	LLVMTargetDataRef layout = compiler->layout;
	if (LLVMPointerSize(layout) != sizeof(void*)) {
		diagnostic(compiler, compiler->functions ? compiler->functions->name : compiler->token, "LLVM native pointer width does not match this compiler host");
		return;
	}
	char* description = LLVMCopyStringRepOfTargetData(layout);
	LLVMSetDataLayout(compiler->module, description);
	LLVMDisposeMessage(description);
}

bool verify_module(struct Compiler* compiler) {
	char* message = NULL;
	bool invalid = LLVMVerifyModule(compiler->module, LLVMReturnStatusAction, &message);
	if (invalid) {
		diagnostic(compiler, compiler->entry ? compiler->entry->name : compiler->token, "LLVM module verification failed: %s", message ? message : "unknown verifier failure");
	}
	if (message) {
		LLVMDisposeMessage(message);
	}
	return !invalid;
}

void optimize_module(struct Compiler* compiler) {
	if (!verify_module(compiler)) {
		return;
	}
	const char* pipelines[] = { "default<O0>", "default<O1>", "default<O2>", "default<O3>" };
	LLVMPassBuilderOptionsRef options = LLVMCreatePassBuilderOptions();
	LLVMPassBuilderOptionsSetVerifyEach(options, true);
	LLVMErrorRef error = LLVMRunPasses(compiler->module, pipelines[compiler->optimization], compiler->machine, options);
	LLVMDisposePassBuilderOptions(options);
	if (error) {
		char* message = LLVMGetErrorMessage(error);
		diagnostic(compiler, compiler->entry ? compiler->entry->name : compiler->token, "LLVM optimization failed: %s", message);
		LLVMDisposeErrorMessage(message);
		return;
	}
	verify_module(compiler);
}

void publish_object(struct Compiler* compiler, const char* output_path) {
	LLVMMemoryBufferRef buffer = NULL;
	char* error = NULL;
	if (LLVMTargetMachineEmitToMemoryBuffer(compiler->machine, compiler->module, LLVMObjectFile, &error, &buffer)) {
		diagnostic(compiler, compiler->entry ? compiler->entry->name : compiler->token, "LLVM object generation failed: %s", error ? error : "unknown target failure");
		if (error) {
			LLVMDisposeMessage(error);
		}
		if (buffer) {
			LLVMDisposeMemoryBuffer(buffer);
		}
		return;
	}
	FILE* output = compiler->open_file(output_path, "wb");
	if (!output) {
		diagnostic(compiler, compiler->entry ? compiler->entry->name : compiler->token, "cannot open object output '%s': %s", output_path, strerror(errno));
		LLVMDisposeMemoryBuffer(buffer);
		return;
	}
	size_t size = LLVMGetBufferSize(buffer);
	bool written = fwrite(LLVMGetBufferStart(buffer), 1, size, output) == size;
	if (fclose(output)) {
		written = false;
	}
	LLVMDisposeMemoryBuffer(buffer);
	if (!written) {
		diagnostic(compiler, compiler->entry ? compiler->entry->name : compiler->token, "cannot write object output '%s'", output_path);
		remove(output_path);
	}
}

void parse_source(struct Compiler* compiler, const char* path) {
	compiler->token = (struct Token){ 0, NULL, 0, 1, 1, "<source>" };
	char* owned_path = allocate(compiler, strlen(path) + 1);
	if (!owned_path) {
		return;
	}
	strcpy(owned_path, path);
	path = owned_path;
	compiler->token = (struct Token){ 0, NULL, 0, 1, 1, path };
	FILE* source = compiler->open_file(path, "rb");
	if (!source) {
		diagnostic(compiler, compiler->token, "cannot open source: %s", strerror(errno));
		return;
	}
	if (fseek(source, 0, SEEK_END)) {
		diagnostic(compiler, compiler->token, "cannot seek source");
		fclose(source);
		return;
	}
	long length = ftell(source);
	if (length < 0 || fseek(source, 0, SEEK_SET)) {
		diagnostic(compiler, compiler->token, "cannot determine source size");
		fclose(source);
		return;
	}
	char* bytes = allocate(compiler, (size_t)length + 1);
	if (!bytes) {
		fclose(source);
		return;
	}
	bool loaded = fread(bytes, 1, (size_t)length, source) == (size_t)length;
	fclose(source);
	if (!loaded) {
		diagnostic(compiler, compiler->token, "cannot read source");
		return;
	}
	if (memchr(bytes, 0, (size_t)length)) {
		diagnostic(compiler, compiler->token, "source contains a NUL byte");
		return;
	}
	tokenize(compiler, path, bytes);
}

void rutile_compiler_clear(struct Compiler* compiler) {
	if (compiler->layout) {
		LLVMDisposeTargetData(compiler->layout);
	}
	if (compiler->builder) {
		LLVMDisposeBuilder(compiler->builder);
	}
	if (compiler->module) {
		LLVMDisposeModule(compiler->module);
	}
	if (compiler->machine) {
		LLVMDisposeTargetMachine(compiler->machine);
	}
	if (compiler->llvm_context) {
		LLVMContextDispose(compiler->llvm_context);
	}
	free(compiler->tokens);
	while (compiler->allocations) {
		struct Allocation* allocation = compiler->allocations;
		compiler->allocations = allocation->next;
		free(allocation->memory);
		free(allocation);
	}
	*compiler = (struct Compiler){ 0 };
}

struct Compiler* rutile_compiler_create(void) {
	return calloc(1, sizeof(struct Compiler));
}

void rutile_compiler_destroy(struct Compiler* compiler) {
	if (!compiler) {
		return;
	}
	rutile_compiler_clear(compiler);
	free(compiler);
}

int compile_module(struct Compiler* compiler, const char* const* source_paths, size_t source_count, const char* output_path, FILE* (*open_file)(const char*, const char*), const char* entry, int optimization, FILE* diagnostics, enum CompilationMode mode) {
	if (!compiler) {
		fprintf(diagnostics ? diagnostics : stderr, "error: compiler context unavailable\n");
		return 1;
	}
	rutile_compiler_clear(compiler);
	compiler->mode = mode;
	compiler->diagnostics = diagnostics ? diagnostics : stderr;
	compiler->open_file = open_file ? open_file : fopen;
	compiler->optimization = optimization;
	if (optimization < 0 || optimization > 3) {
		fprintf(compiler->diagnostics, "error: optimization level must be between 0 and 3\n");
		compiler->failed = true;
		compiler->diagnostics = NULL;
		compiler->open_file = NULL;
		return 1;
	}
	if (!source_count) {
		fprintf(compiler->diagnostics, "error: no source files\n");
		compiler->failed = true;
		compiler->diagnostics = NULL;
		compiler->open_file = NULL;
		return 1;
	}
	for (size_t i = 0; i < source_count; i++) {
		if (!strcmp(source_paths[i], output_path)) {
			fprintf(compiler->diagnostics, "%s: error: output cannot overwrite a source file\n", output_path);
			compiler->failed = true;
			compiler->diagnostics = NULL;
			compiler->open_file = NULL;
			return 1;
		}
	}
	for (size_t i = 0; i < source_count && !compiler->failed; i++) {
		parse_source(compiler, source_paths[i]);
	}
	for (size_t first = 0; first < compiler->token_count && !compiler->failed; first = compiler->token_index) {
		parse_functions(compiler, first);
	}
	if (!compiler->failed) {
		initialize_backend(compiler);
	}
	if (!compiler->failed) {
		analyze(compiler);
	}
	if (!compiler->failed && mode == EXECUTABLE_COMPILATION) {
		select_entry(compiler, entry ? entry : "main");
	}
	if (!compiler->failed) {
		declare_llvm_functions(compiler);
		for (struct FunctionSymbol* symbol = compiler->symbols; symbol && !compiler->failed; symbol = symbol->next) {
			emit_function(compiler, symbol->function);
		}
		emit_runtime_dispatch(compiler);
		if (!compiler->failed && mode == EXECUTABLE_COMPILATION) {
			emit_entry(compiler);
		}
	}
	if (!compiler->failed) {
		optimize_module(compiler);
	}
	if (!compiler->failed) {
		publish_object(compiler, output_path);
	}
	compiler->diagnostics = NULL;
	compiler->open_file = NULL;
	return compiler->failed ? 1 : 0;
}

int rutile_compile(struct Compiler* compiler, const char* const* source_paths, size_t source_count, const char* output_path, FILE* (*open_file)(const char*, const char*), const char* entry, int optimization, FILE* diagnostics) {
	return compile_module(compiler, source_paths, source_count, output_path, open_file, entry, optimization, diagnostics, EXECUTABLE_COMPILATION);
}

int rutile_compile_library(struct Compiler* compiler, const char* const* source_paths, size_t source_count, const char* output_path, FILE* (*open_file)(const char*, const char*), int optimization, FILE* diagnostics) {
	return compile_module(compiler, source_paths, source_count, output_path, open_file, NULL, optimization, diagnostics, LIBRARY_COMPILATION);
}
