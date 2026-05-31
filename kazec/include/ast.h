#ifndef KAZE_AST_H
#define KAZE_AST_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#include "../../utils/include/arena.h"
#include "../../utils/include/string_view.h"

// ============================================================================
// Node Kinds
// ============================================================================

typedef enum NodeKind {
    // Expressions
    NODE_EXPR_INT_LIT,
    NODE_EXPR_FLOAT_LIT,
    NODE_EXPR_STRING_LIT,
    NODE_EXPR_BOOL_LIT,
    NODE_EXPR_NULL_LIT,
    NODE_EXPR_IDENT,
    NODE_EXPR_BINARY_OP,
    NODE_EXPR_UNARY_OP,
    NODE_EXPR_CALL,
    NODE_EXPR_INDEX,
    NODE_EXPR_FIELD_ACCESS,
    NODE_EXPR_PATH,
    NODE_EXPR_CAST,
    NODE_EXPR_POSTFIX,
    NODE_EXPR_CIMPORT,
    NODE_EXPR_STRUCT_LIT,
    NODE_EXPR_TRY,
    // Statements
    NODE_STMT_VAR_DECL,
    NODE_STMT_ASSIGNMENT,
    NODE_STMT_IF,
    NODE_STMT_WHILE,
    NODE_STMT_FOR,
    NODE_STMT_RETURN,
    NODE_STMT_BREAK,
    NODE_STMT_CONTINUE,
    NODE_STMT_BLOCK,
    NODE_STMT_EXPR,
    NODE_STMT_MATCH,
    NODE_STMT_DEFER,
    NODE_STMT_WHEN,
    // Declarations
    NODE_DECL_FUNCTION,
    NODE_DECL_STRUCT,
    NODE_DECL_ENUM,
    NODE_DECL_UNION,
    NODE_DECL_TYPE_ALIAS,
    NODE_DECL_IMPORT,
    // Types
    NODE_TYPE_PRIMITIVE,
    NODE_TYPE_POINTER,
    NODE_TYPE_ARRAY,
    NODE_TYPE_FUNCTION,
    NODE_TYPE_GENERIC,
    // Patterns
    NODE_PATTERN_WILDCARD,
    NODE_PATTERN_IDENT,
    NODE_PATTERN_LITERAL,
    NODE_PATTERN_PATH,
    NODE_PATTERN_TUPLE,
    NODE_PATTERN_STRUCT,

    NODE_KIND_COUNT,
} NodeKind;

// ============================================================================
// Source Location
// ============================================================================

typedef struct {
    const char *filename;
    uint32_t line;
    uint32_t col;
} SourceLoc;

// ============================================================================
// TypeInfo — forward decl, filled by sema
// ============================================================================

typedef struct TypeInfo TypeInfo;

// ============================================================================
// Base Node — all others embed this as their first field
// ============================================================================

typedef struct Node {
    NodeKind kind;
    SourceLoc loc;
    TypeInfo *type_info;
    struct Node *parent;
} Node;

// ============================================================================
// Cast Macros
// ============================================================================

// Checked cast — returns NULL if kind doesn't match
#define NODE_CAST(node, type, expected_kind) \
    ((node)->kind == (expected_kind) ? (type *)(node) : NULL)

// Checked cast accepting multiple valid kinds
#define NODE_CAST_ANY(node, type, ...) \
    ((type *)_node_cast_any((node), \
        (NodeKind[]){__VA_ARGS__}, \
        sizeof((NodeKind[]){__VA_ARGS__}) / sizeof(NodeKind)))

// Kind predicate
#define NODE_IS(node, k) ((node)->kind == (k))

// Unchecked cast — only where kind is already known
#define NODE_CAST_UNSAFE(node, type) ((type *)(node))

// ============================================================================
// Expressions
// ============================================================================

typedef struct { Node base; int64_t value; } ExprIntLit;
typedef struct { Node base; double value; } ExprFloatLit;
typedef struct { Node base; StringView value; bool is_raw; } ExprStringLit;
typedef struct { Node base; bool value; } ExprBoolLit;
typedef struct { Node base; } ExprNullLit;
typedef struct { Node base; StringView name; } ExprIdent;

