#include "../include/type.h"
#include "../include/scope.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

// ============================================================================
// Type size table — indexed by TypeKind (mirrors ast.c's node_sizes)
// ============================================================================

static const size_t type_sizes[TYPE_KIND_COUNT] = {
    [TYPE_ERROR]    = sizeof(TypeInfo),
    [TYPE_VOID]     = sizeof(TypeInfo),
    [TYPE_BOOL]     = sizeof(TypeInfo),
    [TYPE_TYPE]     = sizeof(TypeInfo),
    [TYPE_INT]      = sizeof(TyInt),
    [TYPE_FLOAT]    = sizeof(TyFloat),
    [TYPE_POINTER]  = sizeof(TyPointer),
    [TYPE_ARRAY]    = sizeof(TyArray),
    [TYPE_SLICE]    = sizeof(TySlice),
    [TYPE_FUNCTION] = sizeof(TyFunction),
    [TYPE_STRUCT]   = sizeof(TyAggregate),
    [TYPE_ENUM]     = sizeof(TyAggregate),
    [TYPE_UNION]    = sizeof(TyAggregate),
};

// ============================================================================
// Small helpers
// ============================================================================

static bool sv_eq_cstr(StringView sv, const char *s) {
    size_t n = strlen(s);
    return sv.len == n && memcmp(sv.data, s, n) == 0;
}

// ============================================================================
// type_alloc
// ============================================================================

TypeInfo *type_alloc(TypeContext *ctx, TypeKind kind) {
    assert(kind < TYPE_KIND_COUNT);
    TypeInfo *t = (TypeInfo *)arena_alloc_zeroed(ctx->arena, type_sizes[kind]);
    t->kind = kind;
    return t;
}

// ============================================================================
// type_context_init
// ============================================================================

static TypeInfo *make_int(TypeContext *ctx, uint16_t bits, bool is_signed) {
    TyInt *t = TYPE_CAST_UNSAFE(type_alloc(ctx, TYPE_INT), TyInt);
    t->bits = bits;
    t->is_signed = is_signed;
    return &t->base;
}

static TypeInfo *make_float(TypeContext *ctx, uint16_t bits) {
    TyFloat *t = TYPE_CAST_UNSAFE(type_alloc(ctx, TYPE_FLOAT), TyFloat);
    t->bits = bits;
    return &t->base;
}

void type_context_init(TypeContext *ctx, Arena *arena) {
    ctx->arena = arena;

    ctx->t_error = type_alloc(ctx, TYPE_ERROR);
    ctx->t_void  = type_alloc(ctx, TYPE_VOID);
    ctx->t_bool  = type_alloc(ctx, TYPE_BOOL);
    ctx->t_type  = type_alloc(ctx, TYPE_TYPE);

    ctx->t_i8  = make_int(ctx, 8,  true);
    ctx->t_i16 = make_int(ctx, 16, true);
    ctx->t_i32 = make_int(ctx, 32, true);
    ctx->t_i64 = make_int(ctx, 64, true);

    ctx->t_u8  = make_int(ctx, 8,  false);
    ctx->t_u16 = make_int(ctx, 16, false);
    ctx->t_u32 = make_int(ctx, 32, false);
    ctx->t_u64 = make_int(ctx, 64, false);

    ctx->t_f32 = make_float(ctx, 32);
    ctx->t_f64 = make_float(ctx, 64);
}

// ============================================================================
// Construction
// ============================================================================

TypeInfo *type_pointer(TypeContext *ctx, TypeInfo *pointee, bool is_mutable) {
    TyPointer *t = TYPE_CAST_UNSAFE(type_alloc(ctx, TYPE_POINTER), TyPointer);
    t->pointee = pointee;
    t->is_mutable = is_mutable;
    return &t->base;
}

TypeInfo *type_array(TypeContext *ctx, TypeInfo *element, uint64_t length) {
    TyArray *t = TYPE_CAST_UNSAFE(type_alloc(ctx, TYPE_ARRAY), TyArray);
    t->element = element;
    t->length = length;
    return &t->base;
}

