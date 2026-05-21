#include "../include/lexer.h"
#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <stdlib.h>

VECTOR_IMPLEMENTATION(Token, Token)
VECTOR_IMPLEMENTATION(char *, Error)



#define ARR_LEN(arr) (sizeof(arr)/sizeof(arr[0]))

static inline Token make_undefine_token() {
    return (Token) {.type = TOKEN_UNDEFINED};
}

static void keyword_map_init(HashMap *map, Arena *arena) {
    if(!map) return;

    static struct {
        const char *key;
        TokenType type;
    } keywords_kv[] = {
        {"fn", TOKEN_FN},
        {"var", TOKEN_VAR},
        {"const", TOKEN_CONST},
        {"comptime", TOKEN_COMPTIME},
        {"when", TOKEN_WHEN},
        {"i8", TOKEN_I8},
        {"i16", TOKEN_I16},
        {"i32", TOKEN_I32},
        {"i64", TOKEN_I64},
        {"u8", TOKEN_U8},
        {"u16", TOKEN_U16},
        {"u32", TOKEN_U32},
        {"u64", TOKEN_U64},
        {"struct", TOKEN_STRUCT},
        {"return", TOKEN_RETURN},
        {"union", TOKEN_UNION},
    };
    
    *map = hashmap_create(arena, 0);
    for(size_t i = 0; i < ARR_LEN(keywords_kv); i++) {
        hashmap_put(map, keywords_kv[i].key, &keywords_kv[i].type);
    }
}

static inline bool lexer_is_at_end(Lexer *lexer) {
    return lexer->pos == lexer->len;
}

static inline char lexer_peek(Lexer *lexer) {
    return lexer->source[lexer->pos];
}

static inline char lexer_peek_next(Lexer *lexer) {
    if(lexer->pos >= lexer->len) return 0;
    return lexer->source[lexer->pos + 1];
}

static inline char lexer_advance(Lexer *lexer) {
    if(lexer->pos >= lexer->len) return 0;
    char c = lexer_peek(lexer);
    lexer->col++;
    if(c == '\n') {
        lexer->line++;
        lexer->col = 1;
    }
    lexer->pos++;
    return c;
}

static inline bool is_digit(char c) {
    return c >= '0' && c <= '9';
}

static inline bool is_alpha(char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_';
}

static inline bool is_alphanumeric(char c) {
    return is_digit(c) || is_alpha(c);
}

static inline void lexer_emit_error(Lexer *lexer, const char *fmt, ...) {
    if(!lexer || !fmt) return;
    
    va_list args;
    va_start(args, fmt);

    va_list args_copy;
    va_copy(args_copy, args);
    int needed = vsnprintf(NULL, 0, fmt, args_copy);
    va_end(args_copy);

    if(needed < 0) {
        va_end(args);
        return;
    }

    char *string = arena_alloc(lexer->arena, (size_t)needed + 1);
    if(!string) {
        va_end(args);
        return;
    }

    vsnprintf(string, (size_t)needed + 1, fmt, args);

    va_end(args);

    ErrorVec_push(&lexer->errors, string);
}

static Token create_error_token(Lexer *lexer) {
    return (Token) {
        .lexeme = string_view_create(&lexer->source[lexer->pos], 0),
        .line = 0,
        .col = 0,
        .type = TOKEN_ERROR,
    };
}

static void skip_white_space(Lexer *lexer) {
    while (true) {
        while (isspace(lexer_peek(lexer))) {
            lexer_advance(lexer);
        }

        // comentário de linha
        if (lexer_peek(lexer) == '/' && lexer_peek_next(lexer) == '/') {
            while (!lexer_is_at_end(lexer) && lexer_peek(lexer) != '\n')
                lexer_advance(lexer);
            continue;
        }

        // comentário de bloco
        if (lexer_peek(lexer) == '/' && lexer_peek_next(lexer) == '*') {
            lexer_advance(lexer); // /
            lexer_advance(lexer); // *
            while (!lexer_is_at_end(lexer)) {
                if (lexer_peek(lexer) == '*' && lexer_peek_next(lexer) == '/') {
                    lexer_advance(lexer); // *
                    lexer_advance(lexer); // /
                    break;
                }
                lexer_advance(lexer);
            }
            continue;
        }

        break;
    }
}

