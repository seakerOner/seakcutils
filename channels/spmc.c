/* SPMC - Single Producer Multiple Consumer Channel */

#include "spmc.h"
#include "channels.h"
#include <stdalign.h>
#include <stdatomic.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

typedef struct ChannelSpmc_t {
  Slot *buffer;
  ProducerCursor producer;
  ConsumerCursor consumer;

  size_t capacity;  // number of elements
  size_t elem_size; // sizeof(T)

  _Atomic size_t cons_cont; // Number of active consumers
  _Atomic ChanState state;  // 0 -> Open | 1 -> Closed

} ChannelSpmc;

ChannelSpmc *channel_create_spmc(size_t capacity, size_t elem_size) {
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

ChanState spmc_is_closed(ChannelSpmc *chan) {
  ChanState state = atomic_load_explicit(&chan->state, memory_order_acquire);
  switch (state) {
  case OPEN:
    return OPEN;
  case CLOSED:
    return CLOSED;
  }

  // NOTE: The channel should always have state set, because of that if no valid
  // state is found we return CLOSED state
  return CLOSED;
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
