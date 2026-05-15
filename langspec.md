# Alterações Recomendadas — Especificação da Linguagem v0.0.1

## 1. Visão Geral

Adicionar:

| Propriedade            | Decisão                    |
| ---------------------- | -------------------------- |
| Filosofia principal    | Software nativo leve       |
| Runtime                | Mínimo / opcional          |
| Estratégia de UI       | Wrappers SDL3 na stdlib    |
| Cross-compilation      | Recurso de primeira classe |
| Sistema de build       | Integrado à toolchain      |
| Gerenciador de pacotes | Planejado                  |
| Suporte a WASM         | Via emcc inicialmente      |
| Suporte embarcado      | Planejado desde o início   |

Adicionar descrição:

> A linguagem foi projetada para construir aplicações nativas leves com baixo consumo de memória e forte interoperabilidade com C. O ecossistema prioriza portabilidade, simplicidade, integração da toolchain e baixo overhead de runtime.

---

# 2. Sistema de Arquivos & Módulos

Adicionar:

## 2.1 Scripts de Build

Scripts de build são escritos na própria linguagem.

Exemplo:

```c
// TODO ainda a ser definido
fn build(b: *Build) void {
    const exe = b.executable("app");

    exe.add_source("src/main.kz");

    exe.link_system_library("SDL3");

    b.install(exe);
}
```

---

# 4. Variáveis & Declarações

Existe inconsistência:
Você usa:

* `var`
* `const`

Ficar apenas com:

| Keyword | Significado |
| ------- | ----------- |
| `var`   | mutável     |
| `const` | imutável    |

Exemplo:

```c
const name = "hello";
```

Isso simplifica MUITO a linguagem.

---

# 5. Tipos Primitivos

Adicionar:

| Tipo   | Descrição                             |
| ------ | ------------------------------------- |
| `type` | Objeto de tipo em tempo de compilação |

Necessário para futuros recursos de comptime/generics.

---

# 6. Ponteiros & Memória

Adicionar filosofia explícita:

> O gerenciamento de memória é totalmente manual. A linguagem nunca insere alocações ocultas nem pausas de garbage collector.

Adicionar:

## 6.4 Allocators (Futuro)

```c
fn init(alloc: Allocator) void {}
```

Inspirado em Zig.

---

# 7. Arrays & Slices

Adicionar:

| Recurso         | Status                        |
| --------------- | ----------------------------- |
| Bounds checking | Modo debug opcional no futuro |

Isso ajuda tooling/debugging futuramente sem comprometer builds release.

---

# 8. Funções

Adicionar:

## 8.3 Convenções de Chamada

```c
@cdecl
fn foo() void {}
```

Adicionar tabela:

| Convenção   | Objetivo              |
| ----------- | --------------------- |
| `@cdecl`    | ABI C                 |
| `@stdcall`  | APIs Windows (futuro) |
| `@fastcall` | Futuro                |

---

# 9. Structs

Adicionar:

## 9.1 Garantias de Layout

A ordem dos campos da struct é garantida e corresponde ao layout C gerado por padrão.

Isso é MUITO importante para:

* ABI C,
* serialização,
* embedded,
* networking.

---

# 10. Enums

Adicionar:

## 10.5 Representação

Enums simples geram enums C nativos.

Enums algébricos geram tagged structs/unions.

---

# 12. Tratamento de Erros

Adicionar:

## 12.4 Comportamento do Panic

`panic()` aborta imediatamente o processo por padrão.

Sem stack unwinding.

Isso mantém:

* simplicidade,
* baixo overhead,
* previsibilidade.

---

# 13. Fluxo de Controle

Adicionar:

## 13.5 defer (Planejado)

```c
const file = open("a.txt");
defer close(file);
```

Extremamente alinhado com o projeto.

---

# 15. Interoperabilidade com C

Adicionar:

## 15.3 Filosofia de ABI

A ABI da linguagem é compatível com C por padrão sempre que possível.

Nenhum metadata oculto ou estruturas de runtime são inseridos, exceto quando explicitamente necessário.

---

Adicionar:

## 15.4 Blocos Inline de C (Futuro)

```c
@c {
    #define FOO 10
}
```

Muito útil para integração gradual.

---

# 16. Atributos & Builtins do Compilador

Adicionar:

| Builtin               | Descrição                |
| --------------------- | ------------------------ |
| `@panic(msg)`         | Aborta a execução        |
| `@compile_error(msg)` | Emite erro de compilação |
| `@file()`             | Arquivo atual            |
| `@line()`             | Linha atual              |
| `@function()`         | Função atual             |

---

# 18. Recursos Planejados para o Futuro

Adicionar:

| Recurso                          | Status    | Observações                |
| -------------------------------- | --------- | -------------------------- |
| Gerenciador de pacotes integrado | Futuro    | Similar ao Cargo           |
| Toolchain de cross compilation   | Planejado | Recurso de primeira classe |
| Wrappers SDL3 na stdlib          | Planejado | Oficial                    |
| Suporte a WASM                   | Planejado | emcc inicialmente          |
| Backend nativo                   | Futuro    | Após LLVM                  |
| Compilação incremental           | Futuro    | Otimização da toolchain    |
| Targets embarcados               | Planejado | ESP32, ARM                 |
| Bindings Dear ImGui              | Planejado | UI oficial para tooling    |
| Bindings LVGL                    | Planejado | UI embarcada               |

---

# NOVA SEÇÃO — Filosofia da Toolchain

## 19. Toolchain

O compilador faz parte de uma toolchain de desenvolvimento integrada maior.

Comandos planejados:

```bash
kaze build
kaze run
kaze test
kaze fmt
kaze check
kaze new
kaze package
```

Objetivos:

* cross compilation simples;
* mínima dependência de ferramentas externas;
* builds reproduzíveis;
* workflow leve de desenvolvimento.

---

# NOVA SEÇÃO — Filosofia da Biblioteca Padrão

## 20. Biblioteca Padrão

A biblioteca padrão é intencionalmente modular e leve.

Objetivos principais:

* zero alocações ocultas;
* wrappers finos sobre bibliotecas nativas/do sistema;
* portabilidade;
* baixo overhead de memória.

Módulos planejados:

```text
std/
 ├── core
 ├── mem
 ├── io
 ├── math
 ├── net
 ├── json
 ├── fs
 ├── thread
 ├── sdl
 ├── gpu
 ├── audio
 ├── sqlite
 └── http
```

SDL3 será a principal camada de abstração multiplataforma para:

* janelas,
* input,
* rendering,
* áudio,
* integração com plataforma.

---

# Filosofia Geral Recomendada

O documento já está MUITO forte.

Mas eu reforçaria MUITO estes pilares:

* software nativo leve;
* zero comportamento oculto;
* forte interoperabilidade com C;
* toolchain integrada;
* portabilidade em primeiro lugar;
* uso previsível de memória;
* runtime mínimo.

Isso diferencia claramente a linguagem de:

* Rust,
* C++,
* stacks baseadas em Electron,
* runtimes pesados.
