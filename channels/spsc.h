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

#ifndef SPSC_CHANNEL_H
#define SPSC_CHANNEL_H
/* ==========================================================================
   SPSC - Single Producer Single Consumer Channel
   Lock-free channel for communication between a single producer thread
   and a single consumer thread.
   ========================================================================== */

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

#include <stdatomic.h>
#include <stddef.h>
#include <stdint.h>

/* Channel */
typedef struct ChannelSpsc_t ChannelSpsc;
typedef enum ChanState_t ChanState;

/* --------------------------------------------------------------------------
   Channel creation and destruction
   -------------------------------------------------------------------------- */

/**
 * Creates a new SPSC channel.
 * @param capacity Maximum number of elements the channel can hold
 * @param elem_size Size of each element in bytes
 * @return Pointer to a newly allocated channel, or NULL on failure
 */
ChannelSpsc *channel_create_spsc(const size_t capacity, const size_t elem_size);

/**
 * Destroys a channel and frees all associated memory.
 * @param chan Pointer to the channel
 */
void spsc_destroy(ChannelSpsc *chan);

/* --------------------------------------------------------------------------
   Channel state management
   -------------------------------------------------------------------------- */

/**
 * Closes the channel. After closing, no further sends should be performed.
 * @param chan Pointer to the channel
 */
void spsc_close(ChannelSpsc *chan);

/**
 * Returns the current state of the channel.
 * @param chan Pointer to the channel
 * @return OPEN or CLOSED
 */
ChanState spsc_is_closed(const ChannelSpsc *chan);

typedef struct SenderSpsc_t SenderSpsc;

typedef struct ReceiverSpsc_t ReceiverSpsc;

/* --------------------------------------------------------------------------
   Sender / Receiver handles
   -------------------------------------------------------------------------- */

/**
 * Returns a sender handle for a given channel.
 * The sender can only be used from a single producer thread.
 * @param chan Pointer to the channel
 * @return Pointer to a sender handle, or NULL on failure
 */
SenderSpsc *spsc_get_sender(ChannelSpsc *chan);

/**
 * Returns a receiver handle for a given channel.
 * The receiver can only be used from a single consumer thread.
 * @param chan Pointer to the channel
 * @return Pointer to a receiver handle, or NULL on failure
 */
ReceiverSpsc *spsc_get_receiver(ChannelSpsc *chan);

/* --------------------------------------------------------------------------
   Channel operations
   -------------------------------------------------------------------------- */

/**
 * Attempts to send an element to the channel.
 * This function will fail if the channel is full or if the channel is closed.
 * @param sender Pointer to the sender handle
 * @param element Pointer to the element to send
 * @return SPSC_OK on success, SPSC_ERR_NULL if sender is NULL,
 *         SPSC_ERR_FULL if channel is full, SPSC_ERR_CLOSED if the channel is
 *         closed
 */
int spsc_try_send(SenderSpsc *sender, const void *element);

/**
 * Receives an element from the channel.
 * This function will fail if the channel is empty.
 * @param receiver Pointer to the receiver handle
 * @param out Pointer to memory where the received element will be stored
 * @return SPSC_OK on success, SPSC_ERR_NULL if receiver is NULL,
 *         SPSC_ERR_EMPTY if channel is empty
 */
int spsc_recv(ReceiverSpsc *receiver, void *out);

#endif

#if (defined (SPSC_IMPLEMENTATION))
#include <stdalign.h>
#include <stdlib.h>
#include <string.h>

typedef struct ChannelSpsc_t {
  uint8_t *buffer;
  size_t capacity;  // number of elements
  size_t elem_size; // sizeof(T)

  ProducerCursor producer;
  ConsumerCursor consumer;

  _Atomic ChanState state; // 0 -> Open | 1 -> Closed
} ChannelSpsc;

