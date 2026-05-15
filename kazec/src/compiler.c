#include "../include/compiler.h"
#include "../include/lexer.h"
#include "../../utils/include/arena.h"

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

char *read_file(const char *path, Arena *arena) {
    FILE *file = fopen(path, "r");

    fseek(file, 0, SEEK_END);
    int size = ftell(file);
    fseek(file, 0, SEEK_SET);
    
    char *str = arena_alloc(arena, (size + 1) * sizeof(char));
    int read = fread(str, size, 1, file);
    if(read != size) {
        fprintf(stderr, "fread(%d) != fsize(%d)", read, size);
        exit(1);
    }
    str[size] = 0;

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
    Arena *arena = arena_new(0);
    
    char *input_file = read_file(opt->input_file, arena);
    input_file = trim(&input_file);
    
    Lexer lexer;
    lexer_init(&lexer, input_file, arena);
    
    TokenVec tokens = lexer_get_tokens(&lexer);
    if(opt_has(&opt->opts, OPT_DUMP_TOKENS)) {
        for(size_t i = 0; i < tokens.len; i++) {
            // print_token(TokenVec_get(&tokens, i));
        }
    }
    arena_delete(arena);
    return 0;
}