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
channels.h — Common definitions for lock-free channels

This header defines shared types, constants, and platform utilities used by
all channel implementations (SPSC, MPSC, MPMC, etc).

It does NOT implement any specific channel.
Instead, it provides:
- common return codes
- cache-line aligned cursor structures
- slot metadata
- platform-specific cpu_relax()

All channel implementations depend on this header.

------------------------------------------------------------------------------
DESIGN GOALS

- Lock-free friendly
- Cache-line aware
- Cross-platform
- Minimal assumptions
- No allocation policy

------------------------------------------------------------------------------
CHANNEL STATES

Channels operate in one of two states:

    OPEN    — send/receive operations allowed
    CLOSED  — no further sends, receivers may drain

------------------------------------------------------------------------------
RETURN CODES

All channel operations return standardized status codes:

    CHANNEL_OK          Operation succeeded
    CHANNEL_ERR_NULL    Invalid or NULL argument
    CHANNEL_ERR_EMPTY   Channel is empty
    CHANNEL_ERR_FULL    Channel is full
    CHANNEL_ERR_CLOSED  Channel is closed

These codes are shared across all channel variants.

------------------------------------------------------------------------------
CACHE LINE ALIGNMENT

Producer and consumer cursors are cache-line aligned to avoid false sharing.

Each cursor:
- occupies its own cache line
- contains exactly one atomic index
- includes padding to fill the line

This is critical for high-performance concurrent queues.

------------------------------------------------------------------------------
CPU RELAX

cpu_relax() provides a platform-specific pause/yield instruction used in
busy-wait loops.

It:
- reduces power consumption
- prevents pipeline starvation

The instruction varies by architecture:
- x86/x64   → PAUSE
- ARM       → YIELD
- RISC-V    → PAUSE
- Fallback  → no-op

------------------------------------------------------------------------------
USAGE

This file is included by all channel implementations:

    #include "channels.h"

In exactly ONE source file:

    #define CHANNEL_BASICS_IMPLEMENTATION
    #include "channels.h"

------------------------------------------------------------------------------
*/
#ifndef CHANNELS_H
#define CHANNELS_H

#include <stdalign.h>
#include <stddef.h>
#include <stdint.h>

#ifndef CACHELINE_SIZE
#define CACHELINE_SIZE 64
#endif

// Channel lifecycle state.
typedef enum ChanState_t ChanState;

/* Return codes */
#define CHANNEL_OK 0
#define CHANNEL_ERR_NULL -1
#define CHANNEL_ERR_CLOSED -4
#define CHANNEL_ERR_EMPTY -2
#define CHANNEL_ERR_FULL -3

// Consumer-side cursor.
// Holds the tail index for receive operations.
// Cache-line aligned to avoid false sharing.
typedef struct ConsumerCursor_t ConsumerCursor;

// Producer-side cursor.
// Holds the head index for send operations.
// Cache-line aligned to avoid false sharing.
typedef struct ProducerCursor_t ProducerCursor;

// Slot metadata used by channel buffers.
// Each slot stores:
// - pointer to element storage
// - sequence number for synchronization
typedef struct Slot_t Slot;

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

#if (defined (CHANNEL_BASICS_IMPLEMENTATION))
/* Channel state */
typedef enum ChanState_t { OPEN = 0, CLOSED = 1 } ChanState;

typedef struct ConsumerCursor_t {
  alignas(CACHELINE_SIZE) _Atomic size_t tail;
  char _pad[CACHELINE_SIZE - sizeof(size_t)];
} ConsumerCursor;

typedef struct ProducerCursor_t {
  alignas(CACHELINE_SIZE) _Atomic size_t head;
  char _pad[CACHELINE_SIZE - sizeof(size_t)];
} ProducerCursor;

typedef struct Slot_t {
  alignas(CACHELINE_SIZE) uint8_t *data;
  _Atomic size_t seq;
} Slot;

#endif // !CHANNELS_H

