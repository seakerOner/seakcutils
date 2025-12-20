# Channels

A collection of high-performance, low-level channels implemented in C for inter-thread communication. 
This project aims to provide lock-free, minimal-overhead communication primitives with predictable memory behavior.

> Original repo: <https://github.com/seakerOner/channels>

## Available Channels

 - **Lock-free SPSC channel** for communication between a single producer and a single consumer thread
 - **Lock-free SPMC channel** for communication between a single producer and multiple consumer threads
 - **Lock-free MPSC channel** for communication from multiple producer threads to a single consumer thread
 - **Lock-free MPMC channel** for communication from multiple producer threads to multiple consumer threads

### Channel Comparison

| Channel Type | Producers | Consumers | Lock-free | Blocking / Spin-wait | Use Case | Notes |
|--------------|-----------|-----------|-----------|--------------------|----------|-------|
| **SPSC** (Single Producer / Single Consumer) | 1 | 1 | ✅ | Spin-waits if full/empty | High-performance queue between 1 producer and 1 consumer | Minimal overhead, fully cache-line aligned, safest and fastest option |
| **SPMC** (Single Producer / Multiple Consumers) | 1 | N | ✅ | Producer blocks if full, consumers spin-wait if empty | Single thread dispatching tasks to multiple workers | Each element consumed exactly once; suitable for thread pools |
| **MPSC** (Multiple Producers / Single Consumer) | N | 1 | ✅ | Producers spin-wait if full, consumer blocks if empty | Multiple producers pushing work to a single worker | Safe coordination using per-slot sequence numbers |
| **MPMC** (Multiple Producers / Multiple Consumers) | N | N | ✅ | Producers and consumers spin-wait | High-contention scenarios with multiple threads producing and consuming | Maintains atomic counters for active senders/receivers for safe destruction; fully lock-free |

#### Notes

- Benchmarks were run without batching
- MPSC channel on a 4-producer / 1-consumer setup:
    - Sustained throughput: ~13 million messages per second
    - Stable under long-running workloads (400M+ messages)

- **Spin-waiting** ensures lock-free correctness but may be CPU-intensive.  
- **Element size** is arbitrary, but users must provide the correct size when creating the channel.  
- **Lifecycle management**: All channels require explicit closing of senders/receivers and destruction.  

---
### SPSC Channel

#### Features

- Lock-free SPSC channel for communication between a single producer and a single consumer thread.
- **Optimized for cache-line alignment** to avoid false sharing between producer and consumer cursors. This ensures that cache invalidations between threads are minimized, improving performance in multithreaded applications.
- Dynamically allocated buffer with wrap-around (ring buffer) using modular arithmetic.
- try_send ensures safe sending without overwriting unread data.
- Supports arbitrary element types via element size (elem_size).
- Simple and minimal API with predictable memory behavior.

#### API

```c
typedef enum ChanState {
    OPEN = 0,
    CLOSED = 1
} ChanState;

typedef struct ChannelSpsc_t ChannelSpsc;
typedef struct SenderSpsc_t SenderSpsc;
typedef struct ReceiverSpsc_t ReceiverSpsc;

ChannelSpsc *channel_create_spsc(const size_t capacity,const size_t elem_size);
void spsc_close(ChannelSpsc *chan);
ChanState spsc_is_closed(const ChannelSpsc *chan);
void spsc_destroy(ChannelSpsc *chan);

SenderSpsc *spsc_get_sender(ChannelSpsc *chan);
ReceiverSpsc *spsc_get_receiver(ChannelSpsc *chan);

int spsc_try_send(SenderSpsc *sender, const void *element);
int spsc_recv(ReceiverSpsc *receiver, void* out);
```

#### Usage Example

```c
#include "spsc.h"
#include <stdio.h>
#include <stdint.h>

#define CHANNEL_CAPACITY 100

int main() {
    // Create a channel for 100 long integers
    ChannelSpsc *chan = channel_create_spsc(CHANNEL_CAPACITY, sizeof(long));
    if (!chan) {
        fprintf(stderr, "Failed to create channel\n");
        return 1;
    }

    // Get sender and receiver handles
    SenderSpsc *sender = spsc_get_sender(chan);
    ReceiverSpsc *receiver = spsc_get_receiver(chan);

    printf("Channel example started\n");

    // Sample values to send
    long val0 = 10;
    long val1 = 20;
    long val2 = 230344398;

    printf("Sending: %ld, %ld, %ld\n", val0, val1, val2);

    // Send values
    if (spsc_try_send(sender, &val0) != CHANNEL_OK) {
        fprintf(stderr, "Failed to send val0\n");
    }
    if (spsc_try_send(sender, &val1) != CHANNEL_OK) {
        fprintf(stderr, "Failed to send val1\n");
    }
    if (spsc_try_send(sender, &val2) != CHANNEL_OK) {
        fprintf(stderr, "Failed to send val2\n");
    }

    // Receive values
    long recv0, recv1, recv2;

    if (spsc_recv(receiver, &recv0) == CHANNEL_OK) {
        printf("Received: %ld\n", recv0);
    }
    if (spsc_recv(receiver, &recv1) == CHANNEL_OK) {
        printf("Received: %ld\n", recv1);
    }
    if (spsc_recv(receiver, &recv2) == CHANNEL_OK) {
        printf("Received: %ld\n", recv2);
    }

    // Clean up
    spsc_destroy(chan);

    return 0;
}
```

