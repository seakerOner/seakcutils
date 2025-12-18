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

#ifndef THREADPOOL_H
#define THREADPOOL_H

#include <pthread.h>

void *__set_worker(void *arg);
typedef void *(*__job)(void *);

typedef struct Job_t {
  __job job;
  void *arg;
} __Job__;

typedef struct Worker_t Worker;

typedef struct ThreadPool_t ThreadPool;

ThreadPool *threadpool_init(size_t num_threads);
void threadpool_execute(ThreadPool *threadpool, __job __func, void *arg);

void threadpool_shutdown(ThreadPool *threadpool);

void *__set_worker(void *arg);

#endif
