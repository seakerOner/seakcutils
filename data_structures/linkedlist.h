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

/*-----------------------------------------------------------------------------
  linkedlist_new
  Creates a new linked list.

  elem_size : size in bytes of each element

  Returns a LinkedList with head/tail initialized.

  Notes:
    - Allocates a dummy node for head/tail.
    - Caller must call ll_free to release all memory.
-----------------------------------------------------------------------------*/
LinkedList linkedlist_new(size_t elem_size);

/*-----------------------------------------------------------------------------
  ll_append / ll_push_front
  Adds an element to the front (head) of the list.

  ll   : pointer to a valid LinkedList
  elem : pointer to element data to add

  Returns:
    - 0  on success
    - -1 if ll is NULL

  Notes:
    - Copies elem_size bytes from elem to newly allocated node.
    - ll_push_front is an alias for ll_append.
-----------------------------------------------------------------------------*/
int ll_append(LinkedList *ll, const void *elem);
int ll_push_front(LinkedList *ll, const void *elem);

/*-----------------------------------------------------------------------------
  ll_push_back
  Adds an element to the back (tail) of the list.

  ll   : pointer to a valid LinkedList
  elem : pointer to element data to add

  Returns:
    - 0  on success
    - -1 if ll is NULL

  Notes:
    - Copies elem_size bytes from elem to newly allocated node.
-----------------------------------------------------------------------------*/
int ll_push_back(LinkedList *ll, const void *elem);

/*-----------------------------------------------------------------------------
  ll_pop / ll_pop_front
  Removes an element from the front (head) of the list.

  ll  : pointer to a valid LinkedList
  out : pointer to memory where the element will be copied

  Returns:
    - 0  on success
    - -1 if ll->head is NULL
    - -2 if list is empty

  Notes:
    - Copies elem_size bytes into out.
    - Frees the node.
-----------------------------------------------------------------------------*/
int ll_pop(LinkedList *ll, void *out);
int ll_pop_front(LinkedList *ll, void *out);

/*-----------------------------------------------------------------------------
  ll_pop_back
  Removes an element from the back (tail) of the list.

  ll  : pointer to a valid LinkedList
  out : pointer to memory where the element will be copied

  Returns:
    - 0  on success
    - -1 if ll->tail is NULL
    - -2 if list is empty

  Notes:
    - Copies elem_size bytes into out.
    - Frees the node.
-----------------------------------------------------------------------------*/
int ll_pop_back(LinkedList *ll, void *out);

/*-----------------------------------------------------------------------------
  ll_len
  Returns the number of elements in the linked list.

  ll : pointer to a valid LinkedList

  Returns:
    - 0 if ll is NULL or empty
    - count of elements otherwise
-----------------------------------------------------------------------------*/
size_t ll_len(LinkedList *ll);

/*-----------------------------------------------------------------------------
  ll_is_empty
  Checks whether the linked list is empty.

  ll : pointer to a valid LinkedList

  Returns:
    - 1 if empty
    - 0 if contains elements
    - -1 if ll is NULL
-----------------------------------------------------------------------------*/
int ll_is_empty(LinkedList *ll);

/*-----------------------------------------------------------------------------
  ll_remove
  Removes the first occurrence of an element equal to `elem`.

  ll   : pointer to a valid LinkedList
  elem : pointer to element to remove

  Returns:
    - 1 if removed
    - 0 if not found
    - -1 on error (NULL list or empty)

  Notes:
    - Comparison uses memcmp for elem_size bytes.
    - Frees node and its element memory.
-----------------------------------------------------------------------------*/
int ll_remove(LinkedList *ll, const void *elem);

/*-----------------------------------------------------------------------------
  ll_iterable / ll_iterable_reverse
  Returns the first node for forward or backward iteration.

  ll : pointer to a valid LinkedList

  Returns:
    - pointer to first node (tail or head)
    - NULL if list is empty or ll is NULL

  Notes:
    - Use ll_next() or ll_prev() to traverse nodes.
-----------------------------------------------------------------------------*/
const LlNode *ll_iterable(LinkedList *ll);
const LlNode *ll_iterable_reverse(LinkedList *ll);

/*-----------------------------------------------------------------------------
  ll_next / ll_prev
  Returns the next or previous node in the linked list.

  node : pointer to current LlNode

  Returns:
    - pointer to next or previous node
    - NULL if at end or input is NULL
-----------------------------------------------------------------------------*/
const LlNode *ll_next(const LlNode *node);
const LlNode *ll_prev(const LlNode *node);

