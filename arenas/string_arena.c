#include "string_arena.h"
#include <stdlib.h>
#include <string.h>

StringArena arena_s_create() {
  StringArena a;
  a.data = (void *)0; // NULL
  a.size = 0;
  a.offsets = (void *)0; // NULL
  a.count = 0;
  a.capacity = 0;

  return a;
}

void arena_addS(StringArena *arena, const char *val) {
  size_t len = strlen(val) + 1;

  if (arena->count == arena->capacity) {
    size_t new_cap = arena->capacity == 0 ? 4 : arena->capacity * 2;
    void *tmp = realloc(arena->offsets, new_cap * sizeof(size_t));

    if (!tmp) {
      return;
    }
    arena->offsets = tmp;
    arena->capacity = new_cap;
  }

  char *buf = realloc(arena->data, arena->size + len);

  if (!buf)
    return;

  arena->data = buf;

  arena->offsets[arena->count] = arena->size;
  memcpy(arena->data + arena->size, val, len);

  arena->size += len;
  arena->count++;
}

const char *arena_getS(StringArena *arena, size_t i) {
  if (i >= arena->count) {
    return NULL;
  }

  return arena->data + arena->offsets[i];
}

void arena_s_free(StringArena *arena) {
  free(arena->data);
  free(arena->offsets);
}

void arena_s_reset(StringArena *arena) {
  arena->size = 0;
  arena->count = 0;
}
