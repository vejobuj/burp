#ifndef _LLIST_H
#define _LLIST_H

struct llist_t {
  void *data;
  struct llist_t *prev;
  struct llist_t *next;
};

typedef void (*llist_fn_free)(void *);

struct llist_t *llist_add(struct llist_t*, void*);
void llist_free(struct llist_t*, llist_fn_free);

#endif /* _LLIST_H */

