#include "lexer_test.h"
#include "../../kazec/include/lexer.h"
#include <stdio.h>
#include <string.h>

#define ASSERT(x) \
    (!!(x) ? true : ( \
        fprintf(stderr, "%s:%s:%d: assertion failed: %s\n", __FILE__, __func__, __LINE__, #x), \
        false \
    ))

// ============================================================================
// Helpers
// ============================================================================

typedef struct {
    Lexer   lexer;
    TokenVec tokens;
} LexResult;

static LexResult lex(Arena *arena, const char *src) {
    LexResult r;
    lexer_init(&r.lexer, src, arena);
    r.tokens = lexer_get_tokens(&r.lexer);
    return r;
}

// Return type of token at index, TOKEN_UNDEFINED if out of bounds.
static TokenType tok_type(LexResult *r, size_t i) {
    if (i >= r->tokens.len) return TOKEN_UNDEFINED;
    return TokenVec_get(&r->tokens, i).type;
}

// Return token at index.
static Token tok(LexResult *r, size_t i) {
    return TokenVec_get(&r->tokens, i);
}

// Check lexeme of token at index matches expected string.
static bool tok_lexeme_eq(LexResult *r, size_t i, const char *expected) {
    Token t = tok(r, i);
    size_t elen = strlen(expected);
    return t.lexeme.len == elen && strncmp(t.lexeme.data, expected, elen) == 0;
}

// ============================================================================
// Tests
// ============================================================================

bool test_lex_empty(void) {
    bool ok = true;
    Arena *arena = arena_new(0);

    // Input with only whitespace produces just EOF
    LexResult r = lex(arena, "   ");
    ok &= ASSERT(r.tokens.len >= 1);
    ok &= ASSERT(tok_type(&r, 0) == TOKEN_EOF);

    arena_delete(arena);
    return ok;
}

bool test_lex_int_literal(void) {
    bool ok = true;
    Arena *arena = arena_new(0);

    LexResult r = lex(arena, "42 ");
    ok &= ASSERT(tok_type(&r, 0) == TOKEN_INT_LIT);
    ok &= ASSERT(tok_lexeme_eq(&r, 0, "42"));

    LexResult r2 = lex(arena, "0 ");
    ok &= ASSERT(tok_type(&r2, 0) == TOKEN_INT_LIT);
    ok &= ASSERT(tok_lexeme_eq(&r2, 0, "0"));

    LexResult r3 = lex(arena, "999 ");
    ok &= ASSERT(tok_type(&r3, 0) == TOKEN_INT_LIT);
    ok &= ASSERT(tok_lexeme_eq(&r3, 0, "999"));

    arena_delete(arena);
    return ok;
}

// Verifies the is_float bug fix — floats must produce TOKEN_FLOAT_LIT.
bool test_lex_float_literal(void) {
    bool ok = true;
    Arena *arena = arena_new(0);

    LexResult r = lex(arena, "3.14 ");
    ok &= ASSERT(tok_type(&r, 0) == TOKEN_FLOAT_LIT);
    ok &= ASSERT(tok_lexeme_eq(&r, 0, "3.14"));

    LexResult r2 = lex(arena, "0.0 ");
    ok &= ASSERT(tok_type(&r2, 0) == TOKEN_FLOAT_LIT);
    ok &= ASSERT(tok_lexeme_eq(&r2, 0, "0.0"));

    LexResult r3 = lex(arena, "1.5 ");
    ok &= ASSERT(tok_type(&r3, 0) == TOKEN_FLOAT_LIT);

    // Integer without dot is still INT_LIT
    LexResult r4 = lex(arena, "10 ");
    ok &= ASSERT(tok_type(&r4, 0) == TOKEN_INT_LIT);

    // Bad float (no digit after dot) must be TOKEN_ERROR
    LexResult r5 = lex(arena, "3.x ");
    ok &= ASSERT(tok_type(&r5, 0) == TOKEN_ERROR);
    ok &= ASSERT(r5.lexer.errors.len > 0);

    arena_delete(arena);
    return ok;
}

// Verifies the TOKEN_MINUS bug fix — '-' must tokenize as TOKEN_MINUS.
bool test_lex_minus_operator(void) {
    bool ok = true;
    Arena *arena = arena_new(0);

    LexResult r = lex(arena, "- ");
    ok &= ASSERT(tok_type(&r, 0) == TOKEN_MINUS);
    ok &= ASSERT(tok_lexeme_eq(&r, 0, "-"));

    // Subtraction: a - b → IDENT MINUS IDENT
    LexResult r2 = lex(arena, "a - b ");
    ok &= ASSERT(tok_type(&r2, 0) == TOKEN_IDENT);
    ok &= ASSERT(tok_type(&r2, 1) == TOKEN_MINUS);
    ok &= ASSERT(tok_type(&r2, 2) == TOKEN_IDENT);

    // -= is TOKEN_MINUS_EQ (double-char), not MINUS + EQ
    LexResult r3 = lex(arena, "-= ");
    ok &= ASSERT(tok_type(&r3, 0) == TOKEN_MINUS_EQ);
    ok &= ASSERT(r3.tokens.len >= 1);

    arena_delete(arena);
    return ok;
}

bool test_lex_string_literal(void) {
    bool ok = true;
    Arena *arena = arena_new(0);

    LexResult r = lex(arena, "\"hello\" ");
    ok &= ASSERT(tok_type(&r, 0) == TOKEN_STRING_LIT);
    ok &= ASSERT(tok_lexeme_eq(&r, 0, "hello"));

    LexResult r2 = lex(arena, "\"\" ");
    ok &= ASSERT(tok_type(&r2, 0) == TOKEN_STRING_LIT);
    ok &= ASSERT(tok(&r2, 0).lexeme.len == 0);

    // Escaped quote inside string
    LexResult r3 = lex(arena, "\"say \\\"hi\\\"\" ");
    ok &= ASSERT(tok_type(&r3, 0) == TOKEN_STRING_LIT);

    arena_delete(arena);
    return ok;
}

bool test_lex_identifier(void) {
    bool ok = true;
    Arena *arena = arena_new(0);

    LexResult r = lex(arena, "myVar ");
    ok &= ASSERT(tok_type(&r, 0) == TOKEN_IDENT);
    ok &= ASSERT(tok_lexeme_eq(&r, 0, "myVar"));

    LexResult r2 = lex(arena, "_private ");
    ok &= ASSERT(tok_type(&r2, 0) == TOKEN_IDENT);

    LexResult r3 = lex(arena, "x123 ");
    ok &= ASSERT(tok_type(&r3, 0) == TOKEN_IDENT);

    arena_delete(arena);
    return ok;
}

bool test_lex_keywords(void) {
    bool ok = true;
    Arena *arena = arena_new(0);

    struct { const char *src; TokenType expected; } kws[] = {
        {"fn ",       TOKEN_FN},
        {"var ",      TOKEN_VAR},
        {"const ",    TOKEN_CONST},
        {"comptime ", TOKEN_COMPTIME},
        {"when ",     TOKEN_WHEN},
        {"return ",   TOKEN_RETURN},
        {"struct ",   TOKEN_STRUCT},
        {"union ",    TOKEN_UNION},
        {"i8 ",       TOKEN_I8},
        {"i16 ",      TOKEN_I16},
        {"i32 ",      TOKEN_I32},
        {"i64 ",      TOKEN_I64},
        {"u8 ",       TOKEN_U8},
        {"u16 ",      TOKEN_U16},
        {"u32 ",      TOKEN_U32},
        {"u64 ",      TOKEN_U64},
    };

    for (size_t i = 0; i < sizeof(kws) / sizeof(kws[0]); i++) {
        LexResult r = lex(arena, kws[i].src);
        ok &= ASSERT(tok_type(&r, 0) == kws[i].expected);
    }

    // Keywords embedded in identifiers are still IDENT
    LexResult r = lex(arena, "fn2 ");
    ok &= ASSERT(tok_type(&r, 0) == TOKEN_IDENT);

    arena_delete(arena);
    return ok;
}

bool test_lex_single_char_ops(void) {
    bool ok = true;
    Arena *arena = arena_new(0);

    struct { const char *src; TokenType expected; } ops[] = {
        {"= ",  TOKEN_EQ},
        {". ",  TOKEN_DOT},
        {"* ",  TOKEN_STAR},
        {"+ ",  TOKEN_PLUS},
        {"- ",  TOKEN_MINUS},
        {"/ ",  TOKEN_SLASH},
        {"% ",  TOKEN_MOD},
        {"< ",  TOKEN_LESS},
        {"> ",  TOKEN_GREATER},
        {"{ ",  TOKEN_LBRACE},
        {"} ",  TOKEN_RBRACE},
        {"; ",  TOKEN_SEMICOLON},
        {": ",  TOKEN_COLON},
        {", ",  TOKEN_COMMA},
        {"( ",  TOKEN_LPAREN},
        {") ",  TOKEN_RPAREN},
        {"[ ",  TOKEN_LBRACKET},
        {"] ",  TOKEN_RBRACKET},
        {"! ",  TOKEN_BANG},
        {"@ ",  TOKEN_AT},
    };

    for (size_t i = 0; i < sizeof(ops) / sizeof(ops[0]); i++) {
        LexResult r = lex(arena, ops[i].src);
        ok &= ASSERT(tok_type(&r, 0) == ops[i].expected);
    }

    arena_delete(arena);
    return ok;
}

bool test_lex_double_char_ops(void) {
    bool ok = true;
    Arena *arena = arena_new(0);

    struct { const char *src; TokenType expected; } ops[] = {
        {"<< ",  TOKEN_LESS_LESS},
        {">> ",  TOKEN_GREATER_GREATER},
        {"<= ",  TOKEN_LESS_EQ},
        {">= ",  TOKEN_GREATER_EQ},
        {"+= ",  TOKEN_PLUS_EQ},
        {"-= ",  TOKEN_MINUS_EQ},
        {"*= ",  TOKEN_STAR_EQ},
        {"/= ",  TOKEN_SLASH_EQ},
        {"== ",  TOKEN_EQ_EQ},
        {"!= ",  TOKEN_BANG_EQ},
        {"%= ",  TOKEN_MOD_EQ},
    };

    for (size_t i = 0; i < sizeof(ops) / sizeof(ops[0]); i++) {
        LexResult r = lex(arena, ops[i].src);
        ok &= ASSERT(tok_type(&r, 0) == ops[i].expected);
        ok &= ASSERT(tok(&r, 0).lexeme.len == 2);
    }

    arena_delete(arena);
    return ok;
}

bool test_lex_triple_char_ops(void) {
    bool ok = true;
    Arena *arena = arena_new(0);

    LexResult r1 = lex(arena, "<<= ");
    ok &= ASSERT(tok_type(&r1, 0) == TOKEN_LESS_LESS_EQ);
    ok &= ASSERT(tok(&r1, 0).lexeme.len == 3);

    LexResult r2 = lex(arena, ">>= ");
    ok &= ASSERT(tok_type(&r2, 0) == TOKEN_GREATER_GREATER_EQ);
    ok &= ASSERT(tok(&r2, 0).lexeme.len == 3);

    arena_delete(arena);
    return ok;
}

bool test_lex_line_comment(void) {
    bool ok = true;
    Arena *arena = arena_new(0);

    // Comment is skipped — only the token after the newline is returned
    LexResult r = lex(arena, "// this is a comment\n42 ");
    ok &= ASSERT(tok_type(&r, 0) == TOKEN_INT_LIT);
    ok &= ASSERT(tok_lexeme_eq(&r, 0, "42"));

    // Comment at end of line, token before it is valid
    LexResult r2 = lex(arena, "99 // comment\n ");
    ok &= ASSERT(tok_type(&r2, 0) == TOKEN_INT_LIT);
    ok &= ASSERT(tok_lexeme_eq(&r2, 0, "99"));

    arena_delete(arena);
    return ok;
}

bool test_lex_block_comment(void) {
    bool ok = true;
    Arena *arena = arena_new(0);

    LexResult r = lex(arena, "/* block comment */ 7 ");
    ok &= ASSERT(tok_type(&r, 0) == TOKEN_INT_LIT);
    ok &= ASSERT(tok_lexeme_eq(&r, 0, "7"));

    // Multi-line block comment
    LexResult r2 = lex(arena, "/* line1\nline2\nline3 */ fn ");
    ok &= ASSERT(tok_type(&r2, 0) == TOKEN_FN);

    arena_delete(arena);
    return ok;
}

bool test_lex_source_location(void) {
    bool ok = true;
    Arena *arena = arena_new(0);

    // Single-line: token starts at col 1, line 1
    LexResult r = lex(arena, "x ");
    Token t = tok(&r, 0);
    ok &= ASSERT(t.line == 1);
    ok &= ASSERT(t.col  == 1);

    // Second token on same line
    LexResult r2 = lex(arena, "x y ");
    ok &= ASSERT(tok(&r2, 0).col == 1);
    ok &= ASSERT(tok(&r2, 1).col == 3);

    // Token on second line
    LexResult r3 = lex(arena, "x\ny ");
    ok &= ASSERT(tok(&r3, 0).line == 1);
    ok &= ASSERT(tok(&r3, 1).line == 2);
    ok &= ASSERT(tok(&r3, 1).col  == 1);

    arena_delete(arena);
    return ok;
}

// Verifies the col-tracking bug fix for multi-char operators.
// After a 2-char operator, the following token must have the correct col.
bool test_lex_multichar_op_col(void) {
    bool ok = true;
    Arena *arena = arena_new(0);

    // "x == y": x(col1) sp(col2) ==(col3) sp(col5) y(col6)
    LexResult r = lex(arena, "x == y ");
    ok &= ASSERT(r.tokens.len >= 3);
    ok &= ASSERT(tok(&r, 0).col == 1);   // x
    ok &= ASSERT(tok(&r, 1).col == 3);   // ==
    ok &= ASSERT(tok(&r, 2).col == 6);   // y — fails without the col fix

    // "a <<= b": a(col1) sp(2) <<=(col3) sp(6) b(col7)
    LexResult r2 = lex(arena, "a <<= b ");
    ok &= ASSERT(tok(&r2, 0).col == 1);  // a
    ok &= ASSERT(tok(&r2, 1).col == 3);  // <<=
    ok &= ASSERT(tok(&r2, 2).col == 7);  // b — fails without the col fix

    arena_delete(arena);
    return ok;
}

// Verifies the unterminated-string fix — must NOT crash; must emit TOKEN_ERROR.
bool test_lex_unterminated_string(void) {
    bool ok = true;
    Arena *arena = arena_new(0);

    LexResult r = lex(arena, "\"unterminated");
    ok &= ASSERT(tok_type(&r, 0) == TOKEN_ERROR);
    ok &= ASSERT(r.lexer.errors.len > 0);

    // Valid string followed by unterminated one
    LexResult r2 = lex(arena, "\"ok\" \"bad");
    ok &= ASSERT(tok_type(&r2, 0) == TOKEN_STRING_LIT);
    ok &= ASSERT(tok_type(&r2, 1) == TOKEN_ERROR);
    ok &= ASSERT(r2.lexer.errors.len > 0);

    arena_delete(arena);
    return ok;
}

bool test_lex_unknown_char(void) {
    bool ok = true;
    Arena *arena = arena_new(0);

    // Each unknown character must produce TOKEN_ERROR and an error message
    const char *cases[] = { "# ", "$ ", "? ", "\\ " };
    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
        LexResult r = lex(arena, cases[i]);
        ok &= ASSERT(tok_type(&r, 0) == TOKEN_ERROR);
        ok &= ASSERT(r.lexer.errors.len > 0);
    }

    // Valid tokens after an unknown char must still be lexed
    LexResult r2 = lex(arena, "# 42 ");
    ok &= ASSERT(tok_type(&r2, 0) == TOKEN_ERROR);
    ok &= ASSERT(tok_type(&r2, 1) == TOKEN_INT_LIT);

    arena_delete(arena);
    return ok;
}

