#include "sema_test.h"
#include "../../kazec/include/sema.h"
#include "../../kazec/include/lexer.h"
#include "../../kazec/include/parser.h"
#include "../../kazec/include/type.h"
#include "../../kazec/include/scope.h"
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

// Lex + parse `src` into `*out_prog` and return a fresh, initialised Sema. The
// Program (its decls live in the arena) is handed back so callers can inspect
// the AST — e.g. assert that node->type_info was filled by Pass 2.
static Sema *make_sema(Arena *arena, const char *src, Program *out_prog) {
    Lexer lexer;
    lexer_init(&lexer, src, arena);
    TokenVec tokens = lexer_get_tokens(&lexer);

    Parser parser;
    parser_init(&parser, tokens, arena, "test.kz");
    *out_prog = parse_program(&parser);

    Sema *s = arena_alloc(arena, sizeof(Sema));
    sema_init(s, arena, "test.kz");
    return s;
}

// Run Pass 1 only (declaration collection).
static Sema *analyze(Arena *arena, const char *src) {
    Program prog;
    Sema *s = make_sema(arena, src, &prog);
    sema_collect(s, &prog);
    return s;
}

// Run both passes (Pass 1, then Pass 2 over bodies).
static Sema *analyze_full(Arena *arena, const char *src) {
    Program prog;
    Sema *s = make_sema(arena, src, &prog);
    sema_check(s, &prog);
    return s;
}

// True when some recorded diagnostic contains `needle`.
static bool has_error(Sema *s, const char *needle) {
    for (size_t i = 0; i < s->errors.len; i++) {
        if (strstr(ErrorVec_get(&s->errors, i), needle) != NULL) return true;
    }
    return false;
}

// ============================================================================

bool test_sema_collect_function(void) {
    bool ok = true;
    Arena *arena = arena_new(0);

    Sema *s = analyze(arena, "fn soma(a: i32, b: i32) i32 { return a + b; }");
    ok &= ASSERT(s->had_error == false);

    Symbol *sym = scope_lookup(s->global, sv("soma"));
    ok &= ASSERT(sym != NULL);
    ok &= ASSERT(sym->kind == SYMBOL_FUNCTION);
    ok &= ASSERT(sym->type->kind == TYPE_FUNCTION);

    TyFunction *fn = TYPE_CAST(sym->type, TyFunction, TYPE_FUNCTION);
    ok &= ASSERT(fn != NULL);
    ok &= ASSERT(fn->param_count == 2);
    ok &= ASSERT(fn->params[0] == s->types.t_i32);
    ok &= ASSERT(fn->params[1] == s->types.t_i32);
    ok &= ASSERT(fn->return_type == s->types.t_i32);
    ok &= ASSERT(fn->is_variadic == false);

    arena_delete(arena);
    return ok;
}

bool test_sema_collect_struct(void) {
    bool ok = true;
    Arena *arena = arena_new(0);

    Sema *s = analyze(arena, "const Vec2 = struct { x: f32, y: f32, };");
    ok &= ASSERT(s->had_error == false);

    Symbol *sym = scope_lookup(s->global, sv("Vec2"));
    ok &= ASSERT(sym != NULL);
    ok &= ASSERT(sym->kind == SYMBOL_TYPE);
    ok &= ASSERT(sym->type->kind == TYPE_STRUCT);

    TyAggregate *agg = TYPE_CAST(sym->type, TyAggregate, TYPE_STRUCT);
    ok &= ASSERT(agg != NULL);
    ok &= ASSERT(agg->name.len == 4 && memcmp(agg->name.data, "Vec2", 4) == 0);
    ok &= ASSERT(agg->decl == sym->decl);
    ok &= ASSERT(agg->decl->type_info == sym->type);   // linked back onto the node

    arena_delete(arena);
    return ok;
}

bool test_sema_collect_enum_union(void) {
    bool ok = true;
    Arena *arena = arena_new(0);

    Sema *s = analyze(arena,
        "const Cor = enum { Vermelho, Verde, Azul };\n"
        "const Bits = union { as_i32: i32, as_f32: f32, };");
    ok &= ASSERT(s->had_error == false);

    Symbol *cor = scope_lookup(s->global, sv("Cor"));
    ok &= ASSERT(cor != NULL && cor->kind == SYMBOL_TYPE);
    ok &= ASSERT(cor->type->kind == TYPE_ENUM);

    Symbol *bits = scope_lookup(s->global, sv("Bits"));
    ok &= ASSERT(bits != NULL && bits->kind == SYMBOL_TYPE);
    ok &= ASSERT(bits->type->kind == TYPE_UNION);

    arena_delete(arena);
    return ok;
}

