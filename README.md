# Kaze

Kaze is a modern systems programming language focused on:
- performance
- portability
- simplicity
- native applications
- low-level control

The compiler is written in C and currently targets C code generation as the primary backend.

Future plans include:
- LLVM backend
- native machine code generation
- compile-time evaluation
- integrated build system
- package manager
- cross-platform tooling

---

## Features

- Lightweight compiler
- Fast compilation
- Portable generated code
- Minimal runtime
- Cross-platform support
- Modern language design
- Low-level memory control

---

## Supported Platforms

Current targets:
- Linux
- Windows (MinGW/GCC)

Planned:
- macOS
- BSD
- ARM
- WebAssembly

---

## Building

### Linux

```bash
gcc nob.c -o nob
./nob