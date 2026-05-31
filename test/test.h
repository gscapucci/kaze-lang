#ifndef TEST_H
#define TEST_H

#include "../utils/include/arena.h"

#include <stdbool.h>
#include <stddef.h>

typedef struct TestFramework TestFramework;
typedef struct TestSuite TestSuite;
typedef struct TestSuiteVec TestSuiteVec;
typedef bool (*TestFn)(void);


struct TestSuite {
    const char *name;
    TestFn func;
};

struct TestSuiteVec {
    TestSuite *data;
    size_t cap;
    size_t len;
};

struct TestFramework {
    TestSuiteVec suites;
    size_t total_tests;
    size_t total_failed;
    size_t total_passed;
};

static Arena *global_arena = NULL;

static TestFramework *framework = NULL;

void framework_init();
void framework_deinit();
void framework_run_tests();
void framework_add_suite(TestSuite suite);


#endif /* TEST_H */