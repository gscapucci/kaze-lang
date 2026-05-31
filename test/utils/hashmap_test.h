#ifndef HASHMAP_TEST_H
#define HASHMAP_TEST_H

#include "../test.h"
#include "../../utils/include/hashmap.h"
#include "../../utils/include/arena.h"


bool hashmap_new_map_test();
bool hashmap_put_test();
bool hashmap_get_test();
bool hashmap_contains_test();
bool hashmap_iter_test();

TestSuite hashmap_suite();

#endif /* HASHMAP_TEST_H */