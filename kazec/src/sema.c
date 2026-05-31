#include "../include/sema.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

// ============================================================================
// Diagnostics  (mirrors parser_error's location-prefixed format)
// ============================================================================

static void sema_error(Sema *s, SourceLoc loc, const char *fmt, ...) {
    s->had_error = true;

    va_list args;
    va_start(args, fmt);
    va_list copy;
    va_copy(copy, args);
    int needed = vsnprintf(NULL, 0, fmt, copy);
    va_end(copy);
    if (needed < 0) { va_end(args); return; }

    char *body = arena_alloc(s->arena, (size_t)needed + 1);
    vsnprintf(body, (size_t)needed + 1, fmt, args);
    va_end(args);

    const char *file = loc.filename ? loc.filename
                     : (s->filename ? s->filename : "<input>");
    int total = snprintf(NULL, 0, "%s:%u:%u: %s", file, loc.line, loc.col, body);
    if (total < 0) return;
    char *msg = arena_alloc(s->arena, (size_t)total + 1);
    snprintf(msg, (size_t)total + 1, "%s:%u:%u: %s", file, loc.line, loc.col, body);

    ErrorVec_push(&s->errors, msg);
}

// ============================================================================
// Init
// ============================================================================

void sema_init(Sema *s, Arena *arena, const char *filename) {
    s->arena    = arena;
    s->filename = filename;
    type_context_init(&s->types, arena);
    s->global         = scope_new(arena, NULL);
    s->current        = s->global;
    s->current_return = NULL;
    s->errors         = ErrorVec_create(arena, 8);
    s->had_error      = false;
}

// ============================================================================
// Pass 1 — declaration collection
// ============================================================================

// Register a nominal type (struct/enum/union) as a SYMBOL_TYPE in the global
// scope and link the TypeInfo back onto its declaration node.
static void register_aggregate(Sema *s, StringView name, Node *decl, TypeKind kind) {
    if (scope_lookup_local(s->global, name) != NULL) {
        sema_error(s, decl->loc, "redefinition of '%.*s'",
                   (int)name.len, name.data);
        return;
    }
    TyAggregate *agg = TYPE_CAST_UNSAFE(type_alloc(&s->types, kind), TyAggregate);
    agg->name = name;
    agg->decl = decl;
    decl->type_info = &agg->base;
    scope_define(s->global, SYMBOL_TYPE, name, &agg->base, decl, false);
}

// Build the function's signature type from its parameter and return types.
static TypeInfo *build_fn_type(Sema *s, DeclFunction *fn) {
    TypeInfo **params = NULL;
    size_t count = 0;
    bool variadic = false;

    if (fn->param_count > 0) {
        params = arena_alloc(s->arena, fn->param_count * sizeof(TypeInfo *));
        for (size_t i = 0; i < fn->param_count; i++) {
            if (fn->params[i].is_variadic) { variadic = true; continue; }
            params[count++] = type_resolve(&s->types, s->global, fn->params[i].type);
        }
    }

    TypeInfo *ret = fn->return_type ? type_resolve(&s->types, s->global, fn->return_type)
                                    : s->types.t_void;
    return type_function(&s->types, params, count, ret, variadic);
}