bool test_lex_float_trailing_dot(void) {
    bool ok = true;
    Arena *arena = arena_new(0);

    // "3." — dot with no digit after it must be TOKEN_ERROR
    LexResult r = lex(arena, "3. ");
    ok &= ASSERT(tok_type(&r, 0) == TOKEN_ERROR);
    ok &= ASSERT(r.lexer.errors.len > 0);

    // "3." at EOF (no trailing space) — same result
    LexResult r2 = lex(arena, "3.");
    ok &= ASSERT(tok_type(&r2, 0) == TOKEN_ERROR);
    ok &= ASSERT(r2.lexer.errors.len > 0);

    arena_delete(arena);
    return ok;
}

bool test_lex_long_identifier(void) {
    bool ok = true;
    Arena *arena = arena_new(0);

    // 64-char identifier — longer than the 63-char keyword lookup buffer
    const char *long_id =
        "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa ";
    LexResult r = lex(arena, long_id);
    ok &= ASSERT(tok_type(&r, 0) == TOKEN_IDENT);

    // Must not be mistaken for a keyword
    LexResult r2 = lex(arena, "fn_long_name ");
    ok &= ASSERT(tok_type(&r2, 0) == TOKEN_IDENT);

    arena_delete(arena);
    return ok;
}

bool test_lex_unterminated_block_comment(void) {
    bool ok = true;
    Arena *arena = arena_new(0);

    LexResult r = lex(arena, "/* no closing star-slash");
    ok &= ASSERT(r.lexer.errors.len > 0);

    // Token before an unterminated block comment is still valid
    LexResult r2 = lex(arena, "42 /* oops");
    ok &= ASSERT(tok_type(&r2, 0) == TOKEN_INT_LIT);
    ok &= ASSERT(r2.lexer.errors.len > 0);

    arena_delete(arena);
    return ok;
}

