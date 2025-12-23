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

### Region Arena (Epoch-based, Concurrent, Low-level)

A region-based arena allocator designed for high-throughput, concurrent allocation with explicit reuse via epochs.

This arena divides memory into fixed-size regions, each containing a fixed number of elements.
Regions are allocated lazily and reused across epochs without freeing underlying memory.

#### Region Arena Key Concepts

- `Regions`
    - Each region stores region_capacity elements.
    - Regions are allocated on demand.
- `Epochs`
    - A monotonically increasing generation counter.
    - Calling `r_arena_reset()` advances the epoch.
    - Regions are lazily cleared when first reused in a new epoch.
- `Lock-free allocation`
    - Allocation is **thread-safe** using C11 atomics.
    - No locks on the hot path.
- `Bounded growth`
    - A **fixed** `max_regions` limit is enforced.
    - Exceeding this limit aborts the program.

#### Region Arena Guarantees
- Threading
    - `r_arena_alloc()` and `r_arena_add()` are **thread-safe** for concurrent writers
    - `r_arena_get()` / `r_arena_get_last()` are **read-only** and must not race with concurrent allocations
    - `r_arena_reset()` is **not thread-safe**
    - `r_arena_free()` must **not** race with any arena operation
- Lifetime & Validity
    - Returned pointers are **stable within the current epoch**
    - All pointers become **invalid after** `r_arena_reset()`
    - No per-element free
    - No ownership tracking

- Note:
    - Region memory is cleared lazily on first access in a new epoch
    - `r_arena_reset()` itself performs no memory clearing

#### Region Arena - User Responsibilities (Important)
To use the Region Arena **correctly and safely**, the user must follow these rules:

1. **Epoch boundaries are explicit**
- `r_arena_reset()` defines a **hard lifetime boundary**
- After calling `r_arena_reset()`:
    - **All previously returned pointers are invalid**
    - Any read or write through old pointers is **undefined behavior**

- **Rule**:
Only keep references obtained after the most recent reset.

2. **Reset must be externally synchronized**

- `r_arena_reset()` is **not thread-safe**
- The arena does **not** coordinate with in-flight allocations

- **Rule**:
Ensure that **no threads are allocating or accessing arena memory** when calling `r_arena_reset()`.

- Typical patterns:
    - Barrier / join point
    - End-of-frame
    - End-of-job-graph
    - Single controlling thread

3. **Regions are reused, not freed**

- Memory backing regions is **never freed** until `r_arena_free()`
- On reuse:
    - A region is cleared lazily on first access in a new epoch
    - Old data must be considered garbage

- Rule:
Do not rely on memory contents surviving across resets.

4. **`max_regions` is a hard limit**

- The arena will **abort** if max_regions is exceeded

- Rule:
Choose `max_regions` large enough for your worst-case workload.
This arena favors **fail-fast correctness** over silent corruption.

5. **Logical ownership is external**

- The arena does not track:
    - Who owns an allocation
    - When it is no longer used
- Lifetime is **purely epoch-based**

- Rule:
Use the Region Arena only for objects whose lifetime is:
    - “until next reset”
    - or “until phase ends”

6. **Recommended Usage Pattern**

```text
[ Phase begins ]
  ├─ concurrent r_arena_alloc()
  ├─ concurrent r_arena_add()
  ├─ use allocated objects
[ Phase ends ]
  ├─ ensure all users are done
  ├─ r_arena_reset()
[ Next phase ]

```
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
void *arena_alloc(Arena *arena);                    // Returns allocated pointer to add value (Doesn't resize capacity)
const void *arena_get(Arena *arena, size_t i);      // Returns pointer to element i
const void *arena_get_last(Arena *arena);           // Returns pointer to last element
int arena_pop(Arena *arena, void *out);             // Removes last element and copies to out
```

## Region Arena API

```c
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

- The **Generic Arena** and **String Arena** are **single-threaded**.
- The **Region Arena** supports **concurrent allocation**, but requires explicit synchronization around resets.
- Memory growth is handled automatically for dynamic arenas.
- Resetting any arena does not free memory — it only resets logical state.
- All arenas prioritize predictability and explicit behavior over safety abstractions.