bool test_sema_collect_type_alias(void) {
    bool ok = true;
    Arena *arena = arena_new(0);

    Sema *s = analyze(arena, "const Inteiro = i64;\nconst Bytes = []u8;");
    ok &= ASSERT(s->had_error == false);

    Symbol *intg = scope_lookup(s->global, sv("Inteiro"));
    ok &= ASSERT(intg != NULL && intg->kind == SYMBOL_TYPE);
    ok &= ASSERT(intg->type == s->types.t_i64);   // resolved to canonical i64

    Symbol *bytes = scope_lookup(s->global, sv("Bytes"));
    ok &= ASSERT(bytes != NULL);
    ok &= ASSERT(bytes->type->kind == TYPE_SLICE);
    ok &= ASSERT(TYPE_CAST_UNSAFE(bytes->type, TySlice)->element == s->types.t_u8);

    arena_delete(arena);
    return ok;
}

bool test_sema_named_type_in_signature(void) {
    bool ok = true;
    Arena *arena = arena_new(0);

    Sema *s = analyze(arena,
        "const Vec2 = struct { x: f32, y: f32, };\n"
        "fn mag(v: Vec2) f32 { return v.x; }");
    ok &= ASSERT(s->had_error == false);

    Symbol *vec  = scope_lookup(s->global, sv("Vec2"));
    Symbol *mag  = scope_lookup(s->global, sv("mag"));
    ok &= ASSERT(vec != NULL && mag != NULL);

    TyFunction *fn = TYPE_CAST(mag->type, TyFunction, TYPE_FUNCTION);
    ok &= ASSERT(fn != NULL);
    ok &= ASSERT(fn->param_count == 1);
    ok &= ASSERT(fn->params[0] == vec->type);          // named type resolved
    ok &= ASSERT(fn->params[0]->kind == TYPE_STRUCT);

    arena_delete(arena);
    return ok;
}

bool test_sema_pointer_to_named_type(void) {
    bool ok = true;
    Arena *arena = arena_new(0);

    Sema *s = analyze(arena,
        "const Nodo = struct { valor: i32, };\n"
        "fn head(n: *Nodo) i32 { return n.valor; }");
    ok &= ASSERT(s->had_error == false);

    Symbol *nodo = scope_lookup(s->global, sv("Nodo"));
    Symbol *head = scope_lookup(s->global, sv("head"));
    ok &= ASSERT(nodo != NULL && head != NULL);

    TyFunction *fn = TYPE_CAST(head->type, TyFunction, TYPE_FUNCTION);
    ok &= ASSERT(fn != NULL && fn->param_count == 1);
    TyPointer *p = TYPE_CAST(fn->params[0], TyPointer, TYPE_POINTER);
    ok &= ASSERT(p != NULL);
    ok &= ASSERT(p->pointee == nodo->type);            // *Nodo points at the struct

    arena_delete(arena);
    return ok;
}

bool test_sema_forward_reference(void) {
    bool ok = true;
    Arena *arena = arena_new(0);

    // The function is declared *before* the struct it references.
    Sema *s = analyze(arena,
        "fn use(v: Vec2) f32 { return v.x; }\n"
        "const Vec2 = struct { x: f32, y: f32, };");
    ok &= ASSERT(s->had_error == false);

    Symbol *vec = scope_lookup(s->global, sv("Vec2"));
    Symbol *use = scope_lookup(s->global, sv("use"));
    ok &= ASSERT(vec != NULL && use != NULL);

    TyFunction *fn = TYPE_CAST(use->type, TyFunction, TYPE_FUNCTION);
    ok &= ASSERT(fn != NULL);
    ok &= ASSERT(fn->params[0] == vec->type);          // resolved despite order
    ok &= ASSERT(fn->params[0]->kind == TYPE_STRUCT);

    arena_delete(arena);
    return ok;
}

bool test_sema_global_const(void) {
    bool ok = true;
    Arena *arena = arena_new(0);

    Sema *s = analyze(arena, "const PI: f64 = 3.14;\nvar contador: i32 = 0;");
    ok &= ASSERT(s->had_error == false);

    Symbol *pi = scope_lookup(s->global, sv("PI"));
    ok &= ASSERT(pi != NULL && pi->kind == SYMBOL_VAR);
    ok &= ASSERT(pi->is_const == true);
    ok &= ASSERT(pi->type == s->types.t_f64);

    Symbol *c = scope_lookup(s->global, sv("contador"));
    ok &= ASSERT(c != NULL && c->kind == SYMBOL_VAR);
    ok &= ASSERT(c->is_const == false);
    ok &= ASSERT(c->type == s->types.t_i32);

    arena_delete(arena);
    return ok;
}

