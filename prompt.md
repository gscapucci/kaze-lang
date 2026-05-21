# Testes da AST - Compilador Kaze
## Usando Framework de Testes Existente

## Contexto

Você desenvolveu a **Abstract Syntax Tree (AST)** do compilador Kaze com:
- Node base pattern
- Structs individuais para cada tipo de nó
- Macros de casting seguro (`NODE_CAST`, `NODE_CAST_ANY`, `NODE_IS`)
- Suporte a generics e type annotations

**Objetivo**: Criar testes abrangentes usando o framework de testes existente.

---

## Framework de Testes Disponível

### Estrutura
```
test/
├── main.c                      # Entry point
├── test.h                       # Core framework
├── test.c                       # Implementação do framework
├── kazec/
│   ├── lexer_test.c/.h         # Testes do lexer (referência)
├── utils/
│   ├── hashmap_test.c/.h       # Exemplo completo
```

### API do Framework

```c
// test.h
typedef bool (*TestFn)(void);

typedef struct TestSuite {
    const char *name;
    TestFn func;
} TestSuite;

// Funções principais
void framework_init();
void framework_deinit();
void framework_run_tests();
void framework_add_suite(TestSuite suite);

// Arena global disponível
static Arena *global_arena = NULL;
```

### Padrão de Teste

1. **Arquivo**: `kazec/ast_test.h` e `kazec/ast_test.c`
2. **Macro ASSERT**: Usar conforme exemplo do hashmap_test
3. **Test Suite**: Funções individuais retornam `bool`, agregadas em `xpto_tests()`
4. **TestSuite Struct**: `TestSuite suite = { "Name", test_func }` retornado
5. **Registrar**: Chamar `framework_add_suite()` no `main.c`

---

## Escopo de Testes para AST

### 1. Alocação e Inicialização

- [ ] `test_ast_node_alloc()` — alocar um nó simples
- [ ] `test_ast_node_alloc_all_kinds()` — alocar cada tipo de nó (ExprIntLit, StmtVarDecl, etc)
- [ ] `test_ast_node_parent_pointer()` — parent pointer (se implementado)
- [ ] `test_ast_node_source_loc()` — source location é preservada

### 2. Expressões

#### Literais
- [ ] `test_expr_int_lit()` — criar e validar ExprIntLit
- [ ] `test_expr_float_lit()` — validar ExprFloatLit
- [ ] `test_expr_string_lit()` — validar ExprStringLit com raw strings
- [ ] `test_expr_bool_lit()` — validar ExprBoolLit (true/false)
- [ ] `test_expr_null_lit()` — validar ExprNullLit

#### Identificadores e Acesso
- [ ] `test_expr_ident()` — criar e validar ExprIdent
- [ ] `test_expr_field_access()` — ExprFieldAccess com `.field` e `->`
- [ ] `test_expr_index()` — ExprIndex para array[idx]
- [ ] `test_expr_enum_variant()` — ExprEnumVariant `Type::Variant`

#### Operações
- [ ] `test_expr_binary_op()` — criar BinaryOp com operadores variados (+, -, *, ==, etc)
- [ ] `test_expr_unary_op()` — criar UnaryOp (-x, !flag, *ptr, &var)
- [ ] `test_expr_cast()` — criar Cast com tipos alvo

#### Chamadas e Postfix
- [ ] `test_expr_call_no_args()` — chamada sem argumentos
- [ ] `test_expr_call_with_args()` — chamada com múltiplos args
- [ ] `test_expr_call_comptime()` — chamada @comptime
- [ ] `test_expr_postfix()` — postfix operators (.catch, .?)

### 3. Statements

