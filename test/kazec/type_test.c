#include "type_test.h"
#include "../../kazec/include/type.h"
#include "../../kazec/include/ast.h"
#include "../../utils/include/string_view.h"
#include <stdio.h>
#include <string.h>

#define ASSERT(x) \
    (!!(x) ? true : ( \
        fprintf(stderr, "%s:%s:%d: assertion failed: %s\n", __FILE__, __func__, __LINE__, #x), \
        false \
    ))

static const SourceLoc loc1 = { .filename = "test.kz", .line = 1, .col = 1 };

static StringView sv(const char *s) {
    return string_view_create(s, strlen(s));
}

// ----------------------------------------------------------------------------
// AST type-node builders (mirror what parse_type produces)
// ----------------------------------------------------------------------------

static Node *prim_node(Arena *a, const char *name) {
    TypePrimitive *n = NODE_CAST_UNSAFE(ast_alloc_node(a, NODE_TYPE_PRIMITIVE, loc1), TypePrimitive);
    n->name = sv(name);
    return &n->base;
}

static Node *ptr_node(Arena *a, Node *pointee, bool is_mutable) {
    TypePointer *n = NODE_CAST_UNSAFE(ast_alloc_node(a, NODE_TYPE_POINTER, loc1), TypePointer);
    n->pointee = pointee;
    n->is_mutable = is_mutable;
    return &n->base;
}

static Node *int_lit(Arena *a, int64_t v) {
    ExprIntLit *n = NODE_CAST_UNSAFE(ast_alloc_node(a, NODE_EXPR_INT_LIT, loc1), ExprIntLit);
    n->value = v;
    return &n->base;
}

static Node *array_node(Arena *a, Node *elem, Node *length) {
    TypeArray *n = NODE_CAST_UNSAFE(ast_alloc_node(a, NODE_TYPE_ARRAY, loc1), TypeArray);
    n->element_type = elem;
    n->length = length;
    n->is_slice = (length == NULL);
    return &n->base;
}

static Node *fn_node(Arena *a, Node **params, size_t count, Node *ret) {
    TypeFunction *n = NODE_CAST_UNSAFE(ast_alloc_node(a, NODE_TYPE_FUNCTION, loc1), TypeFunction);
    n->param_types = params;
    n->param_count = count;
    n->return_type = ret;
    return &n->base;
}

// ============================================================================
// Context + allocation
// ============================================================================

bool test_type_context_init(void) {
    bool ok = true;
    Arena *arena = arena_new(0);
    TypeContext ctx;
    type_context_init(&ctx, arena);

    ok &= ASSERT(ctx.arena == arena);
    ok &= ASSERT(ctx.t_error->kind == TYPE_ERROR);
    ok &= ASSERT(ctx.t_void->kind  == TYPE_VOID);
    ok &= ASSERT(ctx.t_bool->kind  == TYPE_BOOL);
    ok &= ASSERT(ctx.t_type->kind  == TYPE_TYPE);

    ok &= ASSERT(ctx.t_i32->kind == TYPE_INT);
    ok &= ASSERT(TYPE_CAST_UNSAFE(ctx.t_i32, TyInt)->bits == 32);
    ok &= ASSERT(TYPE_CAST_UNSAFE(ctx.t_i32, TyInt)->is_signed == true);
    ok &= ASSERT(TYPE_CAST_UNSAFE(ctx.t_u8,  TyInt)->bits == 8);
    ok &= ASSERT(TYPE_CAST_UNSAFE(ctx.t_u8,  TyInt)->is_signed == false);
    ok &= ASSERT(TYPE_CAST_UNSAFE(ctx.t_i64, TyInt)->bits == 64);

    ok &= ASSERT(ctx.t_f32->kind == TYPE_FLOAT);
    ok &= ASSERT(TYPE_CAST_UNSAFE(ctx.t_f32, TyFloat)->bits == 32);
    ok &= ASSERT(TYPE_CAST_UNSAFE(ctx.t_f64, TyFloat)->bits == 64);

    arena_delete(arena);
    return ok;
}

bool test_type_alloc_all_kinds(void) {
    bool ok = true;
    Arena *arena = arena_new(0);
    TypeContext ctx;
    type_context_init(&ctx, arena);

    for (int k = 0; k < TYPE_KIND_COUNT; k++) {
        TypeInfo *t = type_alloc(&ctx, (TypeKind)k);
        ok &= ASSERT(t != NULL);
        ok &= ASSERT(t->kind == (TypeKind)k);
    }

    arena_delete(arena);
    return ok;
}

// ============================================================================
// Primitive name lookup
// ============================================================================

bool test_primitive_from_name(void) {
    bool ok = true;
    Arena *arena = arena_new(0);
    TypeContext ctx;
    type_context_init(&ctx, arena);

    ok &= ASSERT(type_primitive_from_name(&ctx, sv("void")) == ctx.t_void);
    ok &= ASSERT(type_primitive_from_name(&ctx, sv("bool")) == ctx.t_bool);
    ok &= ASSERT(type_primitive_from_name(&ctx, sv("type")) == ctx.t_type);
    ok &= ASSERT(type_primitive_from_name(&ctx, sv("i8"))   == ctx.t_i8);
    ok &= ASSERT(type_primitive_from_name(&ctx, sv("i32"))  == ctx.t_i32);
    ok &= ASSERT(type_primitive_from_name(&ctx, sv("u64"))  == ctx.t_u64);
    ok &= ASSERT(type_primitive_from_name(&ctx, sv("f64"))  == ctx.t_f64);

    arena_delete(arena);
    return ok;
}

bool test_primitive_from_name_unknown(void) {
    bool ok = true;
    Arena *arena = arena_new(0);
    TypeContext ctx;
    type_context_init(&ctx, arena);

    // User-defined names and near-misses are not primitives.
    ok &= ASSERT(type_primitive_from_name(&ctx, sv("Vec2")) == NULL);
    ok &= ASSERT(type_primitive_from_name(&ctx, sv("i33"))  == NULL);
    ok &= ASSERT(type_primitive_from_name(&ctx, sv("int"))  == NULL);
    ok &= ASSERT(type_primitive_from_name(&ctx, sv(""))     == NULL);

    arena_delete(arena);
    return ok;
}

// ============================================================================
// Resolution
// ============================================================================

bool test_resolve_primitive(void) {
    bool ok = true;
    Arena *arena = arena_new(0);
    TypeContext ctx;
    type_context_init(&ctx, arena);

    ok &= ASSERT(type_resolve(&ctx, NULL, prim_node(arena, "i32")) == ctx.t_i32);
    ok &= ASSERT(type_resolve(&ctx, NULL, prim_node(arena, "bool")) == ctx.t_bool);
    ok &= ASSERT(type_resolve(&ctx, NULL, prim_node(arena, "void")) == ctx.t_void);

    arena_delete(arena);
    return ok;
}

bool test_resolve_pointer(void) {
    bool ok = true;
    Arena *arena = arena_new(0);
    TypeContext ctx;
    type_context_init(&ctx, arena);

    Node *node = ptr_node(arena, prim_node(arena, "i32"), true);
    TypeInfo *t = type_resolve(&ctx, NULL, node);
    ok &= ASSERT(t->kind == TYPE_POINTER);

    TyPointer *p = TYPE_CAST_UNSAFE(t, TyPointer);
    ok &= ASSERT(p->is_mutable == true);
    ok &= ASSERT(p->pointee == ctx.t_i32);

    arena_delete(arena);
    return ok;
}

bool test_resolve_const_pointer(void) {
    bool ok = true;
    Arena *arena = arena_new(0);
    TypeContext ctx;
    type_context_init(&ctx, arena);

    // *const u8
    Node *node = ptr_node(arena, prim_node(arena, "u8"), false);
    TyPointer *p = TYPE_CAST(type_resolve(&ctx, NULL, node), TyPointer, TYPE_POINTER);
    ok &= ASSERT(p != NULL);
    ok &= ASSERT(p->is_mutable == false);
    ok &= ASSERT(p->pointee == ctx.t_u8);

    arena_delete(arena);
    return ok;
}

bool test_resolve_array(void) {
    bool ok = true;
    Arena *arena = arena_new(0);
    TypeContext ctx;
    type_context_init(&ctx, arena);

    // [4]i32
    Node *node = array_node(arena, prim_node(arena, "i32"), int_lit(arena, 4));
    TyArray *arr = TYPE_CAST(type_resolve(&ctx, NULL, node), TyArray, TYPE_ARRAY);
    ok &= ASSERT(arr != NULL);
    ok &= ASSERT(arr->length == 4);
    ok &= ASSERT(arr->element == ctx.t_i32);

    arena_delete(arena);
    return ok;
}

bool test_resolve_slice(void) {
    bool ok = true;
    Arena *arena = arena_new(0);
    TypeContext ctx;
    type_context_init(&ctx, arena);

    // []u8
    Node *node = array_node(arena, prim_node(arena, "u8"), NULL);
    TySlice *s = TYPE_CAST(type_resolve(&ctx, NULL, node), TySlice, TYPE_SLICE);
    ok &= ASSERT(s != NULL);
    ok &= ASSERT(s->element == ctx.t_u8);

    arena_delete(arena);
    return ok;
}

bool test_resolve_function(void) {
    bool ok = true;
    Arena *arena = arena_new(0);
    TypeContext ctx;
    type_context_init(&ctx, arena);

    // fn(i32, bool) -> void
    Node *params[2] = { prim_node(arena, "i32"), prim_node(arena, "bool") };
    Node *node = fn_node(arena, params, 2, prim_node(arena, "void"));
    TyFunction *f = TYPE_CAST(type_resolve(&ctx, NULL, node), TyFunction, TYPE_FUNCTION);
    ok &= ASSERT(f != NULL);
    ok &= ASSERT(f->param_count == 2);
    ok &= ASSERT(f->params[0] == ctx.t_i32);
    ok &= ASSERT(f->params[1] == ctx.t_bool);
    ok &= ASSERT(f->return_type == ctx.t_void);

    arena_delete(arena);
    return ok;
}

bool test_resolve_generic_is_error(void) {
    bool ok = true;
    Arena *arena = arena_new(0);
    TypeContext ctx;
    type_context_init(&ctx, arena);

    TypeGeneric *g = NODE_CAST_UNSAFE(ast_alloc_node(arena, NODE_TYPE_GENERIC, loc1), TypeGeneric);
    g->base_name = sv("List");
    ok &= ASSERT(type_resolve(&ctx, NULL, &g->base) == ctx.t_error);

    arena_delete(arena);
    return ok;
}

bool test_resolve_named_is_error(void) {
    bool ok = true;
    Arena *arena = arena_new(0);
    TypeContext ctx;
    type_context_init(&ctx, arena);

    // Named user type (no symbol table yet) resolves to the error type.
    ok &= ASSERT(type_resolve(&ctx, NULL, prim_node(arena, "Vec2")) == ctx.t_error);

    arena_delete(arena);
    return ok;
}

bool test_resolve_null_is_error(void) {
    bool ok = true;
    Arena *arena = arena_new(0);
    TypeContext ctx;
    type_context_init(&ctx, arena);

    ok &= ASSERT(type_resolve(&ctx, NULL, NULL) == ctx.t_error);

    arena_delete(arena);
    return ok;
}

// ============================================================================
// Equality
// ============================================================================

bool test_equals_primitives(void) {
    bool ok = true;
    Arena *arena = arena_new(0);
    TypeContext ctx;
    type_context_init(&ctx, arena);

    ok &= ASSERT(type_equals(ctx.t_i32, ctx.t_i32) == true);
    ok &= ASSERT(type_equals(ctx.t_i32, ctx.t_i64) == false);
    ok &= ASSERT(type_equals(ctx.t_i32, ctx.t_u32) == false);  // signedness
    ok &= ASSERT(type_equals(ctx.t_f32, ctx.t_f64) == false);
    ok &= ASSERT(type_equals(ctx.t_void, ctx.t_bool) == false);

    arena_delete(arena);
    return ok;
}

bool test_equals_pointer(void) {
    bool ok = true;
    Arena *arena = arena_new(0);
    TypeContext ctx;
    type_context_init(&ctx, arena);

    TypeInfo *a = type_pointer(&ctx, ctx.t_i32, true);
    TypeInfo *b = type_pointer(&ctx, ctx.t_i32, true);
    TypeInfo *c = type_pointer(&ctx, ctx.t_i64, true);
    ok &= ASSERT(a != b);                       // distinct allocations
    ok &= ASSERT(type_equals(a, b) == true);    // structurally equal
    ok &= ASSERT(type_equals(a, c) == false);

    arena_delete(arena);
    return ok;
}

bool test_equals_pointer_mutability(void) {
    bool ok = true;
    Arena *arena = arena_new(0);
    TypeContext ctx;
    type_context_init(&ctx, arena);

    TypeInfo *mut = type_pointer(&ctx, ctx.t_u8, true);
    TypeInfo *con = type_pointer(&ctx, ctx.t_u8, false);
    ok &= ASSERT(type_equals(mut, con) == false);

    arena_delete(arena);
    return ok;
}

bool test_equals_array_length(void) {
    bool ok = true;
    Arena *arena = arena_new(0);
    TypeContext ctx;
    type_context_init(&ctx, arena);

    TypeInfo *a4 = type_array(&ctx, ctx.t_i32, 4);
    TypeInfo *b4 = type_array(&ctx, ctx.t_i32, 4);
    TypeInfo *a8 = type_array(&ctx, ctx.t_i32, 8);
    ok &= ASSERT(type_equals(a4, b4) == true);
    ok &= ASSERT(type_equals(a4, a8) == false);

    // slice != array even with same element
    ok &= ASSERT(type_equals(type_slice(&ctx, ctx.t_i32), a4) == false);

    arena_delete(arena);
    return ok;
}

bool test_equals_function(void) {
    bool ok = true;
    Arena *arena = arena_new(0);
    TypeContext ctx;
    type_context_init(&ctx, arena);

    TypeInfo *p1[1] = { ctx.t_i32 };
    TypeInfo *p2[1] = { ctx.t_i64 };

    TypeInfo *a = type_function(&ctx, p1, 1, ctx.t_void, false);
    TypeInfo *b = type_function(&ctx, p1, 1, ctx.t_void, false);
    TypeInfo *c = type_function(&ctx, p2, 1, ctx.t_void, false);  // diff param
    TypeInfo *d = type_function(&ctx, p1, 1, ctx.t_bool, false);  // diff return
    TypeInfo *e = type_function(&ctx, p1, 1, ctx.t_void, true);   // variadic

    ok &= ASSERT(type_equals(a, b) == true);
    ok &= ASSERT(type_equals(a, c) == false);
    ok &= ASSERT(type_equals(a, d) == false);
    ok &= ASSERT(type_equals(a, e) == false);

    arena_delete(arena);
    return ok;
}

bool test_equals_aggregate_nominal(void) {
    bool ok = true;
    Arena *arena = arena_new(0);
    TypeContext ctx;
    type_context_init(&ctx, arena);

    // Two distinct declarations stand in as nominal identity.
    Node *decl_a = ast_alloc_node(arena, NODE_DECL_STRUCT, loc1);
    Node *decl_b = ast_alloc_node(arena, NODE_DECL_STRUCT, loc1);

    TyAggregate *s1 = TYPE_CAST_UNSAFE(type_alloc(&ctx, TYPE_STRUCT), TyAggregate);
    s1->name = sv("Vec2"); s1->decl = decl_a;
    TyAggregate *s2 = TYPE_CAST_UNSAFE(type_alloc(&ctx, TYPE_STRUCT), TyAggregate);
    s2->name = sv("Vec2"); s2->decl = decl_a;       // same decl
    TyAggregate *s3 = TYPE_CAST_UNSAFE(type_alloc(&ctx, TYPE_STRUCT), TyAggregate);
    s3->name = sv("Vec2"); s3->decl = decl_b;       // different decl

    ok &= ASSERT(type_equals(&s1->base, &s2->base) == true);
    ok &= ASSERT(type_equals(&s1->base, &s3->base) == false);

    arena_delete(arena);
    return ok;
}

// ============================================================================
// Queries + formatting
// ============================================================================

bool test_type_queries(void) {
    bool ok = true;
    Arena *arena = arena_new(0);
    TypeContext ctx;
    type_context_init(&ctx, arena);

    ok &= ASSERT(type_is_integer(ctx.t_i32) == true);
    ok &= ASSERT(type_is_integer(ctx.t_f32) == false);
    ok &= ASSERT(type_is_float(ctx.t_f64)   == true);
    ok &= ASSERT(type_is_float(ctx.t_i8)    == false);
    ok &= ASSERT(type_is_numeric(ctx.t_i32) == true);
    ok &= ASSERT(type_is_numeric(ctx.t_f64) == true);
    ok &= ASSERT(type_is_numeric(ctx.t_bool) == false);
    ok &= ASSERT(type_is_bool(ctx.t_bool)   == true);
    ok &= ASSERT(type_is_error(ctx.t_error) == true);
    ok &= ASSERT(type_is_error(ctx.t_i32)   == false);

    arena_delete(arena);
    return ok;
}

bool test_type_to_string(void) {
    bool ok = true;
    Arena *arena = arena_new(0);
    TypeContext ctx;
    type_context_init(&ctx, arena);

    ok &= ASSERT(strcmp(type_to_string(ctx.t_i32, arena), "i32") == 0);
    ok &= ASSERT(strcmp(type_to_string(ctx.t_u8, arena), "u8") == 0);
    ok &= ASSERT(strcmp(type_to_string(ctx.t_f64, arena), "f64") == 0);
    ok &= ASSERT(strcmp(type_to_string(ctx.t_bool, arena), "bool") == 0);
    ok &= ASSERT(strcmp(type_to_string(ctx.t_void, arena), "void") == 0);

    // *const u8
    TypeInfo *cptr = type_pointer(&ctx, ctx.t_u8, false);
    ok &= ASSERT(strcmp(type_to_string(cptr, arena), "*const u8") == 0);

    // *i32
    TypeInfo *mptr = type_pointer(&ctx, ctx.t_i32, true);
    ok &= ASSERT(strcmp(type_to_string(mptr, arena), "*i32") == 0);

    // [4]i32
    TypeInfo *arr = type_array(&ctx, ctx.t_i32, 4);
    ok &= ASSERT(strcmp(type_to_string(arr, arena), "[4]i32") == 0);

    // []u8
    TypeInfo *slc = type_slice(&ctx, ctx.t_u8);
    ok &= ASSERT(strcmp(type_to_string(slc, arena), "[]u8") == 0);

    // fn(i32, bool)->void
    TypeInfo *params[2] = { ctx.t_i32, ctx.t_bool };
    TypeInfo *fn = type_function(&ctx, params, 2, ctx.t_void, false);
    ok &= ASSERT(strcmp(type_to_string(fn, arena), "fn(i32, bool)->void") == 0);

    arena_delete(arena);
    return ok;
}

// ============================================================================
// Suite
// ============================================================================

static bool type_tests(void) {
    bool ok = true;

    struct { TestFn func; const char *name; } tests[] = {
        { test_type_context_init,        "test_type_context_init"        },
        { test_type_alloc_all_kinds,     "test_type_alloc_all_kinds"     },

        { test_primitive_from_name,      "test_primitive_from_name"      },
        { test_primitive_from_name_unknown, "test_primitive_from_name_unknown" },

        { test_resolve_primitive,        "test_resolve_primitive"        },
        { test_resolve_pointer,          "test_resolve_pointer"          },
        { test_resolve_const_pointer,    "test_resolve_const_pointer"    },
        { test_resolve_array,            "test_resolve_array"            },
        { test_resolve_slice,            "test_resolve_slice"            },
        { test_resolve_function,         "test_resolve_function"         },
        { test_resolve_generic_is_error, "test_resolve_generic_is_error" },
        { test_resolve_named_is_error,   "test_resolve_named_is_error"   },
        { test_resolve_null_is_error,    "test_resolve_null_is_error"    },

        { test_equals_primitives,        "test_equals_primitives"        },
        { test_equals_pointer,           "test_equals_pointer"           },
        { test_equals_pointer_mutability,"test_equals_pointer_mutability"},
        { test_equals_array_length,      "test_equals_array_length"      },
        { test_equals_function,          "test_equals_function"          },
        { test_equals_aggregate_nominal, "test_equals_aggregate_nominal" },

        { test_type_queries,             "test_type_queries"             },
        { test_type_to_string,           "test_type_to_string"           },
    };

    for (size_t i = 0; i < sizeof(tests) / sizeof(tests[0]); i++) {
        bool res = tests[i].func();
        if (!res) fprintf(stderr, "test %s failed.\n", tests[i].name);
        ok &= res;
    }
    return ok;
}

TestSuite type_suite(void) {
    return (TestSuite){ .name = "Type", .func = type_tests };
}
