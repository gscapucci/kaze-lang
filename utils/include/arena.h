#ifndef ARENA_H
#define ARENA_H

#include <stddef.h>
#include <stdbool.h>

#define KB(n) ((n) * 1024)
#define ARENA_BLOCK_SIZE ((size_t)KB(4))
#define ARENA_ALIGNMENT 8

typedef struct ArenaBlock ArenaBlock;
typedef struct Arena Arena;

struct ArenaBlock {
    size_t cap;
    size_t used;
    ArenaBlock *next;
    char data[];
};

struct Arena {
    ArenaBlock *normal_head;
    ArenaBlock *normal_current;
    
    ArenaBlock *reallocable_head;
    ArenaBlock *reallocable_current;
    
    size_t default_block_cap;
    size_t alignment;
};

// minimal block size is 4MB - will be set to max(default_block_cap, MB(4))
Arena *arena_new(size_t default_block_cap);
void arena_delete(Arena *arena);


void *arena_alloc(Arena *arena, size_t size);
void *arena_alloc_zeroed(Arena *arena, size_t size);

void *arena_alloc_reallocable(Arena *arena, size_t size);
void *arena_realloc(Arena *arena, void *ptr, size_t new_size);
void arena_free(Arena *arena, void *ptr, size_t size);  // opcional

void arena_clear(Arena *arena);
void arena_clear_normal(Arena *arena);
void arena_clear_reallocable(Arena *arena);

void arena_set_alignment(Arena *arena, size_t alignment);
void arena_set_default_block_cap(Arena *arena, size_t cap);

#endif /* ARENA_H */