TypeInfo *type_slice(TypeContext *ctx, TypeInfo *element) {
    TySlice *t = TYPE_CAST_UNSAFE(type_alloc(ctx, TYPE_SLICE), TySlice);
    t->element = element;
    return &t->base;
}

TypeInfo *type_function(TypeContext *ctx, TypeInfo **params, size_t param_count,
                        TypeInfo *return_type, bool is_variadic) {
    TyFunction *t = TYPE_CAST_UNSAFE(type_alloc(ctx, TYPE_FUNCTION), TyFunction);
    t->params = params;
    t->param_count = param_count;
    t->return_type = return_type;
    t->is_variadic = is_variadic;
    return &t->base;
}

// ============================================================================
// type_primitive_from_name
// ============================================================================

TypeInfo *type_primitive_from_name(TypeContext *ctx, StringView name) {
    if (sv_eq_cstr(name, "void")) return ctx->t_void;
    if (sv_eq_cstr(name, "bool")) return ctx->t_bool;
    if (sv_eq_cstr(name, "type")) return ctx->t_type;

    if (sv_eq_cstr(name, "i8"))  return ctx->t_i8;
    if (sv_eq_cstr(name, "i16")) return ctx->t_i16;
    if (sv_eq_cstr(name, "i32")) return ctx->t_i32;
    if (sv_eq_cstr(name, "i64")) return ctx->t_i64;

    if (sv_eq_cstr(name, "u8"))  return ctx->t_u8;
    if (sv_eq_cstr(name, "u16")) return ctx->t_u16;
    if (sv_eq_cstr(name, "u32")) return ctx->t_u32;
    if (sv_eq_cstr(name, "u64")) return ctx->t_u64;

    if (sv_eq_cstr(name, "f32")) return ctx->t_f32;
    if (sv_eq_cstr(name, "f64")) return ctx->t_f64;

    return NULL;  // user-defined type name — needs the symbol table
}

// ============================================================================
// type_resolve
// ============================================================================

TypeInfo *type_resolve(TypeContext *ctx, Scope *scope, Node *type_node) {
    if (!type_node) return ctx->t_error;

    switch (type_node->kind) {
    case NODE_TYPE_PRIMITIVE: {
        // Holds both builtin primitives ("i32") and named user types (parser
        // reuses TypePrimitive's name slot for the latter).
        TypePrimitive *n = NODE_CAST_UNSAFE(type_node, TypePrimitive);
        TypeInfo *prim = type_primitive_from_name(ctx, n->name);
        if (prim) return prim;
        // Named user type — resolve through the symbol table when available.
        if (scope) {
            Symbol *sym = scope_lookup(scope, n->name);
            if (sym && sym->kind == SYMBOL_TYPE) return sym->type;
        }
        return ctx->t_error;
    }

    case NODE_TYPE_POINTER: {
        TypePointer *n = NODE_CAST_UNSAFE(type_node, TypePointer);
        return type_pointer(ctx, type_resolve(ctx, scope, n->pointee), n->is_mutable);
    }

    case NODE_TYPE_ARRAY: {
        TypeArray *n = NODE_CAST_UNSAFE(type_node, TypeArray);
        TypeInfo *elem = type_resolve(ctx, scope, n->element_type);
        if (n->is_slice) return type_slice(ctx, elem);
        // Length is an expression; only constant int literals are known here.
        // Non-literal lengths await comptime evaluation in sema (length = 0).
        uint64_t length = 0;
        if (n->length && NODE_IS(n->length, NODE_EXPR_INT_LIT)) {
            length = (uint64_t)NODE_CAST_UNSAFE(n->length, ExprIntLit)->value;
        }
        return type_array(ctx, elem, length);
    }

    case NODE_TYPE_FUNCTION: {
        TypeFunction *n = NODE_CAST_UNSAFE(type_node, TypeFunction);
        TypeInfo **params = NULL;
        if (n->param_count > 0) {
            params = arena_alloc(ctx->arena, n->param_count * sizeof(TypeInfo *));
            for (size_t i = 0; i < n->param_count; i++) {
                params[i] = type_resolve(ctx, scope, n->param_types[i]);
            }
        }
        return type_function(ctx, params, n->param_count,
                             type_resolve(ctx, scope, n->return_type), false);
    }

    case NODE_TYPE_GENERIC:
        // Generic instances need the symbol table; sema resolves these.
        return ctx->t_error;

    default:
        return ctx->t_error;
    }
}