/*-----------------------------------------------------------------------------
  ll_contains
  Checks whether the list contains an element equal to `elem`.

  ll   : pointer to a valid LinkedList
  elem : pointer to element to search for

  Returns:
    - 1 if found
    - 0 if not found
    - -1 if ll is NULL

  Warning:
    - Not safe for cryptographic secrets; runtime depends on number of matching bytes.
-----------------------------------------------------------------------------*/
int ll_contains(LinkedList *ll, const void *elem);

/*-----------------------------------------------------------------------------
  ll_contains_secure
  Safer alternative to ll_contains with constant-time comparison.

  ll   : pointer to a valid LinkedList
  elem : pointer to element to search for

  Returns:
    - 1 if found
    - 0 if not found
    - -1 if ll is NULL

  Notes:
    - Traverses every node, so constant-time for timing attacks.
    - Much slower due to cache misses; use only for security-critical data.
-----------------------------------------------------------------------------*/
int ll_contains_secure(LinkedList *ll, const void *elem);

/*-----------------------------------------------------------------------------
  ll_free
  Frees all memory associated with the linked list.

  ll : pointer to a valid LinkedList

  Notes:
    - Frees every node and its element memory.
    - After calling, list is empty and count is zero.
-----------------------------------------------------------------------------*/
void ll_free(LinkedList *ll);

#if (defined(LINKEDLIST_IMPLEMENTATION))
LinkedList linkedlist_new(size_t elem_size) {
  LinkedList ll;
  ll.elem_size = elem_size;
  ll.count = 0;

  LlNode *node = malloc(sizeof(LlNode));

  node->next = 0;
  node->prev = 0;
  node->elem = NULL;
  ll.head = (uintptr_t)node;
  ll.tail = (uintptr_t)node;
  return ll;
}

int ll_push_back(LinkedList *ll, const void *elem) {
  if (!ll)
    return -1;

  if (NODE(ll->tail)->elem == NULL) {
    NODE(ll->tail)->elem = malloc(ll->elem_size);
    memcpy((uint8_t *)(NODE(ll->tail)->elem), (uint8_t *)elem, ll->elem_size);
  } else {
    LlNode *node_tail = malloc(sizeof(LlNode));
    node_tail->elem = malloc(ll->elem_size);
    memcpy((uint8_t *)node_tail->elem, (uint8_t *)elem, ll->elem_size);

    node_tail->next = ll->tail;
    node_tail->prev = 0;
    NODE(ll->tail)->prev = (uintptr_t)node_tail;
    ll->tail = (uintptr_t)node_tail;
  }

  ll->count += 1;
  return 0;
}

int ll_append(LinkedList *ll, const void *elem) {
  if (!ll)
    return -1;

  if (NODE(ll->head)->elem == NULL) {
    NODE(ll->head)->elem = malloc(ll->elem_size);
    memcpy((uint8_t *)(NODE(ll->head)->elem), (uint8_t *)elem, ll->elem_size);
  } else {
    LlNode *node_head = malloc(sizeof(LlNode));
    node_head->elem = malloc(ll->elem_size);
    memcpy((uint8_t *)node_head->elem, (uint8_t *)elem, ll->elem_size);

    node_head->next = 0;
    node_head->prev = ll->head;
    NODE(ll->head)->next = (uintptr_t)node_head;
    ll->head = (uintptr_t)node_head;
  }

  ll->count += 1;

  return 0;
}

int ll_push_front(LinkedList *ll, const void *elem) {
  return ll_append(ll, elem);
}

/* Do not use linkedlist_contains() to compare security critical data, such as
   crypto-graphic secrets, because the required CPU time depends on the number
   of equal bytes. For a safer search see `linkedlist_contains_secure` */
int ll_contains(LinkedList *ll, const void *elem) {
  if (!ll)
    return -1;

  const LlNode *tail = NODE(ll->tail);
  for (size_t x = 0; x < ll->count; x += 1) {
    if (memcmp((uint8_t *)tail->elem, (uint8_t *)elem, ll->elem_size) == 0)
      return 1;
    else
      tail = NODE(tail->next);
  }

  return 0;
}

/* This is a safer alternative to `linkedlist_contains()`. It's not
 * a silver bullet to attacks but it provides constant time comparison
 * and constant time lookup.
 *
 * NOTE: Due to the nature of this data structure the rate of cache misses is
 * way higher than simply using an array and constant lookup means it will
 * traverse every node even if it already found a value. Be very sure of what
 * you are doing and sure this function is really what you need. */
