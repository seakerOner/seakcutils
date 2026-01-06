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
#ifndef LINKEDLIST_H
#define LINKEDLIST_H

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct LlNode_t LlNode;

typedef struct LlNode_t {
  uintptr_t next;
  uintptr_t prev;
  void *elem;
} LlNode;

typedef struct LinkedList_t {
  uintptr_t head;
  uintptr_t tail;
  size_t count;
  size_t elem_size;
} LinkedList;

#define NODE(ptr) ((LlNode *)ptr)

LinkedList linkedlist_new(size_t elem_size);

int ll_append(LinkedList *ll, const void *elem);
int ll_pop(LinkedList *ll, void *out);

int ll_push_back(LinkedList *ll, const void *elem);
int ll_push_front(LinkedList *ll, const void *elem);

int ll_pop_back(LinkedList *ll, void *out);
int ll_pop_front(LinkedList *ll, void *out);

size_t ll_len(LinkedList *ll);
int ll_is_empty(LinkedList *ll);

int ll_remove(LinkedList *ll, const void *elem);
void ll_free(LinkedList *ll);

/* Use `ll_next()` to traverse */
const LlNode *ll_iterable(LinkedList *ll);
/* Use `ll_prev()` to traverse */
const LlNode *ll_iterable_reverse(LinkedList *ll);

const LlNode *ll_next(const LlNode *node);
const LlNode *ll_prev(const LlNode *node);

/* Do not use linkedlist_contains() to compare security critical data, such as
   crypto-graphic secrets, because the required CPU time depends on the number
   of equal bytes. For a safer search see `linkedlist_contains_secure` */
int ll_contains(LinkedList *ll, const void *elem);

/* This is a safer alternative to `linkedlist_contains()`. It's not
 * a silver bullet to attacks but it provides constant time comparison
 * and constant time lookup.
 *
 * NOTE: Due to the nature of this data structure the rate of cache misses is
 * way higher than simply using an array and constant lookup means it will
 * traverse every node even if it already found a value. Be very sure of what
 * you are doing and sure this function is really what you need. */
int ll_contains_secure(LinkedList *ll, const void *elem);

#endif // !LINKEDLIST_H
