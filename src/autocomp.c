/* AutoComplete

   Copyright (C) 2010-2011 Todd M. Lewis and Sebastiano Vigna

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
	}
}

static void search_buff(const buffer *b, const unsigned char *p, const int encoding, const int case_search, const int ext) {
	line_desc *ld = (line_desc *)b->line_desc_list.head, *next;
	int p_len = strlen(p);
	int l, r;
	
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
				if (r - l > p_len && !(case_search ? strncmp : strncasecmp)(p, &ld->line[l], p_len)) {
					if (b->encoding == encoding || is_ascii(&ld->line[l], r - l)) add_string(&ld->line[l], r - l, ext);
				}
				l = r;
			}
			assert(l <= ld->line_len);
			if (stop) return;
		} while (l < ld->line_len - p_len);
		
		ld = next;
	}
}

/* Returns a completion for the (non-NULL) prefix p, showing suffixes from
   all buffers if ext is true. Note that p is free()'d by this function,
   and that, in turn, the returned string must be free()'d by the caller
   if it is non-NULL (a returned NULL means that no completion is available).
   
   If there is more than one completion, this function will invoke request_strings()
   (and subsequently reset_window()) after displaying req_msg. In any case, error 
   will contain a value out of those in the enum info that start with AUTOCOMPLETE_. */

unsigned char *autocomplete(unsigned char *p, char *req_msg, const int ext, int * const error) {
	int i, j, l, m, max_len = 0, min_len = INT_MAX, prefix_len = strlen(p);
	char **entries;
			
	assert(p);
	
	init_hash_table();

	search_buff(cur_buffer, p, cur_buffer->encoding, cur_buffer->opt.case_search, FALSE);
	if (stop) {
		delete_hash_table();
		free(p);
		return NULL;
	}

	if (ext) {
		buffer *b = (buffer *)buffers.head;
		while (b->b_node.next) {
			if (b != cur_buffer) {
				search_buff(b, p, cur_buffer->encoding, cur_buffer->opt.case_search, TRUE);
				if (stop) {
					delete_hash_table();
					free(p);
					return NULL;
				}
			}
			b = (buffer *)b->b_node.next;
		}
 	}

	/* We compact the table into a vector of char pointers. */
	entries = malloc(n * sizeof *entries);
	for(i = j = 0; i < size; i++) 
		if (hash_table[i]) {
			entries[j] = &strings[decode(hash_table[i])];
			l = strlen(entries[j]);
			if (max_len < l) max_len = l;
			if (min_len > l) min_len = l;
			if (hash_table[i] < 0) entries[j][strlen(entries[j])] = EXTERNAL_FLAG_CHAR;
			j++;
		}
	assert(j == n);

	free(p);
	p = NULL;
	
#ifdef NE_TEST
	/* During tests, we always output the middle entry. */
	if (n) {
		if (entries[n/2][strlen(entries[n/2]) - 1] == EXTERNAL_FLAG_CHAR) entries[n/2][strlen(entries[n/2]) - 1] = 0;
		p = str_dup(entries[n/2]);
	}
	*error = AUTOCOMPLETE_COMPLETED;
	free(entries);
	delete_hash_table();
	return p;
#endif

	if (n != 0) {
		qsort(entries, n, sizeof *entries, strdictcmp);  
		/* Find maximum common prefix. */
		m = strlen(entries[0]);
		for(i = 1; i < n; i++) {
			for(j = 0; j < m; j++) 
				if (entries[i][j] != entries[0][j]) break;
			m = j;
		}

		/* If we can output more character than the prefix len, we do so without
			starting the requester. */
		if (m > prefix_len) {
			p = malloc(m + 1);
			strncpy(p, entries[0], m);
			p[m] = 0;
			*error = min_len == m ? AUTOCOMPLETE_COMPLETED : AUTOCOMPLETE_PARTIAL;
		}
		else {
			print_message(req_msg);
			if ((i = request_strings((const char * const *)entries, n, 0, max_len + 1, EXTERNAL_FLAG_CHAR)) != ERROR) {
				i = i >= 0 ? i : -i - 2;
				/* Delete EXTERNAL_FLAG_CHAR at the end of the strings if necessary. */
				if (entries[i][strlen(entries[i]) - 1] == EXTERNAL_FLAG_CHAR) entries[i][strlen(entries[i]) - 1] = 0;
				p = str_dup(entries[i]);
				*error = AUTOCOMPLETE_COMPLETED;
			}
			else *error = AUTOCOMPLETE_CANCELLED;
			reset_window();
		}
	}
	else *error = AUTOCOMPLETE_NO_MATCH;

	free(entries);
	delete_hash_table();
	D(fprintf(stderr,"autocomp returning '%s', entries: %d\n", p, n);)
	return p;
}
