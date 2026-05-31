#include "../include/compiler_opt.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static inline bool ends_with_kz(const char *s) {
    size_t slen = strlen(s);
    if (slen < 4) return false;  // mínimo: "x.kz"
    return strcmp(s + slen - 3, ".kz") == 0;
}

void parse_opts(CompilerOpt *opt, const int argc, const char **argv) {
    if(!opt) return;
    *opt = (CompilerOpt) {
        .input_file = NULL,
        .output_file = "out.c",
        .opts = 0,
    };
    for(int i = 1; i < argc; i++) {
        if(!strcmp(argv[i], "-h") || !strcmp(argv[i], "--help")) {
            opt_set(&opt->opts, OPT_PRINT_HELP);
            continue;
        }
        if(!strcmp(argv[i], "-v") || !strcmp(argv[i], "--version")) {
            opt_set(&opt->opts, OPT_PRINT_VERSION);
            continue;
        }
        if(!strcmp(argv[i], "-o") || !strcmp(argv[i], "--output")) {
            if(i + 1 >= argc) {
                fprintf(stderr, "Expeceted output name after '%s'.", argv[i]);
                exit(1);
            }
            i++;
            opt->output_file = argv[i];
            continue;
        }
        if(!strcmp(argv[i], "--dump-ast")) {
            opt_set(&opt->opts, OPT_DUMP_AST);
            continue;
        }
        if(!strcmp(argv[i], "--dump-tokens")) {
            opt_set(&opt->opts, OPT_DUMP_TOKENS);
            continue;
        }
        if(!strcmp(argv[i], "--dump-c")) {
            opt_set(&opt->opts, OPT_DUMP_C);
            continue;
        }
        if(!strcmp(argv[i], "--dump-types")) {
            opt_set(&opt->opts, OPT_DUMP_TYPES);
            continue;
        }
        if(ends_with_kz(argv[i])) {
            if(opt->input_file != NULL) {
                fprintf(stderr, "Expected only one input file");
                exit(1);
            }
            opt->input_file = argv[i];
            continue;
        }
    }
    if (opt->input_file == NULL && !opt_has(&opt->opts, OPT_PRINT_HELP) && !opt_has(&opt->opts, OPT_PRINT_VERSION)) {
        fprintf(stderr, "Expected input file");
        exit(1);
    }
}

void print_compiler_help() {
    printf("Usage: ./kazec [options] <input file>\n\n");
    
    printf("Options:\n");
    printf("  -h, --help                    Show this help message\n");
    printf("  -v, --version                 Show compiler version\n");
    printf("  -o, --output <file>           Output file name (default: a.out)\n");
    // printf("  -E                  Only preprocess, do not compile\n");
    // printf("  -S                  Generate assembly, do not assemble\n");
    // printf("  -c                  Compile to object file only\n");
    printf("  --dump-ast                    Dump AST and exit\n");
    printf("  --dump-tokens                 Dump token list and exit\n");
    printf("  --dump-types                  Type-check and dump resolved types\n");
    printf("  --dump-c                      Dump C code generated\n");
    // printf("  --verbose           Verbose output during compilation\n");
    
    // printf("\nOutput control:\n");
    // printf("  -I <dir>            Add directory to include search path\n");
    // printf("  -D <macro>=<value>  Define preprocessor macro\n");
    // printf("  -U <macro>          Undefine preprocessor macro\n");
    
    printf("\nOptimization:\n");
    printf("  -O0                 No optimization (default)\n");
    printf("  -O1                 Basic optimizations\n");
    printf("  -O2                 Standard optimizations\n");
    printf("  -O3                 Aggressive optimizations\n");
    printf("  -Os                 Optimize for size\n");
    
    // printf("\nWarning & errors:\n");
    // printf("  -Werror             Treat warnings as errors\n");
    // printf("  -Wall               Enable all warnings\n");
    // printf("  -Wno-unused         Disable unused variable warnings\n");
    // printf("  -Wshadow            Warn on variable shadowing\n");
    
    // printf("\nDebugging:\n");
    // printf("  -g                  Generate debug information\n");
    // printf("  --debug-lex         Debug lexer output\n");
    // printf("  --debug-parse       Debug parser output\n");
    // printf("  --debug-sema        Debug semantic analysis\n");
    
    // printf("\nTarget options:\n");
    // printf("  --target=<triple>   Target triple (default: x86_64-linux)\n");
    // printf("  -m32                Generate 32-bit code\n");
    // printf("  -m64                Generate 64-bit code\n");
    
    printf("\nExamples:\n");
    printf("  ./kazec main.kz\n");
    printf("  ./kazec -o myprogram main.kz\n");
    printf("  ./kazec -c -O2 main.kz\n");
    printf("  ./kazec -I./include -DDEBUG main.kz\n");
    
//     printf("\nEnvironment variables:\n");
//     printf("  NG_PATH         Additional module search paths\n");
//     printf("  NG_C_COMPILER   C compiler to use (default: gcc)\n");
//     printf("  NG_CFLAGS       Extra flags for C compiler\n");
}


void print_version() {
    printf("%s", VERSION);
}