static Token lexer_number(Lexer *lexer) {
    bool is_float = false;
    size_t start_pos = lexer->pos;
    size_t line = lexer->line;
    size_t col = lexer->col;

    while(is_digit(lexer_peek(lexer))) lexer_advance(lexer);

    if(lexer_peek(lexer) == '.') {
        lexer_advance(lexer);
        
        if(!is_digit(lexer_peek(lexer))) {
            lexer_emit_error(lexer, "expected digit after decimal point");
            return create_error_token(lexer);
        }
        while(is_digit(lexer_peek(lexer))) lexer_advance(lexer);
    }

    return create_token(
            is_float ? TOKEN_FLOAT_LIT : TOKEN_INT_LIT,
            string_view_create(&lexer->source[start_pos], (lexer->pos - start_pos)),
            line, col
        );
}

static TokenType check_keyword(Lexer *lexer, const StringView view) {
    if(view.len > 63) return TOKEN_IDENT;
    
    char rep[64] = {0};
    memcpy(rep, view.data, view.len);

    TokenType *type = hashmap_get(&lexer->keyword_map, rep);
    return type == NULL ? TOKEN_IDENT : *type;
}

static Token lexer_ident(Lexer *lexer) {
    size_t pos = lexer->pos;
    size_t line = lexer->line;
    size_t col = lexer->col;
    while(is_alphanumeric(lexer_peek(lexer))) lexer_advance(lexer);
    const StringView view = string_view_create(&lexer->source[pos], lexer->pos - pos);
    TokenType type = check_keyword(lexer, view);
    return create_token(type, view, line, col);
}

static Token lexer_triple_char_op(Lexer *lexer) {
    struct {
        const char *op;
        const TokenType type;
    } op[] = {
        {"<<=", TOKEN_LESS_LESS_EQ},
        {">>=", TOKEN_GREATER_GREATER_EQ},
    };
    for(size_t i = 0; i < ARR_LEN(op); i++) {
        const char *operator = op[i].op;
        const TokenType type = op[i].type;
        const char *lexer_curr = &lexer->source[lexer->pos];
        if(!strncmp(operator, lexer_curr, strlen(operator))) {
            lexer->pos += strlen(operator);
            return create_token(type, string_view_create(lexer_curr, strlen(operator)), lexer->line, lexer->col);
        }
    }
    return make_undefine_token();
}