// Integration: lex a realistic expression and check the full token sequence.
bool test_lex_expression(void) {
    bool ok = true;
    Arena *arena = arena_new(0);

    // "fn add(a: i32, b: i32) i32 { return a + b; } "
    LexResult r = lex(arena, "fn add(a: i32, b: i32) i32 { return a + b; } ");

    TokenType expected[] = {
        TOKEN_FN,
        TOKEN_IDENT,    // add
        TOKEN_LPAREN,
        TOKEN_IDENT,    // a
        TOKEN_COLON,
        TOKEN_I32,
        TOKEN_COMMA,
        TOKEN_IDENT,    // b
        TOKEN_COLON,
        TOKEN_I32,
        TOKEN_RPAREN,
        TOKEN_I32,
        TOKEN_LBRACE,
        TOKEN_RETURN,
        TOKEN_IDENT,    // a
        TOKEN_PLUS,
        TOKEN_IDENT,    // b
        TOKEN_SEMICOLON,
        TOKEN_RBRACE,
        TOKEN_EOF,
    };

    ok &= ASSERT(r.tokens.len == sizeof(expected) / sizeof(expected[0]));
    for (size_t i = 0; i < r.tokens.len && i < sizeof(expected)/sizeof(expected[0]); i++) {
        ok &= ASSERT(tok_type(&r, i) == expected[i]);
    }

    arena_delete(arena);
    return ok;
}

