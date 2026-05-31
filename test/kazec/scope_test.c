#include "scope_test.h"
#include "../../kazec/include/scope.h"
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

static StringView sv(const char *s) {
    return string_view_create(s, strlen(s));
}

static bool name_is(Symbol *sym, const char *s) {
    return sym && sym->name.len == strlen(s) &&
           memcmp(sym->name.data, s, sym->name.len) == 0;
}

// ============================================================================

bool test_scope_new(void) {
    bool ok = true;
    Arena *arena = arena_new(0);

    Scope *global = scope_new(arena, NULL);
    ok &= ASSERT(global != NULL);
    ok &= ASSERT(global->parent == NULL);
    ok &= ASSERT(global->arena == arena);
    ok &= ASSERT(hashmap_count(&global->symbols) == 0);

    Scope *child = scope_new(arena, global);
    ok &= ASSERT(child->parent == global);

    arena_delete(arena);
    return ok;
}

bool test_scope_define_and_lookup(void) {
    bool ok = true;
    Arena *arena = arena_new(0);
    TypeContext ctx; type_context_init(&ctx, arena);

    Scope *s = scope_new(arena, NULL);
    Symbol *def = scope_define(s, SYMBOL_VAR, sv("x"), ctx.t_i32, NULL, false);
    ok &= ASSERT(def != NULL);

    Symbol *got = scope_lookup(s, sv("x"));
    ok &= ASSERT(got == def);                 // same Symbol object
    ok &= ASSERT(got->type == ctx.t_i32);
    ok &= ASSERT(name_is(got, "x"));

    arena_delete(arena);
    return ok;
}

bool test_scope_define_redefinition(void) {
    bool ok = true;
    Arena *arena = arena_new(0);
    TypeContext ctx; type_context_init(&ctx, arena);

    Scope *s = scope_new(arena, NULL);
    Symbol *first = scope_define(s, SYMBOL_VAR, sv("dup"), ctx.t_i32, NULL, false);
    ok &= ASSERT(first != NULL);

    // Redefining the same name in the same scope fails and leaves the original.
    Symbol *second = scope_define(s, SYMBOL_VAR, sv("dup"), ctx.t_bool, NULL, true);
    ok &= ASSERT(second == NULL);
    ok &= ASSERT(scope_lookup(s, sv("dup"))->type == ctx.t_i32);

    arena_delete(arena);
    return ok;
}

bool test_scope_lookup_local_only(void) {
    bool ok = true;
    Arena *arena = arena_new(0);
    TypeContext ctx; type_context_init(&ctx, arena);

    Scope *parent = scope_new(arena, NULL);
    Scope *child  = scope_new(arena, parent);
    scope_define(parent, SYMBOL_VAR, sv("p"), ctx.t_i32, NULL, false);

    // local lookup does not walk parents
    ok &= ASSERT(scope_lookup_local(child, sv("p")) == NULL);
    ok &= ASSERT(scope_lookup_local(parent, sv("p")) != NULL);

    arena_delete(arena);
    return ok;
}

bool test_scope_lookup_parent_chain(void) {
    bool ok = true;
    Arena *arena = arena_new(0);
    TypeContext ctx; type_context_init(&ctx, arena);

    Scope *global = scope_new(arena, NULL);
    Scope *fn     = scope_new(arena, global);
    Scope *block  = scope_new(arena, fn);

    scope_define(global, SYMBOL_FUNCTION, sv("main"), NULL, NULL, false);

    // resolved from a deeply nested scope via the parent chain
    Symbol *got = scope_lookup(block, sv("main"));
    ok &= ASSERT(got != NULL);
    ok &= ASSERT(got->kind == SYMBOL_FUNCTION);
    ok &= ASSERT(scope_lookup_local(block, sv("main")) == NULL);

    arena_delete(arena);
    return ok;
}

bool test_scope_shadowing(void) {
    bool ok = true;
    Arena *arena = arena_new(0);
    TypeContext ctx; type_context_init(&ctx, arena);

    Scope *parent = scope_new(arena, NULL);
    Scope *child  = scope_new(arena, parent);

    scope_define(parent, SYMBOL_VAR, sv("x"), ctx.t_i32, NULL, false);

    // shadowing in the inner scope is allowed (not a redefinition)
    Symbol *inner = scope_define(child, SYMBOL_VAR, sv("x"), ctx.t_bool, NULL, true);
    ok &= ASSERT(inner != NULL);

    ok &= ASSERT(scope_lookup(child, sv("x"))->type  == ctx.t_bool);  // inner wins
    ok &= ASSERT(scope_lookup(parent, sv("x"))->type == ctx.t_i32);   // outer intact

    arena_delete(arena);
    return ok;
}