bool test_sema_redefinition(void) {
    bool ok = true;
    Arena *arena = arena_new(0);

    Sema *s = analyze(arena,
        "fn foo() void { return; }\n"
        "fn foo() void { return; }");
    ok &= ASSERT(s->had_error == true);
    ok &= ASSERT(s->errors.len >= 1);
    ok &= ASSERT(strstr(ErrorVec_get(&s->errors, 0), "redefinition") != NULL);

    arena_delete(arena);
    return ok;
}

bool test_sema_void_return(void) {
    bool ok = true;
    Arena *arena = arena_new(0);

    Sema *s = analyze(arena, "fn nada() void { return; }");
    ok &= ASSERT(s->had_error == false);

    Symbol *fn = scope_lookup(s->global, sv("nada"));
    ok &= ASSERT(fn != NULL);
    TyFunction *ft = TYPE_CAST(fn->type, TyFunction, TYPE_FUNCTION);
    ok &= ASSERT(ft != NULL);
    ok &= ASSERT(ft->param_count == 0);
    ok &= ASSERT(ft->return_type == s->types.t_void);

    arena_delete(arena);
    return ok;
}

// ============================================================================
// Pass 2 — body checking
// ============================================================================

bool test_sema_body_ok(void) {
    bool ok = true;
    Arena *arena = arena_new(0);

    Sema *s = analyze_full(arena,
        "fn add(a: i32, b: i32) i32 { return a + b; }");
    ok &= ASSERT(s->had_error == false);
    ok &= ASSERT(s->errors.len == 0);

    arena_delete(arena);
    return ok;
}

bool test_sema_undeclared_ident(void) {
    bool ok = true;
    Arena *arena = arena_new(0);

    Sema *s = analyze_full(arena, "fn f() i32 { return x; }");
    ok &= ASSERT(s->had_error == true);
    ok &= ASSERT(has_error(s, "undeclared identifier"));

    arena_delete(arena);
    return ok;
}

bool test_sema_return_mismatch(void) {
    bool ok = true;
    Arena *arena = arena_new(0);

    Sema *s = analyze_full(arena, "fn f() i32 { return true; }");
    ok &= ASSERT(s->had_error == true);
    ok &= ASSERT(has_error(s, "return type mismatch"));

    arena_delete(arena);
    return ok;
}

bool test_sema_if_condition_not_bool(void) {
    bool ok = true;
    Arena *arena = arena_new(0);

    Sema *s = analyze_full(arena, "fn f() void { if 1 { return; } }");
    ok &= ASSERT(s->had_error == true);
    ok &= ASSERT(has_error(s, "if condition must be 'bool'"));

    arena_delete(arena);
    return ok;
}

bool test_sema_assign_to_const(void) {
    bool ok = true;
    Arena *arena = arena_new(0);

    Sema *s = analyze_full(arena,
        "fn f() void { const x: i32 = 1; x = 2; }");
    ok &= ASSERT(s->had_error == true);
    ok &= ASSERT(has_error(s, "immutable"));

    arena_delete(arena);
    return ok;
}

bool test_sema_local_inference(void) {
    bool ok = true;
    Arena *arena = arena_new(0);

    // `var x = 5` infers i32, which matches the return type — no errors.
    Sema *s = analyze_full(arena, "fn f() i32 { var x = 5; return x; }");
    ok &= ASSERT(s->had_error == false);
    ok &= ASSERT(s->errors.len == 0);

    arena_delete(arena);
    return ok;
}

bool test_sema_call_arity(void) {
    bool ok = true;
    Arena *arena = arena_new(0);

    Sema *s = analyze_full(arena,
        "fn g(a: i32) i32 { return a; }\n"
        "fn f() i32 { return g(); }");
    ok &= ASSERT(s->had_error == true);
    ok &= ASSERT(has_error(s, "expected 1 argument"));

    arena_delete(arena);
    return ok;
}

bool test_sema_call_arg_type(void) {
    bool ok = true;
    Arena *arena = arena_new(0);

    Sema *s = analyze_full(arena,
        "fn g(a: i32) i32 { return a; }\n"
        "fn f() i32 { return g(true); }");
    ok &= ASSERT(s->had_error == true);
    ok &= ASSERT(has_error(s, "argument 1"));

    arena_delete(arena);
    return ok;
}

