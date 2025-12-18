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
