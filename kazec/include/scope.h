#ifndef KAZE_SCOPE_H
#define KAZE_SCOPE_H

#include <stdbool.h>

#include "../../utils/include/arena.h"
#include "../../utils/include/hashmap.h"
#include "../../utils/include/string_view.h"
#include "ast.h"    // Node
#include "type.h"   // TypeInfo

// ============================================================================
// Symbol — a named binding recorded in a scope
// ============================================================================

typedef enum SymbolKind {
    SYMBOL_VAR,       // var / let / const binding, or a function parameter
    SYMBOL_FUNCTION,  // fn declaration
    SYMBOL_TYPE,      // struct / enum / union / type alias — a name denoting a type
} SymbolKind;

typedef struct Symbol {
    SymbolKind  kind;
    StringView  name;
    TypeInfo   *type;     // VAR: its type; FUNCTION: its fn type; TYPE: the denoted type
    Node       *decl;     // the declaring AST node (for diagnostics / back-reference)
    bool        is_const; // VAR only: immutable binding (let / const)
} Symbol;

const char *symbol_kind_to_string(SymbolKind kind);

// ============================================================================
// Scope — a single lexical scope; chained to its parent for name resolution
// ============================================================================

typedef struct Scope Scope;
struct Scope {
    Scope  *parent;     // NULL for the global scope
    HashMap symbols;    // interned name (cstr) -> Symbol*
    Arena  *arena;
};

// Create a new scope nested under `parent` (NULL for the global scope).
Scope *scope_new(Arena *arena, Scope *parent);

// Define `name` in `scope`. Allocates and returns the new Symbol*, or returns
// NULL if `name` is already defined in THIS scope (redefinition — the caller
// reports the error; the existing binding is left untouched). Shadowing a name
// from an enclosing scope is allowed and is not a redefinition.
Symbol *scope_define(Scope *scope, SymbolKind kind, StringView name,
                     TypeInfo *type, Node *decl, bool is_const);

// Look up `name` only in `scope` (no parent walk). NULL if absent.
Symbol *scope_lookup_local(Scope *scope, StringView name);

// Look up `name` in `scope` and then its ancestors. NULL if undefined.
Symbol *scope_lookup(Scope *scope, StringView name);

#endif /* KAZE_SCOPE_H */
