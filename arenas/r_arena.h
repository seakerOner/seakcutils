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
RegionArena — Segmented arena allocator with epoch-based reset

RegionArena is a high-performance arena allocator built on top of
fixed-size memory regions.

Instead of reallocating a single contiguous buffer, memory is divided
into multiple regions ("chunks") of equal capacity. Regions are allocated
lazily as needed.

This design is intended for:
- job systems
- task schedulers
- ECS-like transient storage
- high-frequency allocations with bulk invalidation

RegionArena favors:
- predictable allocation cost
- minimal synchronization
- bulk reset instead of per-element free

------------------------------------------------------------------------------
MEMORY MODEL

Elements are stored linearly across regions:

    region 0: [0 ... rg_capacity-1]
    region 1: [rg_capacity ... 2*rg_capacity-1]
    region 2: ...

Index mapping:

    region = index / rg_capacity
    offset = index % rg_capacity

Each region owns a fixed contiguous buffer.

------------------------------------------------------------------------------
EPOCH SYSTEM

RegionArena uses an epoch counter to avoid clearing memory eagerly.

- Each region stores the epoch in which it was last used
- On allocation, if a region's epoch does not match the current epoch,
  the region is reset (memset to zero)
- r_arena_reset() simply increments the global epoch

This makes reset O(1), regardless of total allocated memory.

------------------------------------------------------------------------------
THREADING NOTES

- Allocation uses atomic counters
- Regions are created lazily and safely across threads
- Element memory itself is NOT synchronized

This allocator is safe for:
- multiple concurrent producers
- append-only allocation patterns

It is NOT safe for:
- concurrent mutation of the same element
- arbitrary deallocation
- random lifetime management

------------------------------------------------------------------------------
USAGE

In exactly ONE source file:

    #define REGION_ARENA_IMPLEMENTATION
    #include "r_arena.h"

In all other files:

    #include "r_arena.h"

Create a region arena:

    RegionArena arena =
        r_arena_create(sizeof(Job), 4096, 1024);

Allocate elements:

    Job *j = r_arena_alloc(&arena);
    j->fn = my_job;

Append by value copy:

    r_arena_add(&arena, &job);

Access elements:

    int *j = (int *)r_arena_get(&arena, i);
    int *last = (int *)r_arena_get_last(&arena);

Reset arena (invalidates all previous references):

    r_arena_reset(&arena);

Free arena memory:

    r_arena_free(&arena);

------------------------------------------------------------------------------
LIMITS & WARNINGS

- Exceeding max_regions will abort()
- References become invalid after r_arena_reset()
- RegionArena does not shrink memory
- No bounds checks beyond region limits

------------------------------------------------------------------------------
*/
#ifndef R_ARENA_H
#define R_ARENA_H

#include <stddef.h>
#include <stdint.h>

// Represents a single memory region (chunk).
// Epoch is used to lazily reset region contents.
typedef struct Region_t {
  uint8_t *data;
  size_t epoch;
} Region;

// Segmented arena allocator composed of multiple fixed-size regions.
typedef struct RegionArena_t {
  size_t elem_size; // Size of each element
  size_t rg_capacity; // Elements per region

  _Atomic size_t rgs_in_use; // Number of allocated regions
  _Atomic size_t count; // Total allocated elements

  size_t max_rgs; // Maximum allowed regions
  _Atomic size_t current_epoch; // Region pointers
  Region **regions_handler;
} RegionArena;

// Creates a new RegionArena.
//
// elem_size        - size of each element
// region_capacity  - number of elements per region
// max_regions      - maximum number of regions (0 defaults to 1024)
RegionArena r_arena_create(const size_t elem_size, const size_t region_capacity,
                           const size_t max_regions);

// Copies `val` into the arena at the next available slot.
// Returns 0 on success, non-zero on error.
int r_arena_add(RegionArena *arena, const void *val);

// Allocates space for one element and returns a pointer to it.
// Memory is zero-initialized on first use per epoch.
void *r_arena_alloc(RegionArena *arena);

// Returns a pointer to element at index `i`.
// Returns NULL if out of bounds.
const void *r_arena_get(RegionArena *arena, size_t i);

// Returns a pointer to the most recently allocated element.
// Returns NULL if the arena is empty.
const void *r_arena_get_last(RegionArena *arena);

// Resets the arena by advancing the epoch.
// All previously returned pointers become invalid.
void r_arena_free(RegionArena *arena);

// Frees all memory owned by the arena.
void r_arena_reset(RegionArena *arena);


/*-------------------------------------------*/
/*      Platform-dependent cpu_relax()       */
/*-------------------------------------------*/
#if defined(__x86_64__) || defined(__i386__)

#include <immintrin.h>
#define cpu_relax() _mm_pause()
/*-------------------------------------------*/
#elif defined(__aarch64__) || defined(__arm__)

#define cpu_relax() __asm__ __volatile__("yield")
/*-------------------------------------------*/
#elif defined(__riscv)

