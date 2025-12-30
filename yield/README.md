# Yield / Cooperative Tasks

> Note: This module only supports x86-64 processors.

This module provides a **minimal cooperative multitasking** for C, designed to serve as the foundation for 
async-like workflows within `SEAKUTILS`.
It allows functions to be executed in a cooperative task context, enabling them to pause and resume at defined 
points without using threads.

---
## Purpose 

- Enable **async-like behavior** in plain C.
- Support **cooperative tasks** that can yield execution back to a scheduler.
- Provide the building block for higher-level primitives such as futures, coroutines, or cooperative job systems.
- This is not a **full async framework** — it is a lightweight, internal utility for building async-aware systems.

---
## API

- Initialization & Cleanup
```c
void g_anchor_init();   // Initialize the runtime
void g_anchor_free();   // Free all task stacks and resources

void task_run(void(*func)(void *), void *ctx);  
void wait_for_tasks();
```

---
## Yield
```c
void yield(void);
```
Suspends the current task and resumes the next one.
Tasks must call this explicitly to allow cooperative scheduling.

---
## Usage Example
```c
#include "yield/yield.h"
#include <stddef.h>
#include <stdio.h>

void test(void *ctx) {
  size_t num = *(size_t *)ctx;
  for (size_t x = 0; x < num; x++) {
    printf("Hello! %ld \n", x);
    yield();
  }
}

int main() {
  printf("Starting... :D\n\n");
  g_anchor_init();
  size_t x = 5;
  task_run(test, &x);
  task_run(test, &x);
  wait_for_tasks();

  printf("\nEnding... \n");
  g_anchor_free();
  return 0;
}
```
