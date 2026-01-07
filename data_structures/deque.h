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
#ifndef DEQUE_H
#define DEQUE_H

#include <stdalign.h>
#include <stddef.h>
#include <stdint.h>

#ifndef CACHELINE_SIZE
#define CACHELINE_SIZE 64
#endif

typedef struct Deque_T {
  alignas(CACHELINE_SIZE) void *items;

  int64_t head;
  int64_t tail;

  size_t cap;
  size_t elem_size;
} Deque;

/*-----------------------------------------------------------------------------
  deque_new
  Allocates and initializes a new double-ended queue (deque).

  elem_size : size in bytes of each element
  cap       : maximum number of elements

  Returns a Deque struct with allocated memory.

  Notes:
    - head grows towards higher indices, tail towards lower.
    - Caller must eventually call deque_free to release memory.
-----------------------------------------------------------------------------*/
Deque deque_new(size_t elem_size, size_t cap);

/*-----------------------------------------------------------------------------
  deque_push_front
  Pushes an element to the front (head) of the deque.

  q    : pointer to a valid Deque
  elem : pointer to element data to push

  Returns:
    - 0  on success
    - -1 if deque or items buffer is NULL
    - -2 if deque is full

  Notes:
    - Copies elem_size bytes from elem to internal buffer.
-----------------------------------------------------------------------------*/
int deque_push_front(Deque *q, const void *elem);

/*-----------------------------------------------------------------------------
  deque_push_back
  Pushes an element to the back (tail) of the deque.

  q    : pointer to a valid Deque
  elem : pointer to element data to push

  Returns:
    - 0  on success
    - -1 if deque or items buffer is NULL
    - -2 if deque is full

  Notes:
    - Copies elem_size bytes from elem to internal buffer.
-----------------------------------------------------------------------------*/
int deque_push_back(Deque *q, const void *elem);

/*-----------------------------------------------------------------------------
  deque_pop_front
  Pops an element from the front (head) of the deque.

  q   : pointer to a valid Deque
  out : pointer to memory where the popped element will be copied

  Returns:
    - 0  on success
    - -1 if deque or items buffer is NULL
    - -2 if deque is empty

  Notes:
    - Copies elem_size bytes into out.
    - Decrements head.
-----------------------------------------------------------------------------*/
int deque_pop_front(Deque *q, void *out);

/*-----------------------------------------------------------------------------
  deque_pop_back
  Pops an element from the back (tail) of the deque.

  q   : pointer to a valid Deque
  out : pointer to memory where the popped element will be copied

  Returns:
    - 0  on success
    - -1 if deque or items buffer is NULL
    - -2 if deque is empty

  Notes:
    - Copies elem_size bytes into out.
    - Increments tail.
-----------------------------------------------------------------------------*/
int deque_pop_back(Deque *q, void *out);

/*-----------------------------------------------------------------------------
  deque_peek_front
  Returns a pointer to the element at the front (head) without removing it.

  q : pointer to a valid Deque

  Returns:
    - pointer to the front element
    - NULL if deque is NULL, items buffer is NULL, or empty

  Notes:
    - Read-only, does not modify head or tail.
-----------------------------------------------------------------------------*/
const void *deque_peek_front(Deque *q);

/*-----------------------------------------------------------------------------
  deque_peek_back
  Returns a pointer to the element at the back (tail) without removing it.

  q : pointer to a valid Deque

  Returns:
    - pointer to the back element
    - NULL if deque is NULL, items buffer is NULL, or empty

  Notes:
    - Read-only, does not modify head or tail.
-----------------------------------------------------------------------------*/
const void *deque_peek_back(Deque *q);

/*-----------------------------------------------------------------------------
  deque_free
  Frees the internal buffer of the deque.

  q : pointer to a valid Deque

  Notes:
    - After calling, q->items becomes NULL and head/tail are reset.
    - Does not free the Deque struct itself since it is returned by value.
-----------------------------------------------------------------------------*/
void deque_free(Deque *q);

/*-----------------------------------------------------------------------------
  deque_reset
  Resets the deque to empty without freeing memory.

  q : pointer to a valid Deque

  Notes:
    - Sets head and tail to 0.
    - Does not deallocate the internal buffer.
-----------------------------------------------------------------------------*/
void deque_reset(Deque *q);

#endif // !DEQUE_H

#if (defined (DEQUE_IMPLEMENTATION))
#include <stdlib.h>
#include <string.h>

static inline size_t wrap(int64_t logical_num, size_t cap) {
  int64_t r = logical_num % (int64_t)cap;
  return (r < 0) ? (size_t)(r + cap) : (size_t)r;
}

static inline size_t deque_size(Deque *q) {
  return (size_t)(q->head - q->tail);
}
static inline int deque_is_empty(Deque *q) { return q->head == q->tail; }
static inline int deque_is_full(Deque *q) { return deque_size(q) == q->cap; }

Deque deque_new(size_t elem_size, size_t cap) {
  Deque q;
  q.cap = cap;
  q.elem_size = elem_size;
  q.items = malloc(q.cap * q.elem_size);

  q.head = 0;
  q.tail = 0;
  return q;
}

int deque_push_front(Deque *q, const void *elem) {
  if (!q || !q->items)
    return -1;

  if (deque_is_full(q))
    return -2;

  size_t id = wrap(q->head, q->cap);
  size_t t_id = wrap(q->tail, q->cap);

  memcpy((uint8_t *)q->items + (id * q->elem_size), (uint8_t *)elem,
         q->elem_size);
  q->head += 1;

  return 0;
};

int deque_push_back(Deque *q, const void *elem) {
  if (!q || !q->items)
    return -1;

  if (deque_is_full(q))
    return -2;

  q->tail -= 1;
  size_t id = wrap(q->tail, q->cap);

  memcpy((uint8_t *)q->items + (id * q->elem_size), (uint8_t *)elem,
         q->elem_size);

  return 0;
};

int deque_pop_front(Deque *q, void *out) {
  if (!q || !q->items)
    return -1;

  if (deque_is_empty(q))
    return -2;

  q->head -= 1;
  size_t id = wrap(q->head, q->cap);
  memcpy((uint8_t *)out, (uint8_t *)q->items + (id * q->elem_size),
         q->elem_size);

  return 0;
};

const void *deque_peek_front(Deque *q) {
  if (!q || !q->items)
    return NULL;

  if (deque_is_empty(q))
    return NULL;

  size_t id = wrap(q->head - 1, q->cap);
  return (void *)((uint8_t *)q->items + (id * q->elem_size));
};

int deque_pop_back(Deque *q, void *out) {
  if (!q || !q->items)
    return -1;

  if (deque_is_empty(q))
    return -2;

  size_t id = wrap(q->tail, q->cap);
  memcpy((uint8_t *)out, (uint8_t *)q->items + (id * q->elem_size),
         q->elem_size);
  q->tail += 1;

  return 0;
};

const void *deque_peek_back(Deque *q) {
  if (!q || !q->items)
    return NULL;

  if (deque_is_empty(q))
    return NULL;

  size_t id = wrap(q->tail, q->cap);
  return (void *)((uint8_t *)q->items + (id * q->elem_size));
};

void deque_free(Deque *q) {
  if (!q->items)
    return;

  free(q->items);
  q->items = NULL;

  q->tail = 0;
  q->head = 0;
  q->cap = 0;
}

void deque_reset(Deque *q) {
  if (!q->items)
    return;

  q->head = 0;
  q->tail = 0;
}


#endif
