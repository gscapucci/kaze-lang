#ifndef PARSER_TEST_H
#define PARSER_TEST_H

#include "../test.h"

bool test_parse_empty(void);
bool test_parse_var_decls(void);
bool test_parse_function(void);
bool test_parse_precedence(void);
bool test_parse_if_else(void);
bool test_parse_while_for(void);
bool test_parse_struct(void);
bool test_parse_enum(void);
bool test_parse_type_alias(void);
bool test_parse_import(void);
bool test_parse_postfix_chain(void);
bool test_parse_compound_assign(void);
bool test_parse_cimport(void);
bool test_parse_struct_methods(void);
bool test_parse_union(void);
bool test_parse_struct_literal(void);
bool test_parse_match(void);
bool test_parse_when(void);
bool test_parse_defer(void);
bool test_parse_try(void);
bool test_parse_errors(void);

TestSuite parser_test_suite(void);

#endif /* PARSER_TEST_H */
