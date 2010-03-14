/* AutoComplete

   Copyright (C) 2010 Todd M. Lewis and Sebastiano Vigna

   This file is part of ne, the nice editor.

   This program is free software; you can redistribute it and/or modify it
   under the terms of the GNU General Public License as published by the
   Free Software Foundation; either version 2, or (at your option) any
   later version.
   
   This program is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.
   
   You should have received a copy of the GNU General Public License along
   with this program; see the file COPYING.  If not, write to the Free
   Software Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
   02111-1307, USA.  */


#include "ne.h"

#define INITIAL_HASH_TABLE_SIZE (1024)
#define INITIAL_BUFFER_SIZE (16*1024)
#define EXTERNAL_FLAG_CHAR '*'

/* The string buffer. Contains all strings found so far, concatenated. 
   Each string has an additional NUL at its end to make room for an EXTERNAL_FLAG_CHAR. */
static unsigned char *strings;

/* The size of the string buffer, and the first free location. */
static int strings_size, strings_first_free;

/* The hash table. Entries are offsets into strings. */
static int *hash_table;

/* The allocated size of the table, the mask to compute the modulo (i.e., size - 1) and the number of entries in the table. */
static int size, mask, n;

static int hash2(unsigned char *s, int len) {
		unsigned long h = 42;
		while(len-- != 0) h ^= ( h << 5 ) + s[len] + ( h >> 2 );
		return h & mask;
}

static void init_hash_table(void) {
	hash_table = calloc(size = INITIAL_HASH_TABLE_SIZE, sizeof *hash_table);
	mask = size - 1;
	n = 0;
	strings = malloc(strings_size = INITIAL_BUFFER_SIZE);
	strings_first_free = 1; /* We want to use 0 to denote empty slots. */
}

static void delete_hash_table(void) {
	free(strings);
	free(hash_table);
}

/* Add a given string, with specified length, to the string buffer, returning a pointer to the copy. */
static unsigned char *add_to_strings(const unsigned char * const s, int len) {
	if (strings_size - strings_first_free < len + 2) {
		if ( (strings_size *= 2) - strings_first_free < len + 2 ) strings_size += len + 2;
		strings = realloc(strings, strings_size);
	}
	strncpy(strings + strings_first_free, s, len);
	strings[strings_first_free + len] = 0;
	strings[strings_first_free + len + 1] = 0;
	strings_first_free += len + 2;
	return strings + strings_first_free - len - 2;
}


static int code(const int hash, const int ext) {
	return ext ? -hash: hash;
}

static int decode(const int hash) {
	return hash < 0 ? -hash : hash;
}


static void add_string(unsigned char * const s, const int len, const int ext) {
	int hash = hash2(s, len);

	while(hash_table[hash] && strncmp(&strings[decode(hash_table[hash])], s, len)) hash = ( hash + 1 ) & mask;
	if (hash_table[hash]) return;

	n++;
	hash_table[hash] = code(add_to_strings(s, len) - strings, ext);
	if ( size < ( ( n * 4 ) / 3 ) ) {
		int i, l;
		unsigned char *p;
		int *new_hash_table = calloc(size *= 2, sizeof *hash_table);
		mask = size - 1;
		for(i = 0; i < size / 2; i++) {
			if (hash_table[i]) {
				p = &strings[decode(hash_table[i])];
				l = strlen(p);
				hash = hash2(p, l);
				while(new_hash_table[hash] && strncmp(&strings[decode(new_hash_table[hash])], p, l)) hash = ( hash + 1 ) & mask;
				new_hash_table[hash] = code(p - strings, hash_table[i] < 0);
				p += l + 2;
			}
		}
		
		free(hash_table);
		hash_table = new_hash_table;
		assert(p == strings + strings_first_free);
	}
}

static int search_buff(const buffer *b, const unsigned char *p, int case_search, const int ext) {
	line_desc *ld = (line_desc *)b->line_desc_list.head, *next;
	int p_len = strlen(p);
	int l, r, max_len = 0;
	
	assert(p);
	
	while (next = (line_desc *)ld->ld_node.next) {
		l = r = 0;
		do {
			/* find left edge of word */
			while (l < ld->line_len - p_len && !ne_isword(get_char(&ld->line[l], b->encoding), b->encoding)) l += get_char_width(&ld->line[l], b->encoding);
			if    (l < ld->line_len - p_len ) {
				/* find right edge of word */
				r = l + get_char_width(&ld->line[l], b->encoding);
				while (r < ld->line_len && ne_isword(get_char(&ld->line[r], b->encoding), b->encoding)) r += get_char_width(&ld->line[r], b->encoding);
				if (r - l >= p_len && p_len == 0 || !(case_search ? strncmp : strncasecmp)(p, &ld->line[l], p_len)) {
					add_string(&ld->line[l], r - l, ext);
					if (max_len < r - l) max_len = r - l;
				}
				l = r;
			}
			assert(l <= ld->line_len);
			if (stop) return -1;
		} while (l < ld->line_len - p_len);
		
		ld = next;
	}
	
	return max_len;
}

unsigned char *autocomplete(unsigned char *p, const int ext) {
	int i, j, m, max_len = 0;
	char **entries;
			
	assert(p);
	
	init_hash_table();

	max_len = search_buff(cur_buffer, p, cur_buffer->opt.case_search, FALSE);
	if (stop) {
		delete_hash_table();
		free(p);
		return NULL;
	}

	if (ext) {
		buffer *b = (buffer *)buffers.head;
		while (b->b_node.next) {
			if (b != cur_buffer) m = search_buff(b, p, cur_buffer->opt.case_search, TRUE);
			if (stop) {
				delete_hash_table();
				free(p);
				return NULL;
			}
			if (max_len < m) max_len = m;
			b = (buffer *)b->b_node.next;
		}
 	}

	/** We compact the table into a vector of char pointers. */
	entries = malloc(n * sizeof *entries);
	for(i = j = 0; i < size; i++) 
		if (hash_table[i]) {
			hash_table[j] = hash_table[i];
			entries[j] = &strings[decode(hash_table[i])];
			if (hash_table[i] < 0) entries[j][strlen(entries[j])] = EXTERNAL_FLAG_CHAR;
			j++;
		}
	assert(j == n);

	free(p);
	p = NULL;

#ifdef NE_TEST
	/* During tests, we always output the middle entry. */
	if (n) {
		p = str_dup(entries[n/2]);
		if (hash_table[n/2] < 0) p[strlen(p) - 1] = 0;
	}
	free(entries);
	delete_hash_table();
	return p;
#endif
	
	if (n == 1) p = str_dup(entries[0]);
	else {
		qsort(entries, n, sizeof *entries, strdictcmp);  
		if ((i = request_strings((const char * const *)entries, n, 0, max_len, EXTERNAL_FLAG_CHAR)) != ERROR) {
			i = i >= 0 ? i : -i - 2;
			p = str_dup(entries[i]);
			/* Delete EXTERNAL_FLAG_CHAR at the end of the strings if necessary. */
			if (hash_table[i] < 0) p[strlen(p) - 1] = 0;
		}
		reset_window();
	}
	
	free(entries);
	delete_hash_table();
	D(fprintf(stderr,"autocomp returning '%s', entries: %d\n", p, n);)
	return p;
}
