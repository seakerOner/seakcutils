#ifndef MYSTACK_H
#define MYSTACK_H

#include <stddef.h>

typedef struct Stack_t {
  void *items;
  void *sp; // stack pointer starts at the "top" of the stack and decrements
  size_t count;
  size_t elem_size;
  size_t cap;
} Stack;

Stack stack_new(size_t elem_size, size_t cap);
void stack_reset(Stack *s);

int stack_push(Stack *s, const void *elem);
int stack_pop(Stack *s, void *out);

void stack_free(Stack s);

#endif // !MYSTACK_H
