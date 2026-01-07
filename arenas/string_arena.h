#ifndef STRING_ARENA_H
#define STRING_ARENA_H
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
/*
------------------------------------------------------------------------------
StringArena — Append-only string storage arena

StringArena is a simple append-only arena for storing immutable strings.

All strings are stored contiguously in a single character buffer, while
a separate offsets array keeps track of where each string begins.

This design is intended for:
- storing many small strings
- avoiding per-string allocations
- fast indexed access
- bulk reset/free

Typical use cases:
- asset names
- identifiers
- interned strings (manual)
- debug labels
- scripting symbols

------------------------------------------------------------------------------
MEMORY MODEL

Internally, StringArena maintains two buffers:

1) data buffer:
    [ "hello\0world\0foo\0bar\0..." ]

2) offsets array:
    [ 0, 6, 12, 16, ... ]

Each call to arena_addS() appends a null-terminated string to `data`
and records its starting offset.

Strings are immutable once added.

------------------------------------------------------------------------------
LIFETIME & OWNERSHIP

- Returned strings are owned by the arena
- Strings remain valid until:
    - arena_s_reset()
    - arena_s_free()
- No individual string removal is supported

------------------------------------------------------------------------------
USAGE

In exactly ONE source file:

    #define STRING_ARENA_IMPLEMENTATION
    #include "string_arena.h"

In all other files:

    #include "string_arena.h"

Create a string arena:

    StringArena arena = arena_s_create();

Add strings:

    arena_addS(&arena, "hello");
    arena_addS(&arena, "world");

Retrieve strings:

    const char *s0 = arena_getS(&arena, 0);
    const char *s1 = arena_getS(&arena, 1);

Reset arena (keeps allocated memory):

    arena_s_reset(&arena);

Free arena memory:

    arena_s_free(&arena);

------------------------------------------------------------------------------
PERFORMANCE NOTES

- Adding a string is amortized O(1)
- Retrieval is O(1)
- Memory grows but never shrinks unless freed
- Uses realloc internally

------------------------------------------------------------------------------
LIMITS & WARNINGS

- No thread safety
- No bounds checks beyond index validation
- arena_s_reset() does NOT free memory
- Strings must be null-terminated
- realloc failure silently aborts insertion

------------------------------------------------------------------------------
*/
#include <stddef.h>

// Append-only string arena.
// Stores all strings contiguously and indexes them via offsets.
typedef struct {
  char *data; // Contiguous string buffer
  size_t size; // Used bytes in data buffer
  size_t *offsets; // offsets into data buffer
  size_t count; // Number of stored strings
  size_t capacity; // Capacity of offsets array
} StringArena;

// Creates a new, empty StringArena.
StringArena arena_s_create(void);

// Appends a null-terminated string to the arena.
// The string is copied into internal storage.
void arena_addS(StringArena *arena, const char *val);

// Returns the string at index `i`.
// Returns NULL if index is out of bounds.
const char *arena_getS(StringArena *arena, size_t i);

// Resets the arena to empty state.
// Allocated memory is kept for reuse.
void arena_s_free(StringArena *arena);

// Frees all memory owned by the arena.
// The arena must not be used afterwards.
void arena_s_reset(StringArena *arena);
#endif

#if (defined(STRING_ARENA_IMPLEMENTATION))
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
#endif
