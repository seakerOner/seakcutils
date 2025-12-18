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
