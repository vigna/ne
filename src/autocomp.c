/* AutoComplete

	Copyright (C) 2010-2015 Todd M. Lewis and Sebastiano Vigna

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

#define EXTERNAL_FLAG_CHAR '*'

static req_list rl;

static void add_string(unsigned char * const s, const int len, const int ext) {
	static char *buf = NULL;
	static int buflen = 0;
	char *buf_new;
	int cplen = len;

	if (len < 1) {
		if (buf) free(buf);
		buf = NULL;
		buflen = 0;
		return;
	}
	if (len > buflen) {
		if (buf_new = realloc(buf,len * 2 + 1)) {
			buflen = len * 2 + 1;
			buf = buf_new;
		}
		else cplen = buflen - 1;
	}
	strncpy(buf,s,cplen);
	buf[cplen] = '\0';
	req_list_add(&rl,buf,ext);
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
			if (l < ld->line_len - p_len ) {
				/* find right edge of word */
				r = l + get_char_width(&ld->line[l], b->encoding);
				while (r < ld->line_len && ne_isword(get_char(&ld->line[r], b->encoding), b->encoding)) r += get_char_width(&ld->line[r], b->encoding);
				if (r - l > p_len && !(case_search ? strncmp : strncasecmp)(p, &ld->line[l], p_len)) {
					if (b->encoding == encoding || is_ascii(&ld->line[l], r - l)) add_string(&ld->line[l], r - l, ext);
				}
				l = r;
			}
			assert(l <= ld->line_len);
			if (stop) {
				add_string(NULL,-1,0);
				return;
			}
		} while (l < ld->line_len - p_len);
		
		ld = next;
	}
	add_string(NULL,-1,0);
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
			
	assert(p);
	
	req_list_init(&rl, (cur_buffer->opt.case_search ? strcmp : strdictcmp), FALSE, FALSE, EXTERNAL_FLAG_CHAR);

	search_buff(cur_buffer, p, cur_buffer->encoding, cur_buffer->opt.case_search, FALSE);
	if (stop) {
		req_list_free(&rl);
		free(p);
		return NULL;
	}

	if (ext) {
		buffer *b = (buffer *)buffers.head;
		while (b->b_node.next) {
			if (b != cur_buffer) {
				search_buff(b, p, cur_buffer->encoding, cur_buffer->opt.case_search, TRUE);
				if (stop) {
					req_list_free(&rl);
					free(p);
					return NULL;
				}
			}
			b = (buffer *)b->b_node.next;
		}
 	}

	for(i = 0; i < rl.cur_entries; i++) {
		l = strlen(rl.entries[i]);
		if (max_len < l) max_len = l;
		if (min_len > l) min_len = l;
	}
	/* We compact the table into a vector of char pointers. */
	req_list_finalize(&rl);


	free(p);
	p = NULL;
	
#ifdef NE_TEST
	/* During tests, we always output the middle entry. */
	if (rl.cur_entries) {
		if (rl.entries[rl.cur_entries/2][strlen(rl.entries[rl.cur_entries/2]) - 1] == EXTERNAL_FLAG_CHAR) rl.entries[rl.cur_entries/2][strlen(rl.entries[rl.cur_entries/2]) - 1] = 0;
		p = str_dup(rl.entries[rl.cur_entries/2]);
	}
	*error = AUTOCOMPLETE_COMPLETED;
	req_list_free(&rl);
	return p;
#endif

	if (rl.cur_entries > 0) {
		qsort(rl.entries, rl.cur_entries, sizeof(char *), strdictcmpp);
		/* Find maximum common prefix. */
		m = strlen(rl.entries[0]);
		if (rl.entries[0][m-1] == EXTERNAL_FLAG_CHAR) m--;
		for(i = 1; i < rl.cur_entries; i++) {
			for(j = 0; j < m; j++) 
				if (rl.entries[i][j] != rl.entries[0][j]) break;
			m = j;
		}

		/* If we can output more characters than the prefix len, we do so without
			starting the requester. */
		if (m > prefix_len) {
			p = malloc(m + 1);
			strncpy(p, rl.entries[0], m);
			p[m] = 0;
			*error = min_len == m ? AUTOCOMPLETE_COMPLETED : AUTOCOMPLETE_PARTIAL;
		}
		else {
			if (req_msg) print_message(req_msg);
			if ((i = request_strings(&rl, 0)) != ERROR) {
				i = i >= 0 ? i : -i - 2;
				/* Delete EXTERNAL_FLAG_CHAR at the end of the strings if necessary. */
				if (rl.entries[i][strlen(rl.entries[i]) - 1] == EXTERNAL_FLAG_CHAR) rl.entries[i][strlen(rl.entries[i]) - 1] = 0;
				p = str_dup(rl.entries[i]);
				*error = AUTOCOMPLETE_COMPLETED;
			}
			else *error = AUTOCOMPLETE_CANCELLED;
			reset_window();
		}
	}
	else *error = AUTOCOMPLETE_NO_MATCH;

	req_list_free(&rl);
	D(fprintf(stderr,"autocomp returning '%s', entries: %d\n", p, rl.cur_entries);)
	return p;
}
