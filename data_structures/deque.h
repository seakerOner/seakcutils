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
