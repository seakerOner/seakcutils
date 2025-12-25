# Job System

The Job System is a lightweight, low-level API for executing jobs with explicit dependencies on top of a thread pool.
It is designed for **predictable behavior**, **explicit control**, and **minimal overhead**, making it suitable for engine-level systems and performance-critical code.

This is **not** a task graph framework.
It is a small, composable primitive that integrates well with other synchronization tools in the library.

---
- ## Overview
    - Jobs are represented by JobHandle
    - Jobs are created explicitly and scheduled explicitly
    - Dependencies are expressed via continuations
    - Jobs execute on a fixed thread pool
    - Memory management is deterministic and allocation-free during execution
- The system intentionally avoids:
    - Futures / promises
    - Automatic fan-out
    - Implicit synchronization
    - Hidden scheduling behavior

---
## Core Concepts

**JobHandle**
- A `JobHandle` represents a single unit of work:
    - A function: `void (*)(void *)`
    - A user-provided context pointer
    - Optional dependencies

Creating a **job does not execute it**.
A job only runs once it is explicitly scheduled and all its dependencies are resolved.

---
## Dependencies and Continuations

- Dependencies are expressed using **continuations**:
- A job may depend on one or more other jobs
- When a dependency finishes, it releases the dependent job
- When all dependencies are resolved, the job becomes executable

**Important constraints:**
- Each job supports a **single continuation**
- Fan-out (one job triggering multiple jobs) is not **supported by design**
- **Fan-out** and barriers **should be built using** other primitives (**e.g. wait groups**)
This design keeps the job system fast, simple, and predictable. 
Users should not have to pay the cost of fan-out bookkeeping if they are not going to need it

---
## Execution Guarantees

- A job executes **at most once**
- A job executes **only after all its dependencies complete**
- Dependencies are always respected
- Jobs are never re-scheduled internally

Reactive behavior:
- If a job is scheduled before it is ready, it may enter the queue
- If selected while dependencies are still pending, it is dropped
    - This avoids polluting the internal queue with blocked jobs
- Scheduling a job that is already considered completed is a no-op

Execution order:

**Single worker thread** → deterministic order
**Multiple worker threads** → parallel execution, order unspecified, dependencies respected

---
## API

```c

typedef struct Scheduler_t Scheduler;
extern Scheduler *g_scheduler;

ThreadPool *threadpool_init_for_scheduler(size_t num_threads);
void job_scheduler_spawn(ThreadPool *threadpool);
void job_scheduler_shutdown(void);


typedef void (*__job_handle)(void *);
typedef struct JobHandle_t JobHandle;

JobHandle *job_spawn(__job_handle fn, void *ctx);

void job_then(JobHandle *first, JobHandle *then);
void job_wait(JobHandle *job);

void job_chain(size_t num_jobs, ...);
void job_chain_arr(size_t num_jobs, JobHandle **job_list);
```
---
## API Semantics

### `job_spawn`
```c
JobHandle *job_spawn(__job_handle fn, void *ctx);
```

- Creates a job handle
- Does **not** schedule or execute the job
- The job will not run until explicitly scheduled

### `job_wait`
```c
void job_wait(JobHandle *job);
```

- Schedules a job for execution
- Does not block the calling thread
- If the job has unresolved dependencies, execution is deferred automatically

> Despite the name, `job_wait` means submit, not block. 
> The name reflects the intent of explicitly waiting for this specific job to complete

### `job_then`
```c
void job_then(JobHandle *first, JobHandle *then);
```
- Creates a dependency:
    - first is scheduled automatically
    - then will execute after first
    - then will still be scheduled after first (indirectly)

This is intended for simple dependency relationships, not complex graphs.

### `job_chain` / `job_chain_arr`
```c
job_chain(3, job1, job2, job3);
job_chain_arr(3, jobs);
```
Convenience helpers for linear chains:
```text
job1 → job2 → job3
```
- Each job depends on the previous one
- Only the first job is scheduled
- No extra allocation or overhead

---
## Typical Usage Patterns

### Simple Parallel Jobs
```c
#include "job_system/jobsystem.h"
#include <stdio.h>

void jobA(void *ctx) { printf("Job A\n"); }
void jobB(void *ctx) { printf("Job B\n"); }

int main() {
    ThreadPool *pool = threadpool_init_for_scheduler(4);
    job_scheduler_spawn(pool);

    JobHandle *job1 = job_spawn(jobA, NULL);
    JobHandle *job2 = job_spawn(jobB, NULL);

    JobHandle *jobs[3] = [ job1, job2, job3 ];

    job_wait(job1); 
    job_wait(job2); 

    job_scheduler_shutdown();
    return 0;
}
```
Both jobs may execute in parallel.

### Multiple Dependencies (Fan-in)
```c
JobHandle *a = job_spawn(jobA, NULL);
JobHandle *b = job_spawn(jobB, NULL);
JobHandle *c = job_spawn(jobC, NULL);

job_then(a, c);
job_then(b, c);
```
`c` executes only after both `a` and `b` finish.

### Linear Pipeline
```c
job_chain(3, jobA, jobB, jobC);
job_chain_arr(3, jobs);
```
Equivalent to:
```text
jobA → jobB → jobC
```
---
## Performance Characteristics
- Very low scheduling overhead
- No dynamic allocation during job execution
- Dependency chain serialize naturally; independent jobs scale across cores

This system favors explicit structure over flexibility.

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

The arena is automatically reused once it is safe to do so.

---
## What This Job System is (and is Not)

- **This system is**:
    - A fast, explicit job execution primitive
    - A good foundation for engine subsystems
    - Easy to compose with wait groups, barriers, and other tools
- **This system is not**:
    - A task graph engine
    - A future / promise system
    - A general-purpose async framework

---
## Recommendations

- Batch work when possible
- Keep dependency chains shallow
- Use wait groups or higher-level tools for fan-out
- Treat jobs as fire-and-forget units of work

---
## Limitations

- Jobs do not return values
- No built-in blocking or waiting
- No fan-out continuations
- Context lifetime is fully user-managed
- Thread-safe job submission must be handled externally

Some of these limitations are intentional to keep the Job System as slim as possible.
You add what you need, nothing else.

--- 
## Status
> Experimental, Stable Core

The execution model is stable and suitable for heavy workloads.
The API may evolve as the surrounding ecosystem matures.
