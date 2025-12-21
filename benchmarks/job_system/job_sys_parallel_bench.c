#define _GNU_SOURCE
#include "../../job_system/jobsystem.h"
#include <assert.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>


/*
 *  This benchmark is not viable until the job system's arena gets upgraded with a multi region Arena
 * */

#define NUM_JOBS 1000000
#define NUM_THREADS 4
_Atomic size_t completed = 0;

void compute_job(void *ctx) {
  int v = *(int *)ctx;
  volatile int x = v;
  for (int i = 0; i < 200; i++)
    x = x * 31 + i;

  atomic_fetch_add_explicit(&completed, 1, memory_order_relaxed);
}

static inline double diff_sec(struct timespec a, struct timespec b) {
  return (b.tv_sec - a.tv_sec) + (b.tv_nsec - a.tv_nsec) / 1e9;
}

int main() {
  ThreadPool *pool = threadpool_init_for_scheduler(NUM_THREADS);
  job_scheduler_spawn(pool);

  int *ctx = malloc(NUM_JOBS * sizeof(int));
  JobHandle **jobs = malloc(NUM_JOBS * sizeof(JobHandle *));

  for (size_t i = 0; i < NUM_JOBS; i++) {
    ctx[i] = i;
    jobs[i] = job_spawn(compute_job, &ctx[i]);
    assert(jobs[i]);
  }

  for (size_t i = 1; i < NUM_JOBS; i++) {
    job_then(jobs[i - 1], jobs[i]);
  }

  struct timespec start, end;
  clock_gettime(CLOCK_MONOTONIC, &start);
  job_wait(jobs[0]);

  while (atomic_load_explicit(&completed, memory_order_acquire) < NUM_JOBS)
    ; // spin

  clock_gettime(CLOCK_MONOTONIC, &end);

  double t = diff_sec(start, end);

  printf("Jobs: %d\n", NUM_JOBS);
  printf("Threads: %d\n", NUM_THREADS);
  printf("Time: %.3f s\n", t);
  printf("Throughput: %.2f M jobs/s\n", (NUM_JOBS / t) / 1e6);

  free(ctx);
  free(jobs);
  job_scheduler_shutdown();
}
