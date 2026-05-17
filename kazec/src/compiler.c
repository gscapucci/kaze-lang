#include "../include/compiler.h"
#include "../include/lexer.h"
#include "../../utils/include/arena.h"

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

char *read_file(const char *path, Arena *arena) {
    FILE *file = fopen(path, "r");
    if (!file) {
        fprintf(stderr, "could not open file '%s'\n", path);
        exit(1);
    }

    fseek(file, 0, SEEK_END);
    size_t size = ftell(file);
    fseek(file, 0, SEEK_SET);

    char *str = arena_alloc(arena, size + 1);
    size_t read = fread(str, 1, size, file);
    if (read != size) {
        fprintf(stderr, "fread(%zu) != fsize(%zu)\n", read, size);
        exit(1);
    }
    str[size] = '\0';

    fclose(file);
    return str;
}

static bool is_white_space(char c) {
    return c == ' ' || c == '\t' || c == '\n';
}

char *trim(char** input_file) {
    if(input_file == NULL || *input_file == NULL) return NULL;
    
    char *last = &(*input_file)[strlen(*input_file) - 1];
    
    while(is_white_space(**input_file)) (*input_file)++;
    while(is_white_space(*last)) last--;
    
    *(last + 1) = 0;
    
    return *input_file;
}

int compile(CompilerOpt *opt) {
    int ret = 0;
    Arena *arena = arena_new(0);
    
    char *input_file = read_file(opt->input_file, arena);
    input_file = trim(&input_file);
    
    Lexer lexer;
    lexer_init(&lexer, input_file, arena);
    
    TokenVec tokens = lexer_get_tokens(&lexer);
    if(lexer.errors.len > 0) {
        for(size_t i = 0; i < 0; i++) {
            fprintf(stderr, "Error: %s\n", ErrorVec_get(&lexer.errors, i));
        }
        arena_delete(arena);
        return 1;
    }
    if(opt_has(&opt->opts, OPT_DUMP_TOKENS)) {
        for(size_t i = 0; i < tokens.len; i++) {
            Token token = TokenVec_get(&tokens, i);
            printf("Token: '%.*s'(%zu:%zu)\n", (int)token.lexeme.len, token.lexeme.data, token.line, token.col);
        }
        arena_delete(arena);
        return 0;
    }

    // Parser parser;
    // parser_init(&parser, tokens, arena);
    // Program program = parse(&parser, tokens);
    // if(program)
    arena_delete(arena);
    return ret;
}