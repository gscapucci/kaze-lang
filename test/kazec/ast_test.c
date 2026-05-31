#include "ast_test.h"
#include "../../kazec/include/ast.h"
#include "../../utils/include/string_view.h"
#include <stdio.h>
#include <string.h>

#define ASSERT(x) \
    (!!(x) ? true : ( \
        fprintf(stderr, "%s:%s:%d: assertion failed: %s\n", __FILE__, __func__, __LINE__, #x), \
        false \
    ))

static SourceLoc loc1 = { .filename = "test.kz", .line = 1, .col = 1 };
static SourceLoc loc5 = { .filename = "test.kz", .line = 5, .col = 10 };

static StringView sv(const char *s) {
    return string_view_create(s, strlen(s));
}

// ============================================================================
// Allocation
// ============================================================================

bool test_ast_alloc(void) {
    bool ok = true;
    Arena *arena = arena_new(0);

    Node *node = ast_alloc_node(arena, NODE_EXPR_INT_LIT, loc1);
    ok &= ASSERT(node != NULL);
    ok &= ASSERT(node->kind == NODE_EXPR_INT_LIT);
    ok &= ASSERT(node->type_info == NULL);
    ok &= ASSERT(node->parent == NULL);

    arena_delete(arena);
    return ok;
}

bool test_ast_alloc_all_kinds(void) {
    bool ok = true;
    Arena *arena = arena_new(0);

    for (int k = 0; k < NODE_KIND_COUNT; k++) {
        Node *n = ast_alloc_node(arena, (NodeKind)k, loc1);
        ok &= ASSERT(n != NULL);
        ok &= ASSERT(n->kind == (NodeKind)k);
    }

    arena_delete(arena);
    return ok;
}

bool test_ast_source_loc(void) {
    bool ok = true;
    Arena *arena = arena_new(0);

    Node *n = ast_alloc_node(arena, NODE_EXPR_IDENT, loc5);
    ok &= ASSERT(n->loc.line == 5);
    ok &= ASSERT(n->loc.col  == 10);
    ok &= ASSERT(strcmp(n->loc.filename, "test.kz") == 0);

    arena_delete(arena);
    return ok;
}

// ============================================================================
// Expressions — literals
// ============================================================================

bool test_expr_int_lit(void) {
    bool ok = true;
    Arena *arena = arena_new(0);

    ExprIntLit *n = (ExprIntLit *)ast_alloc_node(arena, NODE_EXPR_INT_LIT, loc1);
    n->value = 42;
    ok &= ASSERT(NODE_IS(&n->base, NODE_EXPR_INT_LIT));
    ok &= ASSERT(n->value == 42);

    n->value = -9999;
    ok &= ASSERT(n->value == -9999);

    arena_delete(arena);
    return ok;
}

bool test_expr_float_lit(void) {
    bool ok = true;
    Arena *arena = arena_new(0);

    ExprFloatLit *n = (ExprFloatLit *)ast_alloc_node(arena, NODE_EXPR_FLOAT_LIT, loc1);
    n->value = 3.14;
    ok &= ASSERT(NODE_IS(&n->base, NODE_EXPR_FLOAT_LIT));
    ok &= ASSERT(n->value == 3.14);

    arena_delete(arena);
    return ok;
}

bool test_expr_string_lit(void) {
    bool ok = true;
    Arena *arena = arena_new(0);

    ExprStringLit *n = (ExprStringLit *)ast_alloc_node(arena, NODE_EXPR_STRING_LIT, loc1);
    n->value  = sv("hello world");
    n->is_raw = false;
    ok &= ASSERT(NODE_IS(&n->base, NODE_EXPR_STRING_LIT));
    ok &= ASSERT(n->value.len == strlen("hello world"));
    ok &= ASSERT(strncmp(n->value.data, "hello world", n->value.len) == 0);
    ok &= ASSERT(!n->is_raw);

    ExprStringLit *raw = (ExprStringLit *)ast_alloc_node(arena, NODE_EXPR_STRING_LIT, loc1);
    raw->value  = sv("raw\\nno-escape");
    raw->is_raw = true;
    ok &= ASSERT(raw->is_raw);

    arena_delete(arena);
    return ok;
}

bool test_expr_bool_lit(void) {
    bool ok = true;
    Arena *arena = arena_new(0);

    ExprBoolLit *t = (ExprBoolLit *)ast_alloc_node(arena, NODE_EXPR_BOOL_LIT, loc1);
    t->value = true;
    ok &= ASSERT(t->value == true);

    ExprBoolLit *f = (ExprBoolLit *)ast_alloc_node(arena, NODE_EXPR_BOOL_LIT, loc1);
    f->value = false;
    ok &= ASSERT(f->value == false);

    arena_delete(arena);
    return ok;
}

bool test_expr_null_lit(void) {
    bool ok = true;
    Arena *arena = arena_new(0);

    Node *n = ast_alloc_node(arena, NODE_EXPR_NULL_LIT, loc1);
    ok &= ASSERT(NODE_IS(n, NODE_EXPR_NULL_LIT));

    arena_delete(arena);
    return ok;
}

// ============================================================================
// Expressions — identifiers and access
// ============================================================================

bool test_expr_ident(void) {
    bool ok = true;
    Arena *arena = arena_new(0);

    ExprIdent *n = (ExprIdent *)ast_alloc_node(arena, NODE_EXPR_IDENT, loc1);
    n->name = sv("my_var");
    ok &= ASSERT(NODE_IS(&n->base, NODE_EXPR_IDENT));
    ok &= ASSERT(n->name.len == strlen("my_var"));
    ok &= ASSERT(strncmp(n->name.data, "my_var", n->name.len) == 0);

    arena_delete(arena);
    return ok;
}

bool test_expr_field_access(void) {
    bool ok = true;
    Arena *arena = arena_new(0);

    ExprIdent *obj = (ExprIdent *)ast_alloc_node(arena, NODE_EXPR_IDENT, loc1);
    obj->name = sv("point");

    ExprFieldAccess *dot = (ExprFieldAccess *)ast_alloc_node(arena, NODE_EXPR_FIELD_ACCESS, loc1);
    dot->object     = &obj->base;
    dot->field      = sv("x");
    dot->is_pointer = false;
    ok &= ASSERT(dot->object == &obj->base);
    ok &= ASSERT(strncmp(dot->field.data, "x", dot->field.len) == 0);
    ok &= ASSERT(!dot->is_pointer);

    ExprFieldAccess *arrow = (ExprFieldAccess *)ast_alloc_node(arena, NODE_EXPR_FIELD_ACCESS, loc1);
    arrow->object     = &obj->base;
    arrow->field      = sv("y");
    arrow->is_pointer = true;
    ok &= ASSERT(arrow->is_pointer);

    arena_delete(arena);
    return ok;
}

bool test_expr_index(void) {
    bool ok = true;
    Arena *arena = arena_new(0);

    ExprIdent *arr = (ExprIdent *)ast_alloc_node(arena, NODE_EXPR_IDENT, loc1);
    arr->name = sv("arr");

    ExprIntLit *idx = (ExprIntLit *)ast_alloc_node(arena, NODE_EXPR_INT_LIT, loc1);
    idx->value = 0;

    ExprIndex *n = (ExprIndex *)ast_alloc_node(arena, NODE_EXPR_INDEX, loc1);
    n->object = &arr->base;
    n->index  = &idx->base;
    ok &= ASSERT(n->object == &arr->base);
    ok &= ASSERT(n->index  == &idx->base);
    ok &= ASSERT(NODE_IS(n->index, NODE_EXPR_INT_LIT));

    arena_delete(arena);
    return ok;
}

bool test_expr_enum_variant(void) {
    bool ok = true;
    Arena *arena = arena_new(0);

    TypePrimitive *tp = (TypePrimitive *)ast_alloc_node(arena, NODE_TYPE_PRIMITIVE, loc1);
    tp->name = sv("Color");

    ExprPath *n = (ExprPath *)ast_alloc_node(arena, NODE_EXPR_PATH, loc1);
    n->scope = &tp->base;
    n->name  = sv("Red");
    ok &= ASSERT(NODE_IS(n->scope, NODE_TYPE_PRIMITIVE));
    ok &= ASSERT(strncmp(n->name.data, "Red", n->name.len) == 0);

    arena_delete(arena);
    return ok;
}

// ============================================================================
// Expressions — operations
// ============================================================================

bool test_expr_binary_op(void) {
    bool ok = true;
    Arena *arena = arena_new(0);

    ExprIntLit *left  = (ExprIntLit *)ast_alloc_node(arena, NODE_EXPR_INT_LIT, loc1);
    ExprIntLit *right = (ExprIntLit *)ast_alloc_node(arena, NODE_EXPR_INT_LIT, loc1);
    left->value  = 5;
    right->value = 3;

    ExprBinaryOp *n = (ExprBinaryOp *)ast_alloc_node(arena, NODE_EXPR_BINARY_OP, loc1);
    n->left  = &left->base;
    n->right = &right->base;
    n->op    = sv("+");

    ok &= ASSERT(NODE_IS(&n->base, NODE_EXPR_BINARY_OP));
    ok &= ASSERT(n->left  == &left->base);
    ok &= ASSERT(n->right == &right->base);
    ok &= ASSERT(strncmp(n->op.data, "+", n->op.len) == 0);

    ExprBinaryOp *cmp = (ExprBinaryOp *)ast_alloc_node(arena, NODE_EXPR_BINARY_OP, loc1);
    cmp->left  = &left->base;
    cmp->right = &right->base;
    cmp->op    = sv("==");
    ok &= ASSERT(strncmp(cmp->op.data, "==", cmp->op.len) == 0);

    arena_delete(arena);
    return ok;
}

bool test_expr_unary_op(void) {
    bool ok = true;
    Arena *arena = arena_new(0);

    ExprIdent *operand = (ExprIdent *)ast_alloc_node(arena, NODE_EXPR_IDENT, loc1);
    operand->name = sv("x");

    ExprUnaryOp *neg = (ExprUnaryOp *)ast_alloc_node(arena, NODE_EXPR_UNARY_OP, loc1);
    neg->operand   = &operand->base;
    neg->op        = sv("-");
    neg->is_prefix = true;
    ok &= ASSERT(neg->is_prefix);
    ok &= ASSERT(strncmp(neg->op.data, "-", neg->op.len) == 0);

    ExprUnaryOp *deref = (ExprUnaryOp *)ast_alloc_node(arena, NODE_EXPR_UNARY_OP, loc1);
    deref->operand   = &operand->base;
    deref->op        = sv("*");
    deref->is_prefix = true;
    ok &= ASSERT(deref->is_prefix);

    arena_delete(arena);
    return ok;
}

bool test_expr_cast(void) {
    bool ok = true;
    Arena *arena = arena_new(0);

    ExprIdent *expr = (ExprIdent *)ast_alloc_node(arena, NODE_EXPR_IDENT, loc1);
    expr->name = sv("val");

    TypePrimitive *tp = (TypePrimitive *)ast_alloc_node(arena, NODE_TYPE_PRIMITIVE, loc1);
    tp->name = sv("i32");

    ExprCast *n = (ExprCast *)ast_alloc_node(arena, NODE_EXPR_CAST, loc1);
    n->expr        = &expr->base;
    n->target_type = &tp->base;
    ok &= ASSERT(n->expr        == &expr->base);
    ok &= ASSERT(n->target_type == &tp->base);
    ok &= ASSERT(NODE_IS(n->target_type, NODE_TYPE_PRIMITIVE));

    arena_delete(arena);
    return ok;
}

// ============================================================================
// Expressions — calls and postfix
// ============================================================================

bool test_expr_call(void) {
    bool ok = true;
    Arena *arena = arena_new(0);

    ExprIdent *callee = (ExprIdent *)ast_alloc_node(arena, NODE_EXPR_IDENT, loc1);
    callee->name = sv("foo");

    // No args
    ExprCall *no_args = (ExprCall *)ast_alloc_node(arena, NODE_EXPR_CALL, loc1);
    no_args->callee      = &callee->base;
    no_args->args        = NULL;
    no_args->arg_count   = 0;
    no_args->is_comptime = false;
    ok &= ASSERT(no_args->arg_count == 0);
    ok &= ASSERT(!no_args->is_comptime);

    // With args
    Node **args = (Node **)arena_alloc(arena, 2 * sizeof(Node *));
    ExprIntLit *a0 = (ExprIntLit *)ast_alloc_node(arena, NODE_EXPR_INT_LIT, loc1);
    ExprIntLit *a1 = (ExprIntLit *)ast_alloc_node(arena, NODE_EXPR_INT_LIT, loc1);
    a0->value = 1;
    a1->value = 2;
    args[0] = &a0->base;
    args[1] = &a1->base;

    ExprCall *with_args = (ExprCall *)ast_alloc_node(arena, NODE_EXPR_CALL, loc1);
    with_args->callee    = &callee->base;
    with_args->args      = args;
    with_args->arg_count = 2;
    ok &= ASSERT(with_args->arg_count == 2);
    ok &= ASSERT(NODE_IS(with_args->args[0], NODE_EXPR_INT_LIT));

    // Comptime call
    ExprCall *ct = (ExprCall *)ast_alloc_node(arena, NODE_EXPR_CALL, loc1);
    ct->callee      = &callee->base;
    ct->is_comptime = true;
    ok &= ASSERT(ct->is_comptime);

    arena_delete(arena);
    return ok;
}

bool test_expr_postfix(void) {
    bool ok = true;
    Arena *arena = arena_new(0);

    ExprIdent *expr = (ExprIdent *)ast_alloc_node(arena, NODE_EXPR_IDENT, loc1);
    expr->name = sv("result");

    ExprPostfix *n = (ExprPostfix *)ast_alloc_node(arena, NODE_EXPR_POSTFIX, loc1);
    n->expr = &expr->base;
    n->op   = sv("++");
    ok &= ASSERT(strncmp(n->op.data, "++", n->op.len) == 0);
    ok &= ASSERT(n->expr == &expr->base);

    arena_delete(arena);
    return ok;
}

// ============================================================================
// Statements
// ============================================================================

bool test_stmt_var_decl(void) {
    bool ok = true;
    Arena *arena = arena_new(0);

    ExprIntLit *init = (ExprIntLit *)ast_alloc_node(arena, NODE_EXPR_INT_LIT, loc1);
    init->value = 10;

    StmtVarDecl *var = (StmtVarDecl *)ast_alloc_node(arena, NODE_STMT_VAR_DECL, loc1);
    var->name        = sv("x");
    var->initializer = &init->base;
    var->is_const    = false;
    ok &= ASSERT(NODE_IS(&var->base, NODE_STMT_VAR_DECL));
    ok &= ASSERT(!var->is_const);
    ok &= ASSERT(var->type_annotation == NULL);
    ok &= ASSERT(var->initializer == &init->base);

    StmtVarDecl *cst = (StmtVarDecl *)ast_alloc_node(arena, NODE_STMT_VAR_DECL, loc1);
    cst->name        = sv("PI");
    cst->is_const    = true;
    cst->is_comptime = false;
    ok &= ASSERT(cst->is_const);

    TypePrimitive *tp = (TypePrimitive *)ast_alloc_node(arena, NODE_TYPE_PRIMITIVE, loc1);
    tp->name = sv("i32");

    StmtVarDecl *annotated = (StmtVarDecl *)ast_alloc_node(arena, NODE_STMT_VAR_DECL, loc1);
    annotated->name            = sv("y");
    annotated->type_annotation = &tp->base;
    annotated->initializer     = NULL;
    ok &= ASSERT(annotated->type_annotation == &tp->base);
    ok &= ASSERT(annotated->initializer == NULL);

    arena_delete(arena);
    return ok;
}

bool test_stmt_assignment(void) {
    bool ok = true;
    Arena *arena = arena_new(0);

    ExprIdent *target = (ExprIdent *)ast_alloc_node(arena, NODE_EXPR_IDENT, loc1);
    target->name = sv("x");

    ExprIntLit *value = (ExprIntLit *)ast_alloc_node(arena, NODE_EXPR_INT_LIT, loc1);
    value->value = 99;

    StmtAssignment *n = (StmtAssignment *)ast_alloc_node(arena, NODE_STMT_ASSIGNMENT, loc1);
    n->target = &target->base;
    n->value  = &value->base;
    ok &= ASSERT(NODE_IS(n->target, NODE_EXPR_IDENT));
    ok &= ASSERT(NODE_IS(n->value,  NODE_EXPR_INT_LIT));

    arena_delete(arena);
    return ok;
}

bool test_stmt_if(void) {
    bool ok = true;
    Arena *arena = arena_new(0);

    ExprBoolLit *cond = (ExprBoolLit *)ast_alloc_node(arena, NODE_EXPR_BOOL_LIT, loc1);
    cond->value = true;

    Node **then_body = (Node **)arena_alloc(arena, sizeof(Node *));
    then_body[0] = ast_alloc_node(arena, NODE_STMT_EXPR, loc1);

    // if without else
    StmtIf *only_then = (StmtIf *)ast_alloc_node(arena, NODE_STMT_IF, loc1);
    only_then->condition = &cond->base;
    only_then->then_body = then_body;
    only_then->then_len  = 1;
    only_then->else_body = NULL;
    only_then->else_len  = 0;
    ok &= ASSERT(only_then->else_len == 0);
    ok &= ASSERT(only_then->then_len == 1);

    // if with else
    Node **else_body = (Node **)arena_alloc(arena, sizeof(Node *));
    else_body[0] = ast_alloc_node(arena, NODE_STMT_EXPR, loc1);

    StmtIf *with_else = (StmtIf *)ast_alloc_node(arena, NODE_STMT_IF, loc1);
    with_else->condition = &cond->base;
    with_else->then_body = then_body;
    with_else->then_len  = 1;
    with_else->else_body = else_body;
    with_else->else_len  = 1;
    ok &= ASSERT(with_else->else_len == 1);

    arena_delete(arena);
    return ok;
}

bool test_stmt_while(void) {
    bool ok = true;
    Arena *arena = arena_new(0);

    ExprBoolLit *cond = (ExprBoolLit *)ast_alloc_node(arena, NODE_EXPR_BOOL_LIT, loc1);
    cond->value = true;

    StmtWhile *n = (StmtWhile *)ast_alloc_node(arena, NODE_STMT_WHILE, loc1);
    n->condition = &cond->base;
    n->body      = NULL;
    n->body_len  = 0;
    ok &= ASSERT(NODE_IS(&n->base, NODE_STMT_WHILE));
    ok &= ASSERT(n->body_len == 0);

    arena_delete(arena);
    return ok;
}

bool test_stmt_for(void) {
    bool ok = true;
    Arena *arena = arena_new(0);

    ExprIdent *range = (ExprIdent *)ast_alloc_node(arena, NODE_EXPR_IDENT, loc1);
    range->name = sv("items");

    StmtFor *n = (StmtFor *)ast_alloc_node(arena, NODE_STMT_FOR, loc1);
    n->var_name  = sv("item");
    n->iter_expr = &range->base;
    n->body      = NULL;
    n->body_len  = 0;
    ok &= ASSERT(strncmp(n->var_name.data, "item", n->var_name.len) == 0);
    ok &= ASSERT(n->iter_expr == &range->base);

    arena_delete(arena);
    return ok;
}

bool test_stmt_return(void) {
    bool ok = true;
    Arena *arena = arena_new(0);

    // bare return
    StmtReturn *bare = (StmtReturn *)ast_alloc_node(arena, NODE_STMT_RETURN, loc1);
    bare->value = NULL;
    ok &= ASSERT(bare->value == NULL);

    // return with value
    ExprIntLit *val = (ExprIntLit *)ast_alloc_node(arena, NODE_EXPR_INT_LIT, loc1);
    val->value = 0;

    StmtReturn *with_val = (StmtReturn *)ast_alloc_node(arena, NODE_STMT_RETURN, loc1);
    with_val->value = &val->base;
    ok &= ASSERT(with_val->value != NULL);
    ok &= ASSERT(NODE_IS(with_val->value, NODE_EXPR_INT_LIT));

    arena_delete(arena);
    return ok;
}

bool test_stmt_break_continue(void) {
    bool ok = true;
    Arena *arena = arena_new(0);

    StmtBreakContinue *brk = (StmtBreakContinue *)ast_alloc_node(arena, NODE_STMT_BREAK, loc1);
    brk->label = sv("");
    ok &= ASSERT(NODE_IS(&brk->base, NODE_STMT_BREAK));
    ok &= ASSERT(brk->label.len == 0);

    StmtBreakContinue *cnt = (StmtBreakContinue *)ast_alloc_node(arena, NODE_STMT_CONTINUE, loc1);
    cnt->label = sv("outer");
    ok &= ASSERT(NODE_IS(&cnt->base, NODE_STMT_CONTINUE));
    ok &= ASSERT(strncmp(cnt->label.data, "outer", cnt->label.len) == 0);

    arena_delete(arena);
    return ok;
}

bool test_stmt_block(void) {
    bool ok = true;
    Arena *arena = arena_new(0);

    Node **stmts = (Node **)arena_alloc(arena, 3 * sizeof(Node *));
    stmts[0] = ast_alloc_node(arena, NODE_STMT_EXPR, loc1);
    stmts[1] = ast_alloc_node(arena, NODE_STMT_RETURN, loc1);
    stmts[2] = ast_alloc_node(arena, NODE_STMT_ASSIGNMENT, loc1);

    StmtBlock *n = (StmtBlock *)ast_alloc_node(arena, NODE_STMT_BLOCK, loc1);
    n->statements = stmts;
    n->stmt_count = 3;
    ok &= ASSERT(n->stmt_count == 3);
    ok &= ASSERT(NODE_IS(n->statements[0], NODE_STMT_EXPR));
    ok &= ASSERT(NODE_IS(n->statements[1], NODE_STMT_RETURN));
    ok &= ASSERT(NODE_IS(n->statements[2], NODE_STMT_ASSIGNMENT));

    arena_delete(arena);
    return ok;
}

// ============================================================================
// Declarations
// ============================================================================

bool test_decl_function(void) {
    bool ok = true;
    Arena *arena = arena_new(0);

    // Simple function — no params, no body
    DeclFunction *simple = (DeclFunction *)ast_alloc_node(arena, NODE_DECL_FUNCTION, loc1);
    simple->name        = sv("main");
    simple->params      = NULL;
    simple->param_count = 0;
    simple->return_type = NULL;
    simple->body        = NULL;
    simple->body_len    = 0;
    ok &= ASSERT(simple->param_count == 0);
    ok &= ASSERT(simple->return_type == NULL);

    // With params and return type
    Param *params = (Param *)arena_alloc(arena, 2 * sizeof(Param));
    params[0].name        = sv("a");
    params[0].is_comptime = false;
    params[0].is_variadic = false;
    params[0].type        = ast_alloc_node(arena, NODE_TYPE_PRIMITIVE, loc1);
    params[1].name        = sv("b");
    params[1].is_comptime = false;
    params[1].is_variadic = false;
    params[1].type        = ast_alloc_node(arena, NODE_TYPE_PRIMITIVE, loc1);

    TypePrimitive *ret = (TypePrimitive *)ast_alloc_node(arena, NODE_TYPE_PRIMITIVE, loc1);
    ret->name = sv("i32");

    DeclFunction *with_params = (DeclFunction *)ast_alloc_node(arena, NODE_DECL_FUNCTION, loc1);
    with_params->name        = sv("add");
    with_params->params      = params;
    with_params->param_count = 2;
    with_params->return_type = &ret->base;
    ok &= ASSERT(with_params->param_count == 2);
    ok &= ASSERT(strncmp(with_params->params[0].name.data, "a", 1) == 0);
    ok &= ASSERT(with_params->return_type != NULL);

    // Flags
    DeclFunction *ext = (DeclFunction *)ast_alloc_node(arena, NODE_DECL_FUNCTION, loc1);
    ext->is_extern = true;
    ext->is_inline = false;
    ok &= ASSERT(ext->is_extern);
    ok &= ASSERT(!ext->is_inline);

    arena_delete(arena);
    return ok;
}

bool test_decl_struct(void) {
    bool ok = true;
    Arena *arena = arena_new(0);

    StringView *names = (StringView *)arena_alloc(arena, 2 * sizeof(StringView));
    names[0] = sv("x");
    names[1] = sv("y");

    Node **types = (Node **)arena_alloc(arena, 2 * sizeof(Node *));
    TypePrimitive *tf32_0 = (TypePrimitive *)ast_alloc_node(arena, NODE_TYPE_PRIMITIVE, loc1);
    TypePrimitive *tf32_1 = (TypePrimitive *)ast_alloc_node(arena, NODE_TYPE_PRIMITIVE, loc1);
    tf32_0->name = sv("f32");
    tf32_1->name = sv("f32");
    types[0] = &tf32_0->base;
    types[1] = &tf32_1->base;

    DeclStruct *s = (DeclStruct *)ast_alloc_node(arena, NODE_DECL_STRUCT, loc1);
    s->name        = sv("Vec2");
    s->field_names = names;
    s->field_types = types;
    s->field_count = 2;
    s->is_generic  = false;
    ok &= ASSERT(s->field_count == 2);
    ok &= ASSERT(strncmp(s->field_names[0].data, "x", 1) == 0);
    ok &= ASSERT(NODE_IS(s->field_types[1], NODE_TYPE_PRIMITIVE));

    // Generic struct
    DeclStruct *generic = (DeclStruct *)ast_alloc_node(arena, NODE_DECL_STRUCT, loc1);
    generic->name          = sv("Box");
    generic->is_generic    = true;
    generic->generic_count = 1;
    StringView *gparams = (StringView *)arena_alloc(arena, sizeof(StringView));
    gparams[0] = sv("T");
    generic->generic_params = gparams;
    ok &= ASSERT(generic->is_generic);
    ok &= ASSERT(generic->generic_count == 1);

    arena_delete(arena);
    return ok;
}

bool test_decl_enum(void) {
    bool ok = true;
    Arena *arena = arena_new(0);

    EnumVariant *variants = (EnumVariant *)arena_alloc(arena, 3 * sizeof(EnumVariant));
    variants[0] = (EnumVariant){ .name = sv("Red"),   .kind = ENUM_VARIANT_PLAIN };
    variants[1] = (EnumVariant){ .name = sv("Green"), .kind = ENUM_VARIANT_PLAIN };
    variants[2] = (EnumVariant){ .name = sv("Blue"),  .kind = ENUM_VARIANT_PLAIN };

    DeclEnum *e = (DeclEnum *)ast_alloc_node(arena, NODE_DECL_ENUM, loc1);
    e->name          = sv("Color");
    e->variants      = variants;
    e->variant_count = 3;
    e->is_generic    = false;
    ok &= ASSERT(e->variant_count == 3);
    ok &= ASSERT(strncmp(e->variants[2].name.data, "Blue", 4) == 0);

    // Enum with associated data (tuple-like variants)
    EnumVariant *rv = (EnumVariant *)arena_alloc(arena, 2 * sizeof(EnumVariant));
    rv[0] = (EnumVariant){ .name = sv("Ok"),  .kind = ENUM_VARIANT_TUPLE,
                           .tuple_type = ast_alloc_node(arena, NODE_TYPE_GENERIC, loc1) };
    rv[1] = (EnumVariant){ .name = sv("Err"), .kind = ENUM_VARIANT_TUPLE,
                           .tuple_type = ast_alloc_node(arena, NODE_TYPE_GENERIC, loc1) };

    DeclEnum *result = (DeclEnum *)ast_alloc_node(arena, NODE_DECL_ENUM, loc1);
    result->name          = sv("Result");
    result->variants      = rv;
    result->variant_count = 2;
    result->is_generic    = true;
    result->generic_count = 2;
    ok &= ASSERT(result->variants[0].tuple_type != NULL);
    ok &= ASSERT(result->variants[0].kind == ENUM_VARIANT_TUPLE);
    ok &= ASSERT(result->is_generic);

    arena_delete(arena);
    return ok;
}

bool test_decl_type_alias(void) {
    bool ok = true;
    Arena *arena = arena_new(0);

    TypePrimitive *tp = (TypePrimitive *)ast_alloc_node(arena, NODE_TYPE_PRIMITIVE, loc1);
    tp->name = sv("i32");

    DeclTypeAlias *n = (DeclTypeAlias *)ast_alloc_node(arena, NODE_DECL_TYPE_ALIAS, loc1);
    n->name         = sv("Int");
    n->aliased_type = &tp->base;
    n->is_generic   = false;
    ok &= ASSERT(strncmp(n->name.data, "Int", n->name.len) == 0);
    ok &= ASSERT(n->aliased_type == &tp->base);

    arena_delete(arena);
    return ok;
}

bool test_decl_import(void) {
    bool ok = true;
    Arena *arena = arena_new(0);

    DeclImport *imp = (DeclImport *)ast_alloc_node(arena, NODE_DECL_IMPORT, loc1);
    imp->path            = sv("std/io");
    imp->is_external     = false;
    imp->selective       = NULL;
    imp->selective_count = 0;
    ok &= ASSERT(!imp->is_external);
    ok &= ASSERT(imp->selective_count == 0);

    DeclImport *cimport = (DeclImport *)ast_alloc_node(arena, NODE_DECL_IMPORT, loc1);
    cimport->path        = sv("stdio.h");
    cimport->is_external = true;
    ok &= ASSERT(cimport->is_external);

    arena_delete(arena);
    return ok;
}

// ============================================================================
// Types
// ============================================================================

bool test_type_primitive(void) {
    bool ok = true;
    Arena *arena = arena_new(0);

    const char *names[] = { "i8", "i16", "i32", "i64", "u8", "u16", "u32", "u64", "bool", "void" };
    for (size_t i = 0; i < sizeof(names) / sizeof(names[0]); i++) {
        TypePrimitive *n = (TypePrimitive *)ast_alloc_node(arena, NODE_TYPE_PRIMITIVE, loc1);
        n->name = sv(names[i]);
        ok &= ASSERT(NODE_IS(&n->base, NODE_TYPE_PRIMITIVE));
        ok &= ASSERT(n->name.len == strlen(names[i]));
    }

    arena_delete(arena);
    return ok;
}

bool test_type_pointer(void) {
    bool ok = true;
    Arena *arena = arena_new(0);

    TypePrimitive *inner = (TypePrimitive *)ast_alloc_node(arena, NODE_TYPE_PRIMITIVE, loc1);
    inner->name = sv("i32");

    TypePointer *immut = (TypePointer *)ast_alloc_node(arena, NODE_TYPE_POINTER, loc1);
    immut->pointee    = &inner->base;
    immut->is_mutable = false;
    ok &= ASSERT(!immut->is_mutable);
    ok &= ASSERT(NODE_IS(immut->pointee, NODE_TYPE_PRIMITIVE));

    TypePointer *mut = (TypePointer *)ast_alloc_node(arena, NODE_TYPE_POINTER, loc1);
    mut->pointee    = &inner->base;
    mut->is_mutable = true;
    ok &= ASSERT(mut->is_mutable);

    arena_delete(arena);
    return ok;
}

bool test_type_array(void) {
    bool ok = true;
    Arena *arena = arena_new(0);

    TypePrimitive *elem = (TypePrimitive *)ast_alloc_node(arena, NODE_TYPE_PRIMITIVE, loc1);
    elem->name = sv("u8");

    ExprIntLit *len = (ExprIntLit *)ast_alloc_node(arena, NODE_EXPR_INT_LIT, loc1);
    len->value = 10;

    // Fixed array: [10]u8
    TypeArray *arr = (TypeArray *)ast_alloc_node(arena, NODE_TYPE_ARRAY, loc1);
    arr->element_type = &elem->base;
    arr->length       = &len->base;
    arr->is_slice     = false;
    ok &= ASSERT(!arr->is_slice);
    ok &= ASSERT(arr->length != NULL);

    // Slice: []u8
    TypeArray *slice = (TypeArray *)ast_alloc_node(arena, NODE_TYPE_ARRAY, loc1);
    slice->element_type = &elem->base;
    slice->length       = NULL;
    slice->is_slice     = true;
    ok &= ASSERT(slice->is_slice);
    ok &= ASSERT(slice->length == NULL);

    arena_delete(arena);
    return ok;
}

bool test_type_function(void) {
    bool ok = true;
    Arena *arena = arena_new(0);

    Node **ptypes = (Node **)arena_alloc(arena, 2 * sizeof(Node *));
    ptypes[0] = ast_alloc_node(arena, NODE_TYPE_PRIMITIVE, loc1);
    ptypes[1] = ast_alloc_node(arena, NODE_TYPE_PRIMITIVE, loc1);

    TypePrimitive *ret = (TypePrimitive *)ast_alloc_node(arena, NODE_TYPE_PRIMITIVE, loc1);
    ret->name = sv("bool");

    TypeFunction *fn = (TypeFunction *)ast_alloc_node(arena, NODE_TYPE_FUNCTION, loc1);
    fn->param_types  = ptypes;
    fn->param_count  = 2;
    fn->return_type  = &ret->base;
    ok &= ASSERT(fn->param_count == 2);
    ok &= ASSERT(fn->return_type != NULL);
    ok &= ASSERT(NODE_IS(fn->return_type, NODE_TYPE_PRIMITIVE));

    arena_delete(arena);
    return ok;
}

bool test_type_generic(void) {
    bool ok = true;
    Arena *arena = arena_new(0);

    Node **targs = (Node **)arena_alloc(arena, 2 * sizeof(Node *));
    targs[0] = ast_alloc_node(arena, NODE_TYPE_PRIMITIVE, loc1);
    targs[1] = ast_alloc_node(arena, NODE_TYPE_PRIMITIVE, loc1);

    TypeGeneric *g = (TypeGeneric *)ast_alloc_node(arena, NODE_TYPE_GENERIC, loc1);
    g->base_name     = sv("Result");
    g->type_args     = targs;
    g->type_arg_count = 2;
    ok &= ASSERT(strncmp(g->base_name.data, "Result", g->base_name.len) == 0);
    ok &= ASSERT(g->type_arg_count == 2);

    arena_delete(arena);
    return ok;
}

// ============================================================================
// Cast macros
// ============================================================================

bool test_node_cast_valid(void) {
    bool ok = true;
    Arena *arena = arena_new(0);

    Node *n = ast_alloc_node(arena, NODE_EXPR_INT_LIT, loc1);
    ExprIntLit *casted = NODE_CAST(n, ExprIntLit, NODE_EXPR_INT_LIT);
    ok &= ASSERT(casted != NULL);
    ok &= ASSERT((Node *)casted == n);

    arena_delete(arena);
    return ok;
}

bool test_node_cast_invalid(void) {
    bool ok = true;
    Arena *arena = arena_new(0);

    Node *n = ast_alloc_node(arena, NODE_EXPR_INT_LIT, loc1);
    ExprFloatLit *wrong = NODE_CAST(n, ExprFloatLit, NODE_EXPR_FLOAT_LIT);
    ok &= ASSERT(wrong == NULL);

    arena_delete(arena);
    return ok;
}

bool test_node_cast_any(void) {
    bool ok = true;
    Arena *arena = arena_new(0);

    Node *n = ast_alloc_node(arena, NODE_EXPR_INT_LIT, loc1);

    // Single kind — matches
    ExprIntLit *hit = NODE_CAST_ANY(n, ExprIntLit, NODE_EXPR_INT_LIT);
    ok &= ASSERT(hit != NULL);

    // Multiple kinds — first matches
    ExprIntLit *multi_hit = NODE_CAST_ANY(n, ExprIntLit,
        NODE_EXPR_INT_LIT, NODE_EXPR_FLOAT_LIT, NODE_EXPR_STRING_LIT);
    ok &= ASSERT(multi_hit != NULL);

    // Multiple kinds — last matches
    ExprIntLit *last_hit = NODE_CAST_ANY(n, ExprIntLit,
        NODE_EXPR_FLOAT_LIT, NODE_EXPR_STRING_LIT, NODE_EXPR_INT_LIT);
    ok &= ASSERT(last_hit != NULL);

    // No match
    ExprFloatLit *miss = NODE_CAST_ANY(n, ExprFloatLit,
        NODE_EXPR_FLOAT_LIT, NODE_EXPR_STRING_LIT);
    ok &= ASSERT(miss == NULL);

    arena_delete(arena);
    return ok;
}

bool test_node_is(void) {
    bool ok = true;
    Arena *arena = arena_new(0);

    Node *n = ast_alloc_node(arena, NODE_STMT_IF, loc1);
    ok &= ASSERT( NODE_IS(n, NODE_STMT_IF));
    ok &= ASSERT(!NODE_IS(n, NODE_STMT_WHILE));
    ok &= ASSERT(!NODE_IS(n, NODE_EXPR_INT_LIT));

    arena_delete(arena);
    return ok;
}

bool test_node_cast_unsafe(void) {
    bool ok = true;
    Arena *arena = arena_new(0);

    ExprIntLit *n = (ExprIntLit *)ast_alloc_node(arena, NODE_EXPR_INT_LIT, loc1);
    n->value = 77;

    ExprIntLit *casted = NODE_CAST_UNSAFE(&n->base, ExprIntLit);
    ok &= ASSERT(casted != NULL);
    ok &= ASSERT(casted->value == 77);

    arena_delete(arena);
    return ok;
}

// ============================================================================
// Patterns
// ============================================================================

bool test_patterns(void) {
    bool ok = true;
    Arena *arena = arena_new(0);

    Node *ident = ast_alloc_node(arena, NODE_PATTERN_IDENT, loc1);
    ok &= ASSERT(ident != NULL);
    ok &= ASSERT(NODE_IS(ident, NODE_PATTERN_IDENT));

    Node *tuple = ast_alloc_node(arena, NODE_PATTERN_TUPLE, loc1);
    ok &= ASSERT(tuple != NULL);
    ok &= ASSERT(NODE_IS(tuple, NODE_PATTERN_TUPLE));

    Node *strct = ast_alloc_node(arena, NODE_PATTERN_STRUCT, loc1);
    ok &= ASSERT(strct != NULL);
    ok &= ASSERT(NODE_IS(strct, NODE_PATTERN_STRUCT));

    // Patterns must not match each other
    ok &= ASSERT(!NODE_IS(ident, NODE_PATTERN_TUPLE));
    ok &= ASSERT(!NODE_IS(tuple, NODE_PATTERN_STRUCT));

    arena_delete(arena);
    return ok;
}

// ============================================================================
// StmtExprStmt
// ============================================================================

bool test_stmt_expr_stmt(void) {
    bool ok = true;
    Arena *arena = arena_new(0);

    ExprIntLit *inner = (ExprIntLit *)ast_alloc_node(arena, NODE_EXPR_INT_LIT, loc1);
    inner->value = 7;

    StmtExprStmt *s = (StmtExprStmt *)ast_alloc_node(arena, NODE_STMT_EXPR, loc1);
    s->expr = &inner->base;

    ok &= ASSERT(NODE_IS(&s->base, NODE_STMT_EXPR));
    ok &= ASSERT(s->expr == &inner->base);
    ok &= ASSERT(NODE_IS(s->expr, NODE_EXPR_INT_LIT));

    arena_delete(arena);
    return ok;
}

// ============================================================================
// node_kind_to_string
// ============================================================================

bool test_node_kind_to_string(void) {
    bool ok = true;

    // Spot-check a sample across all categories
    struct { NodeKind kind; const char *expected; } cases[] = {
        { NODE_EXPR_INT_LIT,      "ExprIntLit"      },
        { NODE_EXPR_BINARY_OP,    "ExprBinaryOp"    },
        { NODE_STMT_IF,           "StmtIf"          },
        { NODE_STMT_BREAK,        "StmtBreak"       },
        { NODE_STMT_CONTINUE,     "StmtContinue"    },
        { NODE_DECL_FUNCTION,     "DeclFunction"    },
        { NODE_TYPE_POINTER,      "TypePointer"     },
        { NODE_PATTERN_IDENT,     "PatternIdent"    },
        { NODE_PATTERN_TUPLE,     "PatternTuple"    },
        { NODE_PATTERN_STRUCT,    "PatternStruct"   },
    };

    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
        const char *s = node_kind_to_string(cases[i].kind);
        ok &= ASSERT(s != NULL);
        ok &= ASSERT(strcmp(s, cases[i].expected) == 0);
    }

    // Every kind in [0, NODE_KIND_COUNT) must return a non-NULL, non-"<unknown>" string
    for (int k = 0; k < NODE_KIND_COUNT; k++) {
        const char *s = node_kind_to_string((NodeKind)k);
        ok &= ASSERT(s != NULL);
        ok &= ASSERT(strcmp(s, "<unknown>") != 0);
    }

    arena_delete(arena_new(0));  // keep arena API exercised; no alloc needed here
    return ok;
}

