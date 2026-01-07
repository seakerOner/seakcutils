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
mpsc_channel.h — Multi-Producer Single-Consumer lock-free channel

This channel supports:
- multiple producers
- exactly one consumer
- fixed-capacity ring buffer
- busy-wait synchronization (no blocking, no syscalls)

The implementation is lock-free and cache-line aware.

------------------------------------------------------------------------------
PROPERTIES

- Wait-free for each producer (bounded by buffer capacity)
- Lock-free for consumer
- No dynamic allocation during send/recv
- No external dependencies
- Cross-platform (x86, ARM, RISC-V)

------------------------------------------------------------------------------
LIFETIME

1. channel_create_mpsc()
2. mpsc_get_sender() (N times)
3. mpsc_get_receiver() (exactly once)
4. mpsc_send() / mpsc_recv()
5. mpsc_close()
6. mpsc_close_sender() for each sender
7. mpsc_destroy()

Senders and receiver must be freed by the user.

------------------------------------------------------------------------------
WARNING

This channel uses busy-waiting.
It is intended for short-lived waits (job systems, engine internals).
------------------------------------------------------------------------------
*/
#ifndef MPSC_CHANNEL_H
#define MPSC_CHANNEL_H

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


typedef struct ChannelMpsc_t ChannelMpsc;
typedef enum ChanState_t ChanState;

/*-----------------------------------------------------------------------------
  channel_create_mpsc
  Allocates and initializes a new lock-free channel with a fixed capacity.

  capacity  : number of slots in the ring buffer
  elem_size : size in bytes of each element

  Returns a pointer to ChannelMpsc on success, NULL on allocation failure.

  Notes:
    - Multiple producers can safely send concurrently.
    - Only one consumer is supported.
    - Allocates internal memory for slots and element buffers.
-----------------------------------------------------------------------------*/
ChannelMpsc *channel_create_mpsc(const size_t capacity, const size_t elem_size);

/*-----------------------------------------------------------------------------
  mpsc_close
  Marks the channel as closed.

  chan : pointer to the channel to close

  Notes:
    - After closing, mpsc_send will return CHANNEL_ERR_CLOSED.
    - Consumer may continue to drain the channel until empty.
-----------------------------------------------------------------------------*/
void mpsc_close(ChannelMpsc *chan);

/*-----------------------------------------------------------------------------
  mpsc_is_closed
  Checks whether the channel has been closed.

  chan : pointer to the channel

  Returns:
    - OPEN   if the channel is still open
    - CLOSED if the channel has been closed

  Notes:
    - Non-blocking, safe to call concurrently.
-----------------------------------------------------------------------------*/
ChanState mpsc_is_closed(const ChannelMpsc *chan);

/*-----------------------------------------------------------------------------
  mpsc_destroy
  Frees all memory associated with the channel and waits for all producers
  to finish.

  chan : pointer to the channel

  Notes:
    - Blocks until all active producers call mpsc_close_sender.
    - Also frees element buffers.
    - After this call, the channel pointer becomes invalid.
-----------------------------------------------------------------------------*/
void mpsc_destroy(ChannelMpsc *chan);

typedef struct SenderMpsc_t SenderMpsc;
typedef struct ReceiverMpsc_t ReceiverMpsc;

/*-----------------------------------------------------------------------------
  mpsc_get_sender
  Allocates and returns a producer handle for the given channel.

  chan : pointer to a valid ChannelMpsc

  Returns a pointer to SenderMpsc on success, NULL on failure.

  Notes:
    - Multiple senders can be attached.
    - Each sender must call mpsc_close_sender before freeing.
-----------------------------------------------------------------------------*/
SenderMpsc *mpsc_get_sender(ChannelMpsc *chan);

/*-----------------------------------------------------------------------------
  mpsc_get_receiver
  Allocates and returns a consumer handle for the given channel.

  chan : pointer to a valid ChannelMpsc

  Returns a pointer to ReceiverMpsc on success, NULL on failure.

  Notes:
    - Only one receiver is supported.
    - The receiver does not need to close explicitly.
-----------------------------------------------------------------------------*/
ReceiverMpsc *mpsc_get_receiver(ChannelMpsc *chan);

