#ifndef KAZE_TYPE_H
#define KAZE_TYPE_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#include "../../utils/include/arena.h"
#include "../../utils/include/string_view.h"
#include "ast.h"   // Node, and the `typedef struct TypeInfo TypeInfo;` forward decl

// Forward decl — type_resolve optionally consults a Scope to resolve named
// user types. The full definition lives in scope.h (which includes this file).
typedef struct Scope Scope;

// ============================================================================
// Type Kinds
// ============================================================================

typedef enum TypeKind {
    TYPE_ERROR,     // poisoned — produced on resolution failure, suppresses
                    // cascading diagnostics
    TYPE_VOID,
    TYPE_BOOL,
    TYPE_TYPE,      // the meta-type `type` (the value of a type expression)
    TYPE_INT,       // i8..i64 / u8..u64  (see TyInt.bits + TyInt.is_signed)
    TYPE_FLOAT,     // f32 / f64          (see TyFloat.bits)
    TYPE_POINTER,   // *T / *const T
    TYPE_ARRAY,     // [N]T
    TYPE_SLICE,     // []T
    TYPE_FUNCTION,  // fn(...) -> R
    TYPE_STRUCT,    // user struct  (TyAggregate links back to its DeclStruct)
    TYPE_ENUM,      // user enum    (TyAggregate links back to its DeclEnum)
    TYPE_UNION,     // user union   (TyAggregate links back to its DeclUnion)

    TYPE_KIND_COUNT,
} TypeKind;

// ============================================================================
// Base type — every concrete type embeds this as its first field, mirroring
// the AST's Node "inheritance" via pointer casting.
// ============================================================================

struct TypeInfo {
    TypeKind kind;
};

// ============================================================================
// Cast Macros (mirror NODE_CAST / NODE_IS / NODE_CAST_UNSAFE)
// ============================================================================

// Checked cast — returns NULL if kind doesn't match.
#define TYPE_CAST(t, type, expected_kind) \
    ((t)->kind == (expected_kind) ? (type *)(t) : NULL)

// Kind predicate.
#define TYPE_IS(t, k) ((t)->kind == (k))

// Unchecked cast — only where kind is already known.
#define TYPE_CAST_UNSAFE(t, type) ((type *)(t))

// ============================================================================
// Concrete types — one struct per kind.
// TYPE_ERROR / TYPE_VOID / TYPE_BOOL / TYPE_TYPE carry no payload and use the
// bare TypeInfo. The aggregate kinds (STRUCT/ENUM/UNION) share TyAggregate
// since their fields are identical — the same way the AST reuses
// StmtBreakContinue for BREAK and CONTINUE.
// ============================================================================

typedef struct { TypeInfo base; uint16_t bits; bool is_signed; } TyInt;     // TYPE_INT
typedef struct { TypeInfo base; uint16_t bits; }                TyFloat;    // TYPE_FLOAT

typedef struct {
    TypeInfo  base;
    TypeInfo *pointee;
    bool      is_mutable;   // *T (true) vs *const T (false)
} TyPointer;                                                                 // TYPE_POINTER

typedef struct {
    TypeInfo  base;
    TypeInfo *element;
    uint64_t  length;       // element count
} TyArray;                                                                   // TYPE_ARRAY

typedef struct {
    TypeInfo  base;
    TypeInfo *element;
} TySlice;                                                                   // TYPE_SLICE

typedef struct {
    TypeInfo  base;
    TypeInfo **params;
    size_t     param_count;
    TypeInfo  *return_type;
    bool       is_variadic;
} TyFunction;                                                                // TYPE_FUNCTION

typedef struct {
    TypeInfo   base;
    StringView name;
    Node      *decl;        // DeclStruct / DeclEnum / DeclUnion
} TyAggregate;                                                               // STRUCT/ENUM/UNION

// ============================================================================
// TypeContext — owns the canonical primitive singletons, allocates composites
// ============================================================================

typedef struct TypeContext {
    Arena    *arena;

    TypeInfo *t_error;
    TypeInfo *t_void;
    TypeInfo *t_bool;
    TypeInfo *t_type;

    TypeInfo *t_i8,  *t_i16, *t_i32, *t_i64;
    TypeInfo *t_u8,  *t_u16, *t_u32, *t_u64;
    TypeInfo *t_f32, *t_f64;
} TypeContext;

// Initialise `ctx` and allocate the canonical primitive singletons.
void type_context_init(TypeContext *ctx, Arena *arena);

// Allocate a zeroed type of the correct size for `kind` from `ctx->arena`.
TypeInfo *type_alloc(TypeContext *ctx, TypeKind kind);

// ============================================================================
// Construction — composites are arena-allocated; primitives are canonical
// ============================================================================

TypeInfo *type_pointer(TypeContext *ctx, TypeInfo *pointee, bool is_mutable);
TypeInfo *type_array(TypeContext *ctx, TypeInfo *element, uint64_t length);
TypeInfo *type_slice(TypeContext *ctx, TypeInfo *element);
TypeInfo *type_function(TypeContext *ctx, TypeInfo **params, size_t param_count,
                        TypeInfo *return_type, bool is_variadic);

// Map a primitive keyword ("i32", "f64", "bool", "void", "type") to its
// canonical TypeInfo. Returns NULL when `name` is not a builtin primitive
// (i.e. it is a user-defined type name that needs the symbol table).
TypeInfo *type_primitive_from_name(TypeContext *ctx, StringView name);

// Resolve an AST type node (NODE_TYPE_*) to a TypeInfo. Handles primitives and
// the composites built from them (pointer, array, slice, function). Named user
// types are resolved through `scope` (looking up a SYMBOL_TYPE); pass NULL to
// skip that, in which case named types — and all generics — yield ctx->t_error.
TypeInfo *type_resolve(TypeContext *ctx, Scope *scope, Node *type_node);

// ============================================================================
// Queries
// ============================================================================

bool type_equals(const TypeInfo *a, const TypeInfo *b);

bool type_is_integer(const TypeInfo *t);
bool type_is_float(const TypeInfo *t);
bool type_is_numeric(const TypeInfo *t);
bool type_is_bool(const TypeInfo *t);
bool type_is_error(const TypeInfo *t);

// Human-readable form for diagnostics, e.g. "*const u8", "[4]i32",
// "fn(i32)->bool". Allocated from `arena`.
const char *type_to_string(const TypeInfo *t, Arena *arena);

const char *type_kind_to_string(TypeKind kind);

#endif /* KAZE_TYPE_H */
