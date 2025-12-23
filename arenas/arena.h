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

#ifndef ARENA_H
#define ARENA_H

#include <stddef.h>
#include <stdint.h>

typedef enum { DYNAMIC, FIXED } AllocationPreference;

typedef struct {
  uint8_t *data;
  size_t elem_size;

  _Atomic size_t count;
  size_t capacity;
  AllocationPreference preference; 
} Arena;

Arena arena_create(const size_t elem_size, const size_t starting_capacity,
                   AllocationPreference preference);

int arena_add(Arena *arena, const void *val);
uint8_t *arena_alloc(Arena *arena);
const void *arena_get(Arena *arena, size_t i);
const void *arena_get_last(Arena *arena);
int arena_pop(Arena *arena, void *out);
void arena_free(Arena *arena);
void arena_reset(Arena *arena);

#endif
