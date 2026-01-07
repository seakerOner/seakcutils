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

/*
===========================================================================
 JOB SYSTEM
===========================================================================
Job System: Lock-free job scheduler built on top of a ThreadPool, MPMC
channels, and JobHandles.

Features:
  - Spawn independent jobs
  - Support for dependent jobs (job_then, job_chain, job_chain_arr)
  - Compatible with WaitGroups
  - Automatic arena reset when job count nears max capacity
  - Lock-free scheduling with atomic counters

Typical usage:
  1. Initialize ThreadPool and Scheduler:
        ThreadPool *pool = threadpool_init_for_scheduler(num_threads);
        job_scheduler_spawn(pool);

  2. Spawn independent jobs:
        JobHandle *job = job_spawn(func, ctx);
        job_wait(job);

  3. Spawn dependent jobs:
        JobHandle *A = job_spawn(funcA, ctxA);
        JobHandle *B = job_spawn(funcB, ctxB);
        job_then(A, B); // B runs after A finishes

  4. Create job chains:
        JobHandle *jobs[3] = {job1, job2, job3};
        job_chain_arr(3, jobs); // executes sequentially

  5. Shutdown:
        job_scheduler_shutdown();

===========================================================================
CONSTANTS
===========================================================================
JOB_SCHEDULER_REGION_CAPACITY : Arena region size (4096)
JOB_SCHEDULER_MAX_REGIONS     : Maximum regions (1024)
JOB_SCHEDULER_MAX_JOBS        : Maximum jobs = CAPACITY * MAX_REGIONS

===========================================================================
MAIN TYPES
===========================================================================

Scheduler:
  - ThreadPool *threadpool          : pointer to internal thread pool
  - _Atomic uint8_t accepting_jobs  : 0 = not accepting, 1 = accepting
  - _Atomic size_t active_jobs      : number of active jobs
  - _Atomic size_t jobs_completed_epoch : jobs executed since last reset
  - RegionArena job_arena           : arena storing JobHandles

JobHandle:
  - __job_handle Job               : function to execute
  - void *ctx                      : user-provided context pointer
  - _Atomic size_t unfinished      : counter for dependencies
  - JobHandle *continuation        : pointer to dependent job

ThreadPool:
  - Array of worker threads
  - Each worker consumes jobs from an MPMC channel
  - Runs jobs and schedules dependent continuations

===========================================================================
MAIN FUNCTIONS
===========================================================================

ThreadPool *threadpool_init_for_scheduler(size_t num_threads)
  - Creates a thread pool for the scheduler
  - Initializes lock-free MPMC channels for job dispatch

void job_scheduler_spawn(ThreadPool *threadpool)
  - Initializes the global scheduler (g_scheduler)
  - Allocates internal arena for JobHandles

void job_scheduler_shutdown(void)
  - Shuts down the scheduler and thread pool
  - Frees internal arena memory

JobHandle *job_spawn(__job_handle fn, void *ctx)
  - Creates a JobHandle
  - Job is not executed immediately; it can be scheduled using:
      - job_then
      - job_chain / job_chain_arr
      - job_wait

void job_then(JobHandle *first, JobHandle *then)
  - Schedules `then` to execute **after** `first` finishes

void job_chain(size_t num_jobs, ...)
  - Creates a sequential chain of jobs using variadic arguments

void job_chain_arr(size_t num_jobs, JobHandle **job_list)
  - Creates a sequential chain of jobs using an array

void job_wait(JobHandle *job)
  - Schedules the job for execution immediately
  - Can be used for independent jobs or as the root of a chain

===========================================================================
IMPORTANT NOTES
===========================================================================
- All internal counters are atomic for lock-free thread safety.
- Scheduler performs automatic health checks to reset the arena
  when the job count approaches JOB_SCHEDULER_MAX_JOBS.
- JobHandle::unfinished ensures proper synchronization of dependent jobs.
- job_then / job_chain increment and decrement unfinished correctly.
- Uses RegionArena to reduce malloc/free overhead.

===========================================================================
USAGE EXAMPLE
===========================================================================

#define REGION_ARENA_IMPLEMENTATION
#include "./arenas/r_arena.h"
#define CHANNEL_BASICS_IMPLEMENTATION
#include "channels/channels.h"
#define MPMC_IMPLEMENTATION
#include "channels/mpmc.h"
#define THREADPOOL_IMPLEMENTATION
#include "threadpool/threadpool.h"
#define JOBSYSTEM_IMPLEMENTATION
#include "job_system/jobsystem.h"
#define WAITG_IMPLEMENTATION
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

  / ---------- Phase 1 ---------- /

const size_t phase1_jobs = 1;
WaitGroup wg = wg_init(phase1_jobs);

for (size_t i = 0; i < phase1_jobs; ++i) {
  JobHandle *j = job_spawn(phase1_job, &wg);
  job_wait(j);
}

/ ---------- Barrier ---------- /

wg_wait(&wg); // fan-in point

/ ---------- Phase 2 ---------- /

const int num_jobs = 2;
WaitGroup root_wg = wg_init(num_jobs);
JobHandle *root = job_spawn(phase2_root, &root_wg);
job_wait(root);

wg_wait(&root_wg);

job_scheduler_shutdown();
}
*/
#ifndef JOB_SYSTEM_H
#define JOB_SYSTEM_H

