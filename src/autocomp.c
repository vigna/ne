/* AutoComplete

   Copyright (C) 2010-2019 Todd M. Lewis and Sebastiano Vigna

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
#include "support.h"

#define EXTERNAL_FLAG_CHAR '*'

#define MAX_AUTOCOMPLETE_SCAN (1000000)

static req_list rl;

/* Keeps track of how many strings we have scanned. At MAX_AUTOCOMPLETE_SCAN we return. */
static int count_scanned;

static void add_string(const char * const s, const int len, const int ext) {
	char *buf = strntmp(s, len);
	if (len < 1) return;
	req_list_add(&rl, buf, ext);
}

static void search_buff(const buffer *b, char * p, const int encoding, const bool case_search, const int ext) {
	assert(p);
	const int p_len = strlen(p);
	const int (*cmp)(const char *, const char *, size_t) = case_search ? strncmp : strncasecmp;
	for(line_desc *ld = (line_desc *)b->line_desc_list.head, *next; next = (line_desc *)ld->ld_node.next; ld = next) {
		int64_t l = 0, r = 0;
		do {
			/* find left edge of word */
			while (l < ld->line_len - p_len && !ne_isword(get_char(&ld->line[l], b->encoding), b->encoding)) l += get_char_width(&ld->line[l], b->encoding);
			if (l < ld->line_len - p_len ) {
				int ch;
				/* find right edge of word */
				r = l + get_char_width(&ld->line[l], b->encoding);
				/* accept "'" as a word character if it is followed by another word character, so that
				   words like "don't" are not broken into "don" and "t". */
				while (r < ld->line_len
				       && (    ne_isword(ch=get_char(&ld->line[r], b->encoding), b->encoding)
				            || ( r+1 < ld->line_len && ch == '\'' && ne_isword(get_char(&ld->line[r+1], b->encoding), b->encoding))
				          )
				      ) r += get_char_width(&ld->line[r], b->encoding);
				if ((b != cur_buffer || ld != b->cur_line_desc || b->cur_pos < l || r < b->cur_pos)
				     && r - l > p_len && (b->encoding == encoding || is_ascii(&ld->line[l], r - l))
				     && !cmp(p, &ld->line[l], p_len))
					add_string(&ld->line[l], r - l, ext);
				l = r;
				count_scanned++;
			}
			assert(l <= ld->line_len);
			if (stop || count_scanned >= MAX_AUTOCOMPLETE_SCAN) {
				add_string(NULL, -1, 0);
				return;
			}
		} while (l < ld->line_len - p_len);
	}
	add_string(NULL, -1, 0);
}

/* Returns a completion for the (non-NULL) prefix p, showing suffixes from
   all buffers if ext is true. Note that p is free()'d by this function,
   and that, in turn, the returned string must be free()'d by the caller
   if it is non-NULL (a returned NULL means that no completion is available).

   If there is more than one completion, this function will invoke request_strings()
   (and subsequently reset_window()) after displaying req_msg. In any case, error 
   will contain a value out of those in the enum info that start with AUTOCOMPLETE_. */

char *autocomplete(char *p, char *req_msg, const int ext, int * const error) {
	int max_len = 0, min_len = INT_MAX, prefix_len = strlen(p);
	static int ac_prune = true;
	assert(p);

	req_list_init(&rl, (cur_buffer->opt.case_search ? strcmp : strdictcmp), false, false, EXTERNAL_FLAG_CHAR);
	rl.prune = ac_prune;
	count_scanned = 0;

	search_buff(cur_buffer, p, cur_buffer->encoding, cur_buffer->opt.case_search, false);
	if (stop) {
		req_list_free(&rl);
		free(p);
		return NULL;
	}

	if (ext) {
		buffer *b = (buffer *)buffers.head;
		while (b->b_node.next) {
			if (b != cur_buffer) {
				search_buff(b, p, cur_buffer->encoding, cur_buffer->opt.case_search, true);
				if (stop) {
					req_list_free(&rl);
					free(p);
					return NULL;
				}
			}
			b = (buffer *)b->b_node.next;
		}
 	}

	for(int i = 0; i < rl.cur_entries; i++) {
		const int l = strlen(rl.entries[i]);
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
		int m = strlen(rl.entries[0]);
		if (rl.entries[0][m-1] == EXTERNAL_FLAG_CHAR) m--;
		for(int i = 1; i < rl.cur_entries; i++) {
			int j;
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
			int result = request_strings(&rl, 0);
			ac_prune = rl.prune;
			if (result != ERROR) {
				result = result >= 0 ? result : -result - 2;
				/* Delete EXTERNAL_FLAG_CHAR at the end of the strings if necessary. */
				if (rl.entries[result][strlen(rl.entries[result]) - 1] == EXTERNAL_FLAG_CHAR) rl.entries[result][strlen(rl.entries[result]) - 1] = 0;
				p = str_dup(rl.entries[result]);
				*error = AUTOCOMPLETE_COMPLETED;
			}
			else *error = AUTOCOMPLETE_CANCELLED;
			reset_window();
		}
	}
	else *error = AUTOCOMPLETE_NO_MATCH;

	req_list_free(&rl);
	D(fprintf(stderr, "autocomp returning '%s', entries: %d\n", p, rl.cur_entries);)
	return p;
}