#### Notes

- Thread-safety: Only safe for a single producer thread and single consumer thread.
- Wrap-around: Ring buffer behavior is handled with modulo arithmetic (index = head % capacity).
- Element size: The channel supports arbitrary data types using elem_size. Users must provide correct element size.
- Error handling:
    - `CHANNEL_ERR_FULL`: Send failed, buffer is full.
    - `CHANNEL_ERR_EMPTY`: Receive failed, buffer is empty.
    - `CHANNEL_ERR_NULL`: Null pointer provided.
    - `CHANNEL_ERR_CLOSED`: Channel is closed.
    - Memory: `spsc_try_send` will never overwrite unread elements.

#### Portability & Safety
- **Atomic operations**: The channel relies on C11 atomic operations (`_Atomic` and `atomic_load`/`store`/`fetch_add`). Ensure your compiler fully supports C11 atomics and your platform guarantees atomicity for size_t operations.
- **Platform types**: The channel uses size_t for indexing and capacity. While this usually works on 32 and 64-bit platforms, extremely large channels on unusual architectures may require review.
- **Memory safety**: Users are responsible for providing valid element sizes (elem_size) and pointers for sending/receiving data. Sending invalid pointers or incorrect sizes may corrupt the buffer.

---
### SPMC Channel
#### Features

- **Lock-free SPMC channel** for communication between **a single producer** and **multiple consumer threads**.
- Implemented as a **ring buffer** with **per-slot sequence numbers**, allowing multiple consumers to safely compete for elements.
- The producer **blocks (spin-waits)** when the buffer is full, ensuring that **no unread data is ever overwritten**.
- Each element is consumed **exactly once**, even under heavy contention between consumers.
- Supports **arbitrary element types** via `elem_size`.

#### Design Notes

- **Single Producer**
  - Only one thread may call `spmc_send`.
  - The producer cursor (`producer.head`) is advanced atomically but is not safe for multiple producers.
- **Multiple Consumers**
  - Consumers acquire work using an atomic `fetch_add` on the shared consumer cursor.
  - Each slot is claimed by exactly one consumer.
- **Backpressure**
  - The producer spin-waits when the buffer is full until a slot becomes available.
  - This guarantees correctness without dropping messages.
- **Explicit lifecycle management**
  - No RAII-style cleanup.
  - Users are responsible for closing receivers and destroying the channel explicitly.

#### API
```c

typedef struct ChannelSpmc_t ChannelSpmc;
typedef struct SenderSpmc_t SenderSpmc;
typedef struct ReceiverSpmc_t ReceiverSpmc;

ChannelSpmc *channel_create_spmc(const size_t capacity,const size_t elem_size);
void spmc_close(ChannelSpmc *chan);
ChanState spmc_is_closed(const ChannelSpmc *chan);
void spmc_destroy(ChannelSpmc *chan);

SenderSpmc *spmc_get_sender(ChannelSpmc *chan);
ReceiverSpmc *spmc_get_receiver(ChannelSpmc *chan);

void spmc_close_receiver(ReceiverSpmc *receiver);

int spmc_send(SenderSpmc *sender, const void *element);
int spmc_recv(ReceiverSpmc *receiver, void *out);

```
---
### MPSC Channel

#### Features

- Lock-free MPSC channel for communication from multiple producers to a single consumer thread.
- Optimized for cache-line alignment to avoid false sharing between producers and consumer cursors, and between slots. This minimizes cache invalidations and improves performance under multithreaded load.
- Dynamically allocated ring buffer with per-slot sequence numbers to coordinate producers and consumer safely.
- Spin-wait with cpu_relax() for full slots; producers wait until a slot becomes available without busy-waiting aggressively.
- Supports arbitrary element types via element size (elem_size).
- Multiple producers supported, each with independent sender handles.
- Simple and minimal API with predictable memory behavior.

#### API

```c
typedef struct ChannelMpsc_t  ChannelMpsc;

typedef struct Slot_t Slot;

ChannelMpsc *channel_create_mpsc(const size_t capacity,const size_t elem_size);
void mpsc_close(ChannelMpsc *chan);
ChanState mpsc_is_closed(const ChannelMpsc *chan);
void mpsc_destroy(ChannelMpsc *chan);

typedef struct SenderMpsc_t SenderMpsc;
typedef struct ReceiverMpsc_t ReceiverMpsc;

SenderMpsc *mpsc_get_sender(ChannelMpsc *chan);
ReceiverMpsc *mpsc_get_receiver(ChannelMpsc *chan);

void mpsc_close_sender(SenderMpsc *sender);
int mpsc_send(SenderMpsc *sender, const void *element);
int mpsc_recv(ReceiverMpsc *receiver, void *out);
```

#### Usage Example

