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
#include "./waitg.h"
#include "../channels/channels.h"
#include <stdatomic.h>
#include <stddef.h>

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
