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
spmc_channel.h — Single-Producer Multiple-Consumer lock-free channel

This channel supports:
- exactly one producer
- multiple concurrent consumers
- fixed-capacity ring buffer
- busy-wait synchronization (no blocking, no syscalls)

The implementation is lock-free and cache-line aware.

------------------------------------------------------------------------------
PROPERTIES

- Wait-free for producer (bounded by buffer capacity)
- Lock-free for consumers
- No dynamic allocation during send/recv
- No external dependencies
- Cross-platform (x86, ARM, RISC-V)

------------------------------------------------------------------------------
LIFETIME

1. channel_create_spmc()
2. spmc_get_sender()
3. spmc_get_receiver() (N times)
4. spmc_send() / spmc_recv()
5. spmc_close()
6. spmc_close_receiver() for each receiver
7. spmc_destroy()

Receivers and senders must be freed by the user.

------------------------------------------------------------------------------
WARNING

This channel uses busy-waiting.
It is intended for short-lived waits (job systems, engine internals).

------------------------------------------------------------------------------
*/
#ifndef SPMC_CHANNEL_H
#define SPMC_CHANNEL_H

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

typedef struct ChannelSpmc_t ChannelSpmc;
typedef enum ChanState_t ChanState;

/*-----------------------------------------------------------------------------
  channel_create_spmc
  Allocates and initializes a new lock-free channel with a fixed capacity.

  capacity  : number of slots in the ring buffer
  elem_size : size in bytes of each element

  Returns a pointer to ChannelSpmc on success, NULL on allocation failure.

  Notes:
    - Only one producer is supported.
    - Multiple receivers can be attached.
    - Allocates internal memory for slots and element buffers.
-----------------------------------------------------------------------------*/
ChannelSpmc *channel_create_spmc(const size_t capacity, const size_t elem_size);

/*-----------------------------------------------------------------------------
  spmc_close
  Marks the channel as closed.

  chan : pointer to the channel to close

  Notes:
    - After closing, spmc_send will return CHANNEL_ERR_CLOSED.
    - Receivers may continue to drain the channel until all elements are consumed.
-----------------------------------------------------------------------------*/
void spmc_close(ChannelSpmc *chan);

/*-----------------------------------------------------------------------------
  spmc_is_closed
  Checks whether the channel has been closed.

  chan : pointer to the channel

  Returns:
    - OPEN   if the channel is still open
    - CLOSED if the channel has been closed

  Notes:
    - Non-blocking, safe to call concurrently.
-----------------------------------------------------------------------------*/
ChanState spmc_is_closed(const ChannelSpmc *chan);

/*-----------------------------------------------------------------------------
  spmc_destroy
  Frees all memory associated with the channel and waits for all consumers
  to finish.

  chan : pointer to the channel

  Notes:
    - Blocks until all active receivers call spmc_close_receiver.
    - Also frees element buffers.
    - After this call, the channel pointer becomes invalid.
-----------------------------------------------------------------------------*/
void spmc_destroy(ChannelSpmc *chan);


typedef struct SenderSpmc_t SenderSpmc;
typedef struct ReceiverSpmc_t ReceiverSpmc;

/*-----------------------------------------------------------------------------
  spmc_get_sender
  Allocates and returns a sender handle for the given channel.

  chan : pointer to a valid ChannelSpmc

  Returns a pointer to SenderSpmc on success, NULL on failure.

  Notes:
    - Only one sender should be created per channel.
    - The returned sender must be freed by the user when no longer needed.
-----------------------------------------------------------------------------*/
SenderSpmc *spmc_get_sender(ChannelSpmc *chan);

/*-----------------------------------------------------------------------------
  spmc_get_receiver
  Allocates and returns a receiver handle for the given channel.

  chan : pointer to a valid ChannelSpmc

  Returns a pointer to ReceiverSpmc on success, NULL on failure.

  Notes:
    - Multiple receivers can be attached.
    - Each receiver must call spmc_close_receiver before freeing.
    - Receivers are responsible for reading elements independently.
-----------------------------------------------------------------------------*/
ReceiverSpmc *spmc_get_receiver(ChannelSpmc *chan);

/*-----------------------------------------------------------------------------
  spmc_close_receiver
  Marks a receiver as closed and decrements the channel's consumer count.

  receiver : pointer to a valid ReceiverSpmc

  Notes:
    - Must be called once per receiver before freeing.
    - After this call, spmc_recv will return CHANNEL_ERR_CLOSED.
-----------------------------------------------------------------------------*/
void spmc_close_receiver(ReceiverSpmc *sender);

/*-----------------------------------------------------------------------------
  spmc_send
  Sends an element to the channel.

  sender  : pointer to a valid SenderSpmc
  element : pointer to the element data to send

  Returns:
    - CHANNEL_OK          on success
    - CHANNEL_ERR_NULL    if sender is NULL
    - CHANNEL_ERR_CLOSED  if channel is closed

  Notes:
    - Busy-waits if the ring buffer slot is not available.
    - Lock-free and wait-free for the producer.
    - Copies elem_size bytes from element to the internal buffer.
-----------------------------------------------------------------------------*/
int spmc_send(SenderSpmc *sender, const void *element);

