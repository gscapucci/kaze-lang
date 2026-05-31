#ifndef SCOPE_TEST_H
#define SCOPE_TEST_H

#include "../test.h"

bool test_scope_new(void);
bool test_scope_define_and_lookup(void);
bool test_scope_define_redefinition(void);
bool test_scope_lookup_local_only(void);
bool test_scope_lookup_parent_chain(void);
bool test_scope_shadowing(void);
bool test_scope_undefined(void);
bool test_scope_symbol_fields(void);
bool test_scope_sibling_isolation(void);
bool test_symbol_kind_to_string(void);

TestSuite scope_suite(void);

#endif /* SCOPE_TEST_H */
