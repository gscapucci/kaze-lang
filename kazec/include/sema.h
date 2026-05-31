#ifndef KAZE_SEMA_H
#define KAZE_SEMA_H

#include <stdbool.h>

#include "../../utils/include/arena.h"
#include "ast.h"
#include "type.h"
#include "scope.h"
#include "lexer.h"   // ErrorVec

// ============================================================================
// Sema — semantic analysis state, threaded through the passes
// ============================================================================

typedef struct Sema {
    Arena      *arena;
    const char *filename;
    TypeContext types;          // canonical primitives + composite allocation
    Scope      *global;         // module-level scope (top-level declarations)
    Scope      *current;        // scope currently being analysed
    TypeInfo   *current_return; // return type of the function being checked (NULL outside)
    ErrorVec    errors;         // location-prefixed diagnostics
    bool        had_error;
} Sema;

void sema_init(Sema *s, Arena *arena, const char *filename);

// Pass 1 — collect every top-level declaration into the global scope (types as
// SYMBOL_TYPE, functions as SYMBOL_FUNCTION with a resolved signature, globals
// as SYMBOL_VAR). Names are registered before signatures/alias targets are
// resolved, so declarations may reference one another in any order. Reports
// redefinitions. Does not descend into function bodies (that is Pass 2).
void sema_collect(Sema *s, Program *program);

// Pass 2 — type-check every top-level function body: opens a scope per function
// (parameters bound as locals) and nested scopes per block, resolves names,
// infers/checks expression types (filling node->type_info), and enforces the
// core rules (operand types, call arity/args, return vs signature, bool
// conditions, assignment to const). The error type is a poison that suppresses
// cascading diagnostics. Member access on aggregates and pattern types are not
// yet checked. Iterates top-level functions only (struct methods are deferred).
void sema_check_bodies(Sema *s, Program *program);

// Convenience: run Pass 1 then Pass 2.
void sema_check(Sema *s, Program *program);

#endif /* KAZE_SEMA_H */
