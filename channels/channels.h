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

#ifndef CHANNELS_H
#define CHANNELS_H

#include <stdalign.h>
#include <stddef.h>
#include <stdint.h>

#ifndef CACHELINE_SIZE
#define CACHELINE_SIZE 64
#endif

/* Channel state */
typedef enum ChanState_t { OPEN = 0, CLOSED = 1 } ChanState;

/* Return codes */
#define CHANNEL_OK 0
#define CHANNEL_ERR_NULL -1
#define CHANNEL_ERR_CLOSED -4
#define CHANNEL_ERR_EMPTY -2
#define CHANNEL_ERR_FULL -3

typedef struct {
  alignas(CACHELINE_SIZE) _Atomic size_t tail;
  char _pad[CACHELINE_SIZE - sizeof(size_t)];
} ConsumerCursor;

typedef struct {
  alignas(CACHELINE_SIZE) _Atomic size_t head;
  char _pad[CACHELINE_SIZE - sizeof(size_t)];
} ProducerCursor;

typedef struct Slot_t {
  alignas(CACHELINE_SIZE) uint8_t *data;
  _Atomic size_t seq;
} Slot;

/*-------------------------------------------*/
/*      Platform-dependent cpu_relax()       */
/*-------------------------------------------*/
#if defined(__x86_64__) || defined(__i386__)

#include <immintrin.h>
#define cpu_relax() _mm_pause()
/*-------------------------------------------*/
#elif defined(__aarch64__) || defined(__arm__)

#define cpu_relax() __asm__ __volatile__("yield")
/*-------------------------------------------*/
#elif defined(__riscv)

#define cpu_relax() __asm__ __volatile__("pause")
/*-------------------------------------------*/
#else
#define cpu_relax() ((void)0)
#endif
/*-------------------------------------------*/

#endif // !CHANNELS_H
