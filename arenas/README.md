# Arenas

This directory contains **arena-style memory allocators** designed for fast, predictable allocation patterns.

---

## Design Philosophy

- **Linear allocation**
  - Memory is appended sequentially.
  - No per-object free.
- **Explicit lifetime**
  - The user decides when to reset or free the arena.
- **Predictable performance**
  - Amortized O(1) allocation.
  - Minimal branching and allocator calls.
- Flexible element storage
  - Can store fixed-size elements of any type, not just strings.

---

## Available Arenas

### Generic Arena

A flexible arena for storing fixed-size elements of any type.
- Elements are stored contiguously in memory.
- Supports both dynamic resizing and fixed capacity.
- Provides basic operations: add, get by index, get last, pop, reset, free.

### String Arena

A simple arena for storing **null-terminated strings** (`char *`).

Strings are:
- Stored contiguously in a single growing buffer
- Indexed by offsets
- Never individually freed

---
## Generic Arena API

```c
typedef enum { DYNAMIC, FIXED } AllocationPreference;

typedef struct {
    uint8_t *data;            // Contiguous element storage
    size_t   elem_size;       // Size of each element in bytes
    size_t   count;           // Number of elements currently stored
    size_t   capacity;        // Allocated capacity (number of elements)
    AllocationPreference preference; // Dynamic or fixed capacity
} Arena;

// Creation and destruction
Arena arena_create(size_t elem_size, size_t starting_capacity, AllocationPreference preference);
void  arena_free(Arena *arena);
void  arena_reset(Arena *arena);

// Element operations
int arena_add(Arena *arena, const void *val);        // Returns 0 on success, non-zero on error
const void *arena_get(Arena *arena, size_t i);      // Returns pointer to element i
const void *arena_get_last(Arena *arena);           // Returns pointer to last element
int arena_pop(Arena *arena, void *out);             // Removes last element and copies to out

```

## String Arena API

```c
typedef struct {
    char   *data;     // Contiguous string storage
    size_t  size;     // Total bytes used
    size_t *offsets;  // Offsets to each string
    size_t  count;    // Number of stored strings
    size_t  capacity; // Capacity of offsets array
} StringArena;

StringArena arena_s_create(void);
void arena_addS(StringArena *arena, const char *val);
const char *arena_getS(StringArena *arena, size_t i);
void arena_s_free(StringArena *arena);
void arena_s_reset(StringArena *arena);
```

## Notes

- All arenas are single-threaded by design.
- Memory growth is handled automatically for dynamic arenas.
- Resetting an arena does not free memory — it just resets the count.
- The generic arena works with any fixed-size type, providing predictable, cache-friendly allocation patterns.