bool test_scope_undefined(void) {
    bool ok = true;
    Arena *arena = arena_new(0);

    Scope *s = scope_new(arena, NULL);
    ok &= ASSERT(scope_lookup(s, sv("nope")) == NULL);
    ok &= ASSERT(scope_lookup_local(s, sv("nope")) == NULL);

    arena_delete(arena);
    return ok;
}

bool test_scope_symbol_fields(void) {
    bool ok = true;
    Arena *arena = arena_new(0);
    TypeContext ctx; type_context_init(&ctx, arena);

    Node *decl = ast_alloc_node(arena, NODE_DECL_FUNCTION,
                                (SourceLoc){ .filename = "t.kz", .line = 3, .col = 1 });

    Symbol *fn = scope_define(scope_new(arena, NULL), SYMBOL_FUNCTION,
                              sv("compute"), ctx.t_void, decl, false);
    ok &= ASSERT(fn->kind == SYMBOL_FUNCTION);
    ok &= ASSERT(name_is(fn, "compute"));
    ok &= ASSERT(fn->type == ctx.t_void);
    ok &= ASSERT(fn->decl == decl);
    ok &= ASSERT(fn->is_const == false);

    Scope *s = scope_new(arena, NULL);
    Symbol *c = scope_define(s, SYMBOL_VAR, sv("PI"), ctx.t_f64, NULL, true);
    ok &= ASSERT(c->kind == SYMBOL_VAR);
    ok &= ASSERT(c->is_const == true);

    arena_delete(arena);
    return ok;
}

bool test_scope_sibling_isolation(void) {
    bool ok = true;
    Arena *arena = arena_new(0);
    TypeContext ctx; type_context_init(&ctx, arena);

    Scope *parent = scope_new(arena, NULL);
    Scope *a = scope_new(arena, parent);
    Scope *b = scope_new(arena, parent);

    scope_define(a, SYMBOL_VAR, sv("local"), ctx.t_i32, NULL, false);

    // a sibling scope cannot see bindings defined in the other
    ok &= ASSERT(scope_lookup(a, sv("local")) != NULL);
    ok &= ASSERT(scope_lookup(b, sv("local")) == NULL);

    arena_delete(arena);
    return ok;
}

bool test_symbol_kind_to_string(void) {
    bool ok = true;
    ok &= ASSERT(strcmp(symbol_kind_to_string(SYMBOL_VAR), "var") == 0);
    ok &= ASSERT(strcmp(symbol_kind_to_string(SYMBOL_FUNCTION), "function") == 0);
    ok &= ASSERT(strcmp(symbol_kind_to_string(SYMBOL_TYPE), "type") == 0);
    return ok;
}

// ============================================================================
// Suite
// ============================================================================

static bool scope_tests(void) {
    bool ok = true;

    struct { TestFn func; const char *name; } tests[] = {
        { test_scope_new,                "test_scope_new"                },
        { test_scope_define_and_lookup,  "test_scope_define_and_lookup"  },
        { test_scope_define_redefinition,"test_scope_define_redefinition"},
        { test_scope_lookup_local_only,  "test_scope_lookup_local_only"  },
        { test_scope_lookup_parent_chain,"test_scope_lookup_parent_chain"},
        { test_scope_shadowing,          "test_scope_shadowing"          },
        { test_scope_undefined,          "test_scope_undefined"          },
        { test_scope_symbol_fields,      "test_scope_symbol_fields"      },
        { test_scope_sibling_isolation,  "test_scope_sibling_isolation"  },
        { test_symbol_kind_to_string,    "test_symbol_kind_to_string"    },
    };

    for (size_t i = 0; i < sizeof(tests) / sizeof(tests[0]); i++) {
        bool res = tests[i].func();
        if (!res) fprintf(stderr, "test %s failed.\n", tests[i].name);
        ok &= res;
    }
    return ok;
}

TestSuite scope_suite(void) {
    return (TestSuite){ .name = "Scope", .func = scope_tests };
}
