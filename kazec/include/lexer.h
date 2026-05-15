#ifndef LEXER_H
#define LEXER_H

#include "../../utils/include/arena.h"
#include "../../utils/include/gvector.h"
#include "token.h"

#include <stddef.h>

typedef struct Lexer Lexer;

VECTOR_DEFINITION(Token, Token)
VECTOR_DEFINITION(char *, Error)

struct Lexer {
    Arena *arena;
    const char *source;
    size_t len;
    size_t pos;
    size_t col;
    size_t line;
    ErrorVec errors;
};


void lexer_init(Lexer *lexer, const char *source, Arena *arena);
TokenVec lexer_get_tokens(Lexer *lexer);

#endif /* LEXER_H */