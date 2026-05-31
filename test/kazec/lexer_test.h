#ifndef LEXER_TEST_H
#define LEXER_TEST_H

#include "../test.h"

bool test_lex_empty(void);
bool test_lex_int_literal(void);
bool test_lex_float_literal(void);
bool test_lex_minus_operator(void);
bool test_lex_string_literal(void);
bool test_lex_identifier(void);
bool test_lex_keywords(void);
bool test_lex_single_char_ops(void);
bool test_lex_double_char_ops(void);
bool test_lex_triple_char_ops(void);
bool test_lex_line_comment(void);
bool test_lex_block_comment(void);
bool test_lex_source_location(void);
bool test_lex_multichar_op_col(void);
bool test_lex_unterminated_string(void);
bool test_lex_expression(void);

TestSuite lexer_test_suite(void);

#endif /* LEXER_TEST_H */