// ============================================================================
// DeclTypeAlias — generic params
// ============================================================================

bool test_decl_type_alias_generic(void) {
    bool ok = true;
    Arena *arena = arena_new(0);

    StringView *gparams = (StringView *)arena_alloc(arena, 2 * sizeof(StringView));
    gparams[0] = sv("T");
    gparams[1] = sv("U");

    TypeGeneric *aliased = (TypeGeneric *)ast_alloc_node(arena, NODE_TYPE_GENERIC, loc1);
    aliased->base_name     = sv("Map");
    aliased->type_arg_count = 2;

    DeclTypeAlias *n = (DeclTypeAlias *)ast_alloc_node(arena, NODE_DECL_TYPE_ALIAS, loc1);
    n->name            = sv("MyMap");
    n->aliased_type    = &aliased->base;
    n->is_generic      = true;
    n->generic_params  = gparams;
    n->generic_count   = 2;

    ok &= ASSERT(n->is_generic);
    ok &= ASSERT(n->generic_count == 2);
    ok &= ASSERT(strncmp(n->generic_params[0].data, "T", 1) == 0);
    ok &= ASSERT(strncmp(n->generic_params[1].data, "U", 1) == 0);
    ok &= ASSERT(NODE_IS(n->aliased_type, NODE_TYPE_GENERIC));

    arena_delete(arena);
    return ok;
}