void sema_collect(Sema *s, Program *program) {
    // --- Phase A: register all nominal type names (and reserve alias names) so
    // later phases can resolve references in any declaration order. ----------
    for (size_t i = 0; i < program->decl_count; i++) {
        Node *d = program->decls[i];
        if (!d) continue;

        switch (d->kind) {
        case NODE_DECL_STRUCT:
            register_aggregate(s, NODE_CAST_UNSAFE(d, DeclStruct)->name, d, TYPE_STRUCT);
            break;
        case NODE_DECL_ENUM:
            register_aggregate(s, NODE_CAST_UNSAFE(d, DeclEnum)->name, d, TYPE_ENUM);
            break;
        case NODE_DECL_UNION:
            register_aggregate(s, NODE_CAST_UNSAFE(d, DeclUnion)->name, d, TYPE_UNION);
            break;
        case NODE_DECL_TYPE_ALIAS: {
            // Reserve the name now; its target is resolved in Phase B once all
            // type names are known.
            StringView name = NODE_CAST_UNSAFE(d, DeclTypeAlias)->name;
            if (scope_lookup_local(s->global, name) != NULL) {
                sema_error(s, d->loc, "redefinition of '%.*s'",
                           (int)name.len, name.data);
            } else {
                scope_define(s->global, SYMBOL_TYPE, name, s->types.t_error, d, false);
            }
            break;
        }
        default:
            break;
        }
    }

    // --- Phase B: resolve type-alias targets. --------------------------------
    for (size_t i = 0; i < program->decl_count; i++) {
        Node *d = program->decls[i];
        if (!d || d->kind != NODE_DECL_TYPE_ALIAS) continue;

        DeclTypeAlias *a = NODE_CAST_UNSAFE(d, DeclTypeAlias);
        Symbol *sym = scope_lookup_local(s->global, a->name);
        if (!sym) continue;  // redefinition already reported in Phase A
        sym->type = type_resolve(&s->types, s->global, a->aliased_type);
        d->type_info = sym->type;
    }

    // --- Phase C: resolve function signatures and global variable types. -----
    for (size_t i = 0; i < program->decl_count; i++) {
        Node *d = program->decls[i];
        if (!d) continue;

        switch (d->kind) {
        case NODE_DECL_FUNCTION: {
            DeclFunction *fn = NODE_CAST_UNSAFE(d, DeclFunction);
            if (scope_lookup_local(s->global, fn->name) != NULL) {
                sema_error(s, d->loc, "redefinition of '%.*s'",
                           (int)fn->name.len, fn->name.data);
                break;
            }
            TypeInfo *ft = build_fn_type(s, fn);
            d->type_info = ft;
            scope_define(s->global, SYMBOL_FUNCTION, fn->name, ft, d, false);
            break;
        }
        case NODE_STMT_VAR_DECL: {
            // Top-level const / var / let.
            StmtVarDecl *v = NODE_CAST_UNSAFE(d, StmtVarDecl);
            if (scope_lookup_local(s->global, v->name) != NULL) {
                sema_error(s, d->loc, "redefinition of '%.*s'",
                           (int)v->name.len, v->name.data);
                break;
            }
            // Type comes from the annotation; without one it is inferred from
            // the initializer in Pass 2 (left as the error type for now).
            TypeInfo *vt = v->type_annotation
                         ? type_resolve(&s->types, s->global, v->type_annotation)
                         : s->types.t_error;
            d->type_info = vt;
            scope_define(s->global, SYMBOL_VAR, v->name, vt, d, v->is_const);
            break;
        }
        default:
            break;  // imports, top-level `when`, etc. — not handled in Pass 1
        }
    }
}

// ============================================================================
// Pass 2 — body type checking
// ============================================================================

static TypeInfo *check_expr(Sema *s, Node *expr);
static void      check_stmt(Sema *s, Node *stmt);
static void      check_body(Sema *s, Node **body, size_t n);  // in a fresh child scope

static bool sv_eq(StringView sv, const char *str) {
    size_t n = strlen(str);
    return sv.len == n && memcmp(sv.data, str, n) == 0;
}

// The error type is a poison: when an operand is already an error we skip
// further checks so a single mistake does not cascade into many diagnostics.
static bool is_err(Sema *s, const TypeInfo *t) {
    return !t || t == s->types.t_error || t->kind == TYPE_ERROR;
}

static const char *tstr(Sema *s, const TypeInfo *t) {
    return type_to_string(t, s->arena);
}

