/* List handling functions.

   Copyright (C) 1993-1998 Sebastiano Vigna 
   Copyright (C) 1999-2017 Todd M. Lewis and Sebastiano Vigna

   This file is part of ne, the nice editor.

   This library is free software; you can redistribute it and/or modify it
   under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 3 of the License, or (at your
   option) any later version.

   This library is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
   or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
   for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, see <http://www.gnu.org/licenses/>.  */


#include "ne.h"

/* These functions provide basic management of lists. The basic ideas in this
   file come from the Amiga Exec list management functions and their C
   counterparts developed by Marco Zandonadi. Note that because of the way a
   list is defined, there are never special cases for the empty list. The price
   to pay is that a list is empty not if it's NULL, but rather is l->head->next
   is NULL. The first node of a list is l->head, the last one is
   l->tail_pred. A node is the last in a list if n->next->next == NULL. */

/* Initializes a list before any usage. */

void new_list(list *l) {
	l->head = (node *)&(l->tail);
	l->tail_pred = (node *)&(l->head);
	l->tail = NULL;
}


/* Inserts a node at the head of a list. */

void add_head(list *l, node *n) {
	n->next = l->head;
	n->prev = (node *) l;

	l->head->prev = n;
	l->head = n;
}


/* Inserts a node at the tail of a list. */

void add_tail(list *l, node *n) {
	n->next = (node *)&l->tail;
	n->prev = l->tail_pred;

	l->tail_pred->next = n;
	l->tail_pred = n;
}


/* Removes a node. Note that we do *not* need to know the list. */

void rem(node *n) {
	n->prev->next = n->next;
	n->next->prev = n->prev;
}


/* Adds a node to a list after a specified position. list.head and
   list.tail_pred are valid positions. */

void add(node *n, node *pos) {
	n->next = pos->next;
	n->prev = pos;
	pos->next->prev = n;
	pos->next = n;
}


/* Applies a given deallocation function throughout a whole list, emptying the
   list itself. */

void free_list(list *l, void (func)()) {

	node *n1 = l->head, *n2;

	while(n1->next) {
		n2 = n1->next;
		rem(n1);
		func(n1);
		n1 = n2;
	}
}

/* Applies a given function throughout a whole list. */

void apply_to_list(list *l, void (func)()) {
	node *n1 = l->head;

	while(n1->next) {
		func(n1);
		n1 = n1->next;
	}
}

