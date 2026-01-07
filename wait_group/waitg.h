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
------------------------------------------------------------------------------
WaitGroup — Minimal atomic wait group synchronization primitive for C

This module provides a very small synchronization primitive inspired by
Go's sync.WaitGroup.

A WaitGroup allows one or more threads or tasks to wait until a set of
operations has completed.

The implementation is intentionally minimal:
- Single atomic counter
- Busy-wait based
- No OS-level blocking
- No condition variables or futexes

This makes it suitable for:
- job systems
- task graphs
- engine subsystems
- low-level synchronization in controlled environments

⚠️ Limitations:
- Busy-wait (spin-based)
- No fairness guarantees
- Not suitable for long waits on general-purpose threads
- Requires `cpu_relax()` to be defined by the platform

------------------------------------------------------------------------------
USAGE

In exactly ONE source file:

    #define WAITG_IMPLEMENTATION
    #include "waitg.h"

In all other files:

    #include "waitg.h"

Create a wait group with an initial count:

    WaitGroup wg = wg_init(4);

Increment or decrement the counter:

    wg_add(&wg, 2);
    wg_done(&wg);

Wait until the counter reaches zero:

    wg_wait(&wg);

------------------------------------------------------------------------------
EXAMPLE

    void task(void *ctx) {
        WaitGroup *wg = ctx;
        do_work();
        wg_done(wg);
    }

    int main(void) {
        WaitGroup wg = wg_init(2);

        spawn_task(task, &wg);
        spawn_task(task, &wg);

        wg_wait(&wg);
        return 0;
    }

------------------------------------------------------------------------------
*/

#ifndef WAITG_H
#define WAITG_H

#include <stdatomic.h>
#include <stddef.h>

typedef struct WaitGroup_t {
  _Atomic size_t count;
} WaitGroup;
// Initializes a wait group with an initial counter value.
WaitGroup wg_init(size_t initial);

// Adds `n` to the wait group counter.
void wg_add(WaitGroup *wg, size_t n);

// Decrements the wait group counter by one.
// Must be called once for each completed work unit.
void wg_done(WaitGroup *wg);

// Blocks execution until the wait group counter reaches zero.
// This function spins using cpu_relax().
void wg_wait(WaitGroup *wg);

#endif // !WAITG_H

#if (defined (WAITG_IMPLEMENTATION))
WaitGroup wg_init(size_t initial) {
  WaitGroup wg;
  atomic_init(&wg.count, initial);
  return wg;
}

void wg_add(WaitGroup *wg, size_t n) {
  atomic_fetch_add_explicit(&wg->count, n, memory_order_release);
}

void wg_done(WaitGroup *wg) {
  atomic_fetch_sub_explicit(&wg->count, 1, memory_order_release);
}

void wg_wait(WaitGroup *wg) {
  while (atomic_load_explicit(&wg->count, memory_order_acquire) != 0) {
    cpu_relax();
  }
}
#endif