typedef struct {
    Node base;
    Node *left;
    Node *right;
    StringView op;
} ExprBinaryOp;

typedef struct {
    Node base;
    Node *operand;
    StringView op;
    bool is_prefix;
} ExprUnaryOp;

typedef struct {
    Node base;
    Node *callee;
    Node **args;
    size_t arg_count;
    bool is_comptime;
} ExprCall;

typedef struct {
    Node base;
    Node *object;
    Node *index;
} ExprIndex;

typedef struct {
    Node base;
    Node *object;
    StringView field;
    bool is_pointer;
} ExprFieldAccess;

// Scope resolution with "::" — e.g. Color::Red, c::strlen, ns::sub::fn.
// `scope` is the left operand (enum/type/namespace), `name` the resolved symbol.
typedef struct {
    Node base;
    Node *scope;
    StringView name;
} ExprPath;

typedef struct {
    Node base;
    Node *expr;
    Node *target_type;
} ExprCast;

typedef struct {
    Node base;
    Node *expr;
    StringView op;
} ExprPostfix;

// @cimport("header.h", .{ .defines = {"M", "N=1"} }) — returns a namespace
// struct accessed with "::". `defines` holds the macro strings (may be empty).
typedef struct {
    Node base;
    StringView path;
    StringView *defines;
    size_t define_count;
} ExprCImport;

// Struct literal:  Vec2{ .x = 1.0, .y = 2.0 }  (or anonymous .{ ... }).
typedef struct {
    Node base;
    StringView type_name;     // zero-len when anonymous
    bool is_anonymous;
    StringView *field_names;
    Node **field_values;
    size_t field_count;
} ExprStructLit;

// try expr — propagates an error up the call stack.
typedef struct {
    Node base;
    Node *expr;
} ExprTry;

// ============================================================================
// Statements
// ============================================================================

typedef struct {
    Node base;
    StringView name;
    Node *type_annotation;  // NULL if inferred
    Node *initializer;      // NULL if absent
    bool is_const;
    bool is_comptime;
} StmtVarDecl;

typedef struct {
    Node base;
    Node *target;
    Node *value;
} StmtAssignment;

typedef struct {
    Node base;
    Node *condition;
    Node **then_body;
    size_t then_len;
    Node **else_body;
    size_t else_len;
} StmtIf;

typedef struct {
    Node base;
    Node *condition;
    Node **body;
    size_t body_len;
} StmtWhile;

typedef struct {
    Node base;
    StringView var_name;
    Node *iter_expr;
    Node **body;
    size_t body_len;
} StmtFor;

typedef struct {
    Node base;
    Node *value;  // NULL for bare return
} StmtReturn;

typedef struct {
    Node base;
    StringView label;  // zero-len if absent
} StmtBreakContinue;

typedef struct {
    Node base;
    Node **statements;
    size_t stmt_count;
} StmtBlock;

typedef struct {
    Node base;
    Node *expr;
} StmtExprStmt;

// match subject { pattern => { ... } ... }
typedef struct {
    Node *pattern;
    Node **body;
    size_t body_len;
} MatchArm;

typedef struct {
    Node base;
    Node *subject;
    MatchArm *arms;
    size_t arm_count;
} StmtMatch;

// defer (block | expr-stmt)
typedef struct {
    Node base;
    Node *body;
} StmtDefer;

// when cond { ... } else when cond { ... } else { ... }  (compile-time)
// A branch with condition == NULL is the trailing `else`.
typedef struct {
    Node *condition;
    Node **body;
    size_t body_len;
} WhenBranch;

typedef struct {
    Node base;
    WhenBranch *branches;
    size_t branch_count;
} StmtWhen;

// ============================================================================
// Declarations
// ============================================================================

typedef struct {
    StringView name;
    Node *type;
    bool is_comptime;
    bool is_variadic;
} Param;