ChannelSpsc *channel_create_spsc(const size_t capacity,const size_t elem_size) {
  ChannelSpsc *chan = malloc(sizeof(ChannelSpsc));

  if (!chan) {
    return NULL;
  }

  chan->buffer = (void *)0;
  void *tmp = realloc(chan->buffer, (capacity * elem_size));
  if (!tmp) {
    free(chan);
    return NULL;
  }

  chan->buffer = tmp;
  chan->capacity = capacity;
  chan->elem_size = elem_size;
  chan->producer.head = 0;
  chan->consumer.tail = 0;
  chan->state = OPEN;

  return chan;
};

void spsc_close(ChannelSpsc *chan) {
  atomic_store_explicit(&chan->state, CLOSED, memory_order_release);
}

ChanState spsc_is_closed(const ChannelSpsc *chan) {
  return atomic_load_explicit(&chan->state, memory_order_acquire);
}

void spsc_destroy(ChannelSpsc *chan) {
  if (!chan) {
    return;
  }

  free(chan->buffer);
  free(chan);
}

typedef struct SenderSpsc_t {
  uint8_t *buffer;
  size_t inner_c_cap;
  size_t elem_size;

  _Atomic size_t *head;
  _Atomic size_t *tail;
  _Atomic ChanState *chan_state;
} SenderSpsc;

typedef struct ReceiverSpsc_t {
  uint8_t *buffer;
  size_t inner_c_cap;
  size_t elem_size;

  _Atomic size_t *head;
  _Atomic size_t *tail;
} ReceiverSpsc;

SenderSpsc *spsc_get_sender(ChannelSpsc *chan) {
  if (!chan) {
    return NULL;
  }
  SenderSpsc *sender = malloc(sizeof(SenderSpsc));

  sender->buffer = chan->buffer;
  sender->inner_c_cap = chan->capacity;
  sender->head = &chan->producer.head;
  sender->tail = &chan->consumer.tail;
  sender->elem_size = chan->elem_size;
  sender->chan_state = &chan->state;

  return sender;
}

ReceiverSpsc *spsc_get_receiver(ChannelSpsc *chan) {
  if (!chan) {
    return NULL;
  }
  ReceiverSpsc *receiver = malloc(sizeof(ReceiverSpsc));

  receiver->buffer = chan->buffer;
  receiver->inner_c_cap = chan->capacity;
  receiver->tail = &chan->consumer.tail;
  receiver->head = &chan->producer.head;
  receiver->elem_size = chan->elem_size;

  return receiver;
}

int spsc_try_send(SenderSpsc *sender, const void *element) {
  if (!sender) {
    return CHANNEL_ERR_NULL;
  }
  ChanState state =
      atomic_load_explicit(sender->chan_state, memory_order_acquire);

  if (state == CLOSED) {
    return CHANNEL_ERR_CLOSED;
  }

  size_t head = atomic_load_explicit(sender->head, memory_order_relaxed);
  size_t tail = atomic_load_explicit(sender->tail, memory_order_acquire);
  if (head - tail == sender->inner_c_cap) {
    return CHANNEL_ERR_FULL;
  }

  size_t index = head % sender->inner_c_cap;

  memcpy(sender->buffer + (index * sender->elem_size), element,
         sender->elem_size);

  atomic_fetch_add_explicit(sender->head, 1, memory_order_release);

  return CHANNEL_OK;
}

int spsc_recv(ReceiverSpsc *receiver, void *out) {
  if (!receiver) {
    return CHANNEL_ERR_NULL;
  }
  size_t tail = atomic_load_explicit(receiver->tail, memory_order_relaxed);
  size_t head = atomic_load_explicit(receiver->head, memory_order_acquire);
  if (tail == head) {
    return CHANNEL_ERR_EMPTY;
  }

  size_t index = tail % receiver->inner_c_cap;

  memcpy(out, receiver->buffer + (index * receiver->elem_size),
         receiver->elem_size);
  atomic_fetch_add_explicit(receiver->tail, 1, memory_order_relaxed);
  return CHANNEL_OK;
}
#endif