// Can a value of type `src` (produced by `src_expr`, may be NULL) be assigned
// to a slot of type `dst`? Equal types assign; an untyped numeric *literal*
// adopts any matching numeric target (so `var x: i64 = 2` is fine). Poisoned
// (error) types always "assign" so we don't pile on diagnostics.
static bool assignable(Sema *s, TypeInfo *dst, Node *src_expr, TypeInfo *src) {
    if (is_err(s, dst) || is_err(s, src)) return true;
    if (type_equals(dst, src)) return true;
    if (src_expr) {
        if (NODE_IS(src_expr, NODE_EXPR_INT_LIT) && type_is_integer(dst)) {
            src_expr->type_info = dst;   // literal takes on the target type
            return true;
        }
        if (NODE_IS(src_expr, NODE_EXPR_FLOAT_LIT) && type_is_float(dst)) {
            src_expr->type_info = dst;
            return true;
        }
    }
    return false;
}

// condition expressions (`if`/`while`/`when`) must be bool
static void expect_bool(Sema *s, Node *cond, const char *what) {
    if (!cond) return;
    TypeInfo *t = check_expr(s, cond);
    if (!is_err(s, t) && t->kind != TYPE_BOOL) {
        sema_error(s, cond->loc, "%s condition must be 'bool', got '%s'",
                   what, tstr(s, t));
    }
}

// ---------------------------------------------------------------------------
// Operators
// ---------------------------------------------------------------------------

static bool op_is(StringView op, const char *a, const char *b, const char *c) {
    return sv_eq(op, a) || (b && sv_eq(op, b)) || (c && sv_eq(op, c));
}

static TypeInfo *check_binary(Sema *s, ExprBinaryOp *e) {
    TypeInfo *l = check_expr(s, e->left);
    TypeInfo *r = check_expr(s, e->right);
    StringView op = e->op;

    // logical: bool op bool -> bool
    if (op_is(op, "&&", "||", NULL)) {
        if (!is_err(s, l) && l->kind != TYPE_BOOL)
            sema_error(s, e->base.loc, "operator '%.*s' requires 'bool' operands, got '%s'",
                       (int)op.len, op.data, tstr(s, l));
        return s->types.t_bool;
    }

    // comparisons: -> bool (operands must be compatible)
    if (op_is(op, "==", "!=", NULL) || op_is(op, "<", ">", NULL) ||
        op_is(op, "<=", ">=", NULL)) {
        if (!is_err(s, l) && !is_err(s, r) &&
            !type_equals(l, r) && !(type_is_numeric(l) && type_is_numeric(r))) {
            sema_error(s, e->base.loc, "cannot compare '%s' with '%s'",
                       tstr(s, l), tstr(s, r));
        }
        return s->types.t_bool;
    }

    // bitwise / shift: integer op integer -> integer
    if (op_is(op, "&", "|", "^") || op_is(op, "<<", ">>", NULL)) {
        if (!is_err(s, l) && !is_err(s, r)) {
            if (!type_is_integer(l) || !type_is_integer(r))
                sema_error(s, e->base.loc, "operator '%.*s' requires integer operands",
                           (int)op.len, op.data);
        }
        return is_err(s, l) ? r : l;
    }

    // arithmetic: numeric op numeric (same type) -> that type
    if (is_err(s, l) || is_err(s, r)) return s->types.t_error;
    if (!type_is_numeric(l) || !type_is_numeric(r)) {
        sema_error(s, e->base.loc, "operator '%.*s' requires numeric operands, got '%s' and '%s'",
                   (int)op.len, op.data, tstr(s, l), tstr(s, r));
        return s->types.t_error;
    }
    if (!type_equals(l, r)) {
        sema_error(s, e->base.loc, "mismatched operands for '%.*s': '%s' and '%s'",
                   (int)op.len, op.data, tstr(s, l), tstr(s, r));
        return s->types.t_error;
    }
    return l;
}