- [ ] `test_stmt_var_decl_with_init()` — let x = 5
- [ ] `test_stmt_var_decl_without_init()` — let x: i32
- [ ] `test_stmt_const_decl()` — const PI = 3.14
- [ ] `test_stmt_var_comptime()` — const @foo = ...
- [ ] `test_stmt_assignment()` — x = 10
- [ ] `test_stmt_if_then()` — if sem else
- [ ] `test_stmt_if_then_else()` — if com else
- [ ] `test_stmt_while()` — while loop
- [ ] `test_stmt_for()` — for i in range
- [ ] `test_stmt_return()` — return, return expr
- [ ] `test_stmt_break_continue()` — break, continue
- [ ] `test_stmt_block()` — bloco com múltiplos statements

### 4. Declarações

- [ ] `test_decl_function_simple()` — função sem params/body
- [ ] `test_decl_function_with_params()` — função com params
- [ ] `test_decl_function_with_return_type()` — função com return type
- [ ] `test_decl_function_comptime()` — função @comptime
- [ ] `test_decl_function_extern()` — função @extern "C"
- [ ] `test_decl_struct_simple()` — struct com campos
- [ ] `test_decl_struct_generic()` — struct genérica `struct Box(T)`
- [ ] `test_decl_enum_simple()` — enum com variantes
- [ ] `test_decl_enum_generic()` — enum genérica `Result(T, E)`
- [ ] `test_decl_enum_with_data()` — variantes com associated data
- [ ] `test_decl_type_alias()` — type alias
- [ ] `test_decl_import()` — import statement
- [ ] `test_decl_import_external()` — import externo @cimport

### 5. Tipos (Type Nodes)

- [ ] `test_type_primitive()` — tipos primitivos (i32, f64, bool, etc)
- [ ] `test_type_pointer()` — pointers (@*i32, @**u8)
- [ ] `test_type_pointer_mutable()` — pointer mutável (@*mut)
- [ ] `test_type_array()` — arrays (@[10]i32)
- [ ] `test_type_slice()` — slices (@[]u8)
- [ ] `test_type_function()` — function types
- [ ] `test_type_generic()` — generic types List(T), Box(T, U)

### 6. Macros de Casting

- [ ] `test_node_cast_valid()` — NODE_CAST com tipo correto
- [ ] `test_node_cast_invalid()` — NODE_CAST com tipo errado (retorna NULL)
- [ ] `test_node_cast_any_single()` — NODE_CAST_ANY com um tipo
- [ ] `test_node_cast_any_multiple()` — NODE_CAST_ANY com múltiplos tipos
- [ ] `test_node_is()` — NODE_IS para type check
- [ ] `test_node_cast_unsafe()` — NODE_CAST_UNSAFE (direto)

### 7. Traversal e Estrutura

- [ ] `test_tree_depth()` — calcular profundidade da árvore
- [ ] `test_tree_node_count()` — contar total de nós
- [ ] `test_tree_walk_dfs()` — depth-first traversal
- [ ] `test_tree_walk_bfs()` — breadth-first traversal (opcional)

### 8. Memória e Cleanup

- [ ] `test_ast_free_simple_tree()` — liberar árvore simples
- [ ] `test_ast_free_complex_tree()` — liberar árvore complexa
- [ ] `test_ast_no_memory_leaks()` — valgrind/leak detection

---

## Estrutura de Arquivos Esperada

```
test/kazec/
├── ast_test.h          # Declarações de testes
├── ast_test.c          # Implementação
```

### ast_test.h

```c
#ifndef AST_TEST_H
#define AST_TEST_H

#include "../test.h"

// Testes de alocação
bool test_ast_node_alloc(void);
bool test_ast_node_alloc_all_kinds(void);

// Testes de expressões
bool test_expr_int_lit(void);
bool test_expr_binary_op(void);
bool test_expr_call_with_args(void);
// ... etc

// Testes de statements
bool test_stmt_var_decl_with_init(void);
bool test_stmt_if_then_else(void);
// ... etc

// Testes de declarações
bool test_decl_function_with_params(void);
bool test_decl_struct_generic(void);
// ... etc

// Testes de macros
bool test_node_cast_valid(void);
bool test_node_cast_invalid(void);
// ... etc

// Suite principal
TestSuite ast_suite(void);

#endif // AST_TEST_H
```

