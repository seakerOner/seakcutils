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

Deque deque_new(size_t elem_size, size_t cap);

int deque_push_front(Deque *q, const void *elem);
int deque_push_back(Deque *q, const void *elem);

int deque_pop_front(Deque *q, void *out);
int deque_pop_back(Deque *q, void *out);

const void *deque_peek_front(Deque *q);
const void *deque_peek_back(Deque *q);

void deque_free(Deque *q);
void deque_reset(Deque *q);

#endif // !DEQUE_H
