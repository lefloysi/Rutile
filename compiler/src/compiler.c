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
};

enum NamespaceKind {
	UNRESOLVED_NAMESPACE,
	DECLARED_NAMESPACE,
	ASSOCIATED_NAMESPACE,
};

struct Namespace {
	const char* name;
	size_t length;
	struct Namespace* parent;
	struct Namespace* next;
	struct Record* record;
	enum NamespaceKind kind;
	const char* source_path;
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
};

enum NodeKind {
	INTEGER_EXPRESSION,
	BOOLEAN_EXPRESSION,
	DECIMAL_EXPRESSION,
	STRING_EXPRESSION,
	NAME_EXPRESSION,
	FUNCTION_EXPRESSION,
	BOUND_FUNCTION_EXPRESSION,
	CALL_EXPRESSION,
	NEGATE_EXPRESSION,
	BINARY_EXPRESSION,
	ASSIGNMENT_EXPRESSION,
	NOT_EXPRESSION,
	COMPLEMENT_EXPRESSION,
	PREFIX_UPDATE_EXPRESSION,
	POSTFIX_UPDATE_EXPRESSION,
	ADDRESS_EXPRESSION,
	REFERENCE_BINDING_EXPRESSION,
	REFERENCE_CAST_EXPRESSION,
	REFERENCE_INITIALIZATION_EXPRESSION,
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
	TYPE_NODE,
};

