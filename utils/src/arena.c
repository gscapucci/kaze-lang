#include "../include/arena.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

static inline size_t align_forward(size_t ptr, size_t alignment) {
    size_t mask = alignment - 1;
    return (ptr + mask) & ~mask;
}

static inline size_t max(size_t a, size_t b) {
    return a > b ? a : b;
}

static inline ArenaBlock *block_new(size_t cap) {
    ArenaBlock *block = malloc(sizeof(ArenaBlock) * cap);
    if(block == NULL) {
        fprintf(stderr, "malloc error in %s\n", __func__);
        exit(1);
    }
    block->cap = cap;
    block->next = NULL;
    block->used = 0;
    return block;
} 

Arena *arena_new(size_t default_block_cap) {
    Arena *new_arena = malloc(sizeof(Arena));
    if(new_arena == NULL) {
        fprintf(stderr, "malloc error in %s\n", __func__);
        exit(1);
    }
    new_arena->default_block_cap = max(default_block_cap, ARENA_BLOCK_SIZE);
    new_arena->alignment = 8;
    
    new_arena->normal_current = NULL;
    new_arena->normal_head = NULL;

    new_arena->reallocable_head = NULL;
    new_arena->reallocable_current = NULL;

    return new_arena;
}

void arena_delete(Arena *arena) {
    arena_clear(arena);
    free(arena);
}

void *arena_alloc(Arena *arena, size_t size) {
    if(arena == NULL || size == 0) return NULL;
    
    size = align_forward(size, arena->alignment);
    
    if(arena->normal_head == NULL) {
        ArenaBlock *new_block = block_new(max(size, arena->default_block_cap));
        void *data = new_block->data;
        new_block->used += size;

        arena->normal_head = new_block;
        arena->normal_current = arena->normal_head;

        return data;
    }
    if(arena->normal_current->used + size > arena->normal_current->cap) {
        ArenaBlock *new_block = block_new(max(size, arena->default_block_cap));
        void *data = new_block->data;
        new_block->used += size;

        arena->normal_current->next = new_block;

        return data;
    }
    void *data = arena->normal_current->data + arena->normal_current->used;
    arena->normal_current->used += size;
    return data;

}

void *arena_alloc_zeroed(Arena *arena, size_t size) {
    if(arena == NULL || size == 0) return NULL;
    void *ptr = arena_alloc(arena, size);
    memset(ptr, 0, size);
    return ptr;
}

void *arena_alloc_reallocable(Arena *arena, size_t size) {
    if(arena == NULL || size == 0) return NULL;

    ArenaBlock *new_block = block_new(size);
    void *ptr = new_block->data;
    
    if(arena->reallocable_current == NULL) {
        

        arena->reallocable_head = new_block;
        arena->reallocable_current = arena->reallocable_head;

        return ptr;
    }
    arena->reallocable_current->next = new_block;
    return ptr;
}

void *arena_realloc(Arena *arena, void *ptr, size_t new_size) {
    if(arena == NULL) return NULL;
    if(ptr == NULL) {
        fprintf(stderr, "Could not realloc NULL pointer\n");
        exit(1);
    }
    ArenaBlock *curr = arena->reallocable_head;
    while (ptr != NULL && ptr != curr->data) curr = curr->next;
    if(ptr == NULL) {
        fprintf(stderr, "ptr not found in the arena\n");
        exit(1);
    }
    curr = realloc(curr, sizeof(ArenaBlock) + new_size);
    return curr->data;
}

void arena_clear(Arena *arena) {
    arena_clear_normal(arena);
    arena_clear_reallocable(arena);
}
void arena_clear_normal(Arena *arena) {
    ArenaBlock *curr = arena->normal_head;
    while(curr != NULL) {
        ArenaBlock *next = curr->next;
        free(curr);
        curr = next;
    }
    arena->normal_head = NULL;
    arena->normal_current = NULL;
}
void arena_clear_reallocable(Arena *arena) {
    ArenaBlock *curr = arena->reallocable_head;
    while(curr != NULL) {
        ArenaBlock *next = curr->next;
        free(curr);
        curr = next;
    }
    arena->reallocable_head = NULL;
    arena->reallocable_current = NULL;
}

void arena_set_alignment(Arena *arena, size_t alignment) {
    arena->alignment = alignment;
}
void arena_set_default_block_cap(Arena *arena, size_t cap) {
    arena->default_block_cap = cap;
}