#ifndef GVECTOR_H
#define GVECTOR_H

#include "arena.h"

#define VECTOR_DEFINITION(type, prefix) \
    typedef struct { \
        type *data; \
        size_t len; \
        size_t cap; \
        Arena *arena; \
    } prefix##Vec; \
    \
    prefix##Vec prefix##Vec_create(Arena *arena, size_t init_cap); \
    void prefix##Vec_push(prefix##Vec *vec, type data); \
    type prefix##Vec_pop(prefix##Vec *vec); \
    type prefix##Vec_get(prefix##Vec *vec, size_t index); \
    void prefix##Vec_set(prefix##Vec *vec, size_t index, type data); \
    void prefix##Vec_clear(prefix##Vec *vec); \
    void prefix##Vec_free(prefix##Vec *vec);


#define VECTOR_IMPLEMENTATION(type, prefix) \
    prefix##Vec prefix##Vec_create(Arena *arena, size_t init_cap) { \
        prefix##Vec vec; \
        init_cap = init_cap < 8 ? 8 : init_cap; \
        vec.data = (type *)arena_alloc_reallocable(arena, init_cap * sizeof(type)); \
        vec.cap = init_cap; \
        vec.len = 0; \
        vec.arena = arena; \
        return vec; \
    } \
    \
    void prefix##Vec_push(prefix##Vec *vec, type data) { \
        if (!vec) return; \
        if (vec->len >= vec->cap) { \
            size_t new_cap = vec->cap * 2; \
            vec->data = (type *)arena_realloc(vec->arena, vec->data, \
                                              new_cap * sizeof(type)); \
            vec->cap = new_cap; \
        } \
        vec->data[vec->len++] = data; \
    } \
    \
    type prefix##Vec_pop(prefix##Vec *vec) { \
        if (!vec || vec->len == 0) { \
            type zero = {0}; \
            return zero; \
        } \
        return vec->data[--vec->len]; \
    } \
    \
    type prefix##Vec_get(prefix##Vec *vec, size_t index) { \
        type zero = {0}; \
        if (!vec || index >= vec->len) return zero; \
        return vec->data[index]; \
    } \
    \
    void prefix##Vec_set(prefix##Vec *vec, size_t index, type data) { \
        if (!vec || index >= vec->len) return; \
        vec->data[index] = data; \
    } \
    \
    void prefix##Vec_clear(prefix##Vec *vec) { \
        if (!vec) return; \
        vec->len = 0; \
    } \
    \
    void prefix##Vec_free(prefix##Vec *vec) { \
        if (!vec) return; \
        vec->data = NULL; \
        vec->len = 0; \
        vec->cap = 0; \
        vec->arena = NULL; \
    }


#endif /* GVECTOR_H */