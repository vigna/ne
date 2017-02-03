/* Syntax highlighting from Joe's Own Editor: Simple hash tables.

	Copyright (C) 1992 Joseph H. Allen
	Copyright (C) 2009-2017 Todd M. Lewis and Sebastiano Vigna

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


#ifndef _JOE_HASH_H
#define _JOE_HASH_H 1

struct entry {
	HENTRY *next;
	unsigned char *name;
	unsigned hash_val;
	void *val;
};

struct hash {
	unsigned len;
	HENTRY **tab;
	unsigned nentries;
};

/* Compute hash code for a string */
unsigned long hash PARAMS((unsigned char *s));

/* Create a hash table of specified size, which must be a power of 2 */
HASH *htmk PARAMS((int len));

/* Delete a hash table.  HENTRIES get freed, but name/vals don't. */
void htrm PARAMS((HASH *ht));

/* Add an entry to a hash table.
  Note: 'name' is _not_ strdup()ed */
void *htadd PARAMS((HASH *ht, unsigned char *name, void *val));

/* Look up an entry in a hash table, returns NULL if not found */
void *htfind PARAMS((HASH *ht, unsigned char *name));

#endif
