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
