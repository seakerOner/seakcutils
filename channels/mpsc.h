#ifndef MPSC_CHANNEL
#define MPSC_CHANNEL

#include "channels.h"

typedef struct ChannelMpsc_t ChannelMpsc;

ChannelMpsc *channel_create_mpsc(size_t capacity, size_t elem_size);
void mpsc_close(ChannelMpsc *chan);
ChanState mpsc_is_closed(ChannelMpsc *chan);
void mpsc_destroy(ChannelMpsc *chan);

typedef struct SenderMpsc_t SenderMpsc;
typedef struct ReceiverMpsc_t ReceiverMpsc;

SenderMpsc *mpsc_get_sender(ChannelMpsc *chan);
ReceiverMpsc *mpsc_get_receiver(ChannelMpsc *chan);

void mpsc_close_sender(SenderMpsc *sender);
int mpsc_send(SenderMpsc *sender, const void *element);
int mpsc_recv(ReceiverMpsc *receiver, void *out);

#endif
