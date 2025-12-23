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
#include "../arenas/arena.h"
#include "../channels/mpmc.h"
#include "../threadpool/threadpool.h"
#include <assert.h>
#include <stdatomic.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

Scheduler *g_scheduler = NULL;

static void *__set_worker_scheduler(void *arg);

typedef struct Scheduler_t {
  ThreadPool *threadpool;
  Arena job_arena;
  _Atomic size_t counter;
} Scheduler;

typedef struct JobHandle_t {
  __job_handle Job;
  void *ctx;

  _Atomic size_t unfinished;
  struct JobHandle_t *continuation;
} JobHandle;

void job_scheduler_spawn(ThreadPool *threadpool) {
  Scheduler *sche = malloc(sizeof(Scheduler));
  sche->job_arena = arena_create(sizeof(JobHandle), 4096, FIXED);
  sche->threadpool = threadpool;
  sche->counter = 0;
  g_scheduler = sche;
};

void job_scheduler_shutdown(void) {
  threadpool_shutdown(g_scheduler->threadpool);
  arena_free(&g_scheduler->job_arena);
};

JobHandle *job_spawn(__job_handle fn, void *ctx) {
  JobHandle *job = (JobHandle *)arena_alloc(&g_scheduler->job_arena);
  if (!job)
    return NULL;
  job->Job = fn;
  job->ctx = ctx;
  atomic_init(&job->unfinished, 1);
  job->continuation = NULL;
  return job;
};

void job_then(JobHandle *first, JobHandle *then) {
  first->continuation = then;
  atomic_fetch_add_explicit(&then->unfinished, 1, memory_order_release);
  threadpool_schedule(g_scheduler->threadpool->dispatcher, first);
};

void job_wait(JobHandle *job) {
  threadpool_schedule(g_scheduler->threadpool->dispatcher, job);
};

ThreadPool *threadpool_init_for_scheduler(size_t num_threads) {
  ThreadPool *tp = malloc(sizeof(ThreadPool));

  tp->workers = malloc(num_threads * sizeof(pthread_t));
  tp->num_workers = num_threads;

  tp->channel = channel_create_mpmc(num_threads * 4, sizeof(JobHandle *));
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
      job->Job(job->ctx);
      if (atomic_fetch_sub_explicit(&job->unfinished, 1,
                                    memory_order_acq_rel) == 1 &&
          job->continuation) {
        if (job->continuation) {
          threadpool_schedule(worker->sender, job->continuation);
        }
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

void threadpool_schedule(SenderMpmc *sender, JobHandle *scheduled_job) {
  mpmc_send(sender, &scheduled_job);
};
