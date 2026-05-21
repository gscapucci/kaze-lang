# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## What This Is

Kaze is a systems programming language compiler written in C. The compiler (`kazec`) currently targets C code generation. The language syntax is in `test.kz` and the spec is in `langspec.md`.

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
./build/kazec --dump-ast <file>     # print AST (not yet implemented)
./build/kazec --dump-c <file>       # print generated C (not yet implemented)
./build/kazec -o <output> <file>    # set output file (default: out.c)
./build/kazec -h                    # help
./build/kazec -v                    # version
```

## Architecture

```
utils/          — reusable low-level library (arena allocator, hashmap, string_view, generic vector)
kazec/          — the compiler
  main.c        — entry point: parse opts → compile()
  src/
    compiler_opt.c  — CLI argument parsing, CompilerOpt struct, flag bitmask (OPT_*)
    compiler.c      — top-level compile(): reads file → lexer → (parser stub)
    lexer.c         — tokenizer, produces TokenVec + ErrorVec
    token.c         — Token/TokenType definitions
    ast.c           — AST node allocation, printing, cast helpers
  include/      — headers mirroring src/
test/           — test suite
  test.h/test.c — framework: TestSuite, TestCase, ASSERT macro
  main.c        — registers and runs all suites
  utils/        — hashmap tests
  kazec/        — lexer and AST tests
```

**Data flow (current):** source file → `read_file()` → `lexer_init()` + `lexer_get_tokens()` → `TokenVec`. Parser is stubbed out (commented in `compiler.c`). AST types are fully defined but not yet populated by a parser.

**Memory:** everything allocates through an `Arena *` passed down from `compile()`. The arena is freed at the end of compilation. Vectors (`TokenVec`, `ErrorVec`) use `gvector.h` macros (`VECTOR_DEFINITION` / `VECTOR_IMPLEMENTATION`) backed by the arena's reallocable allocator.

**Keyword lookup:** the lexer builds a `HashMap` at init time mapping keyword strings → `TokenType`. The map is arena-allocated.

## AST

All node types are defined in `kazec/include/ast.h`. Every concrete node embeds `Node` as its first field (C "inheritance" via pointer casting). `ast_alloc_node(arena, kind, loc)` allocates a zeroed node of the correct size for the given `NodeKind`.

Cast macros:
- `NODE_CAST(node, type, expected_kind)` — returns `NULL` if kind mismatches
- `NODE_CAST_ANY(node, type, k1, k2, ...)` — accepts multiple valid kinds
- `NODE_IS(node, k)` — kind predicate
- `NODE_CAST_UNSAFE(node, type)` — unchecked, only when kind is already known

Node categories: expressions (14), statements (10), declarations (5), types (5), patterns (3). The `Program` struct is the module root; it holds `Node **decls`.

## Tests

```bash
./nob test   # build and run all suites
```

Three suites: `HashMap`, `Lexer`, `AST`. Each test function returns `bool`; the framework counts passes/failures per suite. To add a new suite: implement `TestSuite foo_suite(void)` and register it in `test/main.c`.

## Utils Library

- `arena.h/c` — bump allocator with block chaining; supports `arena_alloc_reallocable` + `arena_realloc` for growable buffers
- `hashmap.h/c` — string-keyed hash map, arena-backed; iterate with `hashmap_iter` + `hashmap_next`
- `string_view.h/c` — non-owning `StringView { const char *data; size_t len; }`
- `gvector.h` — macro-generated typed vectors (`VECTOR_DEFINITION(T, Prefix)` in header, `VECTOR_IMPLEMENTATION(T, Prefix)` in one `.c` file)

## Language Keywords (current)

`fn`, `var`, `const`, `comptime`, `when`, `return`, `struct`, `union`, integer types (`i8`/`i16`/`i32`/`i64`/`u8`/`u16`/`u32`/`u64`)
