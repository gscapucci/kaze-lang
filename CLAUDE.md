# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## What This Is

Kaze is a systems programming language compiler written in C. The compiler (`kazec`) currently targets C code generation. The formal grammar is in `grammar.ebnf`, example syntax is in `test.kz`, and design notes are in `langspec.md`.

## Build System

The project uses [nob](https://github.com/tsoding/nob.h) — a single-header C build system. The root `nob.c` orchestrates sub-builds in `utils/` and `kazec/`, then links everything into `./build/kazec`.

```bash
# Bootstrap the build system (first time only)
gcc nob.c -o nob

# Build the compiler
./nob

# Build and run on a .kz file (dumps tokens)
./nob run <file.kz>

# Clean build artifacts
./nob clean
```

Each subdirectory (`utils/`, `kazec/`) has its own `nob.c` that compiles its sources to `.o` files in its `build/` folder. The root `nob.c` invokes these sub-scripts and then links all `.o` files together.

## Compiler CLI (`./build/kazec`)

```bash
./build/kazec <file.kz>             # compile
./build/kazec --dump-tokens <file>  # print token stream and exit
./build/kazec --dump-ast <file>     # print AST and exit
./build/kazec --dump-types <file>   # type-check, print each top-level symbol's resolved type
./build/kazec --dump-c <file>       # print generated C (not yet implemented)
./build/kazec -o <output> <file>    # set output file (default: out.c)
./build/kazec -h                    # help
./build/kazec -v                    # version
```

## Architecture

```
grammar.ebnf    — formal EBNF grammar of the full language
utils/          — reusable low-level library (arena allocator, hashmap, string_view, generic vector)
kazec/          — the compiler
  main.c        — entry point: parse opts → compile()
  src/
    compiler_opt.c  — CLI argument parsing, CompilerOpt struct, flag bitmask (OPT_*)
    compiler.c      — top-level compile(): reads file → lexer → parser → sema → dumps
    lexer.c         — tokenizer, produces TokenVec + ErrorVec
    token.c         — Token/TokenType definitions
    parser.c        — recursive-descent parser, produces Program (AST) + ErrorVec
    ast.c           — AST node allocation, printing, cast helpers
    type.c          — type system: TypeInfo, TypeContext, resolve/equals/to_string
    scope.c         — symbol table: Symbol, Scope (parent-chained), define/lookup
    sema.c          — semantic analysis: Sema state + Pass 1 (collect) + Pass 2 (check)
  include/      — headers mirroring src/
test/           — test suite
  test.h/test.c — framework: TestSuite, TestCase, ASSERT macro
  main.c        — registers and runs all suites
  utils/        — hashmap tests
  kazec/        — lexer, AST, and parser tests
```

**Data flow (current):** source file → `read_file()` → `lexer_init()` + `lexer_get_tokens()` → `TokenVec` → `parser_init()` + `parse_program()` → `Program` (AST). `--dump-tokens` prints the token stream; `--dump-ast` prints the AST. `compile()` now runs the full front end: lexer → parser → semantic analysis (`sema_check`, both passes). Lexer and parser errors abort with exit 1; sema errors (location-prefixed) likewise. `--dump-types` type-checks and prints each top-level symbol with its resolved type. Code generation is not yet implemented (`--dump-c` is still a no-op).

**Parser:** recursive-descent (`kazec/src/parser.c`). Covers the subset of the grammar that the AST can represent: top-level decls (`fn` with `@extern`/`@inline` attributes, `var`/`let`/`const`, `const X = struct/enum`, type alias, `import`), statements, the full expression precedence ladder, and types. Covers `match` (all pattern kinds: wildcard, bind, literal, `Enum::Variant` path, tuple, struct), `when`/`else when`/`else` (top-level and in-function), `union`, `defer`, `try expr`, error-unwrap `expr!` (an `ExprPostfix` with op `"!"`), struct literals `T{ .x = .. }`, struct methods (stored in `DeclStruct.methods`), and rich enum variants (plain/discriminant/tuple/struct). Error handling beyond `try`/`!` uses stdlib functions — `catch` is **not** a keyword (e.g. `catch(parse(x), 0)` parses as an ordinary call). `@cimport("h", .{ .defines = {...} })` parses into `ExprCImport` (namespace struct accessed with `::`); the `defines` option is captured, others ignored. Compound assignment (`x += y`) is desugared to `x = x + y` since `StmtAssignment` has no operator field. Struct-literal vs block ambiguity is resolved by requiring `{` to be followed by `.` (a field initializer).

Not yet handled: generic parameters (`const List(T) = ...`; the `is_generic`/`generic_params` AST fields exist but the parser does not populate them). Errors are collected in `parser.errors` (location-prefixed) and `parser.had_error`; parsing recovers via `synchronize()`.

**Type system (`kazec/src/type.c`):** mirrors the AST's embedded-base "inheritance" — a base `TypeInfo { TypeKind kind; }` and one concrete struct per kind (`TyInt`, `TyFloat`, `TyPointer`, `TyArray`, `TySlice`, `TyFunction`, `TyAggregate` for struct/enum/union), with `TYPE_CAST`/`TYPE_IS`/`TYPE_CAST_UNSAFE` macros and a `type_alloc` size table. `TypeContext` owns the canonical primitive singletons (comparable by pointer). `type_resolve(ctx, scope, node)` turns an AST type node into a `TypeInfo`, resolving named user types through the optional `scope` (NULL → named/generic become `t_error`). `type_equals` is structural (nominal for aggregates, by `decl` pointer); `type_to_string` renders diagnostics. There is no `union` inside a struct anywhere in this codebase — always use the embedded-base pattern.

**Symbol table (`kazec/src/scope.c`):** `Scope` is a parent-chained lexical scope holding a `HashMap` of interned name → `Symbol*` (`SYMBOL_VAR`/`FUNCTION`/`TYPE`). `scope_define` rejects redefinition in the same scope (returns NULL) but allows shadowing across scopes; `scope_lookup_local` checks one scope, `scope_lookup` walks ancestors. Names are interned as `\0`-terminated arena copies because the hashmap stores key pointers and compares with `strcmp`.

**Sema (`kazec/src/sema.c`):** `Sema` bundles the `TypeContext`, global/current `Scope`, and an `ErrorVec`. **Pass 1** (`sema_collect`) runs in three phases so declarations can reference each other in any order: (A) register all nominal type names (struct/enum/union as `TyAggregate`, alias names reserved with a placeholder), (B) resolve type-alias targets, (C) resolve function signatures (`TyFunction`) and global `const`/`var` annotations — defining `SYMBOL_FUNCTION`/`SYMBOL_VAR` and linking each `TypeInfo` back onto its decl's `node->type_info`. It does not descend into function bodies or resolve struct field layouts. Redefinitions are reported via `sema_error` (same location-prefixed format as `parser_error`). Limitation: alias-to-alias chains resolve in declaration order, so an alias pointing at a later-declared alias gets `t_error`. **Pass 2** (`sema_check_bodies`, or `sema_check` for both passes) type-checks each top-level function body — a scope per function (params bound as locals, `comptime T: type` bound as a type name) and a nested scope per block — resolving identifiers, inferring/checking expression types (writing each `node->type_info`), and enforcing: operand types for arithmetic/bitwise/logical/comparison operators, unary `&`/`*`/`!`/`~`, call arity (variadic-aware) and argument types, `return` vs the function's return type, `bool` conditions on `if`/`while`/`when`, indexing of arrays/slices/pointers, local var inference and annotation match, and assignment to immutable (`let`/`const`) bindings. The error type is a **poison**: when an operand is already an error, dependent checks are skipped to avoid cascades. Integer literals default to `i32`, floats to `f64`, strings to `*const u8`, `null` to `*void`, but an untyped numeric **literal** coerces to any matching numeric target via `assignable()` (so `var x: i64 = 2`, `g(3)` against an `i64` param, and `return`/assignment all accept a literal of the right numeric class, adopting the target type onto the literal node). Non-literals must match exactly — there is no implicit widening between named values. Not yet checked (yields the error type without diagnostics): struct/union field access (`x.field`), match-pattern binding types, generics, `@cimport` members, and full error/`try` propagation. Struct methods (inside `DeclStruct.methods`) and `@extern` function bodies are skipped.

**Memory:** everything allocates through an `Arena *` passed down from `compile()`. The arena is freed at the end of compilation. Vectors (`TokenVec`, `ErrorVec`) use `gvector.h` macros (`VECTOR_DEFINITION` / `VECTOR_IMPLEMENTATION`) backed by the arena's reallocable allocator.

**Keyword lookup:** the lexer builds a `HashMap` at init time mapping keyword strings → `TokenType`. The map is arena-allocated.

## AST

All node types are defined in `kazec/include/ast.h`. Every concrete node embeds `Node` as its first field (C "inheritance" via pointer casting). `ast_alloc_node(arena, kind, loc)` allocates a zeroed node of the correct size for the given `NodeKind`.

Cast macros:
- `NODE_CAST(node, type, expected_kind)` — returns `NULL` if kind mismatches
- `NODE_CAST_ANY(node, type, k1, k2, ...)` — accepts multiple valid kinds
- `NODE_IS(node, k)` — kind predicate
- `NODE_CAST_UNSAFE(node, type)` — unchecked, only when kind is already known

Node categories: expressions (17), statements (13), declarations (6), types (5), patterns (6). The `Program` struct is the module root; it holds `Node **decls`. Enum variants are not nodes — `DeclEnum` holds an `EnumVariant[]` (kind = plain/discriminant/tuple/struct); match arms are `MatchArm[]` and `when` branches are `WhenBranch[]` (plain structs, not nodes).

## Tests

```bash
./nob test   # build and run all suites
```

Eight suites: `HashMap`, `Lexer`, `AST`, `Parser`, `Type`, `Scope`, `Sema`, `KzFiles`. Each test function returns `bool`; the framework counts passes/failures per suite. To add a new suite: implement `TestSuite foo_suite(void)`, register it in `test/main.c`, and add the source/object paths to `build_tests()` in the root `nob.c`.

The `KzFiles` suite lives directly in `test/main.c`: it scans `test/kz/*.kz`, lexes + parses each file, and fails on any lexer/parser error. Drop a new `NNN_name.kz` into `test/kz/` and it is picked up automatically (run from the repo root, the suite's working directory). These files double as runnable examples of currently-parseable syntax.

Test coverage summary:
- **HashMap** — create, put/get (100 entries), contains, iteration, overwrite same key, missing key, empty iteration, `is_empty`, size-0 init
- **Lexer** — empty input, int/float literals, operators (single/double/triple char), keywords, identifiers, string literals, comments (line and block), source locations, column tracking after multi-char ops, unterminated string, unterminated block comment, unknown characters, float with trailing dot, long identifiers
- **AST** — allocation of all 47 node kinds, source location, all expression/statement/declaration/type nodes, patterns, `Program` root, cast macros, `node_kind_to_string`, tree traversal (depth, count, DFS)
- **Parser** — empty program, var/let/const decls, functions (attributes, comptime params), expression precedence, if/else, while, for-in, struct (+ methods), enum (plain/tuple/algebraic), union, type alias, imports (plain + selective), postfix chains (`.`/`[]`/`::`), `@cimport` with `.defines`, struct literals, `match` (all pattern kinds), `when`, `defer`, `try`/`!`, compound-assignment desugaring, error recording
- **Type** — `type_context_init` (primitive bits/signedness), `type_alloc` all kinds, `type_primitive_from_name` (builtins + unknown→NULL), `type_resolve` (primitive/pointer/`*const`/array/slice/function; generic/named/NULL→error), `type_equals` (primitive, pointer mutability, array length, function param/return/variadic, nominal aggregate by decl), queries (`is_integer`/`float`/`numeric`/`bool`/`error`), `type_to_string`
- **Scope** — `scope_new` (parent chaining, empty), define + lookup, redefinition rejected (original kept), `scope_lookup_local` vs parent-chain `scope_lookup`, shadowing across scopes, undefined names, Symbol field storage (kind/name/type/decl/is_const), sibling-scope isolation, `symbol_kind_to_string`
- **Sema** — Pass 1 (`sema_collect`): functions as `SYMBOL_FUNCTION` with resolved `TyFunction` signatures, structs/enums/unions as `SYMBOL_TYPE`, type aliases resolved, named types in signatures (incl. `*Named` and forward references), global `const`/`var` as `SYMBOL_VAR`, void return, redefinition. Pass 2 (`sema_check`): valid body passes clean, undeclared identifier, return-type mismatch, non-`bool` `if` condition, assignment to `const`, local `var` inference, call arity, call argument-type mismatch, mismatched binary operands, dereference of a non-pointer, `node->type_info` recorded on expressions (incl. operands), logical-op requires `bool`, index of a non-array, call of a non-function, assignment type mismatch, nested-scope shadowing, field access does not cascade (poison), numeric-literal coercion (`var x: i64 = 2`; non-literal width mismatch rejected). Helpers `make_sema`/`analyze`/`analyze_full` lex+parse a source string and run Pass 1 only or both passes
- **KzFiles** — every `test/kz/*.kz` example lexes and parses with zero errors

## Utils Library

- `arena.h/c` — bump allocator with block chaining; supports `arena_alloc_reallocable` + `arena_realloc` for growable buffers
- `hashmap.h/c` — string-keyed hash map, arena-backed; iterate with `hashmap_iter` + `hashmap_next`; `hashmap_create(arena, 0)` clamps to minimum size 32
- `string_view.h/c` — non-owning `StringView { const char *data; size_t len; }`
- `gvector.h` — macro-generated typed vectors (`VECTOR_DEFINITION(T, Prefix)` in header, `VECTOR_IMPLEMENTATION(T, Prefix)` in one `.c` file)

## Lexer Behaviour

- Unknown characters (`#`, `$`, `?`, `\`, etc.) produce `TOKEN_ERROR` and a `"unexpected character: '%c'"` error message. (`&`, `|`, `^`, `~` are now valid operators.)
- Unterminated string literals produce `TOKEN_ERROR` and an error entry in `lexer.errors`.
- Unterminated block comments (`/* ...` without `*/`) produce an error entry and consume until EOF.
- A float with no digit after the dot (`3.`) produces `TOKEN_ERROR`.
- Identifiers longer than 63 characters are always `TOKEN_IDENT` (keyword lookup buffer limit).
- `TOKEN_EOF` is only emitted when a whitespace/comment token brings the lexer to EOF; inputs with no trailing whitespace end without an EOF token in the vector.

## Language Keywords (lexer — current)

`fn`, `var`, `const`, `let`, `comptime`, `when`, `return`, `struct`, `union`, `enum`, `import`, `if`, `else`, `while`, `for`, `in`, `break`, `continue`, `match`, `defer`, `panic`, `try`, `true`, `false`, `null`, integer types (`i8`/`i16`/`i32`/`i64`/`u8`/`u16`/`u32`/`u64`), float types (`f32`/`f64`), `bool`, `void`, `type`

All grammar keywords are lexed and handled by the parser. `catch` is intentionally **not** a keyword — it lexes as an ordinary identifier so it can be a stdlib function.

## TODO Checklist

What still needs implementing, roughly in recommended order. Done: lexer, parser, AST, type system, symbol table, sema Pass 1 + Pass 2 (core), front-end integration into `compile()`.

**Semantic analysis — close the gaps**
- [ ] Aggregate fields: extend `TyAggregate` with resolved field names+types; populate in a Pass-1 phase after type names are registered
- [ ] `ExprFieldAccess` type checking (`x.field`, `ptr.field`) — currently silent poison
- [ ] Struct-literal field checking (names exist, types match, completeness)
- [ ] `Enum::Variant` paths with discriminants / payloads (tuple & struct variants)
- [ ] `match`-pattern binding types (patterns currently bound as `t_error`)
- [ ] `try` / error-propagation typing and the `expr!` unwrap
- [ ] `@cimport` member typing
- [ ] Alias-to-alias chains resolved regardless of declaration order (currently order-dependent)
- [ ] Struct method bodies (`DeclStruct.methods`) type-checked (Pass 2 only walks top-level fns)
- [ ] `for`-loop iterator protocol beyond array/slice element binding

**Code generation**
- [ ] Emit C from the type-checked AST
- [ ] Wire up `--dump-c` (still a no-op) and `-o <output>` file writing
- [ ] Invoke a C compiler on the generated output (end-to-end build)

**Parser / language features**
- [ ] Generic parameters: populate `is_generic` / `generic_params` (AST fields exist, parser ignores them)
- [ ] Generic type resolution + monomorphization in sema/codegen

**Tooling / polish**
- [ ] `--dump-ast` to optionally print resolved `node->type_info`
- [ ] Optimization flags (`-O0`..`-Os`) are listed in `--help` but not parsed in `parse_opts` (silently ignored)
