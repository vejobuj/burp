#include <stdlib.h>

#include "llist.h"

struct llist_t *llist_add(struct llist_t *list, void *data) {
  struct llist_t *ptr;

  ptr = calloc(1, sizeof *list);
  if (ptr == NULL) {
    return list;
  }

  ptr->data = data;
  ptr->next = NULL;

  if (list == NULL) {
    ptr->prev = ptr;
    return ptr;
  }

  list->prev->next = ptr;
  ptr->prev = list->prev;
  list->prev = ptr;

  return list;
}

void llist_free(struct llist_t *list, llist_fn_free fn) {
  struct llist_t *it = list;

  while (it) {
    struct llist_t *tmp = it->next;
    if (fn && it->data)
      fn(it->data);
    free(it);
    it = tmp;
  }
}

