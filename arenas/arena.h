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
Arena — Simple generic contiguous arena / growable array

This module implements a very small, generic arena-style container.

Conceptually, it behaves like:
- a typed dynamic array
- a linear allocator with indexed access
- a stack-like container (push / pop)

Key characteristics:
- Fixed element size
- Contiguous memory
- Optional dynamic resizing
- Atomic element count (thread-aware, not fully lock-free)

This arena is designed for:
- high-frequency allocations with bulk reset

It is NOT intended to be:
- a safe concurrent container for arbitrary access patterns

------------------------------------------------------------------------------
ALLOCATION MODES

AllocationPreference controls resizing behavior:

    FIXED
        - Capacity never grows
        - Allocation fails (or resets, depending on API)
        - Suitable for bounded systems

    DYNAMIC
        - Capacity grows using realloc()
        - Similar behavior to a vector / array list

------------------------------------------------------------------------------
USAGE

In exactly ONE source file:

    #define ARENA_IMPLEMENTATION
    #include "arena.h"

In all other files:

    #include "arena.h"

Create an arena:

    Arena a = arena_create(sizeof(MyType), 128, DYNAMIC);

Add elements by value copy:

    MyType v = { ... };
    arena_add(&a, &v);

Allocate a zeroed slot and fill manually:

    MyType *ptr = arena_alloc(&a);
    ptr->x = 42;

Access elements:

    MyType *elem = (MyType *)arena_get(&a, 0);
    MyType *last = (MyType *)arena_get_last(&a);

Pop last element:

    MyType out;
    arena_pop(&a, &out);

Reset arena (keeps memory):

    arena_reset(&a);

Free arena memory:

    arena_free(&a);

------------------------------------------------------------------------------
THREADING NOTES

- `count` is atomic
- No synchronization is performed on element data
- Concurrent producers are partially supported
- Concurrent reads/writes to the same element are unsafe

------------------------------------------------------------------------------
*/
#ifndef ARENA_H
#define ARENA_H

#include <stddef.h>
#include <stdint.h>

// Controls how the arena behaves when capacity is exceeded.
typedef enum {
  DYNAMIC, // Arena grows using realloc()
  FIXED    // Arena never grows
} AllocationPreference;

typedef struct {
  uint8_t *data;
  size_t elem_size;

  _Atomic size_t count;
  size_t capacity;
  AllocationPreference preference;
} Arena;

// Creates a new arena.
//
// elem_size          - size of each element in bytes
// starting_capacity  - initial number of elements (0 defaults to 8)
// preference         - FIXED or DYNAMIC
Arena arena_create(const size_t elem_size, const size_t starting_capacity,
                   AllocationPreference preference);

// Copies `val` into the arena.
//
// Returns:
//   0 - success
//   1 - arena is NULL
//   2 - capacity reached and arena is FIXED
//   3 - realloc failed
int arena_add(Arena *arena, const void *val);

// Reserves space for one element and returns a pointer to it.
// The memory is zero-initialized.
//
// Returns NULL on failure.
//
// NOTE:
// - In FIXED mode, capacity overflow may trigger a reset
//   depending on build configuration.
void *arena_alloc(Arena *arena);

// Returns a pointer to the element at index `i`.
// Returns NULL if out of bounds.
const void *arena_get(Arena *arena, size_t i);

// Returns a pointer to the last element in the arena.
// Returns NULL if the arena is empty.
const void *arena_get_last(Arena *arena);

// Removes the last element and copies it into `out`.
// Returns 1 on success, 0 if the arena is empty.
int arena_pop(Arena *arena, void *out);

// Resets the arena count to zero.
// Memory is kept and reused.
void arena_free(Arena *arena);

// Frees all memory owned by the arena.
void arena_reset(Arena *arena);

#endif

#if (defined(ARENA_IMPLEMENTATION))
#include <stdatomic.h>
#include <stdlib.h>
#include <string.h>

Arena arena_create(const size_t elem_size, const size_t starting_capacity,
                   AllocationPreference preference) {
  Arena a;
  a.elem_size = elem_size;
  atomic_init(&a.count, 0);
  a.capacity = starting_capacity == 0 ? 8 : starting_capacity;
  a.data = malloc(a.capacity * elem_size);
  a.preference = preference;

  return a;
};

int arena_add(Arena *arena, const void *val) {
  if (!arena) {
    return 1;
  }

  if (atomic_load_explicit(&arena->count, memory_order_acquire) ==
      arena->capacity) {
    if (arena->preference == FIXED)
      return 2;

    size_t new_cap = arena->capacity * 1.5;
    void *tmp = realloc((uint8_t *)arena->data, new_cap * arena->elem_size);
    if (!tmp)
      return 3;

    arena->data = tmp;
    arena->capacity = new_cap;
  }
  size_t count =
      atomic_fetch_add_explicit(&arena->count, 1, memory_order_acq_rel);

  memcpy((uint8_t *)arena->data + (count * arena->elem_size), val,
         arena->elem_size);
  return 0;
};

/* Will return NULL if max capacity is reached and doesn't resize */
void *arena_alloc(Arena *arena) {
  if (!arena)
    return NULL;

  size_t index =
      atomic_fetch_add_explicit(&arena->count, 1, memory_order_acq_rel);

  if (index >= arena->capacity) {
    atomic_fetch_sub_explicit(&arena->count, 1, memory_order_acq_rel);
    // temporary for job system
#ifdef DEBUG
    fprintf(stderr, "[job_system] arena capacity exceeded — resetting (jobs "
                    "may be lost)\n");
#endif
    arena_reset(arena);
    uint8_t *ptr = (uint8_t *)arena->data;
    memset(ptr, 0, arena->elem_size);
    return ptr;
  }

  uint8_t *ptr = (uint8_t *)arena->data + index * arena->elem_size;
  memset(ptr, 0, arena->elem_size);
  return ptr;
};

const void *arena_get(Arena *arena, size_t i) {
  if (!arena || i >= arena->count || arena->count == 0) {
    return NULL;
  }
  return (uint8_t *)arena->data + (i * arena->elem_size);
};

const void *arena_get_last(Arena *arena) {
  if (!arena || arena->count == 0) {
    return NULL;
  }
  return (uint8_t *)arena->data + ((arena->count - 1) * arena->elem_size);
};

int arena_pop(Arena *arena, void *out) {
  if (!arena || arena->count == 0)
    return 0;

  size_t old =
      atomic_fetch_sub_explicit(&arena->count, 1, memory_order_acq_rel);

  size_t index = old - 1;
  memcpy(out, (uint8_t *)arena->data + (index * arena->elem_size),
         arena->elem_size);
  return 1;
};

void arena_free(Arena *arena) {
  if (!arena)
    return;
  free(arena->data);
  arena->data = NULL;
  atomic_store_explicit(&arena->count, 0, memory_order_release);
  arena->capacity = 0;
};

void arena_reset(Arena *arena) {
  atomic_store_explicit(&arena->count, 0, memory_order_release);
};
#endif