bool test_sema_binary_mismatch(void) {
    bool ok = true;
    Arena *arena = arena_new(0);

    Sema *s = analyze_full(arena,
        "fn f() void { var a: i32 = 1; var b: i64 = 2; var c = a + b; }");
    ok &= ASSERT(s->had_error == true);
    ok &= ASSERT(has_error(s, "mismatched operands"));

    arena_delete(arena);
    return ok;
}

bool test_sema_deref_non_pointer(void) {
    bool ok = true;
    Arena *arena = arena_new(0);

    Sema *s = analyze_full(arena,
        "fn f() void { var a: i32 = 1; var b = *a; }");
    ok &= ASSERT(s->had_error == true);
    ok &= ASSERT(has_error(s, "dereference"));

    arena_delete(arena);
    return ok;
}

// Pass 2 records the inferred type on every expression node it visits.
bool test_sema_expr_types_recorded(void) {
    bool ok = true;
    Arena *arena = arena_new(0);

    Program prog;
    Sema *s = make_sema(arena, "fn f() i32 { return 1 + 2; }", &prog);
    sema_check(s, &prog);
    ok &= ASSERT(s->had_error == false);

    DeclFunction *fn = NODE_CAST(prog.decls[0], DeclFunction, NODE_DECL_FUNCTION);
    ok &= ASSERT(fn != NULL && fn->body_len >= 1);

    StmtReturn *ret = NODE_CAST(fn->body[0], StmtReturn, NODE_STMT_RETURN);
    ok &= ASSERT(ret != NULL && ret->value != NULL);
    ok &= ASSERT(ret->value->type_info == s->types.t_i32);   // (1 + 2) : i32

    ExprBinaryOp *bin = NODE_CAST(ret->value, ExprBinaryOp, NODE_EXPR_BINARY_OP);
    ok &= ASSERT(bin != NULL);
    ok &= ASSERT(bin->left->type_info == s->types.t_i32);    // operands recorded too
    ok &= ASSERT(bin->right->type_info == s->types.t_i32);

    arena_delete(arena);
    return ok;
}

bool test_sema_logical_requires_bool(void) {
    bool ok = true;
    Arena *arena = arena_new(0);

    Sema *s = analyze_full(arena, "fn f() void { var b = 1 && 2; }");
    ok &= ASSERT(s->had_error == true);
    ok &= ASSERT(has_error(s, "requires 'bool'"));

    arena_delete(arena);
    return ok;
}

bool test_sema_index_non_array(void) {
    bool ok = true;
    Arena *arena = arena_new(0);

    Sema *s = analyze_full(arena, "fn f() void { var a: i32 = 1; var b = a[0]; }");
    ok &= ASSERT(s->had_error == true);
    ok &= ASSERT(has_error(s, "cannot index"));

    arena_delete(arena);
    return ok;
}

bool test_sema_call_non_function(void) {
    bool ok = true;
    Arena *arena = arena_new(0);

    Sema *s = analyze_full(arena, "fn f() void { var a: i32 = 1; a(); }");
    ok &= ASSERT(s->had_error == true);
    ok &= ASSERT(has_error(s, "cannot call"));

    arena_delete(arena);
    return ok;
}

bool test_sema_assign_type_mismatch(void) {
    bool ok = true;
    Arena *arena = arena_new(0);

    Sema *s = analyze_full(arena, "fn f() void { var a: i32 = 1; a = true; }");
    ok &= ASSERT(s->had_error == true);
    ok &= ASSERT(has_error(s, "cannot assign"));

    arena_delete(arena);
    return ok;
}

bool test_sema_nested_scope_shadowing(void) {
    bool ok = true;
    Arena *arena = arena_new(0);

    // The inner `x` lives only in the block; the outer `x: i32` is returned.
    Sema *s = analyze_full(arena,
        "fn f() i32 { var x: i32 = 1; if true { var x: i64 = 2; } return x; }");
    ok &= ASSERT(s->had_error == false);
    ok &= ASSERT(s->errors.len == 0);

    arena_delete(arena);
    return ok;
}

// Unchecked member access yields the error type, which must not cascade into
// spurious diagnostics through arithmetic or the return check.
bool test_sema_field_access_no_cascade(void) {
    bool ok = true;
    Arena *arena = arena_new(0);

    Sema *s = analyze_full(arena,
        "const Vec2 = struct { x: f32, y: f32, };\n"
        "fn sum(v: *Vec2) f32 { return v.x + v.y; }");
    ok &= ASSERT(s->had_error == false);
    ok &= ASSERT(s->errors.len == 0);

    arena_delete(arena);
    return ok;
}

