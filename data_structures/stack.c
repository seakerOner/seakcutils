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
