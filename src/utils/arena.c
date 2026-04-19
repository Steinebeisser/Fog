#include "arena.h"

#include <stdio.h>
#include <stdlib.h>

bool arena_init(Arena *a, size_t size) {
    if (!a) return false;

    a->base = malloc(size);
    if (!a->base) return false;

    printf("Allocated %zu bytes\n", size);

    a->capacity = size;
    a->pos = 0;
    return true;
}
void *arena_alloc(Arena *a, size_t size) {
    return arena_alloc_alignment(a, size, sizeof(void*));
}

void *arena_alloc_alignment(Arena *a, size_t size, size_t allignment) {
    // printf("ALLOCATING %zu bytes\n", size);

    // 10
    // 99

    // 10 - (99 % 10)
    // 1

    size_t to_align = (allignment - (a->pos % allignment)) % allignment;
    size += to_align;

    if (ARENA_REMAINING((*a)) < size) 
        return NULL;

    void *ptr = a->base + a->pos + to_align;
    a->pos += size;
    return ptr;
}

void arena_reset(Arena *a) {
    if (!a)
        return;

    a->pos = 0;
}
void arena_free(Arena *a) {
    if (!a || !a->base)
        return;

    free(a->base);
}