// ============================================================================
// DeclImport — selective imports
// ============================================================================

bool test_decl_import_selective(void) {
    bool ok = true;
    Arena *arena = arena_new(0);

    StringView *names = (StringView *)arena_alloc(arena, 2 * sizeof(StringView));
    names[0] = sv("println");
    names[1] = sv("eprintln");

    DeclImport *imp = (DeclImport *)ast_alloc_node(arena, NODE_DECL_IMPORT, loc1);
    imp->path            = sv("std/io");
    imp->is_external     = false;
    imp->selective       = names;
    imp->selective_count = 2;

    ok &= ASSERT(imp->selective_count == 2);
    ok &= ASSERT(strncmp(imp->selective[0].data, "println",  7) == 0);
    ok &= ASSERT(strncmp(imp->selective[1].data, "eprintln", 8) == 0);

    // Zero selective_count means import everything
    DeclImport *all = (DeclImport *)ast_alloc_node(arena, NODE_DECL_IMPORT, loc1);
    all->path            = sv("std/math");
    all->selective       = NULL;
    all->selective_count = 0;
    ok &= ASSERT(all->selective_count == 0);
    ok &= ASSERT(all->selective == NULL);

    arena_delete(arena);
    return ok;
}

// ============================================================================
// Program root
// ============================================================================

