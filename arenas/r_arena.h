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

#ifndef R_ARENA_H
#define R_ARENA_H

#include <stddef.h>
#include <stdint.h>

typedef struct {
  uint8_t *data;
  size_t epoch;
} Region;

typedef struct {
  size_t elem_size;
  size_t rg_capacity;

  _Atomic size_t rgs_in_use;
  _Atomic size_t count;

  size_t max_rgs;
  _Atomic size_t current_epoch;
  Region **regions_handler;
} RegionArena;

RegionArena r_arena_create(const size_t elem_size, const size_t region_capacity,
                           const size_t max_regions);

int r_arena_add(RegionArena *arena, const void *val);
void *r_arena_alloc(RegionArena *arena);
const void *r_arena_get(RegionArena *arena, size_t i);
const void *r_arena_get_last(RegionArena *arena);
void r_arena_free(RegionArena *arena);
void r_arena_reset(RegionArena *arena);

#endif
