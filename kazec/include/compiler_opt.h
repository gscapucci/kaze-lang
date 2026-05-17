#ifndef COMPILER_OPT_H
#define COMPILER_OPT_H

#include <stdbool.h>
#include <stdint.h>

#define VERSION "0.0.1"

typedef struct CompilerOpt CompilerOpt; 

enum Opt: uint64_t {
    OPT_DUMP_TOKENS = (1 << 0),
    OPT_DUMP_AST = (1 << 1),
    OPT_DUMP_C = (1 << 2),
    OPT_PRINT_HELP = (1 << 3),
    OPT_PRINT_VERSION = (1 << 4)
};

static inline void opt_set(uint64_t *iopt, enum Opt eopt) {
    *iopt |= (uint64_t)eopt;
}
static inline bool opt_has(uint64_t *iopt, enum Opt eopt) {
    return (*iopt & (uint64_t)eopt) != 0;
};

struct CompilerOpt {
    const char * input_file;
    const char * output_file;
    uint64_t opts;
};



void print_compiler_help();
void print_version();
void parse_opts(CompilerOpt *result, const int argc, const char **argv);

#endif /* COMPILER_OPT_H */