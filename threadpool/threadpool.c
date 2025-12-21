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

#include "threadpool.h"
#include "../channels/mpmc.h"
#include <stdatomic.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

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

void *__set_worker(void *arg) {
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
