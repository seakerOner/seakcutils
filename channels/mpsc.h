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


#ifndef MPSC_CHANNEL
#define MPSC_CHANNEL

#include "channels.h"

typedef struct ChannelMpsc_t ChannelMpsc;

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

#endif
