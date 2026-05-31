#ifndef AST_TEST_H
#define AST_TEST_H

#include "../test.h"

// Allocation
bool test_ast_alloc(void);
bool test_ast_alloc_all_kinds(void);
bool test_ast_source_loc(void);

// Expressions
bool test_expr_int_lit(void);
bool test_expr_float_lit(void);
bool test_expr_string_lit(void);
bool test_expr_bool_lit(void);
bool test_expr_null_lit(void);
bool test_expr_ident(void);
bool test_expr_binary_op(void);
bool test_expr_unary_op(void);
bool test_expr_call(void);
bool test_expr_index(void);
bool test_expr_field_access(void);
bool test_expr_enum_variant(void);
bool test_expr_cast(void);
bool test_expr_postfix(void);

// Statements
bool test_stmt_var_decl(void);
bool test_stmt_assignment(void);
bool test_stmt_if(void);
bool test_stmt_while(void);
bool test_stmt_for(void);
bool test_stmt_return(void);
bool test_stmt_break_continue(void);
bool test_stmt_block(void);

// Declarations
bool test_decl_function(void);
bool test_decl_struct(void);
bool test_decl_enum(void);
bool test_decl_type_alias(void);
bool test_decl_import(void);

// Types
bool test_type_primitive(void);
bool test_type_pointer(void);
bool test_type_array(void);
bool test_type_function(void);
bool test_type_generic(void);

// Cast macros
bool test_node_cast_valid(void);
bool test_node_cast_invalid(void);
bool test_node_cast_any(void);
bool test_node_is(void);
bool test_node_cast_unsafe(void);

// Traversal helpers
bool test_tree_depth(void);
bool test_tree_node_count(void);
bool test_tree_walk_dfs(void);

TestSuite ast_suite(void);

#endif /* AST_TEST_H */