static TypeInfo *check_unary(Sema *s, ExprUnaryOp *e) {
    TypeInfo *t = check_expr(s, e->operand);
    StringView op = e->op;

    if (sv_eq(op, "&")) return type_pointer(&s->types, t, true);  // address-of
    if (sv_eq(op, "*")) {                                          // dereference
        if (is_err(s, t)) return s->types.t_error;
        if (t->kind != TYPE_POINTER) {
            sema_error(s, e->base.loc, "cannot dereference non-pointer type '%s'", tstr(s, t));
            return s->types.t_error;
        }
        return TYPE_CAST_UNSAFE(t, TyPointer)->pointee;
    }
    if (is_err(s, t)) return s->types.t_error;
    if (sv_eq(op, "!")) {
        if (t->kind != TYPE_BOOL)
            sema_error(s, e->base.loc, "operator '!' requires 'bool', got '%s'", tstr(s, t));
        return s->types.t_bool;
    }
    if (sv_eq(op, "~")) {
        if (!type_is_integer(t))
            sema_error(s, e->base.loc, "operator '~' requires an integer, got '%s'", tstr(s, t));
        return t;
    }
    // unary '-' (and any prefix ++/--): numeric -> same type
    if (sv_eq(op, "-") && !type_is_numeric(t))
        sema_error(s, e->base.loc, "operator '-' requires a numeric operand, got '%s'", tstr(s, t));
    return t;
}

static TypeInfo *check_call(Sema *s, ExprCall *e) {
    TypeInfo *callee = check_expr(s, e->callee);
    for (size_t i = 0; i < e->arg_count; i++) check_expr(s, e->args[i]);

    if (is_err(s, callee)) return s->types.t_error;
    if (callee->kind != TYPE_FUNCTION) {
        sema_error(s, e->base.loc, "cannot call a value of type '%s'", tstr(s, callee));
        return s->types.t_error;
    }

    TyFunction *fn = TYPE_CAST_UNSAFE(callee, TyFunction);
    bool arity_ok = fn->is_variadic ? e->arg_count >= fn->param_count
                                    : e->arg_count == fn->param_count;
    if (!arity_ok) {
        sema_error(s, e->base.loc, "expected %zu argument%s, got %zu",
                   fn->param_count, fn->param_count == 1 ? "" : "s", e->arg_count);
        return fn->return_type;
    }
    for (size_t i = 0; i < fn->param_count; i++) {
        TypeInfo *at = e->args[i]->type_info;  // set by check_expr above
        if (!assignable(s, fn->params[i], e->args[i], at)) {
            sema_error(s, e->args[i]->loc, "argument %zu: expected '%s', got '%s'",
                       i + 1, tstr(s, fn->params[i]), tstr(s, at));
        }
    }
    return fn->return_type;
}

static TypeInfo *check_index(Sema *s, ExprIndex *e) {
    TypeInfo *obj = check_expr(s, e->object);
    TypeInfo *idx = check_expr(s, e->index);

    if (!is_err(s, idx) && !type_is_integer(idx))
        sema_error(s, e->index->loc, "index must be an integer, got '%s'", tstr(s, idx));

    if (is_err(s, obj)) return s->types.t_error;
    switch (obj->kind) {
    case TYPE_ARRAY:   return TYPE_CAST_UNSAFE(obj, TyArray)->element;
    case TYPE_SLICE:   return TYPE_CAST_UNSAFE(obj, TySlice)->element;
    case TYPE_POINTER: return TYPE_CAST_UNSAFE(obj, TyPointer)->pointee;
    default:
        sema_error(s, e->base.loc, "cannot index a value of type '%s'", tstr(s, obj));
        return s->types.t_error;
    }
}

// enum-variant / namespace path:  Enum::Variant
static TypeInfo *check_path(Sema *s, ExprPath *e) {
    if (e->scope && NODE_IS(e->scope, NODE_EXPR_IDENT)) {
        StringView name = NODE_CAST_UNSAFE(e->scope, ExprIdent)->name;
        Symbol *sym = scope_lookup(s->current, name);
        if (sym && sym->kind == SYMBOL_TYPE && sym->type->kind == TYPE_ENUM) {
            return sym->type;  // Color::Red has type Color
        }
    }
    return s->types.t_error;  // namespaces / @cimport members: not checked yet
}

