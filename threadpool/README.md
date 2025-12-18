# Thread Pool

A minimal, high-performance **lock-free thread pool** implemented in C, built entirely on top of the project’s **SPMC (Single Producer, Multiple Consumer) channel**.

This thread pool is designed to be:
- Simple
- Predictable
- Explicit in its lifecycle management
- Free of mutexes and condition variables

All task dispatching and synchronization is handled via lock-free channels.

---

## Features

- **Fixed-size worker pool**
  - Number of threads is defined at initialization.
- **Lock-free task dispatch**
  - Uses a single-producer, multiple-consumer channel internally.
- **Work-stealing by competition**
  - Workers compete for jobs using atomic operations (no locks).
- **Blocking semantics**
  - Workers spin-wait while waiting for work.
  - The dispatcher spin-waits when the queue is full.
- **Graceful shutdown**
  - Global shutdown via channel close.
  - Workers exit automatically once the channel is closed and drained.
- **No dynamic allocation during execution**
  - All memory is allocated during initialization.
- **Zero hidden abstractions**
  - Tasks are simple function pointers + void* arguments.

---

## Design Overview

- The thread pool internally owns:
  - One **SPMC channel** for job dispatch.
  - One **SenderSpmc** used by the dispatcher.
  - One **ReceiverSpmc per worker thread**.
- The main thread is the **single producer**.
- Worker threads are **multiple consumers**, each executing exactly one job at a time.
- Each job is guaranteed to be:
  - Executed **at most once**
  - Executed by **exactly one worker**

---

## Job Representation

```c
typedef void *(*__job)(void *);

typedef struct Job_t {
    __job job;
    void *arg;
} __Job__;
```

- Each job consists of:
    - A function pointer
    - A single void * argument
    - No return value is collected by the thread pool

---

## API 
```c

typedef struct ThreadPool_t ThreadPool;

ThreadPool *threadpool_init(size_t num_threads);
void threadpool_execute(ThreadPool *threadpool, __job func, void *arg);
void threadpool_shutdown(ThreadPool *threadpool);
```

---

## Usage Example

```c

#include "threadpool.h"
#include <stdio.h>
#include <unistd.h>

void *task(void *arg) {
    int id = *(int *)arg;
    printf("Running task %d\n", id);
    sleep(1);
    return NULL;
}

int main() {
    ThreadPool *pool = threadpool_init(4);

    int args[8];
    for (int i = 0; i < 8; i++) {
        args[i] = i;
        threadpool_execute(pool, task, &args[i]);
    }

    threadpool_shutdown(pool);
    return 0;
}
```

---
## Communication Between Tasks

This thread pool is built on top of the same lock-free channels exposed by the library.

This means that:

- Tasks executed by the thread pool can safely:
    - Send messages to other tasks
    - Receive work from other threads
    - Build pipelines, fan-in/fan-out patterns, or feedback loops
- Users may use additional channels (SPSC, SPMC, MPSC, MPMC) and share them between tasks.

The thread pool does not restrict or virtualize communication — it simply executes jobs.
All synchronization and data flow is explicit and user-controlled via channels.

---

## Shutdown Semantics

- threadpool_shutdown:
    - Closes the internal SPMC channel.
    - Waits for all worker threads to finish.
    - Destroys the channel.
Frees all internal resources.
- No new jobs may be submitted after shutdown begins.
- Workers exit only after:
    - The channel is closed and
    - No more jobs are available.

---

## Thread-Safety & Guarantees

- Safe to call threadpool_execute from a single thread only.
- Worker threads are fully isolated and never share job state.
- Jobs are executed exactly once.
- No locking primitives (mutex, condvar) are used anywhere.

---

## Limitations

- Single producer only
    - Submitting jobs from multiple threads is undefined behavior.
- Busy waiting
    - Uses spin-waiting instead of sleeping.
    - Best suited for CPU-bound workloads.
- No future / result handling
    - Jobs cannot return values to the caller.
- No dynamic resizing
    - Worker count is fixed at initialization.
