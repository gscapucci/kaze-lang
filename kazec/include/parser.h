#ifndef PARSER_H
#define PARSER_H

#include "../../utils/include/arena.h"
#include "ast.h"
#include "lexer.h"   // TokenVec, ErrorVec

#include <stddef.h>
#include <stdbool.h>

typedef struct Parser {
    TokenVec    tokens;
    size_t      pos;
    Arena      *arena;
    const char *filename;
    ErrorVec    errors;
    bool        had_error;
} Parser;

void    parser_init(Parser *p, TokenVec tokens, Arena *arena, const char *filename);
Program parse_program(Parser *p);

#endif /* PARSER_H */
