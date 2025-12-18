#ifndef SPSC_CHANNEL
#define SPSC_CHANNEL
/* ==========================================================================
   SPSC - Single Producer Single Consumer Channel
   Lock-free channel for communication between a single producer thread
   and a single consumer thread.
   ========================================================================== */

#include "channels.h"
#include <stdatomic.h>
#include <stddef.h>
#include <stdint.h>

/* Channel */
typedef struct ChannelSpsc_t ChannelSpsc;

/* --------------------------------------------------------------------------
   Channel creation and destruction
   -------------------------------------------------------------------------- */

/**
 * Creates a new SPSC channel.
 * @param capacity Maximum number of elements the channel can hold
 * @param elem_size Size of each element in bytes
 * @return Pointer to a newly allocated channel, or NULL on failure
 */
ChannelSpsc *channel_create_spsc(size_t capacity, size_t elem_size);

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
ChanState spsc_is_closed(ChannelSpsc *chan);

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
