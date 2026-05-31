#include "test.h"
#include "utils/hashmap_test.h"
#include "kazec/lexer_test.h"
#include "kazec/ast_test.h"
#include "kazec/parser_test.h"
#include "kazec/type_test.h"
#include "kazec/scope_test.h"
#include "kazec/sema_test.h"

#include "../kazec/include/lexer.h"
#include "../kazec/include/parser.h"

#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define KZ_DIR "./test/kz"

// ============================================================================
// .kz example-file suite — lexes + parses each file in ./test/kz and fails
// on any lexer/parser error. New NNN_*.kz files are picked up automatically.
// ============================================================================

static char *read_file_z(const char *path, Arena *arena) {
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (sz < 0) { fclose(f); return NULL; }
    char *buf = arena_alloc(arena, (size_t)sz + 1);
    size_t rd = fread(buf, 1, (size_t)sz, f);
    buf[rd] = '\0';
    fclose(f);
    return buf;
}

// Parse a single file; returns true when it lexes and parses with no errors.
static bool kz_file_parses(const char *path) {
    Arena *arena = arena_new(0);
    bool ok = true;

    char *src = read_file_z(path, arena);
    if (!src) {
        fprintf(stderr, "  could not open '%s'\n", path);
        arena_delete(arena);
        return false;
    }

    Lexer lexer;
    lexer_init(&lexer, src, arena);
    TokenVec tokens = lexer_get_tokens(&lexer);

    Parser parser;
    parser_init(&parser, tokens, arena, path);
    parse_program(&parser);

    for (size_t i = 0; i < lexer.errors.len; i++) {
        fprintf(stderr, "  lex: %s\n", ErrorVec_get(&lexer.errors, i));
        ok = false;
    }
    for (size_t i = 0; i < parser.errors.len; i++) {
        fprintf(stderr, "  %s\n", ErrorVec_get(&parser.errors, i));
        ok = false;
    }

    arena_delete(arena);
    return ok;
}

static int cmp_name(const void *a, const void *b) {
    return strcmp((const char *)a, (const char *)b);
}

static bool kz_files_tests(void) {
    DIR *dir = opendir(KZ_DIR);
    if (!dir) {
        fprintf(stderr, "could not open directory '%s'\n", KZ_DIR);
        return false;
    }

    // Collect *.kz names, then sort for deterministic ordering.
    static char names[256][256];
    size_t n = 0;
    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL && n < 256) {
        size_t len = strlen(entry->d_name);
        if (len > 3 && strcmp(entry->d_name + len - 3, ".kz") == 0) {
            snprintf(names[n], sizeof(names[0]), "%s", entry->d_name);
            n++;
        }
    }
    closedir(dir);

    if (n == 0) {
        fprintf(stderr, "no .kz files found in '%s'\n", KZ_DIR);
        return false;
    }

    qsort(names, n, sizeof(names[0]), cmp_name);

    bool ok = true;
    for (size_t i = 0; i < n; i++) {
        char path[512];
        snprintf(path, sizeof(path), "%s/%s", KZ_DIR, names[i]);
        bool res = kz_file_parses(path);
        if (!res) fprintf(stderr, "kz file %s failed to parse.\n", names[i]);
        ok &= res;
    }
    return ok;
}

static TestSuite kz_files_suite(void) {
    return (TestSuite){ .name = "KzFiles", .func = kz_files_tests };
}

int main() {
    framework_init();

    framework_add_suite(hashmap_suite());
    framework_add_suite(lexer_test_suite());
    framework_add_suite(ast_suite());
    framework_add_suite(parser_test_suite());
    framework_add_suite(type_suite());
    framework_add_suite(scope_suite());
    framework_add_suite(sema_suite());
    framework_add_suite(kz_files_suite());
    framework_run_tests();
    framework_deinit();
    return 0;
}
