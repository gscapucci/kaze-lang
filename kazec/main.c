#include "include/compiler.h"

int main(int argc, char **argv) {
    CompilerOpt opts;
    parse_opts(&opts, argc, (const char **)argv);
    return compile(&opts);
}