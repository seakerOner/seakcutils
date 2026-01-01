# Wait Group

The **Wait Group** is a minimal synchronization primitive used to wait for a group of independent operations to complete.

It provides a simple, explicit mechanism to **block until a counter reaches zero**, without spawning threads, allocating memory, 
or hiding execution behavior.

This module is intentionally small, designed to compose naturally with other utilities in this repository, 
especially the **Job System**.

---

## Overview

A `WaitGroup` tracks a counter representing the number of active operations.
Threads may increment or decrement this counter, and one or more threads may wait until it reaches zero.

- Typical use cases include:
    - Waiting for a batch of jobs to finish
    - Synchronizing execution phases
    - Coordinating work across threads without callbacks or futures

The `WaitGroup` itself is completely scheduler-agnostic

---
## API

```c
typedef struct {
    _Atomic size_t count;
} WaitGroup;

WaitGroup wg_init(size_t initial);
void wg_add(WaitGroup *wg, size_t n);
void wg_done(WaitGroup *wg);
void wg_wait(WaitGroup *wg);
```

- **Function Semantics**
    - `wg_init(initial)`
        - Initializes a wait group with an initial counter value
    - `wg_add(wg, n)`
        - Increments the counter by n
        - Must be called before the corresponding work starts
    - `wg_done(wg)`
        - Decrements the counter by 1
        - Must be called exactly once per completed unit of work
    - `wg_wait(wg)`
        - Block (spins) the calling thread until the counter reaches zero

---

## Example (Standlone)

```c
WaitGroup wg = wg_init(2);

thread_start(worker1, &wg);
thread_start(worker2, &wg);

wg_wait(&wg); // blocks until both workers call wg_done()
```

---

## Integration with the Job System

The `WaitGroup` pairs naturally with the **Job System**.

While the Job System handles **execution and dependencies**, the Wait Group provides an 
**explicit synchronization point** for external threads or execution phases.

**Example: Waiting for Jobs to Finish**
```c
#include "jobsystem.h"
#include "wait_group/waitg.h"

WaitGroup wg;

void job_fn(void *ctx) {
    WaitGroup *wg = ctx;
    // do work
    wg_done(wg);
}

int main() {
    ThreadPool *pool = threadpool_init_for_scheduler(4);
    job_scheduler_spawn(pool);

    wg = wg_init(3);

    JobHandle *a = job_spawn(job_fn, &wg);
    JobHandle *b = job_spawn(job_fn, &wg);
    JobHandle *c = job_spawn(job_fn, &wg);

    job_wait(a);
    job_wait(b);
    job_wait(c);

    wg_wait(&wg); // blocks until all jobs finish

    job_scheduler_shutdown();
}
```

**Important distinction**
- `job_wait` submits a job for execution (non-blocking)
- `wg_wait` blocks the calling thread

---

## Guarentees

- Lock-free
- No memory allocation
- No hidden threads
- No scheduler involvement
- Deterministic behavior

--- 

## Relationship to Other Primitives

|  Primitive  | Purpose                     |
|-------------|-----------------------------|
| Channels    | Message passing             |
| Thread Pool | Task execution              |
| Job System  | Dependency-aware scheduling |
| Wait Group  | Phase synchronization       |

---

## Advanced Use: Phased-Based Execution with Job System + `WaitGroup`

> This pattern is useful for engine-style workloads where execution is split into
> explicit phases (e.g update -> physics -> render prep)

### Scenario

We want the following execution order:

```text
Phase 1: Many independent jobs (parallel)
Phase 2: A dependent phase that spawns more jobs (runs once after Phase 1)
Phase 3: Another batch of parallel jobs
```

Key Idea
- Jobs **never block**
- Threads block explicitly using `wg_wait`
- The Job System executes work
- The Wait Group defines phase boundaries

### Example

```c
#include "job_system/jobsystem.h"
#include "wait_group/waitg.h"
#include <stdio.h>

void phase1_job(void *ctx) {
  WaitGroup *wg = ctx;
  printf("Phase 1 job\n");
  wg_done(wg);
}

typedef struct {
  int id;
  WaitGroup *wg_ref;
} RootCtx;

typedef struct {
  int id;
  int parent_id;
  WaitGroup *wg_root_ref;
  RootCtx *r_ctx;
} InnerJobCtx;

void phase2_task_inner_job(void *ctx) {
  InnerJobCtx *i_ctx = ctx;
  printf("Phase 2 task %d, inner job %d \n", i_ctx->parent_id, i_ctx->id);
  wg_done(i_ctx->wg_root_ref);
}

void phase2_job(void *ctx) {
  RootCtx *r_ctx = ctx;
  printf("Phase 2 task %d \n", r_ctx->id);

  const int num_inner_task_jobs = 2;
  JobHandle *jobs[num_inner_task_jobs];
  wg_add(r_ctx->wg_ref, num_inner_task_jobs);

  // Each phase2 job will run 2 jobs in sequence
  for (int x = 0; x < num_inner_task_jobs; x++) {
    InnerJobCtx *ctx = malloc(sizeof(InnerJobCtx));
    ctx->id = x;
    ctx->parent_id = r_ctx->id;
    ctx->wg_root_ref = r_ctx->wg_ref;
    ctx->r_ctx = r_ctx;
    jobs[x] = job_spawn(phase2_task_inner_job, ctx);
  }

  job_chain_arr(num_inner_task_jobs, jobs);

  wg_done(r_ctx->wg_ref);
}

void phase2_root(void *ctx) {
  WaitGroup *root_wg = ctx;
  printf("Phase 2 root \n");

  for (size_t i = 0; i < root_wg->count; ++i) {
    RootCtx *ctx = malloc(sizeof(RootCtx));
    ctx->id = (int)i;
    ctx->wg_ref = root_wg;
    JobHandle *t = job_spawn(phase2_job, ctx);
    job_wait(t);
  }
}

int main() {
  ThreadPool *pool = threadpool_init_for_scheduler(4);
  job_scheduler_spawn(pool);

  const size_t phase1_jobs = 1;
  WaitGroup wg = wg_init(phase1_jobs);

  for (size_t i = 0; i < phase1_jobs; ++i) {
    JobHandle *j = job_spawn(phase1_job, &wg);
    job_wait(j);
  }

  wg_wait(&wg);

  const int num_jobs = 2;
  WaitGroup root_wg = wg_init(num_jobs);
  JobHandle *root = job_spawn(phase2_root, &root_wg);
  job_wait(root);

  wg_wait(&root_wg);

  job_scheduler_shutdown();
}
```

What matters in this example:

- Phase 1 jobs decrement a wait group
- Phase 2 is only started after Phase 1 completes
- Phase 2 jobs spawn additional jobs
- A second wait group synchronizes the entire phase

---

## Why This Pattern Works Well

- **Job System**
    - Executes work
    - Enforces dependencies when needed
    - Remains simple and predictable
- **Wait Group** 
    - Synchronizes large batches of work 
    - Avoids deep continuation chains
    - Makes phase completion explicit
- **No abstraction leakage**
    - No futures
    - No callbacks
    - No hidden allocs
    - No implicit scheduler behavior

---

## When to Use This Pattern

- This approach is ideal for:
    - Game engines (frame phases)
    - Simulation steps
    - ECS system execution
    - Build systems and pipelines
    - Batch processing with strict phase boundaries

---

## Status

Stable and complete

The API is minimal and unlikely to change