static Token lexer_double_char_op(Lexer *lexer) {
    struct {
        const char *op;
        TokenType type;
    } op[] = {
        {"<<", TOKEN_LESS_LESS},
        {">>", TOKEN_GREATER_GREATER},
        {"<=", TOKEN_LESS_EQ},
        {">=", TOKEN_GREATER_EQ},
        {"+=", TOKEN_PLUS_EQ},
        {"-=", TOKEN_MINUS_EQ},
        {"*=", TOKEN_STAR_EQ},
        {"/=", TOKEN_SLASH_EQ},
        {"==", TOKEN_EQ_EQ},
        {"!=", TOKEN_BANG_EQ},
        {"%=", TOKEN_MOD_EQ},
    };
    for(size_t i = 0; i < ARR_LEN(op); i++) {
        const char *operator = op[i].op;
        TokenType type = op[i].type;
        const char *lexer_curr = &lexer->source[lexer->pos];
        if(!strncmp(operator, lexer_curr, strlen(operator))) {
            lexer->pos += strlen(operator);
            return create_token(type, string_view_create(lexer_curr, strlen(operator)), lexer->line, lexer->col);
        }
    }
    return make_undefine_token();
}
static Token lexer_single_char_op(Lexer *lexer) {
    struct {
        const char *op;
        TokenType type;
    } op[] = {
        {"=", TOKEN_EQ},
        {".", TOKEN_DOT},
        {"*", TOKEN_STAR},
        {"+", TOKEN_PLUS},
        {"/", TOKEN_SLASH},
        {"%", TOKEN_MOD},
        {"<", TOKEN_LESS},
        {">", TOKEN_GREATER},
        {"{", TOKEN_LBRACE},
        {"}", TOKEN_RBRACE},
        {";", TOKEN_SEMICOLON},
        {":", TOKEN_COLON},
        {",", TOKEN_COMMA},
        {"(", TOKEN_LPAREN},
        {")", TOKEN_RPAREN},
        {"[", TOKEN_LBRACKET},
        {"]", TOKEN_RBRACKET},
        {"!", TOKEN_BANG},
        {"@", TOKEN_AT}
    };
    for(size_t i = 0; i < ARR_LEN(op); i++) {
        const char *operator = op[i].op;
        const TokenType type = op[i].type;
        const char *lexer_curr = &lexer->source[lexer->pos];
        if(!strncmp(operator, lexer_curr, strlen(operator))) {
            lexer->pos += strlen(operator);
            return create_token(type, string_view_create(lexer_curr, strlen(operator)), lexer->line, lexer->col);
        }
    }
    return make_undefine_token();
}

static Token lexer_string(Lexer *lexer) {
    size_t pos = lexer->pos;
    size_t line = lexer->line;
    size_t col = lexer->col;
    lexer_advance(lexer);
    while(lexer_peek(lexer) != '"') {
        if(lexer_is_at_end(lexer)) {
            fprintf(stderr, "Unterminated string.\n");
            exit(1);
        }
        char c = lexer_advance(lexer);
        if(c == '\\' && lexer_peek(lexer) == '"') lexer_advance(lexer);
    }
    lexer_advance(lexer);
    return create_token(TOKEN_STRING_LIT, string_view_create(&lexer->source[pos + 1], lexer->pos - pos - 2), line, col);
}

static Token lexer_next_token(Lexer *lexer) {
    skip_white_space(lexer);
    if(lexer_is_at_end(lexer)) {
        return create_token(
            TOKEN_EOF,
            string_view_create(&lexer->source[lexer->pos], 0),
            lexer->line, 
            lexer->col
        );
    }
    char c = lexer_peek(lexer);
    if(is_digit(c)) {
        return lexer_number(lexer);
    }
    if(c == '"') {
        return lexer_string(lexer);
    }
    
    if(is_alpha(c)) {
        return lexer_ident(lexer);
    }

    Token token = lexer_triple_char_op(lexer);
    token = token.type == TOKEN_UNDEFINED ? lexer_double_char_op(lexer) : token;
    token = token.type == TOKEN_UNDEFINED ? lexer_single_char_op(lexer) : token;
    if(token.type != TOKEN_UNDEFINED) return token;

    lexer_emit_error(lexer, "Should be unreachable.\n");
    lexer_advance(lexer);
    return create_error_token(lexer);
}

void lexer_init(Lexer *lexer, const char *source, Arena *arena) {
    if(lexer == NULL || source == NULL || arena == NULL) return;
    lexer->arena = arena;
    lexer->source = source;
    lexer->len = strlen(source);
    lexer->pos = 0;
    lexer->col = 1;
    lexer->line = 1;
    keyword_map_init(&lexer->keyword_map, arena);
}

TokenVec lexer_get_tokens(Lexer *lexer) {
    if(!lexer) return (TokenVec) {0};
    TokenVec tokens = TokenVec_create(lexer->arena, 128);
    while(!lexer_is_at_end(lexer)) {
        Token token = lexer_next_token(lexer);
        TokenVec_push(&tokens, token);
        if(token.type == TOKEN_EOF) {
            break;
        }
    }
    return tokens;
}
