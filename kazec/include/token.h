#ifndef TOKEN_H
#define TOKEN_H

#include "../../utils/include/string_view.h"

#include <stddef.h>

typedef struct Token Token;
typedef enum TokenType {
    TOKEN_EOF, // end of file
    TOKEN_UNDEFINED, // não definido ainda, se tornará um erro se não tratado, erro de copilação

    TOKEN_FLOAT_LIT, // 123.321
    TOKEN_INT_LIT, // 123
    TOKEN_STRING_LIT, // "Hello"
    TOKEN_ERROR, // invalid token
    TOKEN_IDENT, // var name, func name, identifiers

    TOKEN_LESS_LESS_EQ, // <<=
    TOKEN_GREATER_GREATER_EQ, // >>=

    TOKEN_ELLIPSIS, // ...

    TOKEN_LESS_LESS, // <<
    TOKEN_GREATER_GREATER, // >>
    TOKEN_LESS_EQ, // <=
    TOKEN_GREATER_EQ, // >=
    TOKEN_PLUS_EQ, // +=
    TOKEN_MINUS_EQ, // -=
    TOKEN_STAR_EQ, // *=
    TOKEN_SLASH_EQ, // /=
    TOKEN_EQ_EQ, // ==
    TOKEN_BANG_EQ, // !=
    TOKEN_MOD_EQ, // %=
    TOKEN_AMP_AMP, // &&
    TOKEN_PIPE_PIPE, // ||
    TOKEN_PLUS_PLUS, // ++
    TOKEN_MINUS_MINUS, // --
    TOKEN_ARROW, // ->
    TOKEN_FAT_ARROW, // =>
    TOKEN_COLON_COLON, // ::

    TOKEN_EQ, // =
    TOKEN_DOT, // .
    TOKEN_STAR, // *
    TOKEN_PLUS, // +
    TOKEN_MINUS, // -
    TOKEN_SLASH, // /
    TOKEN_MOD, // %
    TOKEN_LESS, // <
    TOKEN_GREATER, // >
    TOKEN_AMP, // &
    TOKEN_PIPE, // |
    TOKEN_CARET, // ^
    TOKEN_TILDE, // ~

    TOKEN_LBRACE, // {
    TOKEN_RBRACE, // }
    TOKEN_SEMICOLON, // ;
    TOKEN_COMMA, // ,
    TOKEN_COLON, // :
    TOKEN_LPAREN, // (
    TOKEN_RPAREN, // )
    TOKEN_LBRACKET, // [
    TOKEN_RBRACKET, // ]
    TOKEN_BANG, // !
    TOKEN_AT, // @
    

    //keywords
    TOKEN_FN,
    TOKEN_VAR,
    TOKEN_CONST,
    TOKEN_COMPTIME,
    TOKEN_WHEN,
    TOKEN_I8,
    TOKEN_I16,
    TOKEN_I32,
    TOKEN_I64,
    TOKEN_U8,
    TOKEN_U16,
    TOKEN_U32,
    TOKEN_U64,
    TOKEN_STRUCT,
    TOKEN_RETURN,
    TOKEN_UNION,
    TOKEN_LET,
    TOKEN_IF,
    TOKEN_ELSE,
    TOKEN_WHILE,
    TOKEN_FOR,
    TOKEN_IN,
    TOKEN_BREAK,
    TOKEN_CONTINUE,
    TOKEN_TRUE,
    TOKEN_FALSE,
    TOKEN_NULL,
    TOKEN_ENUM,
    TOKEN_IMPORT,
    TOKEN_MATCH,
    TOKEN_DEFER,
    TOKEN_PANIC,
    TOKEN_TRY,
    TOKEN_F32,
    TOKEN_F64,
    TOKEN_BOOL,
    TOKEN_VOID,
    TOKEN_TYPE,
} TokenType;

struct Token {
    TokenType type; // tipo
    StringView lexeme; // representação no codigo - pesquisar string_view
    size_t line; // linha
    size_t col; // coluna
};

Token create_token(TokenType type, StringView lexeme, size_t line, size_t col);

#endif /* TOKEN_H */