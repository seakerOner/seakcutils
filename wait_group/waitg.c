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