bool test_program(void) {
    bool ok = true;
    Arena *arena = arena_new(0);

    DeclFunction *fn = (DeclFunction *)ast_alloc_node(arena, NODE_DECL_FUNCTION, loc1);
    fn->name        = sv("main");
    fn->param_count = 0;
    fn->return_type = NULL;

    Node **decls = (Node **)arena_alloc(arena, sizeof(Node *));
    decls[0] = &fn->base;

    Program prog;
    prog.filename   = "test.kz";
    prog.decls      = decls;
    prog.decl_count = 1;

    ok &= ASSERT(prog.decl_count == 1);
    ok &= ASSERT(strcmp(prog.filename, "test.kz") == 0);
    ok &= ASSERT(NODE_IS(prog.decls[0], NODE_DECL_FUNCTION));

    // Empty program
    Program empty;
    empty.filename   = "empty.kz";
    empty.decls      = NULL;
    empty.decl_count = 0;
    ok &= ASSERT(empty.decl_count == 0);
    ok &= ASSERT(empty.decls == NULL);

    arena_delete(arena);
    return ok;
}

// ============================================================================
// Traversal helpers + tests
// ============================================================================

static int tree_depth(Node *node) {
    if (!node) return 0;
    switch (node->kind) {
        case NODE_EXPR_BINARY_OP: {
            ExprBinaryOp *n = NODE_CAST_UNSAFE(node, ExprBinaryOp);
            int l = tree_depth(n->left);
            int r = tree_depth(n->right);
            return 1 + (l > r ? l : r);
        }
        case NODE_STMT_BLOCK: {
            StmtBlock *n = NODE_CAST_UNSAFE(node, StmtBlock);
            int max = 0;
            for (size_t i = 0; i < n->stmt_count; i++) {
                int d = tree_depth(n->statements[i]);
                if (d > max) max = d;
            }
            return 1 + max;
        }
        case NODE_STMT_RETURN: {
            StmtReturn *n = NODE_CAST_UNSAFE(node, StmtReturn);
            return 1 + tree_depth(n->value);
        }
        default:
            return 1;
    }
}