typedef struct ThreadPool_t ThreadPool;

#include <stddef.h>

#define JOB_SCHEDULER_REGION_CAPACITY 4096
#define JOB_SCHEDULER_MAX_REGIONS 1024
#define JOB_SCHEDULER_MAX_JOBS                                                 \
  (JOB_SCHEDULER_REGION_CAPACITY * JOB_SCHEDULER_MAX_REGIONS)

ThreadPool *threadpool_init_for_scheduler(size_t num_threads);

typedef struct JobHandle_t JobHandle;

typedef struct Scheduler_t Scheduler;
extern Scheduler *g_scheduler;

typedef void (*__job_handle)(void *);

void job_scheduler_spawn(ThreadPool *threadpool);
void job_scheduler_shutdown(void);

JobHandle *job_spawn(__job_handle fn, void *ctx);
void job_chain(size_t num_jobs, ...);
void job_chain_arr(size_t num_jobs, JobHandle **job_list);
void job_then(JobHandle *first, JobHandle *then);
void job_wait(JobHandle *job);

#endif

#if (defined(JOBSYSTEM_IMPLEMENTATION))
#include <assert.h>
#include <stdarg.h>
#include <stdatomic.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

typedef struct RegionArena_t RegionArena;
typedef struct SenderMpmc_t SenderMpmc;

Scheduler *g_scheduler = NULL;

static void *__set_worker_scheduler(void *arg);
static void _job_scheduler_healthcheck(void);
static void threadpool_schedule(SenderMpmc *sender, JobHandle *scheduled_job);

typedef struct Scheduler_t {
  ThreadPool *threadpool;
  _Atomic uint8_t accepting_jobs; // 0 -> accepting or 1 -> not accepting
  _Atomic size_t active_jobs;
  _Atomic size_t jobs_completed_epoch;
  RegionArena job_arena;
} Scheduler;

typedef struct JobHandle_t {
  __job_handle Job;
  void *ctx;

  _Atomic size_t unfinished;
  struct JobHandle_t *continuation;
} JobHandle;

void job_scheduler_spawn(ThreadPool *threadpool) {
  Scheduler *sche = malloc(sizeof(Scheduler));
  sche->job_arena =
      r_arena_create(sizeof(JobHandle), JOB_SCHEDULER_REGION_CAPACITY,
                     JOB_SCHEDULER_MAX_REGIONS);
  sche->threadpool = threadpool;
  atomic_init(&sche->accepting_jobs, 1);
  atomic_init(&sche->active_jobs, 0);
  atomic_init(&sche->jobs_completed_epoch, 0);
  g_scheduler = sche;
};

void job_scheduler_shutdown(void) {
  threadpool_shutdown(g_scheduler->threadpool);
  r_arena_free(&g_scheduler->job_arena);
  free(g_scheduler);
};

JobHandle *job_spawn(__job_handle fn, void *ctx) {
  // small contention, rare to be needed as scheduler temporarily doesnt accept
  // jobs only when it needs to reset the inner arena
  while (atomic_load_explicit(&g_scheduler->accepting_jobs,
                              memory_order_acquire) != 1) {
    cpu_relax();
  }
  atomic_fetch_add_explicit(&g_scheduler->active_jobs, 1, memory_order_acq_rel);

  JobHandle *job = (JobHandle *)r_arena_alloc(&g_scheduler->job_arena);
  if (!job) {
    atomic_fetch_sub_explicit(&g_scheduler->active_jobs, 1,
                              memory_order_acq_rel);
    return NULL;
  }
  job->Job = fn;
  job->ctx = ctx;
  atomic_init(&job->unfinished, 1);
  job->continuation = NULL;
  return job;
};

/* this will schedule `first` and `then` when `first` finishes */
void job_then(JobHandle *first, JobHandle *then) {
  first->continuation = then;
  atomic_fetch_add_explicit(&then->unfinished, 1, memory_order_release);
  threadpool_schedule(g_scheduler->threadpool->dispatcher, first);
};

