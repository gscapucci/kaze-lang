#ifndef HASHMAP_H
#define HASHMAP_H

#include "../../kazec/include/token.h"
#include "arena.h"

typedef struct HashMapEntry HashMapEntry;
typedef struct HashMap HashMap;
typedef struct HashMapIter HashMapIter;

struct HashMapEntry {
    const char *key;
    void *value;
    struct HashMapEntry *next;
};

struct HashMap {
    HashMapEntry **buckets;
    size_t size;
    size_t count;
    Arena *arena;
};

struct HashMapIter {
    HashMap* map;
    size_t bucket_index;
    HashMapEntry* current;
};

HashMap hashmap_create(Arena *arena, size_t initial_size);
void hashmap_put(HashMap* map, const char* key, void* value);
void* hashmap_get(HashMap* map, const char* key);
bool hashmap_contains(HashMap* map, const char* key);
size_t hashmap_size(HashMap *map);
size_t hashmap_count(HashMap *map);
bool hashmap_is_empty(HashMap *map);


HashMapIter hashmap_iter(HashMap* map);
bool hashmap_next(HashMapIter* iter, const char** key, void** value);

#endif