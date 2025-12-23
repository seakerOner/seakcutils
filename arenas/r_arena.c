#include "r_arena.h"
#include <stdatomic.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
// for `cpu_relax()` :D
#include "../channels/channels.h"

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
  for (int z = 0; z < num_regions; z++) {
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