void job_chain(size_t num_jobs, ...) {
  va_list args;
  JobHandle *job_to_schedule;

  uint8_t first = 1;
  JobHandle *prev_job;
  va_start(args, num_jobs);
  for (size_t x = 0; x < num_jobs; x++) {
    JobHandle *job = va_arg(args, JobHandle *);
    if (first == 1) {
      job_to_schedule = job;
      prev_job = job;
      first = 0;
    } else {
      prev_job->continuation = job;
      atomic_fetch_add_explicit(&job->unfinished, 1, memory_order_release);
      prev_job = job;
    }
  }
  va_end(args);

  threadpool_schedule(g_scheduler->threadpool->dispatcher, job_to_schedule);
}

void job_chain_arr(size_t num_jobs, JobHandle **job_list) {
  JobHandle *job_to_schedule;

  uint8_t first = 1;
  JobHandle *prev_job;
  for (size_t x = 0; x < num_jobs; x++) {
    JobHandle *job = job_list[x];
    if (first == 1) {
      job_to_schedule = job;
      prev_job = job;
      first = 0;
    } else {
      prev_job->continuation = job;
      atomic_fetch_add_explicit(&job->unfinished, 1, memory_order_release);
      prev_job = job;
    }
  }

  threadpool_schedule(g_scheduler->threadpool->dispatcher, job_to_schedule);
}

void job_wait(JobHandle *job) {
  threadpool_schedule(g_scheduler->threadpool->dispatcher, job);
};

ThreadPool *threadpool_init_for_scheduler(size_t num_threads) {
  ThreadPool *tp = malloc(sizeof(ThreadPool));

  tp->workers = malloc(num_threads * sizeof(pthread_t));
  tp->num_workers = num_threads;

  tp->channel =
      channel_create_mpmc(JOB_SCHEDULER_MAX_JOBS, sizeof(JobHandle *));
  tp->dispatcher = mpmc_get_sender(tp->channel);

  for (size_t i = 0; i < num_threads; i++) {
    Worker *worker = malloc(sizeof(Worker));
    worker->receiver = mpmc_get_receiver(tp->channel);
    worker->sender = mpmc_get_sender(tp->channel);
    worker->chan_ref = tp->channel;

    pthread_create(&tp->workers[i], NULL, __set_worker_scheduler, worker);
  }

  return tp;
}

static void *__set_worker_scheduler(void *arg) {
  Worker *worker = (Worker *)arg;
  JobHandle *job;
  while (1) {
    if (mpmc_recv(worker->receiver, &job) == CHANNEL_OK) {
      assert(job != NULL);
      assert(job->Job != NULL);

      if (atomic_load_explicit(&job->unfinished, memory_order_acquire) == 1) {
        // --> run job
        job->Job(job->ctx);
        atomic_fetch_add_explicit(&g_scheduler->jobs_completed_epoch, 1,
                                  memory_order_release);
        atomic_fetch_sub_explicit(&job->unfinished, 1, memory_order_release);

        if (job->continuation) {
          atomic_fetch_sub_explicit(&job->continuation->unfinished, 1,
                                    memory_order_release);
          threadpool_schedule(worker->sender, job->continuation);
        } else {
          _job_scheduler_healthcheck();
        }

        atomic_fetch_sub_explicit(&g_scheduler->active_jobs, 1,
                                  memory_order_acq_rel);
      }
    } else if (mpmc_is_closed(worker->chan_ref) == CLOSED) {
      break;
    }
  }

  mpmc_close_sender(worker->sender);
  mpmc_close_receiver(worker->receiver);
  free(worker->receiver);
  free(worker->sender);
  free(worker);
  return NULL;
};

static void threadpool_schedule(SenderMpmc *sender, JobHandle *scheduled_job) {
  if (atomic_load_explicit(&scheduled_job->unfinished, memory_order_acquire) ==
      0) {
    return;
  }
  mpmc_send(sender, &scheduled_job);
};

static void _job_scheduler_reset(void);

static void _job_scheduler_healthcheck(void) {
  if (atomic_load_explicit(&g_scheduler->jobs_completed_epoch,
                           memory_order_acquire) >
      JOB_SCHEDULER_MAX_JOBS - 20) {
    _job_scheduler_reset();
  }
}

static void _job_scheduler_reset(void) {
  atomic_store_explicit(&g_scheduler->accepting_jobs, 0, memory_order_release);

  while (atomic_load_explicit(&g_scheduler->active_jobs,
                              memory_order_acquire) != 0) {
    cpu_relax();
  }
  r_arena_reset(&g_scheduler->job_arena);
  atomic_store_explicit(&g_scheduler->jobs_completed_epoch, 0,
                        memory_order_release);

  atomic_store_explicit(&g_scheduler->accepting_jobs, 1, memory_order_release);
}
#endif