### ast_test.c — Padrão

```c
#include "ast_test.h"
#include "../../src/ast.h"  // Sua AST implementada
#include <stdio.h>
#include <string.h>

#define ASSERT(x) \
    (!!(x) ? true : ( \
        fprintf(stderr, "%s:%s:%d: assertion failed: %s\n", __FILE__, __func__, __LINE__, #x), \
        false \
    ))

// ============================================================================
// Testes de Alocação
// ============================================================================

bool test_ast_node_alloc() {
    bool ok = true;
    
    SourceLoc loc = {.filename = "test.kaze", .line = 1, .column = 0};
    Node* node = ast_alloc_node(NODE_EXPR_INT_LIT, loc);
    
    ok &= ASSERT(node != NULL);
    ok &= ASSERT(node->kind == NODE_EXPR_INT_LIT);
    ok &= ASSERT(node->loc.line == 1);
    ok &= ASSERT(strcmp(node->loc.filename, "test.kaze") == 0);
    ok &= ASSERT(node->type_info == NULL);  // Sema não anotou ainda
    
    return ok;
}

// ============================================================================
// Testes de Expressões
// ============================================================================

bool test_expr_int_lit() {
    bool ok = true;
    
    SourceLoc loc = {.filename = "test.kaze", .line = 1, .column = 0};
    ExprIntLit* lit = (ExprIntLit*)ast_alloc_node(NODE_EXPR_INT_LIT, loc);
    lit->value = 42;
    
    // Cast e validação
    ExprIntLit* casted = NODE_CAST(&lit->base, ExprIntLit, NODE_EXPR_INT_LIT);
    ok &= ASSERT(casted != NULL);
    ok &= ASSERT(casted->value == 42);
    
    // Cast inválido
    ExprIntLit* invalid = NODE_CAST(&lit->base, ExprIntLit, NODE_EXPR_FLOAT_LIT);
    ok &= ASSERT(invalid == NULL);
    
    return ok;
}

bool test_expr_binary_op() {
    bool ok = true;
    
    SourceLoc loc = {.filename = "test.kaze", .line = 1, .column = 0};
    
    ExprIntLit* left = (ExprIntLit*)ast_alloc_node(NODE_EXPR_INT_LIT, loc);
    left->value = 5;
    
    ExprIntLit* right = (ExprIntLit*)ast_alloc_node(NODE_EXPR_INT_LIT, loc);
    right->value = 3;
    
    ExprBinaryOp* binop = (ExprBinaryOp*)ast_alloc_node(NODE_EXPR_BINARY_OP, loc);
    binop->left = &left->base;
    binop->right = &right->base;
    binop->op = "+";
    
    // Validação
    ExprBinaryOp* casted = NODE_CAST(&binop->base, ExprBinaryOp, NODE_EXPR_BINARY_OP);
    ok &= ASSERT(casted != NULL);
    ok &= ASSERT(strcmp(casted->op, "+") == 0);
    
    return ok;
}

// ============================================================================
// Testes de Statements
// ============================================================================

bool test_stmt_var_decl_with_init() {
    bool ok = true;
    
    SourceLoc loc = {.filename = "test.kaze", .line = 1, .column = 0};
    
    ExprIntLit* init = (ExprIntLit*)ast_alloc_node(NODE_EXPR_INT_LIT, loc);
    init->value = 10;
    
    StmtVarDecl* decl = (StmtVarDecl*)ast_alloc_node(NODE_STMT_VAR_DECL, loc);
    decl->name = "x";
    decl->initializer = &init->base;
    decl->is_const = false;
    
    ok &= ASSERT(NODE_IS(&decl->base, NODE_STMT_VAR_DECL));
    ok &= ASSERT(strcmp(decl->name, "x") == 0);
    
    return ok;
}

// ============================================================================
// Testes de Macros de Casting
// ============================================================================

bool test_node_cast_valid() {
    bool ok = true;
    
    SourceLoc loc = {.filename = "test.kaze", .line = 1, .column = 0};
    Node* node = ast_alloc_node(NODE_EXPR_INT_LIT, loc);
    
    ExprIntLit* casted = NODE_CAST(node, ExprIntLit, NODE_EXPR_INT_LIT);
    ok &= ASSERT(casted != NULL);
    ok &= ASSERT(casted->base.kind == NODE_EXPR_INT_LIT);
    
    return ok;
}

bool test_node_cast_invalid() {
    bool ok = true;
    
    SourceLoc loc = {.filename = "test.kaze", .line = 1, .column = 0};
    Node* node = ast_alloc_node(NODE_EXPR_INT_LIT, loc);
    
    // Cast para tipo errado
    ExprFloatLit* invalid = NODE_CAST(node, ExprFloatLit, NODE_EXPR_FLOAT_LIT);
    ok &= ASSERT(invalid == NULL);
    
    return ok;
}

// ============================================================================
// Suite Agregadora
// ============================================================================

bool ast_tests() {
    bool ok = true;
    
    struct {
        TestFn func;
        const char *fname;
    } test_list[] = {
        {test_ast_node_alloc, "test_ast_node_alloc"},
        {test_expr_int_lit, "test_expr_int_lit"},
        {test_expr_binary_op, "test_expr_binary_op"},
        {test_stmt_var_decl_with_init, "test_stmt_var_decl_with_init"},
        {test_node_cast_valid, "test_node_cast_valid"},
        {test_node_cast_invalid, "test_node_cast_invalid"},
    };
    
    for(size_t i = 0; i < (sizeof(test_list) / sizeof(test_list[0])); i++) {
        bool res = test_list[i].func();
        if(res == false) {
            fprintf(stderr, "test %s failed.\n", test_list[i].fname);
        }
        ok &= res;
    }
    
    return ok;
}

TestSuite ast_suite() {
    TestSuite suite;
    suite.name = "AST";
    suite.func = ast_tests;
    return suite;
}
```

