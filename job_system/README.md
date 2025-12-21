# Job System

A lightweight dependency-aware job system built on top of a lock-free thread pool and multi-producer/multi-consumer channels. Designed for high-performance task scheduling in C with minimal overhead.

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

typedef void (*__job_handle)(void *);

typedef struct JobHandle_t JobHandle;
typedef struct Scheduler_t Scheduler;

extern Scheduler *g_scheduler;

ThreadPool *threadpool_init_for_scheduler(size_t num_threads);
void threadpool_schedule(SenderMpmc *sender, JobHandle *job_handle);

void job_scheduler_spawn(ThreadPool *threadpool);
void job_scheduler_shutdown(void);

JobHandle *job_spawn(__job_handle fn, void *ctx);
void job_then(JobHandle *first, JobHandle *then);
void job_wait(JobHandle *job);

```
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


> **Job System Limitation**
>
> The current Job System uses a fixed-size arena (4096 jobs).
> When the arena reaches capacity, it is reset and any pending or scheduled jobs
> become invalid.
>
> A region-based arena is planned to provide stable job handles, reuse, and
> unbounded job creation. Until then, this job system should be considered
> experimental and not suitable for production workloads.