// Untyped numeric literals adopt the annotated/expected type; non-literals of
// the wrong type still error. Also checks the literal node's recorded type.
bool test_sema_literal_coercion(void) {
    bool ok = true;
    Arena *arena = arena_new(0);

    // i64 / u8 / f32 slots accept plain literals.
    Program prog;
    Sema *s = make_sema(arena,
        "fn g(n: i64) i64 { return n; }\n"
        "fn f() i64 { var a: i64 = 2; var b: u8 = 7; var c: f32 = 1.5; return g(3); }",
        &prog);
    sema_check(s, &prog);
    ok &= ASSERT(s->had_error == false);
    ok &= ASSERT(s->errors.len == 0);

    // The literal `2` initializing `a: i64` adopts i64 (not the default i32).
    DeclFunction *f = NODE_CAST(prog.decls[1], DeclFunction, NODE_DECL_FUNCTION);
    ok &= ASSERT(f != NULL);
    StmtVarDecl *a = NODE_CAST(f->body[0], StmtVarDecl, NODE_STMT_VAR_DECL);
    ok &= ASSERT(a != NULL && a->initializer != NULL);
    ok &= ASSERT(a->initializer->type_info == s->types.t_i64);

    // A non-literal of the wrong width is still rejected.
    Sema *s2 = analyze_full(arena,
        "fn f() void { var a: i32 = 1; var b: i64 = a; }");
    ok &= ASSERT(s2->had_error == true);

    arena_delete(arena);
    return ok;
}

// ============================================================================
// Suite
// ============================================================================

static bool sema_tests(void) {
    bool ok = true;

    struct { TestFn func; const char *name; } tests[] = {
        { test_sema_collect_function,      "test_sema_collect_function"      },
        { test_sema_collect_struct,        "test_sema_collect_struct"        },
        { test_sema_collect_enum_union,    "test_sema_collect_enum_union"    },
        { test_sema_collect_type_alias,    "test_sema_collect_type_alias"    },
        { test_sema_named_type_in_signature,"test_sema_named_type_in_signature"},
        { test_sema_pointer_to_named_type, "test_sema_pointer_to_named_type" },
        { test_sema_forward_reference,     "test_sema_forward_reference"     },
        { test_sema_global_const,          "test_sema_global_const"          },
        { test_sema_redefinition,          "test_sema_redefinition"          },
        { test_sema_void_return,           "test_sema_void_return"           },

        { test_sema_body_ok,               "test_sema_body_ok"               },
        { test_sema_undeclared_ident,      "test_sema_undeclared_ident"      },
        { test_sema_return_mismatch,       "test_sema_return_mismatch"       },
        { test_sema_if_condition_not_bool, "test_sema_if_condition_not_bool" },
        { test_sema_assign_to_const,       "test_sema_assign_to_const"       },
        { test_sema_local_inference,       "test_sema_local_inference"       },
        { test_sema_call_arity,            "test_sema_call_arity"            },
        { test_sema_call_arg_type,         "test_sema_call_arg_type"         },
        { test_sema_binary_mismatch,       "test_sema_binary_mismatch"       },
        { test_sema_deref_non_pointer,     "test_sema_deref_non_pointer"     },
        { test_sema_expr_types_recorded,   "test_sema_expr_types_recorded"   },
        { test_sema_logical_requires_bool, "test_sema_logical_requires_bool" },
        { test_sema_index_non_array,       "test_sema_index_non_array"       },
        { test_sema_call_non_function,     "test_sema_call_non_function"     },
        { test_sema_assign_type_mismatch,  "test_sema_assign_type_mismatch"  },
        { test_sema_nested_scope_shadowing,"test_sema_nested_scope_shadowing"},
        { test_sema_field_access_no_cascade,"test_sema_field_access_no_cascade"},
        { test_sema_literal_coercion,      "test_sema_literal_coercion"      },
    };

    for (size_t i = 0; i < sizeof(tests) / sizeof(tests[0]); i++) {
        bool res = tests[i].func();
        if (!res) fprintf(stderr, "test %s failed.\n", tests[i].name);
        ok &= res;
    }
    return ok;
}

TestSuite sema_suite(void) {
    return (TestSuite){ .name = "Sema", .func = sema_tests };
}
