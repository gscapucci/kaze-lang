#include "test.h"

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

static void run_suite(TestSuite *suite) {
    printf("Running %s suite...\n", suite->name);
    if(suite->func()) {
        printf("%s suite SUCCESS!\n", suite->name);
    } else {
        printf("%s suite FAIL!\n", suite->name);
    }
}

void framework_init() {
    global_arena = arena_new(0);
    if(!global_arena) {
        fprintf(stderr, "malloc error.");
        exit(1);
    }
    framework = arena_alloc(global_arena, sizeof(TestFramework));
    if(!framework) {
        fprintf(stderr, "allocation fail");
        exit(1);
    }
    framework->suites.data = arena_alloc_reallocable(global_arena, 16 * sizeof(TestSuite));
    framework->suites.cap = 16;
    framework->suites.len = 0;
    framework->total_failed = 0;
    framework->total_passed = 0;
    framework->total_tests = 0;

}
void framework_deinit() {
    arena_delete(global_arena);
}

void framework_run_tests() {
    for(size_t i = 0; i < framework->suites.len; i++) {
        run_suite(&framework->suites.data[i]);
    }
}
void framework_add_suite(TestSuite suite) {
    if(framework->suites.len >= framework->suites.cap) {
        framework->suites.cap *= 2;
        framework->suites.data = arena_realloc(global_arena, framework->suites.data, framework->suites.cap * sizeof(TestSuite));
    }
    framework->suites.data[framework->suites.len++] = suite;
}