typedef struct {
    Node base;
    StringView name;
    Param *params;
    size_t param_count;
    Node *return_type;  // NULL for void / inferred
    Node **body;
    size_t body_len;
    bool is_extern;
    bool is_inline;
    bool is_comptime;
} DeclFunction;

typedef struct {
    Node base;
    StringView name;
    StringView *field_names;
    Node **field_types;
    size_t field_count;
    Node **methods;          // DeclFunction nodes declared inside the struct
    size_t method_count;
    bool is_generic;
    StringView *generic_params;
    size_t generic_count;
} DeclStruct;

typedef enum {
    ENUM_VARIANT_PLAIN,         // Red
    ENUM_VARIANT_DISCRIMINANT,  // Red = 5
    ENUM_VARIANT_TUPLE,         // Write(*const u8)
    ENUM_VARIANT_STRUCT,        // Move{ x: i32, y: i32 }
} EnumVariantKind;

typedef struct {
    StringView name;
    EnumVariantKind kind;
    Node *discriminant;         // DISCRIMINANT: the value expression
    Node *tuple_type;           // TUPLE: the associated type
    StringView *field_names;    // STRUCT: named fields
    Node **field_types;
    size_t field_count;
} EnumVariant;

typedef struct {
    Node base;
    StringView name;
    EnumVariant *variants;
    size_t variant_count;
    bool is_generic;
    StringView *generic_params;
    size_t generic_count;
} DeclEnum;

// union { as_i32: i32, as_f32: f32 }  — C-interop tagless union.
typedef struct {
    Node base;
    StringView name;
    StringView *field_names;
    Node **field_types;
    size_t field_count;
} DeclUnion;

typedef struct {
    Node base;
    StringView name;
    Node *aliased_type;
    bool is_generic;
    StringView *generic_params;
    size_t generic_count;
} DeclTypeAlias;

typedef struct {
    Node base;
    StringView path;
    bool is_external;  // @cimport
    StringView *selective;
    size_t selective_count;
} DeclImport;

// ============================================================================
// Types
// ============================================================================

typedef struct { Node base; StringView name; } TypePrimitive;

typedef struct {
    Node base;
    Node *pointee;
    bool is_mutable;
} TypePointer;

typedef struct {
    Node base;
    Node *element_type;
    Node *length;   // NULL for slices
    bool is_slice;
} TypeArray;

typedef struct {
    Node base;
    Node **param_types;
    size_t param_count;
    Node *return_type;
} TypeFunction;

typedef struct {
    Node base;
    StringView base_name;
    Node **type_args;
    size_t type_arg_count;
} TypeGeneric;

// ============================================================================
// Patterns (match arms)
// ============================================================================

typedef struct { Node base; } PatternWildcard;              // _
typedef struct { Node base; StringView name; } PatternIdent; // variable bind
typedef struct { Node base; Node *literal; } PatternLiteral; // 1, "s", true, null
typedef struct {
    Node base;
    StringView type_name;   // zero-len when just ::Variant
    StringView variant;
} PatternPath;                                               // Color::Red
typedef struct {
    Node base;
    Node **elements;
    size_t count;
} PatternTuple;                                             // (a, b, _)
typedef struct {
    Node base;
    StringView type_name;
    StringView *fields;
    size_t field_count;
} PatternStruct;                                            // Point{ x, y }

// ============================================================================
// Program / module root
// ============================================================================

typedef struct {
    const char *filename;
    Node **decls;
    size_t decl_count;
} Program;

// ============================================================================
// API
// ============================================================================

// Allocate a zeroed node of the correct size for `kind` from `arena`.
Node       *ast_alloc_node(Arena *arena, NodeKind kind, SourceLoc loc);

// Internal helper for NODE_CAST_ANY.
Node       *_node_cast_any(Node *node, NodeKind *kinds, size_t count);

void        ast_print_tree(Node *root, int depth);
const char *node_kind_to_string(NodeKind kind);

#endif /* KAZE_AST_H */
