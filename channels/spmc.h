#ifndef SPMC_CHANNEL
#define SPMC_CHANNEL

#include "channels.h"

typedef struct ChannelSpmc_t ChannelSpmc;

ChannelSpmc *channel_create_spmc(size_t capacity, size_t elem_size);
void spmc_close(ChannelSpmc *chan);
ChanState spmc_is_closed(ChannelSpmc *chan);
void spmc_destroy(ChannelSpmc *chan);

typedef struct SenderSpmc_t SenderSpmc;
typedef struct ReceiverSpmc_t ReceiverSpmc;

SenderSpmc *spmc_get_sender(ChannelSpmc *chan);
ReceiverSpmc *spmc_get_receiver(ChannelSpmc *chan);

void spmc_close_receiver(ReceiverSpmc *sender);
int spmc_send(SenderSpmc *sender, const void *element);
int spmc_recv(ReceiverSpmc *receiver, void *out);

#endif
