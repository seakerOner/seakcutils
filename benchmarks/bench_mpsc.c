#define _GNU_SOURCE
#include <pthread.h>
#include <stdatomic.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include "../channels/mpsc.h"
#define MPSC_IMPLEMENTATION

/*
 * Time: 0.308 s
 * Throughput: 13.00 M msgs/s
 * MPSC Benchmark
 * -----------------------------
 * Producers:        4
 * Messages/prod:    1000000
 * Total messages:   4000000
 * Channel capacity: 1024
 * Message size:     8 bytes
 * */

/*
 * Time: 30.250 s
 * Throughput: 13.22 M msgs/s
 * MPSC Benchmark
 * -----------------------------
 * Producers:        4
 * Messages/prod:    100000000
 * Total messages:   400000000
 * Channel capacity: 65535
 * Message size:     8 bytes
 * */

#define NUM_PRODUCERS 4
#define MESSAGES_PER_PRODUCER 100000000
#define CHANNEL_CAPACITY 65535

ChannelMpsc *chan;
atomic_size_t received = 0;

typedef struct {
  int id;
} ProducerArg;

void *producer_fn(void *arg) {
  ProducerArg *parg = arg;
  SenderMpsc *sender = mpsc_get_sender(chan);

  uint64_t value = parg->id;

  for (size_t i = 0; i < MESSAGES_PER_PRODUCER; i++) {
    while (mpsc_send(sender, &value) != CHANNEL_OK) {
      // spin
    }
  }

  mpsc_close_sender(sender);
  free(sender);
  return NULL;
}

void *consumer_fn(void *arg) {
  (void)arg;
  ReceiverMpsc *receiver = mpsc_get_receiver(chan);

  uint64_t value;
  size_t total = NUM_PRODUCERS * MESSAGES_PER_PRODUCER;

  while (atomic_load_explicit(&received, memory_order_acquire) < total) {
    if (mpsc_recv(receiver, &value) == CHANNEL_OK) {
      atomic_fetch_add_explicit(&received, 1, memory_order_release);
    } else {
      cpu_relax();
    }
  }

  free(receiver);
  return NULL;
}

static inline double timespec_diff_sec(struct timespec a, struct timespec b) {
  return (b.tv_sec - a.tv_sec) + (b.tv_nsec - a.tv_nsec) / 1e9;
}

int main(void) {
  chan = channel_create_mpsc(CHANNEL_CAPACITY, sizeof(uint64_t));

  pthread_t producers[NUM_PRODUCERS];
  pthread_t consumer;
  ProducerArg args[NUM_PRODUCERS];

  // Warm-up (optional)
  SenderMpsc *warm_sender = mpsc_get_sender(chan);
  ReceiverMpsc *warm_recv = mpsc_get_receiver(chan);

  for (int i = 0; i < 10000; i++) {
    uint64_t v = i;
    while (mpsc_send(warm_sender, &v) != CHANNEL_OK) {
    }
    while (mpsc_recv(warm_recv, &v) != CHANNEL_OK) {
    }
  }

  mpsc_close_sender(warm_sender);
  free(warm_sender);
  free(warm_recv);
  // ------------

  struct timespec start, end;
  clock_gettime(CLOCK_MONOTONIC, &start);

  pthread_create(&consumer, NULL, consumer_fn, NULL);

  for (int i = 0; i < NUM_PRODUCERS; i++) {
    args[i].id = i;
    pthread_create(&producers[i], NULL, producer_fn, &args[i]);
  }

  for (int i = 0; i < NUM_PRODUCERS; i++) {
    pthread_join(producers[i], NULL);
  }

  pthread_join(consumer, NULL);

  clock_gettime(CLOCK_MONOTONIC, &end);

  double elapsed = timespec_diff_sec(start, end);
  size_t total_msgs = NUM_PRODUCERS * MESSAGES_PER_PRODUCER;

  printf("Messages: %zu\n", total_msgs);
  printf("Time: %.3f s\n", elapsed);
  printf("Throughput: %.2f M msgs/s\n", (total_msgs / elapsed) / 1e6);

  printf("MPSC Benchmark\n");
  printf("-----------------------------\n");
  printf("Producers:        %d\n", NUM_PRODUCERS);
  printf("Messages/prod:    %d\n", MESSAGES_PER_PRODUCER);
  printf("Total messages:   %zu\n", total_msgs);
  printf("Channel capacity: %d\n", CHANNEL_CAPACITY);
  printf("Message size:     %zu bytes\n\n", sizeof(uint64_t));

  mpsc_destroy(chan);
  return 0;
}
