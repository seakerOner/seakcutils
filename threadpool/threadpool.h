#ifndef THREADPOOL_H
#define THREAD_POOL_H

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
