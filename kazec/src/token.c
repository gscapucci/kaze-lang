#include "../include/token.h"


Token create_token(TokenType type, StringView lexeme, size_t line, size_t col) {
    return (Token) {
        .type = type,
        .lexeme = lexeme,
        .line = line,
        .col = col,
    };
}