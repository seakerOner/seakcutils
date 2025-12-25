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

#include "jobsystem.h"
#include "../arenas/r_arena.h"
#include "../channels/mpmc.h"
#include "../threadpool/threadpool.h"
#include <assert.h>
#include <stdarg.h>
#include <stdatomic.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

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
