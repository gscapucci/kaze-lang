#include "hashmap_test.h"
#include <stddef.h>
#define ASSERT(x) \
    (!!(x) ? true : ( \
        fprintf(stderr, "%s:%s:%d: assertion failed: %s\n", __FILE__, __func__, __LINE__, #x), \
        false \
    ))

#include <stdio.h>
#include <string.h>
bool hashmap_new_map_test() {
    bool ok = true;
    Arena *arena = arena_new(0);
    HashMap map = hashmap_create(arena, 20);
    ok &= ASSERT(arena != NULL);
    ok &= ASSERT(map.arena != NULL);
    ok &= ASSERT(map.buckets != NULL);
    for(size_t i = 0; i < map.size; i++) {
        ok &= ASSERT(map.buckets[i] == NULL);
    }
    ok &= ASSERT(map.count == 0);
    ok &= ASSERT(map.size = 20);
    ok &= ASSERT(hashmap_count(&map) == 0);
    ok &= ASSERT(hashmap_size(&map) == 20);
    ok &= ASSERT(hashmap_is_empty(&map));
    arena_delete(arena);
    return ok;
}
bool hashmap_put_and_get_test() {
    bool ok = true;
    
    Arena *arena = arena_new(0);
    HashMap map = hashmap_create(arena, 10);

    for(size_t i = 0; i < 100; i++) {
        
        char *c = arena_alloc(arena, 3 * sizeof(char));
        snprintf(c, 3 * sizeof(char), "%zu", i);
        
        size_t *i2 = arena_alloc(arena, sizeof(size_t));
        *i2 = i;
        hashmap_put(&map, c, i2);

        ok &= ASSERT(map.count == (i + 1));
    }
    for(size_t i = 0; i < 100; i++) {
        char *c = arena_alloc(arena, 3 * sizeof(char));
        snprintf(c, 3 * sizeof(char), "%zu", 99 - i);
        size_t *i2 = hashmap_get(&map, c);
        ok &= ASSERT(i2 != NULL);
        ok &= ASSERT(*i2 == (99 - i));
    }
    arena_delete(arena);
    return ok;
}
bool hashmap_contains_test() {
    bool ok = true;
    
    Arena *arena = arena_new(0);
    HashMap map = hashmap_create(arena, 10);

    for(size_t i = 0; i < 100; i++) {
        
        char *c = arena_alloc(arena, 3 * sizeof(char));
        snprintf(c, 3 * sizeof(char), "%zu", i);
        
        size_t *i2 = arena_alloc(arena, sizeof(size_t));
        
        hashmap_put(&map, c, i2);

        ok &= ASSERT(map.count == (i + 1));
    }

    for(size_t i = 0; i < 100; i++) {
        char *c = arena_alloc(arena, 3 * sizeof(char));
        snprintf(c, 3 * sizeof(char), "%zu", 99 - i);
        ok &= ASSERT(hashmap_contains(&map, c));
    }
    
    return ok;
}
bool hashmap_iter_test() {
    bool ok = true;

    Arena *arena = arena_new(0);
    HashMap map = hashmap_create(arena, 10);

    for(size_t i = 0; i < 100; i++) {
        
        char *c = arena_alloc(arena, 3 * sizeof(char));
        snprintf(c, 3 * sizeof(char), "%zu", i);
        
        size_t *i2 = arena_alloc(arena, sizeof(size_t));
        *i2 = i;
        hashmap_put(&map, c, i2);

        ok &= ASSERT(map.count == (i + 1));
    }

    HashMapIter iter = hashmap_iter(&map);
    char *key;
    size_t *val;
    size_t count = 0;
    while(hashmap_next(&iter, (const char **)&key, (void **)&val)) {
        char str[3] = {0};
        snprintf(str, sizeof(str), "%zu", *val);
        ok &= ASSERT(strcmp(key, str) == 0);
        count++;
    }
    ok &= ASSERT(count == 100);
    return ok;
}

bool hashmap_put_overwrite_test() {
    bool ok = true;
    Arena *arena = arena_new(0);
    HashMap map = hashmap_create(arena, 10);

    size_t *v1 = arena_alloc(arena, sizeof(size_t)); *v1 = 1;
    size_t *v2 = arena_alloc(arena, sizeof(size_t)); *v2 = 2;

    hashmap_put(&map, "key", v1);
    ok &= ASSERT(map.count == 1);

    // Putting the same key again must update the value, not grow count
    hashmap_put(&map, "key", v2);
    ok &= ASSERT(map.count == 1);

    size_t *got = hashmap_get(&map, "key");
    ok &= ASSERT(got != NULL);
    ok &= ASSERT(*got == 2);

    arena_delete(arena);
    return ok;
}

bool hashmap_get_missing_test() {
    bool ok = true;
    Arena *arena = arena_new(0);
    HashMap map = hashmap_create(arena, 10);

    // Get on empty map must return NULL
    ok &= ASSERT(hashmap_get(&map, "missing") == NULL);

    // Get on non-existent key in non-empty map must return NULL
    size_t v = 42;
    hashmap_put(&map, "present", &v);
    ok &= ASSERT(hashmap_get(&map, "missing") == NULL);
    ok &= ASSERT(hashmap_get(&map, "present") == &v);

    arena_delete(arena);
    return ok;
}

bool hashmap_contains_missing_test() {
    bool ok = true;
    Arena *arena = arena_new(0);
    HashMap map = hashmap_create(arena, 10);

    ok &= ASSERT(!hashmap_contains(&map, "missing"));

    size_t v = 1;
    hashmap_put(&map, "present", &v);
    ok &= ASSERT(!hashmap_contains(&map, "missing"));
    ok &= ASSERT(hashmap_contains(&map, "present"));

    arena_delete(arena);
    return ok;
}

bool hashmap_empty_iter_test() {
    bool ok = true;
    Arena *arena = arena_new(0);
    HashMap map = hashmap_create(arena, 10);

    HashMapIter iter = hashmap_iter(&map);
    const char *key;
    void *val;
    // First call must return false immediately for an empty map
    ok &= ASSERT(!hashmap_next(&iter, &key, &val));

    arena_delete(arena);
    return ok;
}

bool hashmap_is_empty_after_put_test() {
    bool ok = true;
    Arena *arena = arena_new(0);
    HashMap map = hashmap_create(arena, 10);

    ok &= ASSERT(hashmap_is_empty(&map));

    size_t v = 1;
    hashmap_put(&map, "k", &v);
    ok &= ASSERT(!hashmap_is_empty(&map));

    arena_delete(arena);
    return ok;
}

bool hashmap_zero_size_init_test() {
    bool ok = true;
    Arena *arena = arena_new(0);

    // Size 0 must be clamped to the internal minimum (32)
    HashMap map = hashmap_create(arena, 0);
    ok &= ASSERT(map.size >= 32);
    ok &= ASSERT(map.buckets != NULL);

    arena_delete(arena);
    return ok;
}

bool hashmap_tests() {
    bool ok = true;
    struct {
        TestFn func;
        const char *fname;
    } test_list[] = {
    {hashmap_new_map_test,           "hashmap_new_map_test"          },
    {hashmap_put_and_get_test,       "hashmap_put_and_get_test"      },
    {hashmap_contains_test,          "hashmap_contains_test"         },
    {hashmap_iter_test,              "hashmap_iter_test"             },
    {hashmap_put_overwrite_test,     "hashmap_put_overwrite_test"    },
    {hashmap_get_missing_test,       "hashmap_get_missing_test"      },
    {hashmap_contains_missing_test,  "hashmap_contains_missing_test" },
    {hashmap_empty_iter_test,        "hashmap_empty_iter_test"       },
    {hashmap_is_empty_after_put_test,"hashmap_is_empty_after_put_test"},
    {hashmap_zero_size_init_test,    "hashmap_zero_size_init_test"   }
    };
    for(size_t i = 0; i < (sizeof(test_list)/ sizeof(test_list[0])); i++) {
        bool res = test_list[i].func(); 
        if(res == false) fprintf(stderr, "test %s fail.\n", test_list[i].fname);
        ok &= res;
    }
    return ok;
}

TestSuite hashmap_suite() {
    TestSuite suite;
    suite.name = "HashMap";
    suite.func = hashmap_tests;
    return suite;
}