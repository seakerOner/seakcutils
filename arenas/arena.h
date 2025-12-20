#ifndef ARENA_H
#define ARENA_H

#include <stddef.h>
#include <stdint.h>

typedef enum { DYNAMIC, FIXED } AllocationPreference;

typedef struct {
  uint8_t *data;
  size_t elem_size;

  size_t count;
  size_t capacity;
  AllocationPreference preference; 
} Arena;

Arena arena_create(const size_t elem_size, const size_t starting_capacity,
                   AllocationPreference preference);

int arena_add(Arena *arena, const void *val);
const void *arena_get(Arena *arena, size_t i);
const void *arena_get_last(Arena *arena);
int arena_pop(Arena *arena, void *out);
void arena_free(Arena *arena);
void arena_reset(Arena *arena);

#endif
