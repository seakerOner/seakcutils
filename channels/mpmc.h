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
mpmc_channel.h — Multi-Producer Multi-Consumer lock-free channel

This channel supports:
- multiple producers
- multiple consumers
- fixed-capacity ring buffer
- busy-wait synchronization (no blocking, no syscalls)

The implementation is lock-free and cache-line aware.

------------------------------------------------------------------------------
PROPERTIES

- Wait-free for producers (bounded by buffer capacity)
- Lock-free for consumers
- No dynamic allocation during send/recv
- No external dependencies
- Cross-platform (x86, ARM, RISC-V)

------------------------------------------------------------------------------
LIFETIME

1. channel_create_mpmc()
2. mpmc_get_sender() (N times)
3. mpmc_get_receiver() (N times)
4. mpmc_send() / mpmc_recv()
5. mpmc_close()
6. mpmc_close_sender() / mpmc_close_receiver()
7. mpmc_destroy()

Senders and receivers must be freed by the user.

------------------------------------------------------------------------------
WARNING

This channel uses busy-waiting.
It is intended for short-lived waits (job systems, engine internals).
------------------------------------------------------------------------------
*/
#ifndef MPMC_CHANNEL_H
#define MPMC_CHANNEL_H

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

typedef struct ChannelMpmc_t ChannelMpmc;
typedef enum ChanState_t ChanState;

/*-----------------------------------------------------------------------------
  channel_create_mpmc
  Allocates and initializes a new lock-free multi-producer/multi-consumer
channel.

  capacity  : number of slots in the ring buffer
  elem_size : size in bytes of each element

  Returns a pointer to ChannelMpmc on success, NULL on allocation failure.

  Notes:
    - Multiple producers and multiple consumers can safely operate concurrently.
    - Allocates internal memory for slots and element buffers.
-----------------------------------------------------------------------------*/
ChannelMpmc *channel_create_mpmc(const size_t capacity, const size_t elem_size);

/*-----------------------------------------------------------------------------
  mpmc_close
  Marks the channel as closed.

  chan : pointer to the channel to close

  Notes:
    - After closing, mpmc_send will return CHANNEL_ERR_CLOSED.
    - Consumers may continue to drain the channel until empty.
-----------------------------------------------------------------------------*/
void mpmc_close(ChannelMpmc *chan);

/*-----------------------------------------------------------------------------
  mpmc_is_closed
  Checks whether the channel has been closed.

  chan : pointer to the channel

  Returns:
    - OPEN   if the channel is still open
    - CLOSED if the channel has been closed

  Notes:
    - Non-blocking, safe to call concurrently.
-----------------------------------------------------------------------------*/
ChanState mpmc_is_closed(const ChannelMpmc *chan);

/*-----------------------------------------------------------------------------
  mpmc_destroy
  Frees all memory associated with the channel and waits for all producers and
consumers to finish.

  chan : pointer to the channel

  Notes:
    - Blocks until all active producers call mpmc_close_sender and all consumers
call mpmc_close_receiver.
    - Also frees element buffers.
    - After this call, the channel pointer becomes invalid.
-----------------------------------------------------------------------------*/
void mpmc_destroy(ChannelMpmc *chan);

typedef struct SenderMpmc_t SenderMpmc;
typedef struct ReceiverMpmc_t ReceiverMpmc;

/*-----------------------------------------------------------------------------
  mpmc_get_sender
  Allocates and returns a producer handle for the given channel.

  chan : pointer to a valid ChannelMpmc

  Returns a pointer to SenderMpmc on success, NULL on failure.

  Notes:
    - Multiple senders can be attached concurrently.
    - Each sender must call mpmc_close_sender before freeing.
-----------------------------------------------------------------------------*/
SenderMpmc *mpmc_get_sender(ChannelMpmc *chan);

/*-----------------------------------------------------------------------------
  mpmc_get_receiver
  Allocates and returns a consumer handle for the given channel.

  chan : pointer to a valid ChannelMpmc

  Returns a pointer to ReceiverMpmc on success, NULL on failure.

  Notes:
    - Multiple receivers can be attached concurrently.
    - Each receiver must call mpmc_close_receiver before freeing.
-----------------------------------------------------------------------------*/
ReceiverMpmc *mpmc_get_receiver(ChannelMpmc *chan);

/*-----------------------------------------------------------------------------
  mpmc_close_sender
  Marks a sender as closed and decrements the channel's producer count.

  sender : pointer to a valid SenderMpmc

  Notes:
    - Must be called once per sender before freeing.
    - After this call, mpmc_send will return CHANNEL_ERR_CLOSED for this sender.
-----------------------------------------------------------------------------*/
void mpmc_close_receiver(ReceiverMpmc *sender);

/*-----------------------------------------------------------------------------
  mpmc_close_receiver
  Marks a receiver as closed and decrements the channel's consumer count.

  receiver : pointer to a valid ReceiverMpmc

  Notes:
    - Must be called once per receiver before freeing.
    - After this call, mpmc_recv will return CHANNEL_ERR_CLOSED for this
receiver.
-----------------------------------------------------------------------------*/
void mpmc_close_sender(SenderMpmc *sender);