static int tree_count(Node *node) {
    if (!node) return 0;
    switch (node->kind) {
        case NODE_EXPR_BINARY_OP: {
            ExprBinaryOp *n = NODE_CAST_UNSAFE(node, ExprBinaryOp);
            return 1 + tree_count(n->left) + tree_count(n->right);
        }
        case NODE_STMT_BLOCK: {
            StmtBlock *n = NODE_CAST_UNSAFE(node, StmtBlock);
            int total = 1;
            for (size_t i = 0; i < n->stmt_count; i++)
                total += tree_count(n->statements[i]);
            return total;
        }
        case NODE_STMT_RETURN: {
            StmtReturn *n = NODE_CAST_UNSAFE(node, StmtReturn);
            return 1 + tree_count(n->value);
        }
        default:
            return 1;
    }
}

bool test_tree_depth(void) {
    bool ok = true;
    Arena *arena = arena_new(0);

    // Build: (1 + 2)  — depth should be 2
    ExprIntLit *a = (ExprIntLit *)ast_alloc_node(arena, NODE_EXPR_INT_LIT, loc1);
    ExprIntLit *b = (ExprIntLit *)ast_alloc_node(arena, NODE_EXPR_INT_LIT, loc1);
    ExprBinaryOp *add = (ExprBinaryOp *)ast_alloc_node(arena, NODE_EXPR_BINARY_OP, loc1);
    add->left  = &a->base;
    add->right = &b->base;
    add->op    = sv("+");
    ok &= ASSERT(tree_depth(&add->base) == 2);

    // Build: ((1+2)+3) — depth should be 3
    ExprIntLit *c = (ExprIntLit *)ast_alloc_node(arena, NODE_EXPR_INT_LIT, loc1);
    ExprBinaryOp *outer = (ExprBinaryOp *)ast_alloc_node(arena, NODE_EXPR_BINARY_OP, loc1);
    outer->left  = &add->base;
    outer->right = &c->base;
    outer->op    = sv("+");
    ok &= ASSERT(tree_depth(&outer->base) == 3);

    arena_delete(arena);
    return ok;
}

