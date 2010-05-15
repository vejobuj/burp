/*
 *  llist.h
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
