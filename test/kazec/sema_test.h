#ifndef SEMA_TEST_H
#define SEMA_TEST_H

#include "../test.h"

bool test_sema_collect_function(void);
bool test_sema_collect_struct(void);
bool test_sema_collect_enum_union(void);
bool test_sema_collect_type_alias(void);
bool test_sema_named_type_in_signature(void);
bool test_sema_pointer_to_named_type(void);
bool test_sema_forward_reference(void);
bool test_sema_global_const(void);
bool test_sema_redefinition(void);
bool test_sema_void_return(void);

// Pass 2 — body checking
bool test_sema_body_ok(void);
bool test_sema_undeclared_ident(void);
bool test_sema_return_mismatch(void);
bool test_sema_if_condition_not_bool(void);
bool test_sema_assign_to_const(void);
bool test_sema_local_inference(void);
bool test_sema_call_arity(void);
bool test_sema_call_arg_type(void);
bool test_sema_binary_mismatch(void);
bool test_sema_deref_non_pointer(void);
bool test_sema_expr_types_recorded(void);
bool test_sema_logical_requires_bool(void);
bool test_sema_index_non_array(void);
bool test_sema_call_non_function(void);
bool test_sema_assign_type_mismatch(void);
bool test_sema_nested_scope_shadowing(void);
bool test_sema_field_access_no_cascade(void);
bool test_sema_literal_coercion(void);

TestSuite sema_suite(void);

#endif /* SEMA_TEST_H */
