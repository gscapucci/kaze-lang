#include "../include/hashmap.h"
#include <string.h>

static size_t hash_string(const char * s) {
    size_t hash = 5381;
    int c;
    while((c = *s++)) {
        hash = ((hash << 5) + hash) + c;
    }
    return hash;
}

HashMap hashmap_create(Arena *arena, size_t initial_size) {
    HashMap map;
    map.arena = arena;
    map.count = 0;
    map.size = initial_size;
    map.buckets = arena_alloc_reallocable(arena, initial_size * sizeof(HashMapEntry*));
    if (map.buckets) {
        memset(map.buckets, 0, initial_size * sizeof(HashMapEntry*));
    }
    return map;
}

void hashmap_put(HashMap* map, const char* key, void* value) {
    if(!map || !key) return;
    size_t index = hash_string(key) % map->size;
    
    HashMapEntry *entry = map->buckets[index];
    while(entry) {
        if(strcmp(entry->key, key) == 0) {
            entry->value = value;
            return;
        }
        entry = entry->next;
    }

    entry = arena_alloc(map->arena, sizeof(HashMapEntry));
    entry->key = key;
    entry->value = value;
    entry->next = map->buckets[index];
    map->buckets[index] = entry;
    map->count++;
}

void* hashmap_get(HashMap* map, const char* key) {
    if(!map || !key) return NULL;

    size_t index = hash_string(key) % map->size;

    HashMapEntry *entry = map->buckets[index];
    while(entry) {
        if(strcmp(entry->key, key) == 0) {
            return entry->value;
        }
        entry = entry->next;
    }
    return NULL;
}

bool hashmap_contains(HashMap* map, const char* key) {
    if(!map || !key) return false;

    size_t index = hash_string(key) % map->size;

    HashMapEntry *entry = map->buckets[index];
    while (entry) {
        if(strcmp(entry->key, key) == 0) {
            return true;
        }
        entry = entry->next;
    }
    return false;
}

HashMapIter hashmap_iter(HashMap* map) {
    HashMapIter iter;
    iter.map = map;
    iter.bucket_index = 0;
    iter.current = map->buckets[0];
    return iter;
}

bool hashmap_next(HashMapIter* iter, const char** key, void** value) {
    if(iter->current == NULL && iter->bucket_index == iter->map->size) return false;
    if(iter->current == NULL) {
        iter->bucket_index++;
        iter->current = iter->map->buckets[iter->bucket_index];
        return hashmap_next(iter, key, value);
    }
    *key = iter->current->key;
    *value = iter->current->value;
    iter->current = iter->current->next;
    return true;
}

size_t hashmap_size(HashMap *map) {
    if(!map) return 0;
    return map->size;
}

size_t hashmap_count(HashMap *map) {
    if(!map) return 0;
    return map->count;
}

bool hashmap_is_empty(HashMap *map) {
    return !map || map->count == 0;
}