// ============================================================================
// Aggregator
// ============================================================================

static bool lexer_tests(void) {
    bool ok = true;

    struct { TestFn func; const char *name; } tests[] = {
        { test_lex_empty,              "test_lex_empty"              },
        { test_lex_int_literal,        "test_lex_int_literal"        },
        { test_lex_float_literal,      "test_lex_float_literal"      },
        { test_lex_minus_operator,     "test_lex_minus_operator"     },
        { test_lex_string_literal,     "test_lex_string_literal"     },
        { test_lex_identifier,         "test_lex_identifier"         },
        { test_lex_keywords,           "test_lex_keywords"           },
        { test_lex_single_char_ops,    "test_lex_single_char_ops"    },
        { test_lex_double_char_ops,    "test_lex_double_char_ops"    },
        { test_lex_triple_char_ops,    "test_lex_triple_char_ops"    },
        { test_lex_line_comment,       "test_lex_line_comment"       },
        { test_lex_block_comment,      "test_lex_block_comment"      },
        { test_lex_source_location,    "test_lex_source_location"    },
        { test_lex_multichar_op_col,   "test_lex_multichar_op_col"   },
        { test_lex_unknown_char,               "test_lex_unknown_char"               },
        { test_lex_float_trailing_dot,         "test_lex_float_trailing_dot"         },
        { test_lex_long_identifier,            "test_lex_long_identifier"            },
        { test_lex_unterminated_string,        "test_lex_unterminated_string"        },
        { test_lex_unterminated_block_comment, "test_lex_unterminated_block_comment" },
        { test_lex_expression,                 "test_lex_expression"                 },
    };

    for (size_t i = 0; i < sizeof(tests) / sizeof(tests[0]); i++) {
        bool res = tests[i].func();
        if (!res) fprintf(stderr, "test %s failed.\n", tests[i].name);
        ok &= res;
    }
    return ok;
}

TestSuite lexer_test_suite(void) {
    return (TestSuite){ .name = "Lexer", .func = lexer_tests };
}
