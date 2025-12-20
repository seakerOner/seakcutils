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