```c
#include "mpsc.h"
#include <stdio.h>
#include <stdint.h>
#include <pthread.h>

#define CHANNEL_CAPACITY 100
#define NUM_PRODUCERS 4

ChannelMpsc *chan;

void *producer(void *arg) {
    int id = *(int*)arg;
    SenderMpsc *sender = mpsc_get_sender(chan);

    for (int i = 0; i < 10; i++) {
        int value = id * 100 + i;
        mpsc_send(sender, &value);
        printf("Producer %d sent %d\n", id, value);
    }

    mpsc_close_sender(sender);
    return NULL;
}

void *consumer(void *arg) {
    ReceiverMpsc *receiver = mpsc_get_receiver(chan);
    int value;
    int received_count = 0;

    while (1) {
        if (mpsc_recv(receiver, &value) == CHANNEL_OK) {
            printf("Consumer received %d\n", value);
            received_count++;
        } else if (mpsc_is_closed(chan) && received_count >= NUM_PRODUCERS * 10) {
            break;
        }
    }

    return NULL;
}

int main() {
    chan = channel_create_mpsc(CHANNEL_CAPACITY, sizeof(int));

    pthread_t producers[NUM_PRODUCERS];
    int ids[NUM_PRODUCERS];

    for (int i = 0; i < NUM_PRODUCERS; i++) {
        ids[i] = i;
        pthread_create(&producers[i], NULL, producer, &ids[i]);
    }

    pthread_t cons;
    pthread_create(&cons, NULL, consumer, NULL);

    for (int i = 0; i < NUM_PRODUCERS; i++) {
        pthread_join(producers[i], NULL);
    }
    mpsc_close(chan);
    pthread_join(cons, NULL);

    mpsc_destroy(chan);
    return 0;
}
```

#### Notes

- Thread-safety: Safe for multiple producer threads and a single consumer thread.
- Ring buffer behavior: Wrap-around is handled with per-slot sequence numbers (seq) and modulo arithmetic.
- Element size: Supports arbitrary data types via `elem_size`; user must provide the correct size.
- Error handling:
    - `CHANNEL_ERR_NULL`: Null pointer provided.
    - `CHANNEL_ERR_CLOSED`: Channel is closed.
    - `CHANNEL_ERR_EMPTY`: Receive failed; buffer is empty.
- `cpu_relax()`: Spin-wait function for producers. Can be used externally for custom waiting logic.

---

### MPMC Channel

#### Features

- **Lock-free MPMC channel** for communication between **multiple producer** threads and **multiple consumer** threads.
- Maintains atomic counters for active producers and consumers to support safe destruction.
- Producers spin-wait if the buffer is full; consumers spin-wait if the buffer is empty.
- Supports arbitrary element types via `elem_size`.
- All memory is allocated at creation; no hidden allocations during send/receive operations.

#### Design Notes

- **Multiple Producers**
    - Each producer acquires a slot via an atomic fetch-and-add on the head cursor.
    - Each sender maintains its own active state and updates the shared producer count for safe destruction.
- **Multiple Consumers**
    - Each consumer acquires a slot via an atomic fetch-and-add on the tail cursor.
    - Each receiver maintains its own active state and updates the shared consumer count for safe destruction.
- **Backpressure**
    - Producers and consumers spin-wait when the channel is full or empty, ensuring no overwriting of unread elements.
- **Explicit lifecycle management**
    - Users must close senders and receivers explicitly before destroying the channel.

#### API

```c
typedef struct ChannelMpmc_t ChannelMpmc;

ChannelMpmc *channel_create_mpmc(const size_t capacity,const size_t elem_size);
void mpmc_close(ChannelMpmc *chan);
ChanState mpmc_is_closed(const ChannelMpmc *chan);
void mpmc_destroy(ChannelMpmc *chan);

typedef struct SenderMpmc_t SenderMpmc;
typedef struct ReceiverMpmc_t ReceiverMpmc;

SenderMpmc *mpmc_get_sender(ChannelMpmc *chan);
ReceiverMpmc *mpmc_get_receiver(ChannelMpmc*chan);

void mpmc_close_receiver(ReceiverMpmc *sender);
void mpmc_close_sender(SenderMpmc *sender);
int mpmc_send(SenderMpmc *sender, const void *element);
int mpmc_recv(ReceiverMpmc *receiver, void *out);

```
#### Notes

- **Thread-safety**: Safe for multiple producers and multiple consumers.
- **Ring buffer behavior**: Wrap-around is handled with per-slot sequence numbers and modulo arithmetic.
- **Element size**: Supports arbitrary element types via elem_size. Users must provide the correct size.
- **Error handling**:
    - `CHANNEL_ERR_NULL`: Null pointer provided.
    - `CHANNEL_ERR_CLOSED`: Channel or sender/receiver is closed.
    - `CHANNEL_ERR_EMPTY`: Receive failed; buffer is empty.
- Spin-wait (`cpu_relax`) is used internally for contention; may be CPU-intensive under high load.
- Destruction waits for all active senders and receivers to finish, ensuring safe memory deallocation.
