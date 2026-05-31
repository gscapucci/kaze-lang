#include "parser_test.h"
#include "../../kazec/include/lexer.h"
#include "../../kazec/include/parser.h"
#include "../../kazec/include/ast.h"
#include <stdio.h>
#include <string.h>

#define ASSERT(x) \
    (!!(x) ? true : ( \
        fprintf(stderr, "%s:%s:%d: assertion failed: %s\n", __FILE__, __func__, __LINE__, #x), \
        false \
    ))

// ============================================================================
// Helpers
// ============================================================================

typedef struct {
    Parser  parser;
    Program prog;
} ParseResult;

static ParseResult parse_src(Arena *arena, const char *src) {
    Lexer lexer;
    lexer_init(&lexer, src, arena);
    TokenVec tokens = lexer_get_tokens(&lexer);

    ParseResult r;
    parser_init(&r.parser, tokens, arena, "test.kz");
    r.prog = parse_program(&r.parser);
    return r;
}

static bool sv_eq(StringView sv, const char *s) {
    size_t n = strlen(s);
    return sv.len == n && strncmp(sv.data, s, n) == 0;
}

// ============================================================================
// Tests
// ============================================================================

bool test_parse_empty(void) {
    bool ok = true;
    Arena *arena = arena_new(0);

    ParseResult r = parse_src(arena, "   \n  ");
    ok &= ASSERT(r.prog.decl_count == 0);
    ok &= ASSERT(r.parser.errors.len == 0);

    arena_delete(arena);
    return ok;
}

bool test_parse_var_decls(void) {
    bool ok = true;
    Arena *arena = arena_new(0);

    ParseResult r = parse_src(arena,
        "var x: i32 = 0;\n"
        "let name = \"hi\";\n"
        "const PI: f64 = 3.14;\n");

    ok &= ASSERT(r.parser.errors.len == 0);
    ok &= ASSERT(r.prog.decl_count == 3);

    StmtVarDecl *a = NODE_CAST(r.prog.decls[0], StmtVarDecl, NODE_STMT_VAR_DECL);
    ok &= ASSERT(a != NULL);
    ok &= ASSERT(sv_eq(a->name, "x"));
    ok &= ASSERT(a->is_const == false);
    ok &= ASSERT(a->type_annotation != NULL);
    ok &= ASSERT(a->initializer != NULL);

    StmtVarDecl *b = NODE_CAST(r.prog.decls[1], StmtVarDecl, NODE_STMT_VAR_DECL);
    ok &= ASSERT(b != NULL);
    ok &= ASSERT(b->is_const == true);             // let is immutable
    ok &= ASSERT(b->type_annotation == NULL);

    StmtVarDecl *c = NODE_CAST(r.prog.decls[2], StmtVarDecl, NODE_STMT_VAR_DECL);
    ok &= ASSERT(c != NULL && c->is_const == true);

    arena_delete(arena);
    return ok;
}

bool test_parse_function(void) {
    bool ok = true;
    Arena *arena = arena_new(0);

    ParseResult r = parse_src(arena,
        "@inline fn add(comptime T: type, a: i32, b: i32) i32 { return a; }");

    ok &= ASSERT(r.parser.errors.len == 0);
    ok &= ASSERT(r.prog.decl_count == 1);

    DeclFunction *f = NODE_CAST(r.prog.decls[0], DeclFunction, NODE_DECL_FUNCTION);
    ok &= ASSERT(f != NULL);
    ok &= ASSERT(sv_eq(f->name, "add"));
    ok &= ASSERT(f->is_inline == true);
    ok &= ASSERT(f->param_count == 3);
    ok &= ASSERT(f->params[0].is_comptime == true);
    ok &= ASSERT(sv_eq(f->params[1].name, "a"));
    ok &= ASSERT(f->return_type != NULL);
    ok &= ASSERT(f->body_len == 1);
    ok &= ASSERT(NODE_IS(f->body[0], NODE_STMT_RETURN));

    arena_delete(arena);
    return ok;
}

bool test_parse_precedence(void) {
    bool ok = true;
    Arena *arena = arena_new(0);

    // a + b * c  →  (+ a (* b c))
    ParseResult r = parse_src(arena, "fn f() void { x = a + b * c; }");
    ok &= ASSERT(r.parser.errors.len == 0);

    DeclFunction *f = NODE_CAST(r.prog.decls[0], DeclFunction, NODE_DECL_FUNCTION);
    ok &= ASSERT(f != NULL && f->body_len == 1);

    StmtAssignment *as = NODE_CAST(f->body[0], StmtAssignment, NODE_STMT_ASSIGNMENT);
    ok &= ASSERT(as != NULL);
    ExprBinaryOp *plus = NODE_CAST(as->value, ExprBinaryOp, NODE_EXPR_BINARY_OP);
    ok &= ASSERT(plus != NULL);
    ok &= ASSERT(sv_eq(plus->op, "+"));
    ExprBinaryOp *mul = NODE_CAST(plus->right, ExprBinaryOp, NODE_EXPR_BINARY_OP);
    ok &= ASSERT(mul != NULL && sv_eq(mul->op, "*"));
    ok &= ASSERT(NODE_IS(plus->left, NODE_EXPR_IDENT));

    arena_delete(arena);
    return ok;
}

bool test_parse_if_else(void) {
    bool ok = true;
    Arena *arena = arena_new(0);

    ParseResult r = parse_src(arena,
        "fn f() void { if x > 0 { return; } else { return; } }");
    ok &= ASSERT(r.parser.errors.len == 0);

    DeclFunction *f = NODE_CAST(r.prog.decls[0], DeclFunction, NODE_DECL_FUNCTION);
    ok &= ASSERT(f != NULL && f->body_len == 1);
    StmtIf *iff = NODE_CAST(f->body[0], StmtIf, NODE_STMT_IF);
    ok &= ASSERT(iff != NULL);
    ok &= ASSERT(iff->condition != NULL);
    ok &= ASSERT(iff->then_len == 1);
    ok &= ASSERT(iff->else_len == 1);

    arena_delete(arena);
    return ok;
}

bool test_parse_while_for(void) {
    bool ok = true;
    Arena *arena = arena_new(0);

    ParseResult r = parse_src(arena,
        "fn f() void { while a < 10 { a++; } for it in xs { g(it); } }");
    ok &= ASSERT(r.parser.errors.len == 0);

    DeclFunction *f = NODE_CAST(r.prog.decls[0], DeclFunction, NODE_DECL_FUNCTION);
    ok &= ASSERT(f != NULL && f->body_len == 2);
    ok &= ASSERT(NODE_IS(f->body[0], NODE_STMT_WHILE));
    StmtFor *fr = NODE_CAST(f->body[1], StmtFor, NODE_STMT_FOR);
    ok &= ASSERT(fr != NULL && sv_eq(fr->var_name, "it"));
    ok &= ASSERT(fr->body_len == 1);

    arena_delete(arena);
    return ok;
}

bool test_parse_struct(void) {
    bool ok = true;
    Arena *arena = arena_new(0);

    ParseResult r = parse_src(arena,
        "const Vec2 = struct { x: f32, y: f32, fn len(self: *Self) f32 { return x; } };");
    ok &= ASSERT(r.parser.errors.len == 0);
    ok &= ASSERT(r.prog.decl_count == 1);

    DeclStruct *s = NODE_CAST(r.prog.decls[0], DeclStruct, NODE_DECL_STRUCT);
    ok &= ASSERT(s != NULL);
    ok &= ASSERT(sv_eq(s->name, "Vec2"));
    ok &= ASSERT(s->field_count == 2);             // method dropped, fields kept
    ok &= ASSERT(sv_eq(s->field_names[0], "x"));

    arena_delete(arena);
    return ok;
}

bool test_parse_enum(void) {
    bool ok = true;
    Arena *arena = arena_new(0);

    ParseResult r = parse_src(arena,
        "const Msg = enum { Quit, Move{ x: i32, y: i32 }, Write(*const u8) };");
    ok &= ASSERT(r.parser.errors.len == 0);

    DeclEnum *e = NODE_CAST(r.prog.decls[0], DeclEnum, NODE_DECL_ENUM);
    ok &= ASSERT(e != NULL);
    ok &= ASSERT(e->variant_count == 3);
    ok &= ASSERT(sv_eq(e->variants[0].name, "Quit"));
    ok &= ASSERT(e->variants[0].kind == ENUM_VARIANT_PLAIN);
    ok &= ASSERT(e->variants[1].kind == ENUM_VARIANT_STRUCT && e->variants[1].field_count == 2);
    ok &= ASSERT(e->variants[2].kind == ENUM_VARIANT_TUPLE && e->variants[2].tuple_type != NULL);

    arena_delete(arena);
    return ok;
}

bool test_parse_type_alias(void) {
    bool ok = true;
    Arena *arena = arena_new(0);

    ParseResult r = parse_src(arena, "const Bytes = []u8;");
    ok &= ASSERT(r.parser.errors.len == 0);

    DeclTypeAlias *a = NODE_CAST(r.prog.decls[0], DeclTypeAlias, NODE_DECL_TYPE_ALIAS);
    ok &= ASSERT(a != NULL && sv_eq(a->name, "Bytes"));
    TypeArray *arr = NODE_CAST(a->aliased_type, TypeArray, NODE_TYPE_ARRAY);
    ok &= ASSERT(arr != NULL && arr->is_slice == true);

    arena_delete(arena);
    return ok;
}

bool test_parse_import(void) {
    bool ok = true;
    Arena *arena = arena_new(0);

    ParseResult r = parse_src(arena,
        "import \"std/io\";\n"
        "import \"std/mem\" { alloc, free };\n");
    ok &= ASSERT(r.parser.errors.len == 0);
    ok &= ASSERT(r.prog.decl_count == 2);

    DeclImport *i0 = NODE_CAST(r.prog.decls[0], DeclImport, NODE_DECL_IMPORT);
    ok &= ASSERT(i0 != NULL && sv_eq(i0->path, "std/io"));
    DeclImport *i1 = NODE_CAST(r.prog.decls[1], DeclImport, NODE_DECL_IMPORT);
    ok &= ASSERT(i1 != NULL && i1->selective_count == 2);
    ok &= ASSERT(sv_eq(i1->selective[1], "free"));

    arena_delete(arena);
    return ok;
}

bool test_parse_postfix_chain(void) {
    bool ok = true;
    Arena *arena = arena_new(0);

    // obj.field[i](arg)::Variant  — exercises ., [], (), ::
    ParseResult r = parse_src(arena, "fn f() void { y = obj.field[i]; z = E::Red; }");
    ok &= ASSERT(r.parser.errors.len == 0);

    DeclFunction *f = NODE_CAST(r.prog.decls[0], DeclFunction, NODE_DECL_FUNCTION);
    ok &= ASSERT(f != NULL && f->body_len == 2);

    StmtAssignment *s0 = NODE_CAST(f->body[0], StmtAssignment, NODE_STMT_ASSIGNMENT);
    ok &= ASSERT(s0 != NULL);
    ExprIndex *idx = NODE_CAST(s0->value, ExprIndex, NODE_EXPR_INDEX);
    ok &= ASSERT(idx != NULL);
    ok &= ASSERT(NODE_IS(idx->object, NODE_EXPR_FIELD_ACCESS));

    StmtAssignment *s1 = NODE_CAST(f->body[1], StmtAssignment, NODE_STMT_ASSIGNMENT);
    ExprPath *ev = NODE_CAST(s1->value, ExprPath, NODE_EXPR_PATH);
    ok &= ASSERT(ev != NULL && sv_eq(ev->name, "Red"));

    arena_delete(arena);
    return ok;
}

bool test_parse_compound_assign(void) {
    bool ok = true;
    Arena *arena = arena_new(0);

    // acc += 2  desugars to  acc = acc + 2
    ParseResult r = parse_src(arena, "fn f() void { acc += 2; }");
    ok &= ASSERT(r.parser.errors.len == 0);

    DeclFunction *f = NODE_CAST(r.prog.decls[0], DeclFunction, NODE_DECL_FUNCTION);
    StmtAssignment *as = NODE_CAST(f->body[0], StmtAssignment, NODE_STMT_ASSIGNMENT);
    ok &= ASSERT(as != NULL);
    ExprBinaryOp *bin = NODE_CAST(as->value, ExprBinaryOp, NODE_EXPR_BINARY_OP);
    ok &= ASSERT(bin != NULL && sv_eq(bin->op, "+"));

    arena_delete(arena);
    return ok;
}

bool test_parse_cimport(void) {
    bool ok = true;
    Arena *arena = arena_new(0);

    ParseResult r = parse_src(arena,
        "const stb = @cimport(\"stb_image.h\", .{ .defines = {\"STB_IMAGE_IMPLEMENTATION\", \"X=1\"} });\n"
        "fn f() void { const n = c::strlen(s); }");
    ok &= ASSERT(r.parser.errors.len == 0);

    // @cimport with options → ExprCImport carrying the captured defines.
    StmtVarDecl *v = NODE_CAST(r.prog.decls[0], StmtVarDecl, NODE_STMT_VAR_DECL);
    ok &= ASSERT(v != NULL && v->initializer != NULL);
    ExprCImport *ci = NODE_CAST(v->initializer, ExprCImport, NODE_EXPR_CIMPORT);
    ok &= ASSERT(ci != NULL);
    ok &= ASSERT(sv_eq(ci->path, "stb_image.h"));
    ok &= ASSERT(ci->define_count == 2);
    ok &= ASSERT(sv_eq(ci->defines[0], "STB_IMAGE_IMPLEMENTATION"));

    // c::strlen(s)  →  call whose callee is a '::' scope resolution.
    DeclFunction *f = NODE_CAST(r.prog.decls[1], DeclFunction, NODE_DECL_FUNCTION);
    StmtVarDecl *n = NODE_CAST(f->body[0], StmtVarDecl, NODE_STMT_VAR_DECL);
    ExprCall *call = NODE_CAST(n->initializer, ExprCall, NODE_EXPR_CALL);
    ok &= ASSERT(call != NULL);
    ok &= ASSERT(NODE_IS(call->callee, NODE_EXPR_PATH));

    arena_delete(arena);
    return ok;
}

bool test_parse_struct_methods(void) {
    bool ok = true;
    Arena *arena = arena_new(0);

    ParseResult r = parse_src(arena,
        "const Vec2 = struct { x: f32, y: f32, "
        "fn len(self: *Self) f32 { return x; } fn zero() void { } };");
    ok &= ASSERT(r.parser.errors.len == 0);

    DeclStruct *s = NODE_CAST(r.prog.decls[0], DeclStruct, NODE_DECL_STRUCT);
    ok &= ASSERT(s != NULL);
    ok &= ASSERT(s->field_count == 2);
    ok &= ASSERT(s->method_count == 2);
    ok &= ASSERT(NODE_IS(s->methods[0], NODE_DECL_FUNCTION));

    arena_delete(arena);
    return ok;
}

bool test_parse_union(void) {
    bool ok = true;
    Arena *arena = arena_new(0);

    ParseResult r = parse_src(arena, "const Bits = union { as_i32: i32, as_f32: f32 };");
    ok &= ASSERT(r.parser.errors.len == 0);

    DeclUnion *u = NODE_CAST(r.prog.decls[0], DeclUnion, NODE_DECL_UNION);
    ok &= ASSERT(u != NULL && sv_eq(u->name, "Bits"));
    ok &= ASSERT(u->field_count == 2);
    ok &= ASSERT(sv_eq(u->field_names[1], "as_f32"));

    arena_delete(arena);
    return ok;
}

bool test_parse_struct_literal(void) {
    bool ok = true;
    Arena *arena = arena_new(0);

    ParseResult r = parse_src(arena, "fn f() void { var v = Vec2{ .x = 1, .y = 2 }; }");
    ok &= ASSERT(r.parser.errors.len == 0);

    DeclFunction *f = NODE_CAST(r.prog.decls[0], DeclFunction, NODE_DECL_FUNCTION);
    StmtVarDecl *vd = NODE_CAST(f->body[0], StmtVarDecl, NODE_STMT_VAR_DECL);
    ExprStructLit *lit = NODE_CAST(vd->initializer, ExprStructLit, NODE_EXPR_STRUCT_LIT);
    ok &= ASSERT(lit != NULL);
    ok &= ASSERT(sv_eq(lit->type_name, "Vec2"));
    ok &= ASSERT(lit->is_anonymous == false);
    ok &= ASSERT(lit->field_count == 2);
    ok &= ASSERT(sv_eq(lit->field_names[0], "x"));

    arena_delete(arena);
    return ok;
}

bool test_parse_match(void) {
    bool ok = true;
    Arena *arena = arena_new(0);

    ParseResult r = parse_src(arena,
        "fn f() void { match m {"
        " E::A => { x(); }"
        " P{ a, b } => { }"
        " (u, v) => { }"
        " 1 => { }"
        " bind => { }"
        " _ => { }"
        " } }");
    ok &= ASSERT(r.parser.errors.len == 0);

    DeclFunction *f = NODE_CAST(r.prog.decls[0], DeclFunction, NODE_DECL_FUNCTION);
    StmtMatch *m = NODE_CAST(f->body[0], StmtMatch, NODE_STMT_MATCH);
    ok &= ASSERT(m != NULL);
    ok &= ASSERT(m->arm_count == 6);
    ok &= ASSERT(NODE_IS(m->arms[0].pattern, NODE_PATTERN_PATH));
    ok &= ASSERT(NODE_IS(m->arms[1].pattern, NODE_PATTERN_STRUCT));
    ok &= ASSERT(NODE_IS(m->arms[2].pattern, NODE_PATTERN_TUPLE));
    ok &= ASSERT(NODE_IS(m->arms[3].pattern, NODE_PATTERN_LITERAL));
    ok &= ASSERT(NODE_IS(m->arms[4].pattern, NODE_PATTERN_IDENT));
    ok &= ASSERT(NODE_IS(m->arms[5].pattern, NODE_PATTERN_WILDCARD));

    arena_delete(arena);
    return ok;
}

bool test_parse_when(void) {
    bool ok = true;
    Arena *arena = arena_new(0);

    ParseResult r = parse_src(arena,
        "when a { x(); } else when b { y(); } else { z(); }");
    ok &= ASSERT(r.parser.errors.len == 0);

    StmtWhen *w = NODE_CAST(r.prog.decls[0], StmtWhen, NODE_STMT_WHEN);
    ok &= ASSERT(w != NULL);
    ok &= ASSERT(w->branch_count == 3);
    ok &= ASSERT(w->branches[0].condition != NULL);
    ok &= ASSERT(w->branches[1].condition != NULL);
    ok &= ASSERT(w->branches[2].condition == NULL);  // trailing else

    arena_delete(arena);
    return ok;
}

bool test_parse_defer(void) {
    bool ok = true;
    Arena *arena = arena_new(0);

    ParseResult r = parse_src(arena,
        "fn f() void { defer close(x); defer { a(); b(); } }");
    ok &= ASSERT(r.parser.errors.len == 0);

    DeclFunction *f = NODE_CAST(r.prog.decls[0], DeclFunction, NODE_DECL_FUNCTION);
    ok &= ASSERT(f->body_len == 2);
    StmtDefer *d0 = NODE_CAST(f->body[0], StmtDefer, NODE_STMT_DEFER);
    ok &= ASSERT(d0 != NULL && NODE_IS(d0->body, NODE_STMT_EXPR));
    StmtDefer *d1 = NODE_CAST(f->body[1], StmtDefer, NODE_STMT_DEFER);
    ok &= ASSERT(d1 != NULL && NODE_IS(d1->body, NODE_STMT_BLOCK));

    arena_delete(arena);
    return ok;
}

bool test_parse_try(void) {
    bool ok = true;
    Arena *arena = arena_new(0);

    // `try` and unwrap `!` are language constructs; `catch` is now an ordinary
    // stdlib function call, so it parses as a plain ExprCall.
    ParseResult r = parse_src(arena,
        "fn f() void { const a = try g(); const b = catch(g(), 0); const c = g()!; }");
    ok &= ASSERT(r.parser.errors.len == 0);

    DeclFunction *f = NODE_CAST(r.prog.decls[0], DeclFunction, NODE_DECL_FUNCTION);

    StmtVarDecl *a = NODE_CAST(f->body[0], StmtVarDecl, NODE_STMT_VAR_DECL);
    ok &= ASSERT(a != NULL && NODE_IS(a->initializer, NODE_EXPR_TRY));

    StmtVarDecl *b = NODE_CAST(f->body[1], StmtVarDecl, NODE_STMT_VAR_DECL);
    ExprCall *call = NODE_CAST(b->initializer, ExprCall, NODE_EXPR_CALL);
    ok &= ASSERT(call != NULL && NODE_IS(call->callee, NODE_EXPR_IDENT));
    ok &= ASSERT(sv_eq(NODE_CAST_UNSAFE(call->callee, ExprIdent)->name, "catch"));

    StmtVarDecl *c = NODE_CAST(f->body[2], StmtVarDecl, NODE_STMT_VAR_DECL);
    ExprPostfix *unwrap = NODE_CAST(c->initializer, ExprPostfix, NODE_EXPR_POSTFIX);
    ok &= ASSERT(unwrap != NULL && sv_eq(unwrap->op, "!"));

    arena_delete(arena);
    return ok;
}

bool test_parse_errors(void) {
    bool ok = true;
    Arena *arena = arena_new(0);

    // Missing semicolon should record at least one error.
    ParseResult r = parse_src(arena, "var x = 1");
    ok &= ASSERT(r.parser.errors.len > 0);
    ok &= ASSERT(r.parser.had_error == true);

    arena_delete(arena);
    return ok;
}

// ============================================================================
// Suite
// ============================================================================

static bool parser_tests(void) {
    bool ok = true;
    struct { TestFn func; const char *name; } tests[] = {
        { test_parse_empty,           "test_parse_empty"           },
        { test_parse_var_decls,       "test_parse_var_decls"       },
        { test_parse_function,        "test_parse_function"        },
        { test_parse_precedence,      "test_parse_precedence"      },
        { test_parse_if_else,         "test_parse_if_else"         },
        { test_parse_while_for,       "test_parse_while_for"       },
        { test_parse_struct,          "test_parse_struct"          },
        { test_parse_enum,            "test_parse_enum"            },
        { test_parse_type_alias,      "test_parse_type_alias"      },
        { test_parse_import,          "test_parse_import"          },
        { test_parse_postfix_chain,   "test_parse_postfix_chain"   },
        { test_parse_compound_assign, "test_parse_compound_assign" },
        { test_parse_cimport,         "test_parse_cimport"         },
        { test_parse_struct_methods,  "test_parse_struct_methods"  },
        { test_parse_union,           "test_parse_union"           },
        { test_parse_struct_literal,  "test_parse_struct_literal"  },
        { test_parse_match,           "test_parse_match"           },
        { test_parse_when,            "test_parse_when"            },
        { test_parse_defer,           "test_parse_defer"           },
        { test_parse_try,             "test_parse_try"             },
        { test_parse_errors,          "test_parse_errors"          },
    };

    for (size_t i = 0; i < sizeof(tests) / sizeof(tests[0]); i++) {
        bool res = tests[i].func();
        if (!res) fprintf(stderr, "test %s failed.\n", tests[i].name);
        ok &= res;
    }
    return ok;
}

TestSuite parser_test_suite(void) {
    return (TestSuite){ .name = "Parser", .func = parser_tests };
}
