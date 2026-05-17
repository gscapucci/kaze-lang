#include "../include/arena.h"
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

static inline size_t align_forward(size_t ptr, size_t alignment) {
    size_t mask = alignment - 1;
    return (ptr + mask) & ~mask;
}

static inline size_t arena_max(size_t a, size_t b) {
    return a > b ? a : b;
}

static inline ArenaBlock *block_new(size_t cap) {
    ArenaBlock *block = malloc(sizeof(ArenaBlock) + cap);
    if (!block) {
        fprintf(stderr, "malloc error\n");
        exit(1);
    }

    block->cap = cap;
    block->used = 0;
    block->next = NULL;

    return block;
}

Arena *arena_new(size_t default_block_cap) {
    Arena *new_arena = malloc(sizeof(Arena));
    if(new_arena == NULL) {
        fprintf(stderr, "malloc error in %s\n", __func__);
        exit(1);
    }
    new_arena->default_block_cap = arena_max(default_block_cap, ARENA_BLOCK_SIZE);
    new_arena->alignment = 8;
    
    new_arena->current = NULL;
    new_arena->head = NULL;

    new_arena->tracked = NULL;
    new_arena->tracked_cap = 0;
    new_arena->tracked_len = 0;

    return new_arena;
}

void arena_delete(Arena *arena) {
    arena_clear(arena);
    free(arena->tracked);
    free(arena);
}

void *arena_alloc(Arena *arena, size_t size) {
    if (!arena || size == 0) return NULL;

    size = align_forward(size, arena->alignment);

    ArenaBlock *b = arena->current;

    if (!b) {
        b = block_new(arena->default_block_cap);
        arena->head = b;
        arena->current = b;
    }

    if (b->used + size > b->cap) {
        ArenaBlock *new_block = block_new(arena_max(size, arena->default_block_cap));
        b->next = new_block;
        arena->current = new_block;
        b = new_block;
    }

    void *ptr = b->data + b->used;
    b->used += size;

    return ptr;
}

void *arena_alloc_zeroed(Arena *arena, size_t size) {
    if(arena == NULL || size == 0) return NULL;
    void *ptr = arena_alloc(arena, size);
    memset(ptr, 0, size);
    return ptr;
}

void *arena_alloc_reallocable(Arena *arena, size_t size) {
    if (!arena || size == 0) return NULL;

    if (arena->tracked_len >= arena->tracked_cap) {
        size_t new_cap = arena->tracked_cap == 0 ? 8 : arena->tracked_cap * 2;
        void **new_tracked = realloc(arena->tracked, new_cap * sizeof(void *));
        if (!new_tracked) { fprintf(stderr, "realloc error\n"); exit(1); }
        arena->tracked = new_tracked;
        arena->tracked_cap = new_cap;
    }

    void *data = malloc(size);
    if (!data) { fprintf(stderr, "malloc error\n"); exit(1); }
    arena->tracked[arena->tracked_len++] = data;
    return data;
}

void *arena_realloc(Arena *arena, void *ptr, size_t new_size) {
    if (!arena || !ptr) return NULL;

    for (size_t i = 0; i < arena->tracked_len; i++) {
        if (ptr == arena->tracked[i]) {
            void *new_ptr = realloc(arena->tracked[i], new_size);
            if (!new_ptr) { fprintf(stderr, "realloc error\n"); exit(1); }
            arena->tracked[i] = new_ptr;
            return new_ptr;
        }
    }

    fprintf(stderr, "arena_realloc: pointer not tracked\n");
    exit(1);
}



void arena_clear(Arena *arena) {
    arena_clear_normal(arena);
    arena_clear_reallocable(arena);
}
void arena_clear_normal(Arena *arena) {
    ArenaBlock *b = arena->head;

    while (b) {
        ArenaBlock *next = b->next;
        free(b);
        b = next;
    }

    arena->head = NULL;
    arena->current = NULL;
}

void arena_clear_reallocable(Arena *arena) {
    if(!arena) return;
    for(size_t i = 0; i < arena->tracked_len; i++) {
        free(arena->tracked[i]);
    }
    arena->tracked_len = 0;
}

void arena_set_alignment(Arena *arena, size_t alignment) {
    arena->alignment = alignment;
}
void arena_set_default_block_cap(Arena *arena, size_t cap) {
    arena->default_block_cap = cap;
}