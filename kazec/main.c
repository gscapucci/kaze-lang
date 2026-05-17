#include "include/compiler.h"
#include "include/compiler_opt.h"

int main(int argc, char **argv) {
    CompilerOpt opts;
    parse_opts(&opts, argc, (const char **)argv);
    if (opt_has(&opts.opts, OPT_PRINT_HELP)) {
        print_compiler_help();
        return 0;
    }
    if (opt_has(&opts.opts, OPT_PRINT_VERSION)) {
        print_version();
        return 0;
    }
    return compile(&opts);
}