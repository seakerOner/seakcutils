#ifndef LINKEDLIST_H
#define LINKEDLIST_H

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// TODO: Linked List is always leaking 48 bytes in 2 blocks, grep *TODO
// TODO: When the Linked List is emptied it cannot regrow properly

typedef struct LlNode_t LlNode;

typedef struct LlNode_t {
  LlNode *next;
  LlNode *prev;
  void *elem;
} LlNode;

typedef struct LinkedList_t {
  LlNode *head;
  LlNode *tail;
  size_t count;
  size_t elem_size;
} LinkedList;

LinkedList linkedlist_new(size_t elem_size) {
  LinkedList ll;
  ll.elem_size = elem_size;
  ll.count = 0;

  LlNode *node = malloc(sizeof(LlNode));
  ll.head = malloc(sizeof(LlNode)); // *TODO: leaking this malloc probably
  ll.tail = malloc(sizeof(LlNode)); // *TODO: leaking this malloc probably
  ll.head->elem = NULL;
  ll.tail->elem = NULL;
  ll.head->next = NULL;
  ll.tail->prev = NULL;

  node->next = NULL;
  node->prev = NULL;
  ll.head = node;
  ll.tail = node;
  return ll;
}

int ll_push_back(LinkedList *ll, const void *elem) {
  if (!ll)
    return -1;

  if (ll->tail->elem == NULL) {
    ll->tail->elem = malloc(ll->elem_size);
    memcpy((uint8_t *)ll->tail->elem, (uint8_t *)elem, ll->elem_size);
  } else {
    LlNode *node_tail = malloc(sizeof(LlNode));
    node_tail->elem = malloc(ll->elem_size);
    memcpy((uint8_t *)node_tail->elem, (uint8_t *)elem, ll->elem_size);

    node_tail->next = ll->tail;
    node_tail->prev = NULL;
    ll->tail->prev = node_tail;
    ll->tail = node_tail;
  }

  ll->count += 1;
  return 0;
}

int ll_append(LinkedList *ll, const void *elem) {
  if (!ll)
    return -1;

  if (ll->head->elem == NULL) {
    ll->head->elem = malloc(ll->elem_size);
    memcpy((uint8_t *)ll->head->elem, (uint8_t *)elem, ll->elem_size);
  } else {
    LlNode *node_head = malloc(sizeof(LlNode));
    node_head->elem = malloc(ll->elem_size);
    memcpy((uint8_t *)node_head->elem, (uint8_t *)elem, ll->elem_size);

    node_head->next = NULL;
    node_head->prev = ll->head;
    ll->head->next = node_head;
    ll->head = node_head;
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

  const LlNode *tail = ll->tail;
  for (size_t x = 0; x < ll->count; x += 1) {
    if (memcmp((uint8_t *)tail->elem, (uint8_t *)elem, ll->elem_size) == 0)
      return 1;
    else
      tail = tail->next;
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

  const LlNode *tail = ll->tail;
  for (size_t x = 0; x < ll->count; x += 1) {
    const uint8_t *tail_val = (const uint8_t *)tail->elem;

    uint8_t acum = 0;
    for (size_t i = 0; i < ll->elem_size; i += 1)
      acum |= (tail_val[i] ^ elem_val[i]);

    if (acum == 0)
      contains = 1;

    tail = tail->next;
    acum = 0;
  }

  if (contains)
    return 1;
  else
    return 0;
}

size_t ll_len(LinkedList *ll) {
  if (!ll)
    return 0;

  return ll->count;
}

/* Use `ll_next()` to traverse */
const LlNode *ll_iterable(LinkedList *ll) {
  if (!ll || !ll->tail)
    return NULL;

  return ll->tail;
};

/* Use `ll_prev()` to traverse */
const LlNode *ll_iterable_reverse(LinkedList *ll) {
  if (!ll || !ll->head)
    return NULL;

  return ll->head;
};

const LlNode *ll_next(const LlNode *node) {
  if (!node || !node->next)
    return NULL;

  return node->next;
}

const LlNode *ll_prev(const LlNode *node) {
  if (!node || !node->prev)
    return NULL;

  return node->prev;
}

/* TODO: FIX THIS */
int ll_remove(LinkedList *ll, void *elem) {
  if (!ll)
    return -1;

  // if (ll_is_empty(ll))
  //   return -1;

  LlNode *tail = ll->tail;
  for (size_t x = 0; x < ll->count; x += 1) {
    if (memcmp((uint8_t *)tail->elem, (uint8_t *)elem, ll->elem_size) == 0) {
      if (tail->next && tail->prev) {
        tail->prev->next = tail->next;
        tail->next->prev = tail->prev;
      } else if (!tail->next && tail->prev) {
        tail->prev->next = NULL;
        ll->head = tail->prev;
      } else if (tail->next && !tail->prev) {
        tail->next->prev = NULL;
        ll->tail = tail->next;
      }

      ll->count -= 1;
      free(tail->elem);
      free(tail);
      return 1;
    } else
      tail = tail->next;
  }

  return 0;
}

int ll_is_empty(LinkedList *ll) {
  if (!ll)
    return -1;

  return ll->count == 0;
}

int ll_pop(LinkedList *ll, void *out) {
  if (!ll->head)
    return -1;

  if (ll_is_empty(ll))
    return -2;

  LlNode *detached_head = ll->head;
  if (!ll->head->prev)
    ll->head = NULL;
  else {
    ll->head = detached_head->prev;
    ll->head->next = NULL;
  }

  memcpy((uint8_t *)out, (uint8_t *)detached_head->elem, ll->elem_size);
  free(detached_head->elem);
  free(detached_head);
  ll->count -= 1;

  return 0;
}

int ll_pop_back(LinkedList *ll, void *out) {
  if (!ll->tail)
    return -1;

  if (ll_is_empty(ll))
    return -2;

  LlNode *detached_tail = ll->tail;
  if (!ll->tail->next)
    ll->tail = NULL;
  else {
    ll->tail = detached_tail->next;
    ll->tail->prev = NULL;
  };

  memcpy((uint8_t *)out, (uint8_t *)detached_tail->elem, ll->elem_size);
  free(detached_tail->elem);
  free(detached_tail);
  ll->count -= 1;
  return 0;
}

int ll_pop_front(LinkedList *ll, void *out) { return ll_pop(ll, out); }

void ll_free(LinkedList *ll) {
  if (!ll)
    return;

  LlNode *tail = ll->tail;
  for (size_t x = 0; x < ll->count; x += 1) {
    if (tail->next) {
      LlNode *next = tail->next;
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
#endif // !LINKEDLIST_H