int ll_contains_secure(LinkedList *ll, const void *elem) {
  if (!ll)
    return -1;

  int contains = 0;
  const uint8_t *elem_val = (const uint8_t *)elem;

  const LlNode *tail = NODE(ll->tail);
  for (size_t x = 0; x < ll->count; x += 1) {
    const uint8_t *tail_val = (const uint8_t *)tail->elem;

    uint8_t acum = 0;
    for (size_t i = 0; i < ll->elem_size; i += 1)
      acum |= (tail_val[i] ^ elem_val[i]);

    if (acum == 0)
      contains = 1;

    tail = NODE(tail->next);
    acum = 0;
  }

  return contains;
}

size_t ll_len(LinkedList *ll) {
  if (!ll)
    return 0;

  return ll->count;
}

/* Use `ll_next()` to traverse */
const LlNode *ll_iterable(LinkedList *ll) {
  if (!ll || ll->tail == 0)
    return NULL;

  return NODE(ll->tail);
};

/* Use `ll_prev()` to traverse */
const LlNode *ll_iterable_reverse(LinkedList *ll) {
  if (!ll || ll->head == 0)
    return NULL;

  return NODE(ll->head);
};

const LlNode *ll_next(const LlNode *node) {
  if (!node || node->next == 0)
    return NULL;

  return NODE(node->next);
}

const LlNode *ll_prev(const LlNode *node) {
  if (!node || node->prev == 0)
    return NULL;

  return NODE(node->prev);
}

int ll_is_empty(LinkedList *ll) {
  if (!ll)
    return -1;

  return ll->count == 0;
}

int ll_remove(LinkedList *ll, const void *elem) {
  if (!ll)
    return -1;

  if (ll_is_empty(ll))
    return -1;

  LlNode *tail = NODE(ll->tail);
  for (size_t x = 0; x < ll->count; x += 1) {
    if (memcmp((uint8_t *)tail->elem, (uint8_t *)elem, ll->elem_size) == 0) {
      if (tail->next == 0 && tail->prev == 0) {
        NODE(tail->prev)->next = tail->next;
        NODE(tail->next)->prev = tail->prev;
      } else if (tail->next == 0 && tail->prev != 0) {
        NODE(tail->prev)->next = 0;
        ll->head = tail->prev;
      } else if (tail->next != 0 && tail->prev == 0) {
        NODE(tail->next)->prev = 0;
        ll->tail = tail->next;
      }

      ll->count -= 1;
      free(tail->elem);
      free(tail);
      return 1;
    } else
      tail = NODE(tail->next);
  }

  return 0;
}

int ll_pop(LinkedList *ll, void *out) {
  if (ll->head == 0)
    return -1;

  if (ll_is_empty(ll))
    return -2;

  LlNode *detached_head = NODE(ll->head);
  if (detached_head->prev == 0)
    ll->head = 0;
  else {
    ll->head = detached_head->prev;
    NODE(ll->head)->next = 0;
  }

  memcpy((uint8_t *)out, (uint8_t *)detached_head->elem, ll->elem_size);
  free(detached_head->elem);
  free(detached_head);
  ll->count -= 1;

  if (ll->count == 0) {
    LlNode *node = malloc(sizeof(LlNode));
    node->next = 0;
    node->prev = 0;
    node->elem = NULL;
    ll->head = (uintptr_t)node;
    ll->tail = (uintptr_t)node;
  }
  return 0;
}

int ll_pop_back(LinkedList *ll, void *out) {
  if (ll->tail == 0)
    return -1;

  if (ll_is_empty(ll))
    return -2;

  LlNode *detached_tail = NODE(ll->tail);
  if (detached_tail->next == 0)
    ll->tail = 0;
  else {
    ll->tail = detached_tail->next;
    NODE(ll->tail)->prev = 0;
  };

  memcpy((uint8_t *)out, (uint8_t *)detached_tail->elem, ll->elem_size);
  free(detached_tail->elem);
  free(detached_tail);
  ll->count -= 1;

  if (ll->count == 0) {
    LlNode *node = malloc(sizeof(LlNode));
    node->next = 0;
    node->prev = 0;
    node->elem = NULL;
    ll->head = (uintptr_t)node;
    ll->tail = (uintptr_t)node;
  }
  return 0;
}

int ll_pop_front(LinkedList *ll, void *out) { return ll_pop(ll, out); }

void ll_free(LinkedList *ll) {
  if (!ll)
    return;

  LlNode *tail = NODE(ll->tail);
  for (size_t x = 0; x < ll->count; x += 1) {
    if (tail->next != 0) {
      LlNode *next = NODE(tail->next);
      free(tail->elem);
      free(tail);
      tail = next;
    } else {
      free(tail->elem);
      free(tail);
    }
  }

  ll->count = 0;
}
#endif
