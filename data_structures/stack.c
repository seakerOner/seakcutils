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
#include "stack.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

Stack stack_new(size_t elem_size, size_t cap) {
  Stack s;
  s.elem_size = elem_size;
  s.items = malloc(s.elem_size * cap);
  s.sp = (uint8_t *)s.items + (s.elem_size * cap);
  s.cap = cap;
  s.count = 0;
  return s;
}

void stack_reset(Stack *s) {
  if (!s->items)
    return;
  s->count = 0;
  s->sp = (uint8_t *)s->items + (s->elem_size * s->cap);
}

int stack_push(Stack *s, const void *elem) {
  if (!s->items || s->count > s->cap)
    return -1;
  s->sp -= s->elem_size;
  memcpy((uint8_t *)s->sp, (uint8_t *)elem, s->elem_size);
  s->count++;
  return 0;
}

int stack_pop(Stack *s, void *out) {
  if (!s->items || s->count == 0)
    return -1;

  memcpy((uint8_t *)out, (uint8_t *)s->sp, s->elem_size);
  s->sp += s->elem_size;
  s->count--;

  return 0;
};

void stack_free(Stack s) {
  if (!s.items)
    return;
  free(s.items);
  s.items = NULL;
  s.cap = 0;
  s.sp = NULL;
};