#define cpu_relax() __asm__ __volatile__("pause")
/*-------------------------------------------*/
#else
#define cpu_relax() ((void)0)
#endif
/*-------------------------------------------*/

#endif // !CHANNELS_H


#if (defined (REGION_ARENA_IMPLEMENTATION))
#include <stdatomic.h>
#include <stdlib.h>
#include <string.h>

RegionArena r_arena_create(const size_t elem_size, const size_t region_capacity,
                           const size_t max_regions) {
  RegionArena arena;
  arena.rg_capacity = region_capacity;
  arena.elem_size = elem_size;
  arena.max_rgs = max_regions == 0 ? 1024 : max_regions;
  atomic_init(&arena.current_epoch, 0);
  atomic_init(&arena.count, 0);
  atomic_init(&arena.rgs_in_use, 1);

  arena.regions_handler = calloc(arena.max_rgs, sizeof(Region *));
  arena.regions_handler[0] = malloc(sizeof(Region));
  arena.regions_handler[0]->data = calloc(arena.rg_capacity, arena.elem_size);
  arena.regions_handler[0]->epoch =
      atomic_load_explicit(&arena.current_epoch, memory_order_acquire);
  return arena;
};

static inline void _ensure_region(RegionArena *arena, const size_t region) {
  size_t used = atomic_load_explicit(&arena->rgs_in_use, memory_order_acquire);
  if (region >= arena->max_rgs) {
    abort();
  }
  if (region < used) {
    if (arena->regions_handler[region]->epoch !=
        atomic_load_explicit(&arena->current_epoch, memory_order_acquire)) {
      arena->regions_handler[region]->epoch =
          atomic_load_explicit(&arena->current_epoch, memory_order_acquire);
      memset(arena->regions_handler[region]->data, 0,
             arena->rg_capacity * arena->elem_size);
    }
    return;
  }

  if (atomic_compare_exchange_strong(&arena->rgs_in_use, &used, region + 1)) {
    arena->regions_handler[region] = malloc(sizeof(Region));
    arena->regions_handler[region]->data =
        calloc(arena->rg_capacity, arena->elem_size);
    arena->regions_handler[region]->epoch =
        atomic_load_explicit(&arena->current_epoch, memory_order_acquire);
  } else {
    while (atomic_load_explicit(&arena->rgs_in_use, memory_order_acquire) <=
           region) {
      cpu_relax();
    }
  }
}

int r_arena_add(RegionArena *arena, const void *val) {
  if (!arena) {
    return 1;
  }

  size_t count =
      atomic_fetch_add_explicit(&arena->count, 1, memory_order_acq_rel);

  size_t index = count % arena->rg_capacity;
  size_t region = count / arena->rg_capacity;

  _ensure_region(arena, region);

  memcpy((uint8_t *)arena->regions_handler[region]->data +
             (index * arena->elem_size),
         val, arena->elem_size);
  return 0;
};

void *r_arena_alloc(RegionArena *arena) {
  if (!arena)
    return NULL;

  size_t count =
      atomic_fetch_add_explicit(&arena->count, 1, memory_order_acq_rel);

  size_t region = count / arena->rg_capacity;
  size_t index = count % arena->rg_capacity;

  _ensure_region(arena, region);

  return (uint8_t *)arena->regions_handler[region]->data +
         (index * arena->elem_size);
};

const void *r_arena_get(RegionArena *arena, size_t i) {
  size_t count = atomic_load_explicit(&arena->count, memory_order_acquire);
  if (!arena || i >= count || count == 0) {
    return NULL;
  }

  size_t region = i / arena->rg_capacity;
  size_t index = i % arena->rg_capacity;
  return (uint8_t *)arena->regions_handler[region]->data +
         (index * arena->elem_size);
};

const void *r_arena_get_last(RegionArena *arena) {
  size_t count = atomic_load_explicit(&arena->count, memory_order_acquire);
  if (!arena || count == 0) {
    return NULL;
  }
  count -= 1;

  size_t index = count % arena->rg_capacity;
  size_t region = count / arena->rg_capacity;
  return (uint8_t *)arena->regions_handler[region]->data +
         (index * arena->elem_size);
};

void r_arena_free(RegionArena *arena) {
  if (!arena)
    return;

  size_t num_regions =
      atomic_load_explicit(&arena->rgs_in_use, memory_order_acquire);
  for (size_t z = 0; z < num_regions; z++) {
    free(arena->regions_handler[z]->data);
    free(arena->regions_handler[z]);
  }
  free(arena->regions_handler);
  arena->regions_handler = NULL;

  atomic_store_explicit(&arena->count, 0, memory_order_release);
  arena->rg_capacity = 0;
};

/* NOTE: User is responsible for use after-free on references  */
void r_arena_reset(RegionArena *arena) {
  atomic_fetch_add_explicit(&arena->current_epoch, 1, memory_order_acq_rel);
  atomic_store_explicit(&arena->count, 0, memory_order_release);
};
#endif
