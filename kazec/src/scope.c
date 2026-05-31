#include "../include/scope.h"

#include <string.h>

// Longest name we will copy onto the stack for a lookup key. Identifiers this
// long are never defined (the lexer caps keyword-lookup at 63 chars), so a
// miss here is a genuine "not found".
#define SCOPE_NAME_MAX 256

// ============================================================================
// Helpers
// ============================================================================

const char *symbol_kind_to_string(SymbolKind kind) {
    switch (kind) {
    case SYMBOL_VAR:      return "var";
    case SYMBOL_FUNCTION: return "function";
    case SYMBOL_TYPE:     return "type";
    default:              return "<?>";
    }
}

// Intern `sv` as a stable, null-terminated key in the arena (the hashmap stores
// the pointer and compares with strcmp, so keys must outlive the map and be
// '\0'-terminated).
static const char *intern(Arena *arena, StringView sv) {
    char *s = arena_alloc(arena, sv.len + 1);
    memcpy(s, sv.data, sv.len);
    s[sv.len] = '\0';
    return s;
}

// Copy `sv` into `buf` (null-terminated) for use as a transient lookup key.
// Returns false when the name is too long to be a defined symbol.
static bool key_on_stack(char buf[SCOPE_NAME_MAX], StringView sv) {
    if (sv.len >= SCOPE_NAME_MAX) return false;
    memcpy(buf, sv.data, sv.len);
    buf[sv.len] = '\0';
    return true;
}

// ============================================================================
// API
// ============================================================================

Scope *scope_new(Arena *arena, Scope *parent) {
    Scope *scope = arena_alloc(arena, sizeof(Scope));
    scope->parent  = parent;
    scope->arena   = arena;
    scope->symbols = hashmap_create(arena, 0);  // clamps to a minimum size
    return scope;
}

Symbol *scope_define(Scope *scope, SymbolKind kind, StringView name,
                     TypeInfo *type, Node *decl, bool is_const) {
    if (scope_lookup_local(scope, name) != NULL) {
        return NULL;  // redefinition in this scope
    }

    Symbol *sym = arena_alloc(scope->arena, sizeof(Symbol));
    sym->kind     = kind;
    sym->name     = name;
    sym->type     = type;
    sym->decl     = decl;
    sym->is_const = is_const;

    hashmap_put(&scope->symbols, intern(scope->arena, name), sym);
    return sym;
}

Symbol *scope_lookup_local(Scope *scope, StringView name) {
    char buf[SCOPE_NAME_MAX];
    if (!key_on_stack(buf, name)) return NULL;
    return (Symbol *)hashmap_get(&scope->symbols, buf);
}

Symbol *scope_lookup(Scope *scope, StringView name) {
    for (Scope *s = scope; s != NULL; s = s->parent) {
        Symbol *sym = scope_lookup_local(s, name);
        if (sym) return sym;
    }
    return NULL;
}
