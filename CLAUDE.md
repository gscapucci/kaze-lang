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
    lexer.c         — tokenizer, produces TokenVec
    token.c         — Token/TokenType definitions
  include/      — headers mirroring src/
```

**Data flow (current):** source file → `read_file()` → `lexer_init()` + `lexer_get_tokens()` → `TokenVec`. Parser is stubbed out (commented in `compiler.c`).

**Memory:** everything allocates through an `Arena *` passed down from `compile()`. The arena is freed at the end of compilation. Vectors (`TokenVec`, `ErrorVec`) use `gvector.h` macros (`VECTOR_DEFINITION` / `VECTOR_IMPLEMENTATION`) backed by the arena's reallocable allocator.

**Keyword lookup:** the lexer builds a `HashMap` at init time mapping keyword strings → `TokenType`. The map is arena-allocated.

## Utils Library

- `arena.h/c` — bump allocator with block chaining; supports `arena_alloc_reallocable` + `arena_realloc` for growable buffers
- `hashmap.h/c` — string-keyed hash map, arena-backed
- `string_view.h/c` — non-owning `StringView { const char *data; size_t len; }`
- `gvector.h` — macro-generated typed vectors (`VECTOR_DEFINITION(T, Prefix)` in header, `VECTOR_IMPLEMENTATION(T, Prefix)` in one `.c` file)

## Language Keywords (current)

`fn`, `var`, `const`, `comptime`, `when`, `return`, `struct`, `union`, integer types (`i8`/`i16`/`i32`/`i64`/`u8`/`u16`/`u32`/`u64`)
