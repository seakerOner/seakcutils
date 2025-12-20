// Copyright 2025 Seaker <seakerone@proton.me>

// Permission is hereby granted, free of charge, to any person obtaining
// a copy of this software and associated documentation files (the
// "Software"), to deal in the Software without restriction, including
// without limitation the rights to use, copy, modify, merge, publish,
// distribute, sublicense, and/or sell copies of the Software, and to
// permit persons to whom the Software is furnished to do so, subject to
// the following conditions:

// The above copyright notice and this permission notice shall be
// included in all copies or substantial portions of the Software.

// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
// EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
// MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
// NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
// LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
// OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
// WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

#include "arena.h"
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

Arena arena_create(const size_t elem_size, const size_t starting_capacity,
                   AllocationPreference preference) {
  Arena a;
  a.elem_size = elem_size;
  a.count = 0;
  a.capacity = starting_capacity == 0 ? 8 : starting_capacity;
  a.data = malloc(a.capacity * elem_size);
  a.preference = preference;

  return a;
};

int arena_add(Arena *arena, const void *val) {
  if (!arena) {
    return 1;
  }

  if (arena->count == arena->capacity) {
    if (arena->preference == FIXED)
      return 2;

    size_t new_cap = arena->capacity * 1.5;
    void *tmp = realloc(arena->data, new_cap * arena->elem_size);
    if (!tmp)
      return 3;

    arena->data = tmp;
    arena->capacity = new_cap;
  }

  memcpy(arena->data + (arena->count * arena->elem_size), val,
         arena->elem_size);

  arena->count++;
  return 0;
};

const void *arena_get(Arena *arena, size_t i) {
  if (!arena || i >= arena->count || arena->count == 0) {
    return NULL;
  }
  return arena->data + (i * arena->elem_size);
};

const void *arena_get_last(Arena *arena) {
  if (!arena || arena->count == 0) {
    return NULL;
  }
  return arena->data + ((arena->count - 1) * arena->elem_size);
};

int arena_pop(Arena *arena, void *out) {
  if (!arena || arena->count == 0)
    return 0;

  arena->count--;
  memcpy(out, arena->data + (arena->count * arena->elem_size),
         arena->elem_size);
  return 1;
};

void arena_free(Arena *arena) {
  if (!arena)
    return;
  free(arena->data);
  arena->data = NULL;
  arena->count = 0;
  arena->capacity = 0;
};

void arena_reset(Arena *arena) { arena->count = 0; };
