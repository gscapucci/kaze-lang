#ifndef TYPE_TEST_H
#define TYPE_TEST_H

#include "../test.h"

// Context + allocation
bool test_type_context_init(void);
bool test_type_alloc_all_kinds(void);

// Primitive name lookup
bool test_primitive_from_name(void);
bool test_primitive_from_name_unknown(void);

// Resolution from AST type nodes
bool test_resolve_primitive(void);
bool test_resolve_pointer(void);
bool test_resolve_const_pointer(void);
bool test_resolve_array(void);
bool test_resolve_slice(void);
bool test_resolve_function(void);
bool test_resolve_generic_is_error(void);
bool test_resolve_named_is_error(void);
bool test_resolve_null_is_error(void);

// Equality
bool test_equals_primitives(void);
bool test_equals_pointer(void);
bool test_equals_pointer_mutability(void);
bool test_equals_array_length(void);
bool test_equals_function(void);
bool test_equals_aggregate_nominal(void);

// Queries + formatting
bool test_type_queries(void);
bool test_type_to_string(void);

TestSuite type_suite(void);

#endif /* TYPE_TEST_H */
