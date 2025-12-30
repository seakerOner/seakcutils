#ifndef ASYNC_H
#define ASYNC_H

#include <stdatomic.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

// x86-64bits assembly implementation

typedef struct Context_t Context;

typedef struct ContextAnchor_t ContextAnchor;

extern ContextAnchor *g_ctxs;

void __attribute__((naked)) yield(void);
void task_run(void(*func), void *ctx);
void wait_for_tasks();

void g_anchor_init();
void g_anchor_free();

#endif // !ASYNC_H