/*-----------------------------------------------------------------------------
  spmc_recv
  Receives an element from the channel.

  receiver : pointer to a valid ReceiverSpmc
  out      : pointer to memory where the element will be copied

  Returns:
    - CHANNEL_OK          on success
    - CHANNEL_ERR_NULL    if receiver is NULL
    - CHANNEL_ERR_CLOSED  if receiver or channel is closed

  Notes:
    - Busy-waits if no new element is available.
    - Lock-free for multiple consumers.
    - Copies elem_size bytes into the memory pointed to by out.
    - Each receiver independently consumes elements.
-----------------------------------------------------------------------------*/
int spmc_recv(ReceiverSpmc *receiver, void *out);

#endif

#if (defined(SPMC_IMPLEMENTATION))
#include <stdalign.h>
#include <stdatomic.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

typedef struct ChannelSpmc_t {
  Slot *buffer;
  size_t capacity;  // number of elements
  size_t elem_size; // sizeof(T)
  ProducerCursor producer;
  ConsumerCursor consumer;

  _Atomic size_t cons_cont; // Number of active consumers
  _Atomic ChanState state;  // 0 -> Open | 1 -> Closed

} ChannelSpmc;

ChannelSpmc *channel_create_spmc(const size_t capacity,
                                 const size_t elem_size) {
  ChannelSpmc *chan = malloc(sizeof(ChannelSpmc));

  if (!chan) {
    return NULL;
  }

  chan->buffer = malloc(capacity * sizeof(Slot));

  for (size_t i = 0; i < capacity; i++) {
    chan->buffer[i].seq = i;
    chan->buffer[i].data = malloc(elem_size);
  }

  chan->capacity = capacity;
  chan->elem_size = elem_size;
  chan->producer.head = 0;
  chan->consumer.tail = 0;
  chan->cons_cont = 0;
  chan->state = OPEN;

  return chan;
};

void spmc_close(ChannelSpmc *chan) {
  atomic_store_explicit(&chan->state, CLOSED, memory_order_release);
};

ChanState spmc_is_closed(const ChannelSpmc *chan) {
  return atomic_load_explicit(&chan->state, memory_order_acquire);
};

void spmc_destroy(ChannelSpmc *chan) {
  if (!chan) {
    return;
  }

  size_t cons_cont;

  spmc_close(chan);
  do {
    cons_cont = atomic_load_explicit(&chan->cons_cont, memory_order_acquire);
  } while (cons_cont != 0);

  for (size_t x = 0; x < chan->capacity; x++) {
    free(chan->buffer[x].data);
  }
  free(chan->buffer);
  free(chan);
};

typedef struct SenderSpmc_t {
  Slot *buffer;
  size_t inner_c_cap;
  size_t elem_size;

  _Atomic ChanState *chan_state;
  _Atomic size_t *head;
} SenderSpmc;

typedef struct ReceiverSpmc_t {
  Slot *buffer;
  size_t inner_c_cap;
  size_t elem_size;

  _Atomic size_t *head;
  _Atomic size_t *tail;

  _Atomic ChanState receiver_state;
  _Atomic ChanState *chan_state;
  _Atomic size_t *chan_cons_count;
} ReceiverSpmc;

SenderSpmc *spmc_get_sender(ChannelSpmc *chan) {
  if (!chan) {
    return NULL;
  }
  SenderSpmc *sender = malloc(sizeof(SenderSpmc));

  sender->buffer = chan->buffer;
  sender->inner_c_cap = chan->capacity;
  sender->head = &chan->producer.head;
  sender->elem_size = chan->elem_size;
  sender->chan_state = &chan->state;

  return sender;
};

ReceiverSpmc *spmc_get_receiver(ChannelSpmc *chan) {
  if (!chan) {
    return NULL;
  }
  ReceiverSpmc *receiver = malloc(sizeof(ReceiverSpmc));

  receiver->buffer = chan->buffer;
  receiver->inner_c_cap = chan->capacity;
  receiver->tail = &chan->consumer.tail;
  receiver->head = &chan->producer.head;
  receiver->elem_size = chan->elem_size;
  receiver->receiver_state = OPEN;
  receiver->chan_state = &chan->state;

  atomic_fetch_add_explicit(&chan->cons_cont, 1, memory_order_release);
  receiver->chan_cons_count = &chan->cons_cont;
  return receiver;
};

void spmc_close_receiver(ReceiverSpmc *receiver) {
  atomic_fetch_sub_explicit(receiver->chan_cons_count, 1, memory_order_release);
  atomic_store_explicit(&receiver->receiver_state, CLOSED,
                        memory_order_release);
};

int spmc_send(SenderSpmc *sender, const void *element) {
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
};

int spmc_recv(ReceiverSpmc *receiver, void *out) {
  if (!receiver) {
    return CHANNEL_ERR_NULL;
  }
  if (atomic_load_explicit(&receiver->receiver_state, memory_order_acquire) ==
      CLOSED) {
    return CHANNEL_ERR_CLOSED;
  }
  size_t tail =
      atomic_fetch_add_explicit(receiver->tail, 1, memory_order_acq_rel);

  Slot *slot = &receiver->buffer[tail % receiver->inner_c_cap];

  while (atomic_load_explicit(&slot->seq, memory_order_acquire) != tail + 1) {
    if (atomic_load_explicit(receiver->chan_state, memory_order_acquire) ==
        CLOSED) {
      return CHANNEL_ERR_CLOSED;
    }
    cpu_relax();
  }
  memcpy(out, slot->data, receiver->elem_size);

  // set slot for next future cycle
  atomic_store_explicit(&slot->seq, tail + receiver->inner_c_cap,
                        memory_order_release);
  return CHANNEL_OK;
};
#endif
