#ifndef STRING_ARENA_H
#define STRING_ARENA_H
/*
StringArena — Design & Invariants
--------------------------------

StringArena is a minimal string arena allocator that stores multiple
null-terminated strings in a single contiguous memory buffer.

Core invariants:

1. `data` points to a contiguous memory region of size `size` bytes.
2. Strings are stored back-to-back and are always null-terminated (`'\0'`).
3. `offsets[i]` stores the starting byte offset of the i-th string in `data`.
4. `count` is the number of strings currently stored.
5. `capacity` is the allocated capacity of the `offsets` array (not bytes).
6. Offsets are monotonically increasing.
7. `arena_getS()` returns a pointer to a valid C string as long as the arena
   is not resized or freed.

Memory model:

- Individual strings are NOT allocated separately.
- All string data lives in a single growable buffer.
- Returned string pointers remain valid until:
  - `arena_addS()` reallocates the buffer
  - `arena_free()` is called
  - `arena_reset()` is called

This design favors:
- Cache locality
- Fewer allocations
- Simple lifetime management

This is a minimal implementation, not thread-safe.
*/


#include <stddef.h>

typedef struct {
  char *data;
  size_t size;
  size_t *offsets;
  size_t count;
  size_t capacity;

} StringArena;

StringArena arena_s_create(void);
void arena_addS(StringArena *arena, const char *val);
const char *arena_getS(StringArena *arena, size_t i);
void arena_s_free(StringArena *arena);
void arena_s_reset(StringArena *arena);
#endif