static TypeInfo *check_struct_lit(Sema *s, ExprStructLit *e) {
    for (size_t i = 0; i < e->field_count; i++) check_expr(s, e->field_values[i]);
    if (e->is_anonymous) return s->types.t_error;  // inferred later
    Symbol *sym = scope_lookup(s->current, e->type_name);
    if (sym && sym->kind == SYMBOL_TYPE) return sym->type;
    sema_error(s, e->base.loc, "unknown type '%.*s' in struct literal",
               (int)e->type_name.len, e->type_name.data);
    return s->types.t_error;
}

// ---------------------------------------------------------------------------
// check_expr — computes, records (node->type_info), and returns the type
// ---------------------------------------------------------------------------

static TypeInfo *check_expr(Sema *s, Node *expr) {
    if (!expr) return s->types.t_error;
    TypeInfo *t;

    switch (expr->kind) {
    case NODE_EXPR_INT_LIT:    t = s->types.t_i32; break;          // default int
    case NODE_EXPR_FLOAT_LIT:  t = s->types.t_f64; break;          // default float
    case NODE_EXPR_BOOL_LIT:   t = s->types.t_bool; break;
    case NODE_EXPR_STRING_LIT: t = type_pointer(&s->types, s->types.t_u8, false); break;
    case NODE_EXPR_NULL_LIT:   t = type_pointer(&s->types, s->types.t_void, true); break;

    case NODE_EXPR_IDENT: {
        ExprIdent *id = NODE_CAST_UNSAFE(expr, ExprIdent);
        Symbol *sym = scope_lookup(s->current, id->name);
        if (!sym) {
            sema_error(s, expr->loc, "use of undeclared identifier '%.*s'",
                       (int)id->name.len, id->name.data);
            t = s->types.t_error;
        } else {
            t = sym->type ? sym->type : s->types.t_error;
        }
        break;
    }

    case NODE_EXPR_BINARY_OP: t = check_binary(s, NODE_CAST_UNSAFE(expr, ExprBinaryOp)); break;
    case NODE_EXPR_UNARY_OP:  t = check_unary(s, NODE_CAST_UNSAFE(expr, ExprUnaryOp));  break;
    case NODE_EXPR_CALL:      t = check_call(s, NODE_CAST_UNSAFE(expr, ExprCall));      break;
    case NODE_EXPR_INDEX:     t = check_index(s, NODE_CAST_UNSAFE(expr, ExprIndex));    break;
    case NODE_EXPR_PATH:      t = check_path(s, NODE_CAST_UNSAFE(expr, ExprPath));      break;
    case NODE_EXPR_STRUCT_LIT:t = check_struct_lit(s, NODE_CAST_UNSAFE(expr, ExprStructLit)); break;

    case NODE_EXPR_CAST: {
        ExprCast *c = NODE_CAST_UNSAFE(expr, ExprCast);
        check_expr(s, c->expr);
        t = type_resolve(&s->types, s->current, c->target_type);
        break;
    }

    case NODE_EXPR_FIELD_ACCESS: {
        // Aggregate field types are not resolved yet (Pass 1 keeps structs
        // opaque), so member access stays an error for now — without spurious
        // diagnostics thanks to poison suppression.
        check_expr(s, NODE_CAST_UNSAFE(expr, ExprFieldAccess)->object);
        t = s->types.t_error;
        break;
    }

    case NODE_EXPR_POSTFIX:  t = check_expr(s, NODE_CAST_UNSAFE(expr, ExprPostfix)->expr); break;
    case NODE_EXPR_TRY:      t = check_expr(s, NODE_CAST_UNSAFE(expr, ExprTry)->expr);     break;
    case NODE_EXPR_CIMPORT:  t = s->types.t_error; break;

    default:
        t = s->types.t_error;
        break;
    }

    expr->type_info = t;
    return t;
}

// ---------------------------------------------------------------------------
// Statements
// ---------------------------------------------------------------------------