### main.c — Registrar Suite

```c
#include "test/test.h"
#include "test/kazec/ast_test.h"
#include "test/kazec/lexer_test.h"
#include "test/utils/hashmap_test.h"

int main() {
    framework_init();
    
    // Registrar suites
    framework_add_suite(hashmap_suite());
    framework_add_suite(lexer_suite());
    framework_add_suite(ast_suite());  // Novo!
    
    // Rodar todos os testes
    framework_run_tests();
    
    framework_deinit();
    return 0;
}
```

---

## Checklist de Implementação

- [ ] Criar `test/kazec/ast_test.h`
- [ ] Criar `test/kazec/ast_test.c` com padrão acima
- [ ] Implementar testes básicos (alocação, expressões, statements)
- [ ] Implementar testes de macros de casting
- [ ] Registrar suite em `main.c`
- [ ] Compilar com `cbuild`
- [ ] Rodar e validar com valgrind (sem memory leaks)

---

## Comandos Esperados

```bash
# Compilar testes
cbuild test

# Rodar testes
./build/test

# Valgrind (memory leak detection)
valgrind ./build/test
```

---

## Notas Importantes

1. **Arena Global**: Use `global_arena` do framework para alocações
2. **ASSERT Macro**: Padrão — já está pronto, copie do hashmap_test
3. **Test Naming**: `test_<category>_<specific>()`
4. **Return bool**: Cada função retorna `bool` — combine com `ok &= ASSERT(...)`
5. **Cleanup**: Se alocar em arena, libera tudo no `framework_deinit()`

---

## Próximas Fases

1. ✅ Testes básicos da AST (alocação, nós simples)
2. ✅ Testes de expressões e statements
3. Integração com parser (quando pronto)
4. Testes de sema (type annotations)

---