enum TypeSyntaxKind {
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
	union {
		uint64_t integer;
		double decimal;
		struct Enumerator* enumerator;
		struct Function* function;
		struct {
			struct Function* function;
			struct Expression* receiver;
			enum ReceiverKind kind;
		} bound_function;
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
			struct Statement* initialization;
		} member;
		struct {
			unsigned char* bytes;
			size_t count;
		} string;
		struct {
			struct Token name;
			struct Variable* binding;
			struct Statement* initialization;
		} variable;
		struct {
			struct Token name;
			struct Function* binding;
			struct Expression* callee;
			struct Expression* receiver_evaluation;
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
};

struct ScopeBinding {
	struct Token name;
	struct Variable* variable;
	struct ScopeBinding* next;
};

struct Scope {
	struct ScopeBinding* bindings;
	struct Scope* parent;
	struct Statement* initialization;
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

struct Record {
	enum TypeKind kind;
	enum RecordState state;
	struct Token name;
	struct Span span;
	struct Namespace* associated;
	union {
		struct Member* members;
		struct Enumerator* enumerators;
	} data;
	LLVMTypeRef llvm_type;
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

struct Function {
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
	enum ParameterArity arity;
	struct Function* next;
};

struct FunctionSymbol {
	struct Function* function;
	struct FunctionSymbol* next;
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
	bool failed;
	LLVMContextRef llvm_context;
	LLVMModuleRef module;
	LLVMBuilderRef builder;
	LLVMTargetMachineRef machine;
	int optimization;
	int next_string;
};

struct Lexer {
	struct Compiler* compiler;
	const char* path;
	const char* cursor;
	int line;
	int column;
};

const struct Keyword keywords[] = { { "namespace", NAMESPACE }, { "fn", FN }, { "var", VAR }, { "return", RETURN }, { "import", IMPORT }, { "auto", AUTO }, { "struct", STRUCT }, { "union", UNION }, { "enum", ENUM }, { "sizeof", SIZEOF }, { "if", IF }, { "else", ELSE }, { "while", WHILE }, { "for", FOR }, { "switch", SWITCH }, { "case", CASE }, { "default", DEFAULT }, { "break", BREAK }, { "continue", CONTINUE }, { "true", TRUE }, { "false", FALSE } };

const struct BuiltinType builtin_types[] = { { "i08", { SIGNED_INTEGER_TYPE, { 8 } } }, { "i16", { SIGNED_INTEGER_TYPE, { 16 } } }, { "i32", { SIGNED_INTEGER_TYPE, { 32 } } }, { "i64", { SIGNED_INTEGER_TYPE, { 64 } } }, { "u08", { UNSIGNED_INTEGER_TYPE, { 8 } } }, { "u16", { UNSIGNED_INTEGER_TYPE, { 16 } } }, { "u32", { UNSIGNED_INTEGER_TYPE, { 32 } } }, { "u64", { UNSIGNED_INTEGER_TYPE, { 64 } } }, { "usize", { UNSIGNED_INTEGER_TYPE, { sizeof(void*) * CHAR_BIT } } }, { "f32", { FLOAT_TYPE, { 32 } } }, { "f64", { FLOAT_TYPE, { 64 } } }, { "char", { CHAR_TYPE, { 0 } } }, { "bool", { BOOL_TYPE, { 0 } } }, { "void", { VOID_TYPE, { 0 } } } };

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
	if (a.kind != b.kind) {
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
		else {
			space = namespace_local(compiler, result.owner, part);
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

struct Token parse_name(struct Compiler* compiler) {
	struct Token name = compiler->token;
	if (!expect(compiler, NAME, "an identifier")) {
		return name;
	}

	while (take(compiler, QUALIFY)) {
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
		struct Namespace* associated = namespace_local(compiler, owner, lookup.name);
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
		if (type.data.element->kind == VOID_TYPE || type.data.element->kind == REFERENCE_TYPE) {
			diagnostic(compiler, syntax->span.first, "reference requires a non-reference value type");
		}
		type.kind = REFERENCE_TYPE;
		return type;
	case FUNCTION_SYNTAX:
		type.kind = FUNCTION_TYPE;
		type.data.function = syntax->data.function;
		type.data.function->result = resolve_type(compiler, type.data.function->result_syntax);
		for (struct Variable* parameter = type.data.function->parameters; parameter; parameter = parameter->next) {
			parameter->type = resolve_type(compiler, parameter->syntax);
			if (is_void(parameter->type) || parameter->type.kind == FUNCTION_TYPE) {
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
			if (is_void(*type.data.array.element) || type.data.array.element->kind == REFERENCE_TYPE) {
				diagnostic(compiler, syntax->span.first, "array elements cannot have void or reference type");
			}
		}
		return type;
	}
	diagnostic(compiler, syntax->span.first, "invalid type syntax");
	return type;
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
		if ((kind == STRUCT || kind == UNION || kind == ENUM) && compiler->tokens[index + 1].kind == NAME && (following == '{' || following == ';') && name_equal(name, compiler->tokens[index + 1])) {
			return true;
		}
	}
	return false;
}

bool scan_type(struct Compiler* compiler, size_t* position) {
	size_t index = *position;
	if (index >= compiler->token_count) {
		return false;
	}
	int kind = compiler->tokens[index++].kind;
	if (kind == '(') {
		if (index < compiler->token_count && compiler->tokens[index].kind == ')') {
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
					if (index >= compiler->token_count || compiler->tokens[index++].kind != ',' || !scan_type(compiler, &index)) {
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
	case '&':
	case '*':
		next_token(compiler);
		expression = new_expression(compiler, token.kind == '&' ? ADDRESS_EXPRESSION : DEREFERENCE_EXPRESSION, token);
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
		if (type_start(compiler)) {
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
			field->data.member.name = compiler->token;
			expect(compiler, NAME, "a field name");
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
	struct QualifiedName declared = declaration_name(compiler, parse_name(compiler));
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
	if (receiver && (!function->parameters || !token_text(function->parameters->name, "this"))) {
		struct Variable* parameter = allocate(compiler, sizeof(struct Variable));
		struct TypeNode* reference = allocate(compiler, sizeof(struct TypeNode));
		struct TypeNode* object = allocate(compiler, sizeof(struct TypeNode));
		if (!parameter || !reference || !object) {
			return;
		}
		parameter->name = declared.name;
		parameter->name.start = "this";
		parameter->name.length = 4;
		object->kind = NAMED_SYNTAX;
		object->span.first = receiver->name;
		object->data.named.kind = receiver->kind;
		object->data.named.name = receiver->name;
		reference->kind = REFERENCE_SYNTAX;
		reference->span.first = declared.name;
		reference->data.element = object;
		parameter->syntax = reference;
		parameter->next = function->parameters;
		function->parameters = parameter;
	}
	if (take(compiler, ARROW)) {
		function->result_syntax = parse_type(compiler);
	}
	function->kind = take(compiler, ';') ? FUNCTION_DECLARATION : FUNCTION_DEFINITION;
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
		take(compiler, ';');
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
		for (struct FunctionSymbol* symbol = compiler->symbols; symbol; symbol = symbol->next) {
			if (symbol->function->owner == owner && name_equal(symbol->function->local_name, lookup.name)) {
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
	name = resolve_symbol(compiler, name);
	for (struct Namespace* owner = name.owner; ; owner = owner->parent) {
		for (struct TagSymbol* tag = compiler->tags; tag; tag = tag->next) {
			if (tag->record->kind != ENUM_TYPE) {
				continue;
			}
			for (struct Enumerator* value = tag->record->data.enumerators; value; value = value->next) {
				if (namespace_name_equal(value->name, owner, name)) {
					return value;
				}
				struct Token local = value->name;
				for (size_t index = 0; index + 1 < value->name.length; index++) {
					if (value->name.start[index] == ':' && value->name.start[index + 1] == ':') {
						local.start = value->name.start + index + 2;
						local.length = value->name.length - index - 2;
					}
				}
				struct Namespace enum_scope = { tag->record->name.start, tag->record->name.length, NULL, NULL };
				if (!owner && namespace_name_equal(name, &enum_scope, local)) {
					return value;
				}
			}
		}
		if (!owner) {
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
	unsigned count = 0;
	uint64_t maximum_size = 0;
	unsigned maximum_alignment = 0;
	struct Member* storage = NULL;
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
		LLVMStructSetBody(record->llvm_type, &byte, 1, false);
		record->state = RESOLVED_RECORD;
		return;
	}
	LLVMTypeRef* members = allocate(compiler, (count + 1) * sizeof(LLVMTypeRef));
	if (!members) {
		return;
	}
	if (record->kind == STRUCT_TYPE) {
		unsigned index = 0;
		for (struct Member* member = record->data.members; member; member = member->next) {
			members[index++] = llvm_type(compiler, member->type);
		}
		LLVMStructSetBody(record->llvm_type, members, count, false);
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
			if (is_void(parameter->type)) {
				diagnostic(compiler, parameter->name, "parameter cannot have void type");
			}
			if (find_variable(scope, parameter->name)) {
				diagnostic(compiler, parameter->name, "duplicate parameter");
			}
			parameter->scope_next = scope;
			scope = parameter;
		}
		if (function->owner && function->owner->record && function->parameters && token_text(function->parameters->name, "this")) {
			struct Type receiver = function->parameters->type;
			struct Record* owner = function->owner->record;
			if (receiver.kind != REFERENCE_TYPE || receiver.data.element->kind != owner->kind || receiver.data.element->data.record != owner) {
				diagnostic(compiler, function->parameters->name, "associated receiver must reference its owning type");
				return;
			}
		}
		if (function->arity == VARIADIC_PARAMETERS && function->kind == FUNCTION_DEFINITION) {
			diagnostic(compiler, function->name, "variadic function definitions are not supported yet");
		}
		struct Function* previous = NULL;
		for (struct FunctionSymbol* symbol = compiler->symbols; symbol; symbol = symbol->next) {
			if (symbol->function->owner == function->owner && name_equal(symbol->function->local_name, function->local_name)) {
				previous = symbol->function;
				break;
			}
		}
		if (previous) {
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
			else if (function->kind == FUNCTION_DEFINITION) {
				if (previous->kind == FUNCTION_DEFINITION) {
					diagnostic(compiler, function->name, "duplicate function definition");
				}
				else {
					previous->kind = FUNCTION_DEFINITION;
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

	scope_remove(scope, expression->data.variable.binding);
}

unsigned analyze_statements(struct Compiler* compiler, struct Function* function, struct Statement* statements, struct Scope* scope, struct Flow* flow);
void prepare_reference_fields(struct Compiler* compiler, struct Variable* variable, struct Statement* initialization);

struct Function* receiver_function(struct Compiler* compiler, struct Type object, struct Token name) {
	for (struct FunctionSymbol* symbol = compiler->symbols; symbol; symbol = symbol->next) {
		if (symbol->function->owner == object.data.record->associated && name_equal(symbol->function->local_name, name)) {
			return symbol->function;
		}
	}

	struct Function* function = find_function(compiler, name);
	struct Variable* receiver = function ? function->parameters : NULL;
	if (!receiver || !token_text(receiver->name, "this") || receiver->type.kind != REFERENCE_TYPE || !type_equal(*receiver->type.data.element, object)) {
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
					struct Member* field = object.data.record->data.members;
					while (field && !name_equal(field->name, name)) {
						field = field->next;
					}
					struct Function* method = receiver_function(compiler, object, name);
					if (field || (method && method->owner == object.data.record->associated && method->parameters && token_text(method->parameters->name, "this"))) {
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
							expression->data.member.initialization = NULL;
							expression->type = field->type.kind == REFERENCE_TYPE ? *field->type.data.element : field->type;
						}
						else {
							expression->kind = BOUND_FUNCTION_EXPRESSION;
							expression->data.bound_function.function = method;
							expression->data.bound_function.receiver = receiver;
							expression->data.bound_function.kind = BIND_RECEIVER;
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
				if (context->initialization && context->initialization->data.declaration.variable == expression->data.variable.binding) {
					expression->data.variable.initialization = context->initialization;
					break;
				}
			}
		}
		break;
	}
	case CALL_EXPRESSION:
		bind_expression(compiler, expression->data.call.callee, scope);
		if (compiler->failed) {
			break;
		}
		struct Argument* receiver_argument = NULL;
		if (expression->data.call.callee->kind == BOUND_FUNCTION_EXPRESSION) {
			struct Expression* callee = expression->data.call.callee;
			struct Function* function = callee->data.bound_function.function;
			struct Expression* receiver = callee->data.bound_function.receiver;
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
				struct Statement* argument_context = allocate(compiler, sizeof(struct Statement));
				if (!argument_context) {
					break;
				}
				argument_context->kind = DECLARATION_STATEMENT;
				argument_context->data.declaration.variable = argument->variable;
				argument_context->data.declaration.initialization = argument->initialization;
				argument_scope.initialization = argument_context;
				if (parameter->type.kind != REFERENCE_TYPE) {
					struct Statement* first = argument->initialization->data.block;
					struct Expression* binding = first && first->kind == EXPRESSION_STATEMENT ? first->data.expression : NULL;
					if (binding && binding->kind == ASSIGNMENT_EXPRESSION && (binding->data.binary.operation == '=' || binding->data.binary.operation == COPY_ASSIGN) && binding->data.binary.left->kind == NAME_EXPRESSION && name_equal(binding->data.binary.left->data.variable.name, parameter->name)) {
						argument->initializer = binding->data.binary.right;
						argument_context->data.declaration.initializer = argument->initializer;
						argument->initialization->data.block = first->next;
						bind_expression(compiler, argument->initializer, scope);
						if (binding->data.binary.operation == '=') {
							move_expression(compiler, argument->initializer, scope);
						}
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
		for (struct Member* field = object.data.record->data.members; field; field = field->next) {
			if (name_equal(field->name, expression->data.member.name)) {
				expression->data.member.binding = field;
				expression->type = field->type;
				if (expression->type.kind == REFERENCE_TYPE) {
					expression->type = *expression->type.data.element;
				}
				break;
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
			}
			expression->kind = BOUND_FUNCTION_EXPRESSION;
			expression->data.bound_function.function = function;
			expression->data.bound_function.receiver = receiver;
			expression->data.bound_function.kind = receiver_kind;
			expression->type = (struct Type){ FUNCTION_TYPE, { .function = function } };
			break;
		}
		struct Expression* root = expression;
		while (root->kind == FIELD_EXPRESSION || root->kind == INDEX_EXPRESSION) {
			root = root->kind == FIELD_EXPRESSION ? root->data.member.object : root->data.binary.left;
		}
		for (struct Scope* context = scope; context; context = context->parent) {
			if (context->initialization && root->kind == NAME_EXPRESSION && context->initialization->data.declaration.variable == root->data.variable.binding) {
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
	case COMPLEMENT_EXPRESSION:
	case NEGATE_EXPRESSION:
		bind_expression(compiler, expression->data.operand, scope);
		expression->type = expression->data.operand->type;
		break;
	case ASSIGNMENT_EXPRESSION:
	case BINARY_EXPRESSION:
		bind_expression(compiler, expression->data.binary.left, scope);
		bind_expression(compiler, expression->data.binary.right, scope);
		expression->type = !is_void(expression->data.binary.left->type) ? expression->data.binary.left->type : expression->data.binary.right->type;
		if (expression->kind == ASSIGNMENT_EXPRESSION && expression->data.binary.operation == '=') {
			move_expression(compiler, expression->data.binary.right, scope);
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

void check_integer(struct Compiler* compiler, struct Expression* expression, struct Type* expected, uint64_t magnitude, bool negative) {
	if (!expected) {
		diagnostic(compiler, expression->span.first, "integer literal has no type context");
		return;
	}
	if (expected->kind == POINTER_TYPE && !magnitude) {
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
	return expression->kind == REFERENCE_CAST_EXPRESSION || (expression->kind == CALL_EXPRESSION && expression->data.call.binding->result.kind == REFERENCE_TYPE) || expression->kind == NAME_EXPRESSION || expression->kind == DEREFERENCE_EXPRESSION || (expression->kind == INDEX_EXPRESSION && (expression->data.binary.left->type.kind == POINTER_TYPE || lvalue_expression(expression->data.binary.left))) || expression->kind == POINTER_FIELD_EXPRESSION || (expression->kind == FIELD_EXPRESSION && lvalue_expression(expression->data.member.object));
}

bool contains_reference_fields(struct Type type);
bool initialization_path_equal(struct Compiler* compiler, struct Expression* left, struct Expression* right);

void validate_reference_field_read(struct Compiler* compiler, struct Expression* expression) {
	struct Statement* context = expression->data.member.initialization;
	if (!context || expression->data.member.binding->type.kind != REFERENCE_TYPE) {
		return;
	}
	struct Expression* root = expression;
	while (root->kind == FIELD_EXPRESSION || root->kind == INDEX_EXPRESSION) {
		root = root->kind == FIELD_EXPRESSION ? root->data.member.object : root->data.binary.left;
		if (root->kind == FIELD_EXPRESSION && root->data.member.binding->type.kind == REFERENCE_TYPE) {
			return;
		}
	}
	if (root->kind != NAME_EXPRESSION || root->data.variable.binding != context->data.declaration.variable) {
		return;
	}
	if (root->data.variable.binding->type.kind == REFERENCE_TYPE) {
		return;
	}
	if (context->data.declaration.initializer) {
		return;
	}
	for (struct Statement* statement = context->data.declaration.initialization->data.block; statement; statement = statement->next) {
		if (statement->kind != EXPRESSION_STATEMENT) {
			continue;
		}
		struct Expression* binding = statement->data.expression;
		if (binding->kind == REFERENCE_INITIALIZATION_EXPRESSION && binding->type.kind == REFERENCE_TYPE && initialization_path_equal(compiler, binding->data.binary.left, expression)) {
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
		check_expression(compiler, expression, expected->data.element);
		if (compiler->failed) {
			return;
		}
		if (!lvalue_expression(expression) && expression->kind != FUNCTION_EXPRESSION) {
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
	case REFERENCE_INITIALIZATION_EXPRESSION: {
		struct Expression* target = expression->data.binary.left;
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
	case ASSIGNMENT_EXPRESSION: {
		struct Expression* left = expression->data.binary.left;
		check_expression(compiler, left, NULL);
		if (left->type.kind != REFERENCE_TYPE && contains_reference_fields(left->type)) {
			diagnostic(compiler, expression->span.first, "aggregate assignment cannot rebind reference fields");
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
			check_expression(compiler, left, NULL);
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
		if ((source.kind == FLOAT_TYPE && target.kind == POINTER_TYPE) || (source.kind == POINTER_TYPE && target.kind == FLOAT_TYPE)) {
			diagnostic(compiler, expression->span.first, "cannot cast between floating-point and pointer types");
		}
		if (!is_void(target) && ((!numeric_type(source) && source.kind != POINTER_TYPE) || (!numeric_type(target) && target.kind != POINTER_TYPE))) {
			diagnostic(compiler, expression->span.first, "cast requires scalar types");
		}
		break;
	}
	case REFERENCE_CAST_EXPRESSION:
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
				argument->passed_type = parameter->type;
				parameter = parameter->next;
			}
			else if (expression->data.call.binding->arity == VARIADIC_PARAMETERS) {
				check_expression(compiler, argument->expression, NULL);
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
	case STRING_EXPRESSION:
	case NAME_EXPRESSION:
	case FUNCTION_EXPRESSION:
		if (expression->kind == NAME_EXPRESSION && expression->data.variable.initialization && expression->data.variable.binding->type.kind == REFERENCE_TYPE) {
			struct Statement* context = expression->data.variable.initialization;
			struct Statement* statement = context->data.declaration.initialization->data.block;
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

	if (expected && !type_equal(expression->type, *expected)) {
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

bool initialization_path_equal(struct Compiler* compiler, struct Expression* left, struct Expression* right) {
	if (!left || !right || left->kind != right->kind) {
		return false;
	}
	if (left->kind == NAME_EXPRESSION) {
		return name_equal(left->data.variable.name, right->data.variable.name);
	}
	if (left->kind == FIELD_EXPRESSION) {
		return name_equal(left->data.member.name, right->data.member.name) && initialization_path_equal(compiler, left->data.member.object, right->data.member.object);
	}
	if (left->kind == INDEX_EXPRESSION) {
		uint64_t first;
		uint64_t second;
		return case_integer(compiler, left->data.binary.right, (struct Type){ UNSIGNED_INTEGER_TYPE, { sizeof(void*) * CHAR_BIT } }, &first) && case_integer(compiler, right->data.binary.right, (struct Type){ UNSIGNED_INTEGER_TYPE, { sizeof(void*) * CHAR_BIT } }, &second) && first == second && initialization_path_equal(compiler, left->data.binary.left, right->data.binary.left);
	}
	return false;
}

bool contains_reference_fields(struct Type type) {
	if (type.kind == REFERENCE_TYPE) {
		return true;
	}
	if (type.kind == ARRAY_TYPE) {
		return contains_reference_fields(*type.data.array.element);
	}
	if (type.kind == STRUCT_TYPE || type.kind == UNION_TYPE) {
		for (struct Member* member = type.data.record->data.members; member; member = member->next) {
			if (contains_reference_fields(member->type)) {
				return true;
			}
		}
	}
	return false;
}

void prepare_reference_path(struct Compiler* compiler, struct Type type, struct Expression* path, struct Statement* initialization) {
	if (compiler->failed || !contains_reference_fields(type)) {
		return;
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
		for (struct Member* member = type.data.record->data.members; member; member = member->next) {
			struct Expression* field = new_expression(compiler, FIELD_EXPRESSION, member->name);
			if (!field) {
				return;
			}
			field->data.member.object = path;
			field->data.member.name = member->name;
			prepare_reference_path(compiler, member->type, field, initialization);
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
	struct Expression* path = new_expression(compiler, NAME_EXPRESSION, variable->name);
	if (!path) {
		return;
	}
	path->data.variable.name = variable->name;
	prepare_reference_path(compiler, variable->type, path, initialization);
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
		if (existing && existing->variable) {
			diagnostic(compiler, variable->name, "duplicate variable");
			break;
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
		if (statement->data.declaration.operation == '=' && variable->type.kind != REFERENCE_TYPE) {
			move_expression(compiler, statement->data.declaration.initializer, scope);
		}
		check_expression(compiler, statement->data.declaration.initializer, variable->syntax ? &variable->type : NULL);
		if (variable->type.kind == REFERENCE_TYPE) {
			scope_declare(compiler, scope, variable);
		}
		if (!variable->syntax && statement->data.declaration.initializer) {
			variable->type = statement->data.declaration.initializer->type;
		}
		if (is_void(variable->type)) {
			diagnostic(compiler, variable->name, "variable cannot have void type");
		}
		if (variable->type.kind == FUNCTION_TYPE) {
			diagnostic(compiler, variable->name, "function entities require explicit address-of to obtain a pointer");
		}
		if (!statement->data.declaration.initializer) {
			prepare_reference_fields(compiler, variable, statement->data.declaration.initialization);
		}
		if (statement->data.declaration.initialization) {
			struct Scope initialization_scope = { NULL, scope, statement };
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
			check_expression(compiler, statement->data.expression, &function->result);
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

void analyze_function(struct Compiler* compiler, struct Function* function) {
	if (function->kind == FUNCTION_DECLARATION) {
		return;
	}
	struct Scope scope = { NULL, NULL };
	for (struct Variable* parameter = function->parameters; parameter; parameter = parameter->next) {
		scope_declare(compiler, &scope, parameter);
	}
	unsigned exits = analyze_statements(compiler, function, function->statements, &scope, NULL);
	if ((exits & NORMAL_EXIT) && !is_void(function->result)) {
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

void analyze(struct Compiler* compiler) {
	check_namespace_names(compiler);
	register_tags(compiler);
	check_enumerator_names(compiler);
	for (struct TagSymbol* tag = compiler->tags; tag && !compiler->failed; tag = tag->next) {
		if (tag->record->state != FORWARD_RECORD) {
			resolve_record(compiler, tag->record);
		}
	}
	register_functions(compiler);
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
	if (function->kind == FUNCTION_DECLARATION) {
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
	size_t length = strlen(prefix) + symbol_name.length;
	char* name = allocate(compiler, length + 1);
	if (!name) {
		return NULL;
	}
	memcpy(name, prefix, strlen(prefix));
	memcpy(name + strlen(prefix), symbol_name.start, symbol_name.length);
	return name;
}

void declare_llvm_functions(struct Compiler* compiler) {
	for (struct FunctionSymbol* symbol = compiler->symbols; symbol && !compiler->failed; symbol = symbol->next) {
		struct Function* function = symbol->function;
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
void emit_statements(struct Compiler* compiler, struct Statement* statements, struct Flow* flow);

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
				LLVMBuildStore(compiler->builder, initial, argument->variable->slot);
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
	LLVMValueRef callee = emit_expression(compiler, expression->data.call.callee);
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

LLVMValueRef emit_field_slot(struct Compiler* compiler, struct Expression* expression) {

	struct Expression* object = expression->data.member.object;
	struct Type type = object->type;
	LLVMValueRef address;
	if (expression->kind == POINTER_FIELD_EXPRESSION) {
		address = emit_expression(compiler, object);
		type = *type.data.element;
	}
	else if (lvalue_expression(object)) {
		address = emit_address(compiler, object);
	}
	else {
		LLVMValueRef value = emit_expression(compiler, object);
		if (!value) {
			return NULL;
		}
		address = entry_alloca(compiler, type);
		LLVMBuildStore(compiler->builder, value, address);
	}
	if (!address) {
		return NULL;
	}
	return type.kind == UNION_TYPE ? address : LLVMBuildStructGEP2(compiler->builder, llvm_type(compiler, type), address, expression->data.member.binding->index, "field");
}

LLVMValueRef emit_address(struct Compiler* compiler, struct Expression* expression) {
	switch (expression->kind) {
	case FIELD_EXPRESSION:
	case POINTER_FIELD_EXPRESSION: {
		LLVMValueRef slot = emit_field_slot(compiler, expression);
		if (slot && expression->data.member.binding->type.kind == REFERENCE_TYPE) {
			return LLVMBuildLoad2(compiler->builder, llvm_type(compiler, expression->data.member.binding->type), slot, "field_referent");
		}
		return slot;
	}
	case REFERENCE_CAST_EXPRESSION:
		return expression->data.operand->kind == FUNCTION_EXPRESSION ? expression->data.operand->data.function->llvm_value : emit_address(compiler, expression->data.operand);
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
			LLVMValueRef address;
			if (lvalue_expression(base)) {
				address = emit_address(compiler, base);
			}
			else {
				LLVMValueRef value = emit_expression(compiler, base);
				address = entry_alloca(compiler, base->type);
				LLVMBuildStore(compiler->builder, value, address);
			}
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

LLVMValueRef emit_expression(struct Compiler* compiler, struct Expression* expression) {
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
	case BOOLEAN_EXPRESSION:
		return LLVMConstInt(llvm_type(compiler, expression->type), expression->data.integer, false);
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
		return address ? LLVMBuildLoad2(compiler->builder, llvm_type(compiler, expression->type), address, "field_value") : NULL;
	}
	case ADDRESS_EXPRESSION:
		return expression->data.operand->kind == FUNCTION_EXPRESSION ? expression->data.operand->data.function->llvm_value : emit_address(compiler, expression->data.operand);
	case FUNCTION_EXPRESSION:
		return expression->data.function->llvm_value;
	case DEREFERENCE_EXPRESSION:
		if (expression->type.kind == FUNCTION_TYPE) {
			return emit_expression(compiler, expression->data.operand);
		}
	case INDEX_EXPRESSION: {
		LLVMValueRef address = emit_address(compiler, expression);
		return address ? LLVMBuildLoad2(compiler->builder, llvm_type(compiler, expression->type), address, "value") : NULL;
	}
	case NAME_EXPRESSION:
		if (expression->type.kind == FUNCTION_TYPE) {
			return emit_address(compiler, expression);
		}
		return LLVMBuildLoad2(compiler->builder, llvm_type(compiler, expression->type), emit_address(compiler, expression), "value");
	case REFERENCE_CAST_EXPRESSION:
		if (expression->type.kind == FUNCTION_TYPE) {
			return emit_address(compiler, expression);
		}
		return LLVMBuildLoad2(compiler->builder, llvm_type(compiler, expression->type), emit_address(compiler, expression), "reference_value");
	case REFERENCE_INITIALIZATION_EXPRESSION: {
		LLVMValueRef value = emit_expression(compiler, expression->data.binary.right);
		struct Expression* target = expression->data.binary.left;
		LLVMValueRef slot = target->kind == NAME_EXPRESSION ? target->data.variable.binding->slot : emit_field_slot(compiler, target);
		LLVMBuildStore(compiler->builder, value, slot);
		return value;
	}
	case REFERENCE_BINDING_EXPRESSION:
		return expression->data.operand->kind == FUNCTION_EXPRESSION ? expression->data.operand->data.function->llvm_value : emit_address(compiler, expression->data.operand);
	case NEGATE_EXPRESSION: {
		LLVMValueRef operand = emit_expression(compiler, expression->data.operand);
		if (!operand) {
			return NULL;
		}
		return expression->type.kind == FLOAT_TYPE ? LLVMBuildFNeg(compiler->builder, operand, "negative") : LLVMBuildNeg(compiler->builder, operand, "negative");
	}
	case ASSIGNMENT_EXPRESSION: {
		struct Expression* target = expression->data.binary.left;
		LLVMValueRef address = emit_address(compiler, target);
		const struct Operator* operation = find_operator(expression->data.binary.operation);
		LLVMValueRef left = operation->compound ? LLVMBuildLoad2(compiler->builder, llvm_type(compiler, target->type), address, "previous") : NULL;
		LLVMValueRef value = emit_expression(compiler, expression->data.binary.right);
		if (operation->compound) {
			value = emit_binary(compiler, operation->compound, left, value, target->type, expression->data.binary.right->type);
		}
		LLVMBuildStore(compiler->builder, value, address);
		return value;
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
			LLVMValueRef right = emit_truth(compiler, emit_expression(compiler, expression->data.binary.right), expression->data.binary.right->type);
			LLVMBasicBlockRef right_end = LLVMGetInsertBlock(compiler->builder);
			LLVMBuildBr(compiler->builder, merge);
			LLVMPositionBuilderAtEnd(compiler->builder, merge);
			LLVMValueRef phi = LLVMBuildPhi(compiler->builder, LLVMInt1TypeInContext(compiler->llvm_context), "logical");
			LLVMValueRef values[] = { LLVMConstInt(LLVMInt1TypeInContext(compiler->llvm_context), operation->token == LOGICAL_OR, false), right };
			LLVMBasicBlockRef blocks[] = { previous, right_end };
			LLVMAddIncoming(phi, values, blocks, 2);
			return phi;
		}
		LLVMValueRef right = emit_expression(compiler, expression->data.binary.right);
		return emit_binary(compiler, operation->token, left, right, expression->data.binary.left->type, expression->data.binary.right->type);
	}
	case CALL_EXPRESSION: {
		LLVMValueRef call = emit_call(compiler, expression);
		if (call && expression->data.call.binding->result.kind == REFERENCE_TYPE && expression->type.kind != FUNCTION_TYPE) {
			return LLVMBuildLoad2(compiler->builder, llvm_type(compiler, expression->type), call, "reference_result");
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
			LLVMBuildStore(compiler->builder, emit_expression(compiler, statement->data.declaration.initializer), variable->slot);
		}
		else if (!statement->data.declaration.initialization) {
			LLVMBuildStore(compiler->builder, LLVMConstNull(llvm_type(compiler, variable->type)), variable->slot);
		}
		if (statement->data.declaration.initialization) {
			emit_statement(compiler, statement->data.declaration.initialization, flow);
		}
		break;
	}
	case EXPRESSION_STATEMENT:
		emit_expression(compiler, statement->data.expression);
		break;
	case RETURN_STATEMENT:
		if (statement->data.expression) {
			LLVMBuildRet(compiler->builder, emit_expression(compiler, statement->data.expression));
		}
		else {
			LLVMBuildRetVoid(compiler->builder);
		}
		break;
	case BREAK_STATEMENT:
		LLVMBuildBr(compiler->builder, flow->break_target);
		break;
	case CONTINUE_STATEMENT:
		while (flow->kind != LOOP_FLOW) {
			flow = flow->parent;
		}
		LLVMBuildBr(compiler->builder, flow->continue_target);
		break;
	case IF_STATEMENT: {
		LLVMValueRef condition = emit_truth(compiler, emit_expression(compiler, statement->data.branch.condition), statement->data.branch.condition->type);
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
}

void emit_statements(struct Compiler* compiler, struct Statement* statements, struct Flow* flow) {
	for (struct Statement* statement = statements; statement && !compiler->failed; statement = statement->next) {
		if (LLVMGetBasicBlockTerminator(LLVMGetInsertBlock(compiler->builder))) {
			break;
		}
		emit_statement(compiler, statement, flow);
	}
}

void emit_function(struct Compiler* compiler, struct Function* function) {
	if (function->kind == FUNCTION_DECLARATION) {
		return;
	}
	LLVMBasicBlockRef block = LLVMAppendBasicBlockInContext(compiler->llvm_context, function->llvm_value, "entry");
	LLVMPositionBuilderAtEnd(compiler->builder, block);
	unsigned index = 0;
	for (struct Variable* parameter = function->parameters; parameter; parameter = parameter->next) {
		parameter->slot = entry_alloca(compiler, parameter->type);
		LLVMBuildStore(compiler->builder, LLVMGetParam(function->llvm_value, index++), parameter->slot);
	}
	emit_statements(compiler, function->statements, NULL);
	if (!compiler->failed && !LLVMGetBasicBlockTerminator(LLVMGetInsertBlock(compiler->builder))) {
		LLVMBuildRetVoid(compiler->builder);
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
