#ifndef MPMC_CHANNEL
#define MPMC_CHANNEL

#include "channels.h"

typedef struct ChannelMpmc_t ChannelMpmc;

ChannelMpmc *channel_create_mpmc(size_t capacity, size_t elem_size);
void mpmc_close(ChannelMpmc *chan);
ChanState mpmc_is_closed(ChannelMpmc *chan);
void mpmc_destroy(ChannelMpmc *chan);

typedef struct SenderMpmc_t SenderMpmc;
typedef struct ReceiverMpmc_t ReceiverMpmc;

SenderMpmc *mpmc_get_sender(ChannelMpmc *chan);
ReceiverMpmc *mpmc_get_receiver(ChannelMpmc*chan);

void mpmc_close_receiver(ReceiverMpmc *sender);
void mpmc_close_sender(SenderMpmc *sender);
int mpmc_send(SenderMpmc *sender, const void *element);
int mpmc_recv(ReceiverMpmc *receiver, void *out);

#endif
