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