/*-----------------------------------------------------------------------------
  mpsc_close_sender
  Marks a sender as closed and decrements the channel's producer count.

  sender : pointer to a valid SenderMpsc

  Notes:
    - Must be called once per sender before freeing.
    - After this call, mpsc_send will return CHANNEL_ERR_CLOSED for this sender.
-----------------------------------------------------------------------------*/
void mpsc_close_sender(SenderMpsc *sender);

/*-----------------------------------------------------------------------------
  mpsc_send
  Sends an element to the channel.

  sender  : pointer to a valid SenderMpsc
  element : pointer to the element data to send

  Returns:
    - CHANNEL_OK          on success
    - CHANNEL_ERR_NULL    if sender is NULL
    - CHANNEL_ERR_CLOSED  if channel is closed

  Notes:
    - Busy-waits if the ring buffer slot is not available.
    - Lock-free and wait-free for each producer.
    - Copies elem_size bytes from element to the internal buffer.
-----------------------------------------------------------------------------*/
int mpsc_send(SenderMpsc *sender, const void *element);

/*-----------------------------------------------------------------------------
  mpsc_recv
  Receives an element from the channel.

  receiver : pointer to a valid ReceiverMpsc
  out      : pointer to memory where the element will be copied

  Returns:
    - CHANNEL_OK          on success
    - CHANNEL_ERR_NULL    if receiver is NULL
    - CHANNEL_ERR_EMPTY   if no new element is available

  Notes:
    - Only one consumer is supported.
    - Lock-free for the consumer.
    - Copies elem_size bytes into the memory pointed to by out.
-----------------------------------------------------------------------------*/
int mpsc_recv(ReceiverMpsc *receiver, void *out);

#endif

#if (defined(MPSC_IMPLEMENTATION))
#include <stdalign.h>
#include <stdatomic.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

typedef struct ChannelMpsc_t {
  Slot *buffer;
  size_t capacity;  // number of elements
  size_t elem_size; // sizeof(T)
  ProducerCursor producer;
  ConsumerCursor consumer;

  _Atomic size_t prod_cont; // Number of active producers
  _Atomic ChanState state;  // 0 -> Open | 1 -> Closed
} ChannelMpsc;

ChannelMpsc *channel_create_mpsc(const size_t capacity,
                                 const size_t elem_size) {
  ChannelMpsc *chan = malloc(sizeof(ChannelMpsc));

  if (!chan) {
    return NULL;
  }

  chan->buffer = (void *)0;

  void *tmp = realloc(chan->buffer, (capacity * sizeof(Slot)));
  if (!tmp) {
    free(chan);
    return NULL;
  }

  chan->buffer = tmp;

  for (size_t i = 0; i < capacity; i++) {
    chan->buffer[i].seq = i;
    chan->buffer[i].data = (void *)0;
    void *tmp = realloc(chan->buffer[i].data, elem_size);
    if (!tmp) {
      return NULL;
    }
    chan->buffer[i].data = tmp;
  }

  chan->capacity = capacity;
  chan->elem_size = elem_size;
  chan->producer.head = 0;
  chan->consumer.tail = 0;
  chan->prod_cont = 0;
  chan->state = OPEN;

  return chan;
};

void mpsc_close(ChannelMpsc *chan) {
  atomic_store_explicit(&chan->state, CLOSED, memory_order_release);
}

ChanState mpsc_is_closed(const ChannelMpsc *chan) {
  return atomic_load_explicit(&chan->state, memory_order_acquire);
}

void mpsc_destroy(ChannelMpsc *chan) {
  if (!chan) {
    return;
  }

  size_t prod_cont;
  ChanState chan_state;

  mpsc_close(chan);
  do {
    prod_cont = atomic_load_explicit(&chan->prod_cont, memory_order_acquire);
    chan_state = atomic_load_explicit(&chan->state, memory_order_acquire);
  } while (prod_cont != 0);

  for (size_t x = 0; x < chan->capacity; x++) {
    free(chan->buffer[x].data);
  }
  free(chan->buffer);
  free(chan);
}