/*-----------------------------------------------------------------------------
  mpmc_send
  Sends an element to the channel.

  sender  : pointer to a valid SenderMpmc
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
int mpmc_send(SenderMpmc *sender, const void *element);

/*-----------------------------------------------------------------------------
  mpmc_recv
  Receives an element from the channel.

  receiver : pointer to a valid ReceiverMpmc
  out      : pointer to memory where the element will be copied

  Returns:
    - CHANNEL_OK          on success
    - CHANNEL_ERR_NULL    if receiver is NULL
    - CHANNEL_ERR_CLOSED  if channel or receiver is closed

  Notes:
    - Busy-waits until an element becomes available or the channel closes.
    - Lock-free for the consumer.
    - Copies elem_size bytes into the memory pointed to by out.
-----------------------------------------------------------------------------*/
int mpmc_recv(ReceiverMpmc *receiver, void *out);

#endif

#if (defined(MPMC_IMPLEMENTATION))
#include <stdalign.h>
#include <stdatomic.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

typedef struct ChannelMpmc_t {
  Slot *buffer;
  _Atomic ChanState state; // 0 -> Open | 1 -> Closed

  size_t capacity;  // number of elements
  size_t elem_size; // sizeof(T)
  ProducerCursor producer;
  ConsumerCursor consumer;

  _Atomic size_t cons_cont; // Number of active consumers
  _Atomic size_t prod_cont; // Number of active producers

} ChannelMpmc;

ChannelMpmc *channel_create_mpmc(const size_t capacity,
                                 const size_t elem_size) {
  ChannelMpmc *chan = malloc(sizeof(ChannelMpmc));

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
  chan->prod_cont = 0;
  chan->state = OPEN;

  return chan;
};

void mpmc_close(ChannelMpmc *chan) {
  atomic_store_explicit(&chan->state, CLOSED, memory_order_release);
};

ChanState mpmc_is_closed(const ChannelMpmc *chan) {
  return atomic_load_explicit(&chan->state, memory_order_acquire);
};

void mpmc_destroy(ChannelMpmc *chan) {
  if (!chan) {
    return;
  }

  size_t cons_cont;
  size_t prod_cont;

  mpmc_close(chan);
  do {
    cons_cont = atomic_load_explicit(&chan->cons_cont, memory_order_acquire);
    prod_cont = atomic_load_explicit(&chan->prod_cont, memory_order_acquire);
  } while (cons_cont != 0 || prod_cont != 0);

  for (size_t x = 0; x < chan->capacity; x++) {
    free(chan->buffer[x].data);
  }
  free(chan->buffer);
  free(chan);
};

typedef struct SenderMpmc_t {
  Slot *buffer;
  size_t inner_c_cap;
  size_t elem_size;

  _Atomic ChanState sender_state;
  _Atomic ChanState *chan_state;
  _Atomic size_t *head;
  _Atomic size_t *chan_prod_count;
} SenderMpmc;

typedef struct ReceiverMpmc_t {
  Slot *buffer;
  size_t inner_c_cap;
  size_t elem_size;

  _Atomic size_t *head;
  _Atomic size_t *tail;

  _Atomic ChanState receiver_state;
  _Atomic ChanState *chan_state;
  _Atomic size_t *chan_cons_count;
} ReceiverMpmc;

SenderMpmc *mpmc_get_sender(ChannelMpmc *chan) {
  if (!chan) {
    return NULL;
  }
  SenderMpmc *sender = malloc(sizeof(SenderMpmc));

  sender->buffer = chan->buffer;
  sender->inner_c_cap = chan->capacity;
  sender->head = &chan->producer.head;
  sender->elem_size = chan->elem_size;
  sender->chan_state = &chan->state;
  sender->sender_state = OPEN;

  atomic_fetch_add_explicit(&chan->prod_cont, 1, memory_order_release);
  sender->chan_prod_count = &chan->prod_cont;
  return sender;
};

ReceiverMpmc *mpmc_get_receiver(ChannelMpmc *chan) {
  if (!chan) {
    return NULL;
  }
  ReceiverMpmc *receiver = malloc(sizeof(ReceiverMpmc));

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

void mpmc_close_receiver(ReceiverMpmc *receiver) {
  atomic_fetch_sub_explicit(receiver->chan_cons_count, 1, memory_order_release);
  atomic_store_explicit(&receiver->receiver_state, CLOSED,
                        memory_order_release);
};

void mpmc_close_sender(SenderMpmc *sender) {
  atomic_fetch_sub_explicit(sender->chan_prod_count, 1, memory_order_release);
  atomic_store_explicit(&sender->sender_state, CLOSED, memory_order_release);
};

int mpmc_send(SenderMpmc *sender, const void *element) {
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

int mpmc_recv(ReceiverMpmc *receiver, void *out) {
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
