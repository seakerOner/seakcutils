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

/*------------------------------------------------------------------------------
threadpool.h — Minimal Multi-Threaded Job Executor

This thread pool implementation allows submitting arbitrary jobs (functions with
a single void* argument) to a fixed pool of worker threads. The job dispatching
is lock-free and uses a Multi-Producer Multi-Consumer (MPMC) channel for
queueing.

------------------------------------------------------------------------------
STRUCTS

__Job__:
  Represents a single unit of work.
  - job : function pointer (__job) to execute
  - arg : argument to pass to the function

Worker:
  Represents a single thread inside the thread pool.
  - receiver : channel receiver to get jobs
  - sender   : channel sender to propagate jobs if needed
  - chan_ref : reference to the shared MPMC channel

ThreadPool:
  Represents the pool itself.
  - workers     : array of pthread_t for each thread
  - num_workers : number of threads in the pool
  - channel     : shared MPMC channel used for job dispatch
  - dispatcher  : sender handle used to submit jobs

------------------------------------------------------------------------------
FUNCTIONS

threadpool_init
  Allocates and initializes a new thread pool.

  num_threads : number of worker threads

  Returns a pointer to ThreadPool on success, NULL on failure.

  Notes:
    - Allocates an MPMC channel with capacity = num_threads * 4.
    - Spawns num_threads worker threads.
    - Each worker listens for jobs via the channel.
    - The thread pool is ready to execute jobs immediately.

threadpool_execute
  Submit a job to the thread pool.

  threadpool : pointer to a valid ThreadPool
  __func     : function pointer (__job) representing the job
  arg        : argument to pass to the job function

  Notes:
    - Non-blocking; will busy-wait internally only if the channel is full.
    - Jobs are executed in order of arrival by available workers.
    - Multiple producers can safely call this concurrently.

threadpool_shutdown
  Gracefully shuts down the thread pool.

  threadpool : pointer to a valid ThreadPool

  Notes:
    - Closes the dispatcher and underlying channel.
    - Joins all worker threads, waiting for them to finish.
    - Frees all associated memory.
    - After this call, the ThreadPool pointer becomes invalid.*/

#ifndef THREADPOOL_H
#define THREADPOOL_H

#include <pthread.h>

typedef struct ReceiverMpmc_t ReceiverMpmc;
typedef struct SenderMpmc_t SenderMpmc;
typedef struct ChannelMpmc_t ChannelMpmc;

typedef void *(*__job)(void *);

typedef struct Job_t {
  __job job;
  void *arg;
} __Job__;

typedef struct Worker_t {
  ReceiverMpmc *receiver;
  SenderMpmc *sender;
  ChannelMpmc *chan_ref;
} Worker;

typedef struct ThreadPool_t {
  pthread_t *workers;
  size_t num_workers;

  ChannelMpmc *channel;
  SenderMpmc *dispatcher;

} ThreadPool;

ThreadPool *threadpool_init(size_t num_threads);
void threadpool_execute(ThreadPool *threadpool, __job __func, void *arg);

void threadpool_shutdown(ThreadPool *threadpool);

#endif

#if (defined(THREADPOOL_IMPLEMENTATION))
#include <stdatomic.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

static void *__set_worker(void *arg);

ThreadPool *threadpool_init(size_t num_threads) {
  ThreadPool *tp = malloc(sizeof(ThreadPool));

  tp->workers = malloc(num_threads * sizeof(pthread_t));
  tp->num_workers = num_threads;

  tp->channel = channel_create_mpmc(num_threads * 4, sizeof(__Job__));
  tp->dispatcher = mpmc_get_sender(tp->channel);

  for (size_t i = 0; i < num_threads; i++) {
    Worker *worker = malloc(sizeof(Worker));
    worker->receiver = mpmc_get_receiver(tp->channel);
    worker->sender = mpmc_get_sender(tp->channel);
    worker->chan_ref = tp->channel;

    pthread_create(&tp->workers[i], NULL, __set_worker, worker);
  }

  return tp;
}

void threadpool_execute(ThreadPool *threadpool, __job __func, void *arg) {
  __Job__ job;
  job.job = __func;
  job.arg = arg;

  mpmc_send(threadpool->dispatcher, &job);
};

void threadpool_shutdown(ThreadPool *threadpool) {
  mpmc_close_sender(threadpool->dispatcher);
  mpmc_close(threadpool->channel);
  for (size_t x = 0; x < threadpool->num_workers; x++) {
    pthread_join(threadpool->workers[x], NULL);
  }
  mpmc_destroy(threadpool->channel);
  free(threadpool->workers);
  free(threadpool->dispatcher);
  free(threadpool);
};

static void *__set_worker(void *arg) {
  Worker *worker = (Worker *)arg;
  __Job__ job;
  while (1) {
    if (mpmc_recv(worker->receiver, &job) == CHANNEL_OK) {
      job.job(job.arg);
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
#endif
