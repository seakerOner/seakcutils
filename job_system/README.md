# Job System

The Job System provides a lightweight, low-level abstraction for executing dependent jobs across a thread pool. It is designed for predictable performance, explicit memory ownership, and minimal synchronization overhead.

---
## Overview

- Jobs are represented by `JobHandle`
- Jobs can be chained using continuations (`job_then`)
- Execution is backed by a thread pool using an MPMC channel
- Memory is managed via a multi-region arena

--- 
## Features

- Dependency management
    - Jobs can have continuations that are scheduled automatically when the previous job finishes.
- Parallel execution
    - Multiple worker threads execute jobs concurrently while respecting dependencies.
- Deterministic single-threaded execution
    - When using a single worker thread, jobs execute in the order they are scheduled.
- With multiple threads, independent jobs may execute in any order but still respecting dependency.
    - Lock-free design
- Uses MPMC channels and atomic operations internally.
    - No dynamic allocation during execution
- All job handles are allocated from a pre-allocated arena.

---
## Architecture Overview

- The system has a global scheduler (`Scheduler`) that manages a job arena and a thread pool.
- Job handles (`JobHandle`) store:
    - The function to execute
    - A context pointer (`void *ctx`)
    - A counter tracking unfinished work
    - Optional continuation jobs
- The thread pool executes jobs from a channel and automatically schedules continuations.
- Channels ensure safe, lock-free communication between threads.

---
## API

```c

extern Scheduler *g_scheduler;

ThreadPool *threadpool_init_for_scheduler(size_t num_threads);

typedef struct JobHandle_t JobHandle;

typedef struct Scheduler_t Scheduler;

typedef void (*__job_handle)(void *);

void job_scheduler_spawn(ThreadPool *threadpool);
void job_scheduler_shutdown(void);

JobHandle *job_spawn(__job_handle fn, void *ctx);
void job_then(JobHandle *first, JobHandle *then);
void job_wait(JobHandle *job);

```

- `job_spawn` creates a new job handle
- `job_then` creates a dependency between jobs
- `job_wait` schedules the job and waits for its completion
- `job_scheduler_spawn` initializes the scheduler
- `job_scheduler_shutdown` shuts everything down and frees memory

---
## Job Capacity

The Job System uses a region-based arena:

```c
#define JOB_SCHEDULER_REGION_CAPACITY 4096
#define JOB_SCHEDULER_MAX_REGIONS 1024
#define JOB_SCHEDULER_MAX_JOBS (JOB_SCHEDULER_REGION_CAPACITY * JOB_SCHEDULER_MAX_REGIONS)
```

- Theoretical maximum: ~4.2 million jobs
- Practical tested limit: ~1 million jobs spawned at once
- The limit is affected by:
    - Channel capacity
    - Available memory
    - Job dependency patterns

The arena is reset automatically once the completed job counter approaches the maximum capacity, ensuring stable reuse of memory.

---
## Channel Sizing

- Each job increments the scheduler’s `active_jobs` counter on spawn
- `active_jobs` is decremented **only after the job finishes and no continuation remains**
- If a job has a continuation, the next job is scheduled automatically
- The arena is reset only when:
    - No jobs are active
    - The completed job counter reaches a safe threshold (**MAX_JOBS - 20**)

---
## Performance Notes

- Job execution is fully parallel across worker threads
- Dependency chains (`job_then`) are serialized locally by design, while independent jobs remain fully parallel (`job_wait`)
- Job throughput depends heavily on job granularity and continuation depth
- Benchmarks vary significantly based on workload and are intentionally not provided as absolute numbers

---
## Recommendations

Prefer batching jobs instead of spawning millions at once
Avoid extremely deep continuation chains
Adjust arena and channel sizes for your workload
This system is intended for engine-level usage, not fine-grained task scheduling

---

>Experimental Status
>
>This job system is stable for heavy workloads but still evolving. APIs and internal behavior may change.

---
## Usage Example

```c

#include "jobsystem.h"
#include <stdio.h>

void jobA(void *ctx) { printf("Job A\n"); }
void jobB(void *ctx) { printf("Job B\n"); }
void jobC(void *ctx) { printf("Job C\n"); }

int main() {
    ThreadPool *pool = threadpool_init_for_scheduler(4);
    job_scheduler_spawn(pool);

    JobHandle *job1 = job_spawn(jobA, NULL);
    JobHandle *job2 = job_spawn(jobB, NULL);
    JobHandle *job3 = job_spawn(jobC, NULL);

    job_then(job1, job2); // job2 runs after job1
    job_then(job3, job2); // job2 also depends on job3

    job_wait(job2); // schedule job2 

    job_scheduler_shutdown();
    return 0;
}

```
### Notes on execution order:
- With **1 worker thread**: Jobs execute in the order they were scheduled (**deterministic**).
- With **multiple threads**: Independent jobs may run **concurrently in any order**, **but** continuations **always respect dependencies**.

---
## Design Considerations

- Lock-free channels are used for scheduling, avoiding mutexes and condition variables.
- Continuation counters ensure that dependent jobs are executed only after all prerequisites complete.
- Arena allocation avoids dynamic memory allocation for job handles at runtime.
- Busy-waiting is used for scheduling, making it more suitable for CPU-bound workloads.

---
## Limitations
- Job execution is fire-and-forget: there is no result returned from jobs.
- Users are responsible for ensuring proper context data lifetime.
- Submitting jobs from multiple threads without proper synchronization is undefined behavior.
- No built-in futures or promises; dependencies are handled only via job_then.