static void check_var_decl(Sema *s, StmtVarDecl *v) {
    TypeInfo *annotated = v->type_annotation
                        ? type_resolve(&s->types, s->current, v->type_annotation)
                        : NULL;
    TypeInfo *init = v->initializer ? check_expr(s, v->initializer) : NULL;

    TypeInfo *vt;
    if (annotated) {
        vt = annotated;
        if (init && !assignable(s, annotated, v->initializer, init)) {
            sema_error(s, v->base.loc, "cannot initialize '%.*s' of type '%s' with '%s'",
                       (int)v->name.len, v->name.data, tstr(s, annotated), tstr(s, init));
        }
    } else if (init) {
        vt = init;  // inferred from the initializer
    } else {
        sema_error(s, v->base.loc, "cannot infer type of '%.*s' without a type or initializer",
                   (int)v->name.len, v->name.data);
        vt = s->types.t_error;
    }

    v->base.type_info = vt;
    if (scope_lookup_local(s->current, v->name) != NULL) {
        sema_error(s, v->base.loc, "redefinition of '%.*s'",
                   (int)v->name.len, v->name.data);
    } else {
        scope_define(s->current, SYMBOL_VAR, v->name, vt, &v->base, v->is_const);
    }
}

static void check_assignment(Sema *s, StmtAssignment *a) {
    TypeInfo *target = check_expr(s, a->target);
    TypeInfo *value  = check_expr(s, a->value);

    if (NODE_IS(a->target, NODE_EXPR_IDENT)) {
        StringView name = NODE_CAST_UNSAFE(a->target, ExprIdent)->name;
        Symbol *sym = scope_lookup(s->current, name);
        if (sym && sym->kind == SYMBOL_VAR && sym->is_const) {
            sema_error(s, a->base.loc, "cannot assign to immutable binding '%.*s'",
                       (int)name.len, name.data);
        }
    }
    if (!assignable(s, target, a->value, value)) {
        sema_error(s, a->base.loc, "cannot assign '%s' to '%s'",
                   tstr(s, value), tstr(s, target));
    }
}

static void check_return(Sema *s, StmtReturn *r) {
    TypeInfo *ret = s->current_return ? s->current_return : s->types.t_void;
    if (r->value) {
        TypeInfo *vt = check_expr(s, r->value);
        if (!assignable(s, ret, r->value, vt)) {
            sema_error(s, r->base.loc, "return type mismatch: expected '%s', got '%s'",
                       tstr(s, ret), tstr(s, vt));
        }
    } else if (ret->kind != TYPE_VOID && !is_err(s, ret)) {
        sema_error(s, r->base.loc, "non-void function must return a '%s'", tstr(s, ret));
    }
}