// ============================================================================
// Queries
// ============================================================================

bool type_is_integer(const TypeInfo *t) { return t && t->kind == TYPE_INT; }
bool type_is_float(const TypeInfo *t)   { return t && t->kind == TYPE_FLOAT; }
bool type_is_bool(const TypeInfo *t)    { return t && t->kind == TYPE_BOOL; }
bool type_is_error(const TypeInfo *t)   { return t && t->kind == TYPE_ERROR; }
bool type_is_numeric(const TypeInfo *t) {
    return type_is_integer(t) || type_is_float(t);
}

bool type_equals(const TypeInfo *a, const TypeInfo *b) {
    if (a == b) return true;          // canonical primitives compare by pointer
    if (!a || !b) return false;
    if (a->kind != b->kind) return false;

    switch (a->kind) {
    case TYPE_INT: {
        const TyInt *x = TYPE_CAST_UNSAFE(a, TyInt);
        const TyInt *y = TYPE_CAST_UNSAFE(b, TyInt);
        return x->bits == y->bits && x->is_signed == y->is_signed;
    }
    case TYPE_FLOAT:
        return TYPE_CAST_UNSAFE(a, TyFloat)->bits == TYPE_CAST_UNSAFE(b, TyFloat)->bits;

    case TYPE_POINTER: {
        const TyPointer *x = TYPE_CAST_UNSAFE(a, TyPointer);
        const TyPointer *y = TYPE_CAST_UNSAFE(b, TyPointer);
        return x->is_mutable == y->is_mutable && type_equals(x->pointee, y->pointee);
    }
    case TYPE_ARRAY: {
        const TyArray *x = TYPE_CAST_UNSAFE(a, TyArray);
        const TyArray *y = TYPE_CAST_UNSAFE(b, TyArray);
        return x->length == y->length && type_equals(x->element, y->element);
    }
    case TYPE_SLICE:
        return type_equals(TYPE_CAST_UNSAFE(a, TySlice)->element,
                           TYPE_CAST_UNSAFE(b, TySlice)->element);

    case TYPE_FUNCTION: {
        const TyFunction *x = TYPE_CAST_UNSAFE(a, TyFunction);
        const TyFunction *y = TYPE_CAST_UNSAFE(b, TyFunction);
        if (x->param_count != y->param_count) return false;
        if (x->is_variadic != y->is_variadic) return false;
        if (!type_equals(x->return_type, y->return_type)) return false;
        for (size_t i = 0; i < x->param_count; i++) {
            if (!type_equals(x->params[i], y->params[i])) return false;
        }
        return true;
    }

    case TYPE_STRUCT:
    case TYPE_ENUM:
    case TYPE_UNION:
        // Nominal: same type iff same declaration.
        return TYPE_CAST_UNSAFE(a, TyAggregate)->decl ==
               TYPE_CAST_UNSAFE(b, TyAggregate)->decl;

    default:
        return true;  // ERROR/VOID/BOOL/TYPE: identity by kind (already equal)
    }
}

// ============================================================================
// type_to_string  /  type_kind_to_string
// ============================================================================

