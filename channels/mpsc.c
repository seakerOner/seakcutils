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

/* MPSC - Multi-Producer Single Consumer Channel */

#include "mpsc.h"
#include "channels.h"
#include <stdalign.h>
#include <stdatomic.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

typedef struct ChannelMpsc_t {
  Slot *buffer;
  ProducerCursor producer;
  ConsumerCursor consumer;

  size_t capacity;  // number of elements
  size_t elem_size; // sizeof(T)

  _Atomic size_t prod_cont; // Number of active producers
  _Atomic ChanState state;  // 0 -> Open | 1 -> Closed
} ChannelMpsc;

ChannelMpsc *channel_create_mpsc(const size_t capacity,const size_t elem_size) {
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