bool test_tree_node_count(void) {
    bool ok = true;
    Arena *arena = arena_new(0);

    // Single leaf
    Node *leaf = ast_alloc_node(arena, NODE_EXPR_INT_LIT, loc1);
    ok &= ASSERT(tree_count(leaf) == 1);

    // (a + b): 3 nodes
    ExprIntLit *a = (ExprIntLit *)ast_alloc_node(arena, NODE_EXPR_INT_LIT, loc1);
    ExprIntLit *b = (ExprIntLit *)ast_alloc_node(arena, NODE_EXPR_INT_LIT, loc1);
    ExprBinaryOp *add = (ExprBinaryOp *)ast_alloc_node(arena, NODE_EXPR_BINARY_OP, loc1);
    add->left  = &a->base;
    add->right = &b->base;
    add->op    = sv("+");
    ok &= ASSERT(tree_count(&add->base) == 3);

    // block { return (a+b); }: 4 nodes (block + return + binop + leaf)
    StmtReturn *ret = (StmtReturn *)ast_alloc_node(arena, NODE_STMT_RETURN, loc1);
    ret->value = &add->base;

    Node **stmts = (Node **)arena_alloc(arena, sizeof(Node *));
    stmts[0] = &ret->base;

    StmtBlock *block = (StmtBlock *)ast_alloc_node(arena, NODE_STMT_BLOCK, loc1);
    block->statements = stmts;
    block->stmt_count = 1;
    ok &= ASSERT(tree_count(&block->base) == 5);  // block + return + binop + a + b

    arena_delete(arena);
    return ok;
}

