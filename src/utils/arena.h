#ifndef FOG_ARENA_H
#define FOG_ARENA_H

#include <stddef.h>
#include <stdbool.h>

typedef struct {
    char *base;
    size_t capacity;
    size_t pos;
} Arena;

#define ARENA_REMAINING(a) (a.capacity - a.pos)

bool arena_init(Arena *a, size_t size);
void *arena_alloc(Arena *a, size_t size);
void *arena_alloc_alignment(Arena *a, size_t size, size_t allignment);
void arena_reset(Arena *a);
void arena_free(Arena *a);

#endif // FOG_ARENA_H
