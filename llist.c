/* Copyright (c) 2010 Dave Reisner
 *
 * llist.c
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use,
 * copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following
 * conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
 * OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
 * HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */

#include <stdlib.h>

#include "llist.h"

struct llist_t *llist_add(struct llist_t *list, void *data) {
  struct llist_t *ptr;

  ptr = calloc(1, sizeof *list);
  if (ptr == NULL) {
    return(list);
  }

  ptr->data = data;
  ptr->next = NULL;

  if (list == NULL) {
    ptr->prev = ptr;
    return(ptr);
  }

  list->prev->next = ptr;
  ptr->prev = list->prev;
  list->prev = ptr;

  return(list);
}

void llist_free(struct llist_t *list, void (*fn)(void*)) {
  struct llist_t *it = list;

  while (it) {
    struct llist_t *next = it->next;
    if (fn && it->data) {
      fn(it->data);
    }
    free(it);
    it = next;
  }
}

