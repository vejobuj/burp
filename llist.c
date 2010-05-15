/*
 *  llist.c
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

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