typedef struct { NodeKind *kinds; size_t len; size_t cap; } KindList;

static void dfs_collect(Node *node, KindList *list, Arena *arena) {
    if (!node) return;
    if (list->len >= list->cap) {
        list->cap *= 2;
        list->kinds = (NodeKind *)arena_realloc(arena, list->kinds, list->cap * sizeof(NodeKind));
    }
    list->kinds[list->len++] = node->kind;
    switch (node->kind) {
        case NODE_EXPR_BINARY_OP: {
            ExprBinaryOp *n = NODE_CAST_UNSAFE(node, ExprBinaryOp);
            dfs_collect(n->left,  list, arena);
            dfs_collect(n->right, list, arena);
            break;
        }
        case NODE_STMT_RETURN: {
            StmtReturn *n = NODE_CAST_UNSAFE(node, StmtReturn);
            dfs_collect(n->value, list, arena);
            break;
        }
        default: break;
    }
}

bool test_tree_walk_dfs(void) {
    bool ok = true;
    Arena *arena = arena_new(0);

    // Build: return (a + b)
    ExprIntLit *a = (ExprIntLit *)ast_alloc_node(arena, NODE_EXPR_INT_LIT, loc1);
    ExprIntLit *b = (ExprIntLit *)ast_alloc_node(arena, NODE_EXPR_INT_LIT, loc1);
    ExprBinaryOp *add = (ExprBinaryOp *)ast_alloc_node(arena, NODE_EXPR_BINARY_OP, loc1);
    add->left  = &a->base;
    add->right = &b->base;
    add->op    = sv("+");

    StmtReturn *ret = (StmtReturn *)ast_alloc_node(arena, NODE_STMT_RETURN, loc1);
    ret->value = &add->base;

    KindList list = {
        .kinds = (NodeKind *)arena_alloc_reallocable(arena, 8 * sizeof(NodeKind)),
        .len = 0, .cap = 8
    };
    dfs_collect(&ret->base, &list, arena);

    // Expected DFS order: RETURN, BINARY_OP, INT_LIT, INT_LIT
    ok &= ASSERT(list.len == 4);
    ok &= ASSERT(list.kinds[0] == NODE_STMT_RETURN);
    ok &= ASSERT(list.kinds[1] == NODE_EXPR_BINARY_OP);
    ok &= ASSERT(list.kinds[2] == NODE_EXPR_INT_LIT);
    ok &= ASSERT(list.kinds[3] == NODE_EXPR_INT_LIT);

    arena_delete(arena);
    return ok;
}

