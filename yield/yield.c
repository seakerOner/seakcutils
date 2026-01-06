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
#include "yield.h"
#include <stdatomic.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

// x86-64bits assembly implementation

#define STACK_SIZE (64 * 1024)

typedef enum {
  CTX_READY,
  CTX_DEAD,
} ContextState;

typedef struct Context_t {
  void *rsp;
  void *stack;
  ContextState state;
} Context;

typedef struct ContextAnchor_t {
  Context *ctxs;
  size_t count;
  size_t index;
  size_t cap;
} ContextAnchor;

void __remake_ctx_stack(size_t x) {
  g_ctxs->ctxs[x].stack = malloc(STACK_SIZE);
  uintptr_t top = (uintptr_t)g_ctxs->ctxs[x].stack + STACK_SIZE;
  top &= ~0xF; // align to 16 bytes
  g_ctxs->ctxs[x].rsp = (void *)top;
  g_ctxs->ctxs[x].state = CTX_READY;
}

void _ctx_anchor_healthcheck() {
  if (g_ctxs->count >= g_ctxs->cap) {
    size_t old_cap = g_ctxs->cap;
    void *x = realloc(g_ctxs->ctxs, (sizeof(Context) * g_ctxs->cap) * 2);
    if (!x) {
      return;
    }
    g_ctxs->ctxs = x;
    g_ctxs->cap *= 2;

    for (size_t x = old_cap; x < g_ctxs->cap; x++) {
      g_ctxs->ctxs[x].stack = malloc(STACK_SIZE);
      uintptr_t top = (uintptr_t)g_ctxs->ctxs[x].stack + STACK_SIZE;
      top &= ~0xF; // align to 16 bytes
      g_ctxs->ctxs[x].rsp = (void *)top;
      g_ctxs->ctxs[x].state = CTX_READY;
    }
  }
}

void __attribute__((naked)) yield(void) {
  __asm__ __volatile__(
      "pushq %rdi\n" // save context registers on stack (callee + rdi)
      "pushq %rbp\n"
      "pushq %rbx\n"
      "pushq %r12\n"
      "pushq %r13\n"
      "pushq %r14\n"
      "pushq %r15\n"
      "movq %rsp, %rdi\n" // stack pointer to 1st argument
      "jmp switch_context\n");
}

ContextAnchor *g_ctxs = NULL;

void __attribute__((naked)) remake_ctx(void *rsp) {
  __asm__ __volatile__(
      "movq %rdi, %rsp \n" // set stack pointer from saved context
      "popq %r15\n"
      "popq %r14\n"
      "popq %r13\n"
      "popq %r12\n"
      "popq %rbx\n"
      "popq %rbp\n"
      "popq %rdi\n"
      "ret\n");
}

void finish_run(void) {
  size_t id = g_ctxs->index;
  size_t last = g_ctxs->count - 1;
  g_ctxs->ctxs[id].state = CTX_DEAD;

  if (id != last) {
    Context tmp = g_ctxs->ctxs[id];
    g_ctxs->ctxs[id] = g_ctxs->ctxs[last];
    g_ctxs->ctxs[last] = tmp;
  }
  g_ctxs->count--;

  if (g_ctxs->index >= g_ctxs->count)
    g_ctxs->index = 0;

  remake_ctx(g_ctxs->ctxs[g_ctxs->index].rsp);
}

void task_run(void(*func), void *ctx) {
  // set registers and return calls for stack frame
  size_t id = g_ctxs->count;

  if (g_ctxs->ctxs[id].state == CTX_DEAD) {
    free(g_ctxs->ctxs[id].stack);
    __remake_ctx_stack(id);
  }
  void **rsp = (void **)g_ctxs->ctxs[id].rsp;

  *(--rsp) = finish_run; // ret after func
  *(--rsp) = func;       // ret
  *(--rsp) = ctx;        // arg1 for func
  *(--rsp) = 0;          // push rbp
  *(--rsp) = 0;          // push rbx
  *(--rsp) = 0;          // push r12
  *(--rsp) = 0;          // push r13
  *(--rsp) = 0;          // push r14
  *(--rsp) = 0;          // push r15

  g_ctxs->ctxs[id].rsp = rsp;
  g_ctxs->count++;

  _ctx_anchor_healthcheck();
}

void switch_context(void *rsp) {
  g_ctxs->ctxs[g_ctxs->index].rsp = rsp;

  g_ctxs->index++;
  if (g_ctxs->index >= g_ctxs->count)
    g_ctxs->index = 0;

  remake_ctx(g_ctxs->ctxs[g_ctxs->index].rsp);
}

void wait_for_tasks() {
  while (g_ctxs->count > 1) {
    yield();
  }
}

void g_anchor_init() {
  if (g_ctxs) {
    return;
  }
  g_ctxs = malloc(sizeof(ContextAnchor));
  g_ctxs->cap = 1024;
  g_ctxs->count = 1; // 0 is reserved for main context
  g_ctxs->index = 0;
  g_ctxs->ctxs = calloc(g_ctxs->cap, sizeof(Context));

  for (size_t x = 0; x < g_ctxs->cap; x++) {
    __remake_ctx_stack(x);
  }
}

void g_anchor_free() {
  if (!g_ctxs) {
    return;
  }
  for (size_t i = 0; i < g_ctxs->cap; i++) {
    if (g_ctxs->ctxs[i].stack != NULL) {
      free(g_ctxs->ctxs[i].stack);
    }
  }
  free(g_ctxs->ctxs);
  free(g_ctxs);
  g_ctxs = NULL;
}
