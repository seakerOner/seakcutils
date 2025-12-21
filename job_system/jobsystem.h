#ifndef JOB_SYSTEM_H
#define JOB_SYSTEM_H

#include "../threadpool/threadpool.h"
#include <stddef.h>

ThreadPool *threadpool_init_for_scheduler(size_t num_threads);

typedef struct JobHandle_t JobHandle ;
void threadpool_schedule(SenderMpmc *sender, JobHandle *job_handle);

typedef struct Scheduler_t Scheduler;
extern Scheduler *g_scheduler;

typedef void (*__job_handle)(void *);

void job_scheduler_spawn(ThreadPool *threadpool);
void job_scheduler_shutdown(void);

JobHandle *job_spawn(__job_handle fn, void *ctx);
void job_then(JobHandle *first, JobHandle *then);
void job_wait(JobHandle *job);

#endif