// ============================================================================
// Aggregator
// ============================================================================

static bool ast_tests(void) {
    bool ok = true;

    struct { TestFn func; const char *name; } tests[] = {
        { test_ast_alloc,               "test_ast_alloc"               },
        { test_ast_alloc_all_kinds,     "test_ast_alloc_all_kinds"     },
        { test_ast_source_loc,          "test_ast_source_loc"          },

        { test_expr_int_lit,            "test_expr_int_lit"            },
        { test_expr_float_lit,          "test_expr_float_lit"          },
        { test_expr_string_lit,         "test_expr_string_lit"         },
        { test_expr_bool_lit,           "test_expr_bool_lit"           },
        { test_expr_null_lit,           "test_expr_null_lit"           },
        { test_expr_ident,              "test_expr_ident"              },
        { test_expr_field_access,       "test_expr_field_access"       },
        { test_expr_index,              "test_expr_index"              },
        { test_expr_enum_variant,       "test_expr_enum_variant"       },
        { test_expr_binary_op,          "test_expr_binary_op"          },
        { test_expr_unary_op,           "test_expr_unary_op"           },
        { test_expr_cast,               "test_expr_cast"               },
        { test_expr_call,               "test_expr_call"               },
        { test_expr_postfix,            "test_expr_postfix"            },

        { test_stmt_var_decl,           "test_stmt_var_decl"           },
        { test_stmt_assignment,         "test_stmt_assignment"         },
        { test_stmt_if,                 "test_stmt_if"                 },
        { test_stmt_while,              "test_stmt_while"              },
        { test_stmt_for,                "test_stmt_for"                },
        { test_stmt_return,             "test_stmt_return"             },
        { test_stmt_break_continue,     "test_stmt_break_continue"     },
        { test_stmt_block,              "test_stmt_block"              },

        { test_decl_function,           "test_decl_function"           },
        { test_decl_struct,             "test_decl_struct"             },
        { test_decl_enum,               "test_decl_enum"               },
        { test_decl_type_alias,         "test_decl_type_alias"         },
        { test_decl_import,             "test_decl_import"             },

        { test_type_primitive,          "test_type_primitive"          },
        { test_type_pointer,            "test_type_pointer"            },
        { test_type_array,              "test_type_array"              },
        { test_type_function,           "test_type_function"           },
        { test_type_generic,            "test_type_generic"            },

        { test_patterns,                "test_patterns"                },
        { test_stmt_expr_stmt,          "test_stmt_expr_stmt"          },
        { test_node_kind_to_string,     "test_node_kind_to_string"     },
        { test_decl_type_alias_generic, "test_decl_type_alias_generic" },
        { test_decl_import_selective,   "test_decl_import_selective"   },
        { test_program,                 "test_program"                 },

        { test_node_cast_valid,         "test_node_cast_valid"         },
        { test_node_cast_invalid,       "test_node_cast_invalid"       },
        { test_node_cast_any,           "test_node_cast_any"           },
        { test_node_is,                 "test_node_is"                 },
        { test_node_cast_unsafe,        "test_node_cast_unsafe"        },

        { test_tree_depth,              "test_tree_depth"              },
        { test_tree_node_count,         "test_tree_node_count"         },
        { test_tree_walk_dfs,           "test_tree_walk_dfs"           },
    };

    for (size_t i = 0; i < sizeof(tests) / sizeof(tests[0]); i++) {
        bool res = tests[i].func();
        if (!res) fprintf(stderr, "test %s failed.\n", tests[i].name);
        ok &= res;
    }
    return ok;
}

TestSuite ast_suite(void) {
    return (TestSuite){ .name = "AST", .func = ast_tests };
}