const char *type_kind_to_string(TypeKind kind) {
    switch (kind) {
    case TYPE_ERROR:    return "error";
    case TYPE_VOID:     return "void";
    case TYPE_BOOL:     return "bool";
    case TYPE_TYPE:     return "type";
    case TYPE_INT:      return "int";
    case TYPE_FLOAT:    return "float";
    case TYPE_POINTER:  return "pointer";
    case TYPE_ARRAY:    return "array";
    case TYPE_SLICE:    return "slice";
    case TYPE_FUNCTION: return "function";
    case TYPE_STRUCT:   return "struct";
    case TYPE_ENUM:     return "enum";
    case TYPE_UNION:    return "union";
    default:            return "<?>";
    }
}

// Bounded append into buf[*len .. cap).
static void ts_put(char *buf, size_t cap, size_t *len, const char *s) {
    while (*s && *len + 1 < cap) buf[(*len)++] = *s++;
    buf[*len] = '\0';
}

static void ts_write(const TypeInfo *t, char *buf, size_t cap, size_t *len) {
    if (!t) { ts_put(buf, cap, len, "<null>"); return; }

    char tmp[32];
    switch (t->kind) {
    case TYPE_VOID:  ts_put(buf, cap, len, "void"); break;
    case TYPE_BOOL:  ts_put(buf, cap, len, "bool"); break;
    case TYPE_TYPE:  ts_put(buf, cap, len, "type"); break;
    case TYPE_ERROR: ts_put(buf, cap, len, "<error>"); break;

    case TYPE_INT: {
        const TyInt *x = TYPE_CAST_UNSAFE(t, TyInt);
        snprintf(tmp, sizeof(tmp), "%c%u", x->is_signed ? 'i' : 'u', x->bits);
        ts_put(buf, cap, len, tmp);
        break;
    }
    case TYPE_FLOAT:
        snprintf(tmp, sizeof(tmp), "f%u", TYPE_CAST_UNSAFE(t, TyFloat)->bits);
        ts_put(buf, cap, len, tmp);
        break;

    case TYPE_POINTER: {
        const TyPointer *x = TYPE_CAST_UNSAFE(t, TyPointer);
        ts_put(buf, cap, len, x->is_mutable ? "*" : "*const ");
        ts_write(x->pointee, buf, cap, len);
        break;
    }
    case TYPE_ARRAY: {
        const TyArray *x = TYPE_CAST_UNSAFE(t, TyArray);
        snprintf(tmp, sizeof(tmp), "[%llu]", (unsigned long long)x->length);
        ts_put(buf, cap, len, tmp);
        ts_write(x->element, buf, cap, len);
        break;
    }
    case TYPE_SLICE:
        ts_put(buf, cap, len, "[]");
        ts_write(TYPE_CAST_UNSAFE(t, TySlice)->element, buf, cap, len);
        break;

    case TYPE_FUNCTION: {
        const TyFunction *x = TYPE_CAST_UNSAFE(t, TyFunction);
        ts_put(buf, cap, len, "fn(");
        for (size_t i = 0; i < x->param_count; i++) {
            if (i) ts_put(buf, cap, len, ", ");
            ts_write(x->params[i], buf, cap, len);
        }
        ts_put(buf, cap, len, ")->");
        ts_write(x->return_type, buf, cap, len);
        break;
    }

    case TYPE_STRUCT:
    case TYPE_ENUM:
    case TYPE_UNION: {
        const TyAggregate *x = TYPE_CAST_UNSAFE(t, TyAggregate);
        snprintf(tmp, sizeof(tmp), "%.*s", (int)x->name.len, x->name.data);
        ts_put(buf, cap, len, tmp);
        break;
    }

    default:
        ts_put(buf, cap, len, "<?>");
        break;
    }
}

const char *type_to_string(const TypeInfo *t, Arena *arena) {
    enum { CAP = 256 };
    char *buf = arena_alloc(arena, CAP);
    size_t len = 0;
    buf[0] = '\0';
    ts_write(t, buf, CAP, &len);
    return buf;
}