typedef struct SenderMpsc_t {
  Slot *buffer;
  size_t inner_c_cap;
  size_t elem_size;

  _Atomic size_t *head;
  _Atomic size_t *tail;
  _Atomic size_t *chan_prod_count;
  _Atomic ChanState *chan_state;
  _Atomic ChanState sender_state;
} SenderMpsc;

typedef struct ReceiverMpsc_t {
  Slot *buffer;
  size_t inner_c_cap;
  size_t elem_size;

  _Atomic size_t *head;
  _Atomic size_t *tail;
} ReceiverMpsc;

SenderMpsc *mpsc_get_sender(ChannelMpsc *chan) {
  if (!chan) {
    return NULL;
  }
  SenderMpsc *sender = malloc(sizeof(SenderMpsc));

  sender->buffer = chan->buffer;
  sender->inner_c_cap = chan->capacity;
  sender->head = &chan->producer.head;
  sender->tail = &chan->consumer.tail;
  sender->elem_size = chan->elem_size;
  sender->chan_state = &chan->state;
  sender->sender_state = OPEN;

  atomic_fetch_add_explicit(&chan->prod_cont, 1, memory_order_release);
  sender->chan_prod_count = &chan->prod_cont;

  return sender;
}

ReceiverMpsc *mpsc_get_receiver(ChannelMpsc *chan) {
  if (!chan) {
    return NULL;
  }
  ReceiverMpsc *receiver = malloc(sizeof(ReceiverMpsc));

  receiver->buffer = chan->buffer;
  receiver->inner_c_cap = chan->capacity;
  receiver->tail = &chan->consumer.tail;
  receiver->head = &chan->producer.head;
  receiver->elem_size = chan->elem_size;

  return receiver;
}

void mpsc_close_sender(SenderMpsc *sender) {
  atomic_fetch_sub_explicit(sender->chan_prod_count, 1, memory_order_release);
  atomic_store_explicit(&sender->sender_state, CLOSED, memory_order_release);
}

int mpsc_send(SenderMpsc *sender, const void *element) {
  if (!sender) {
    return CHANNEL_ERR_NULL;
  }
  ChanState state =
      atomic_load_explicit(sender->chan_state, memory_order_acquire);

  if (state == CLOSED) {
    return CHANNEL_ERR_CLOSED;
  }

  size_t head =
      atomic_fetch_add_explicit(sender->head, 1, memory_order_acq_rel);
  Slot *slot = &sender->buffer[head % sender->inner_c_cap];

  while (atomic_load_explicit(&slot->seq, memory_order_acquire) != head) {
    if (atomic_load_explicit(sender->chan_state, memory_order_acquire) ==
        CLOSED) {
      return CHANNEL_ERR_CLOSED;
    }
    cpu_relax();
  }

  memcpy(slot->data, element, sender->elem_size);

  // set slot for consumer
  atomic_store_explicit(&slot->seq, head + 1, memory_order_release);

  return CHANNEL_OK;
}

int mpsc_recv(ReceiverMpsc *receiver, void *out) {
  if (!receiver) {
    return CHANNEL_ERR_NULL;
  }
  size_t tail = atomic_load_explicit(receiver->tail, memory_order_relaxed);
  size_t head = atomic_load_explicit(receiver->head, memory_order_acquire);
  if (tail == head) {
    return CHANNEL_ERR_EMPTY;
  }
  Slot *slot = &receiver->buffer[tail % receiver->inner_c_cap];
  if (atomic_load_explicit(&slot->seq, memory_order_acquire) != tail + 1) {
    return CHANNEL_ERR_EMPTY;
  }

  memcpy(out, slot->data, receiver->elem_size);

  // set slot for next future cycle
  atomic_store_explicit(&slot->seq, tail + receiver->inner_c_cap,
                        memory_order_release);
  atomic_fetch_add_explicit(receiver->tail, 1, memory_order_relaxed);
  return CHANNEL_OK;
}
#endif
