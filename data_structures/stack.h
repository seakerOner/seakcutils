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

/*-----------------------------------------------------------------------------
  stack_new
  Creates a new fixed-capacity stack.

  elem_size : size in bytes of each element
  cap       : maximum number of elements

  Returns a Stack struct with allocated memory.

  Notes:
    - Stack grows from high memory towards low (sp decrements on push).
    - Caller must eventually call stack_free to release memory.
-----------------------------------------------------------------------------*/
Stack stack_new(size_t elem_size, size_t cap);

/*-----------------------------------------------------------------------------
  stack_reset
  Resets the stack to empty without freeing memory.

  s : pointer to a valid Stack

  Notes:
    - Sets count to 0 and stack pointer to the top of the buffer.
    - Does not deallocate the internal buffer.
-----------------------------------------------------------------------------*/
void stack_reset(Stack *s);

/*-----------------------------------------------------------------------------
  stack_push
  Pushes an element onto the stack.

  s    : pointer to a valid Stack
  elem : pointer to the element data to push

  Returns:
    - 0  on success
    - -1 if stack is NULL or full

  Notes:
    - Copies elem_size bytes from elem to internal buffer.
    - Decrements the stack pointer.
-----------------------------------------------------------------------------*/
int stack_push(Stack *s, const void *elem);

/*-----------------------------------------------------------------------------
  stack_pop
  Pops an element from the stack.

  s   : pointer to a valid Stack
  out : pointer to memory where the popped element will be copied

  Returns:
    - 0  on success
    - -1 if stack is NULL or empty

  Notes:
    - Copies elem_size bytes from the internal buffer into out.
    - Increments the stack pointer.
-----------------------------------------------------------------------------*/
int stack_pop(Stack *s, void *out);

/*-----------------------------------------------------------------------------
  stack_free
  Frees the internal buffer of the stack.

  s : Stack struct (passed by value)

  Notes:
    - After calling, s.items becomes NULL and sp is reset.
    - Does not free the Stack struct itself since it is returned by value.
-----------------------------------------------------------------------------*/
void stack_free(Stack s);

#endif // !MYSTACK_H

#if (defined(MYSTACK_IMPLEMENTATION))
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
#endif