static void check_stmt(Sema *s, Node *stmt) {
    if (!stmt) return;

    switch (stmt->kind) {
    case NODE_STMT_VAR_DECL:   check_var_decl(s, NODE_CAST_UNSAFE(stmt, StmtVarDecl)); break;
    case NODE_STMT_ASSIGNMENT: check_assignment(s, NODE_CAST_UNSAFE(stmt, StmtAssignment)); break;
    case NODE_STMT_RETURN:     check_return(s, NODE_CAST_UNSAFE(stmt, StmtReturn)); break;
    case NODE_STMT_EXPR:       check_expr(s, NODE_CAST_UNSAFE(stmt, StmtExprStmt)->expr); break;
    case NODE_STMT_BREAK:
    case NODE_STMT_CONTINUE:   break;

    case NODE_STMT_IF: {
        StmtIf *n = NODE_CAST_UNSAFE(stmt, StmtIf);
        expect_bool(s, n->condition, "if");
        check_body(s, n->then_body, n->then_len);
        check_body(s, n->else_body, n->else_len);
        break;
    }
    case NODE_STMT_WHILE: {
        StmtWhile *n = NODE_CAST_UNSAFE(stmt, StmtWhile);
        expect_bool(s, n->condition, "while");
        check_body(s, n->body, n->body_len);
        break;
    }
    case NODE_STMT_FOR: {
        StmtFor *n = NODE_CAST_UNSAFE(stmt, StmtFor);
        TypeInfo *it = check_expr(s, n->iter_expr);
        TypeInfo *elem = s->types.t_error;
        if (!is_err(s, it)) {
            if (it->kind == TYPE_ARRAY)      elem = TYPE_CAST_UNSAFE(it, TyArray)->element;
            else if (it->kind == TYPE_SLICE) elem = TYPE_CAST_UNSAFE(it, TySlice)->element;
        }
        Scope *prev = s->current;
        s->current = scope_new(s->arena, prev);
        scope_define(s->current, SYMBOL_VAR, n->var_name, elem, stmt, false);
        for (size_t i = 0; i < n->body_len; i++) check_stmt(s, n->body[i]);
        s->current = prev;
        break;
    }
    case NODE_STMT_BLOCK: {
        StmtBlock *n = NODE_CAST_UNSAFE(stmt, StmtBlock);
        check_body(s, n->statements, n->stmt_count);
        break;
    }
    case NODE_STMT_DEFER:
        check_stmt(s, NODE_CAST_UNSAFE(stmt, StmtDefer)->body);
        break;

    case NODE_STMT_MATCH: {
        StmtMatch *n = NODE_CAST_UNSAFE(stmt, StmtMatch);
        check_expr(s, n->subject);
        for (size_t i = 0; i < n->arm_count; i++) {
            // Pattern bindings are not typed yet; just check arm bodies.
            check_body(s, n->arms[i].body, n->arms[i].body_len);
        }
        break;
    }
    case NODE_STMT_WHEN: {
        StmtWhen *n = NODE_CAST_UNSAFE(stmt, StmtWhen);
        for (size_t i = 0; i < n->branch_count; i++) {
            expect_bool(s, n->branches[i].condition, "when");  // NULL (else) is ignored
            check_body(s, n->branches[i].body, n->branches[i].body_len);
        }
        break;
    }

    default:
        break;
    }
}

static void check_body(Sema *s, Node **body, size_t n) {
    Scope *prev = s->current;
    s->current = scope_new(s->arena, prev);
    for (size_t i = 0; i < n; i++) check_stmt(s, body[i]);
    s->current = prev;
}

// ---------------------------------------------------------------------------
// Functions / driver
// ---------------------------------------------------------------------------

static void check_function(Sema *s, DeclFunction *fn) {
    Scope *prev = s->current;
    Scope *fscope = scope_new(s->arena, s->global);
    s->current = fscope;

    for (size_t i = 0; i < fn->param_count; i++) {
        Param *p = &fn->params[i];
        if (p->is_variadic) continue;
        // A `comptime T: type` parameter introduces a type name.
        if (p->is_comptime && p->type && NODE_IS(p->type, NODE_TYPE_PRIMITIVE) &&
            sv_eq(NODE_CAST_UNSAFE(p->type, TypePrimitive)->name, "type")) {
            scope_define(fscope, SYMBOL_TYPE, p->name, s->types.t_error, NULL, false);
            continue;
        }
        TypeInfo *pt = type_resolve(&s->types, s->global, p->type);
        scope_define(fscope, SYMBOL_VAR, p->name, pt, NULL, false);
    }

    s->current_return = fn->return_type
                      ? type_resolve(&s->types, s->global, fn->return_type)
                      : s->types.t_void;

    for (size_t i = 0; i < fn->body_len; i++) check_stmt(s, fn->body[i]);

    s->current_return = NULL;
    s->current = prev;
}

void sema_check_bodies(Sema *s, Program *program) {
    for (size_t i = 0; i < program->decl_count; i++) {
        Node *d = program->decls[i];
        if (d && d->kind == NODE_DECL_FUNCTION && !NODE_CAST_UNSAFE(d, DeclFunction)->is_extern) {
            check_function(s, NODE_CAST_UNSAFE(d, DeclFunction));
        }
    }
}

void sema_check(Sema *s, Program *program) {
    sema_collect(s, program);
    sema_check_bodies(s, program);
}
