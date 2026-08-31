/* AutoComplete

   Copyright (C) 2010-2026 Todd M. Lewis and Sebastiano Vigna

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

#define MAX_AUTOCOMPLETE_SCAN (1000000)

static req_list rl;

/* Keeps track of how many strings we have scanned. At MAX_AUTOCOMPLETE_SCAN we return. */
static int count_scanned;

static void add_string(const char * const s, const int len, const char ext) {
	char *buf = strntmp(s, len);
	if (len < 1) return;
	req_list_add(&rl, buf, ext);
}

static void search_buff(const buffer *b, char * p, const int encoding, const bool case_search, const char ext) {
	assert(p);
	const int p_len = strlen(p);
	const int (*cmp)(const char *, const char *, size_t) = (const int (*)(const char *, const char *, size_t))(case_search ? strncmp : strncasecmp);
	for(line_desc *ld = (line_desc *)b->line_desc_list.head, *next; next = (line_desc *)ld->ld_node.next; ld = next) {
		int64_t l = 0, r = 0;
		do {
			/* find left edge of word */
			while (l < ld->line_len - p_len && !ne_isword(get_char(&ld->line[l], b->encoding), b->encoding)) l = next_pos(ld->line, l, b->encoding);
			if (l < ld->line_len - p_len ) {
				int ch;
				/* find right edge of word */
				r = next_pos(ld->line, l, b->encoding);
				/* accept "'" as a word character if it is followed by another word character, so that
				   words like "don't" are not broken into "don" and "t". */
				while (r < ld->line_len
				       && ( ne_isword(ch=get_char(&ld->line[r], b->encoding), b->encoding)
				            || ( r+1 < ld->line_len && ch == '\'' && ne_isword(get_char(&ld->line[r+1], b->encoding), b->encoding))
				          )
				      ) r = next_pos(ld->line, r, b->encoding);
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


/* A qsort comparison function for sorting a req_list_entry list
   in dictionary order. */

static int req_list_entries_qsorter(const void *a, const void *b) {
	const req_list_entry *rle_a = (const req_list_entry *)a;
	const req_list_entry *rle_b = (const req_list_entry *)b;

	return strdictcmp((const char *)rle_a->string, (const char *)rle_b->string);
}


/* Returns a completion for the (non-NULL) prefix p, showing suffixes from
   all buffers if ext is true. Note that p is free()'d by this function,
   and that, in turn, the returned string must be free()'d by the caller
   if it is non-NULL (a returned NULL means that no completion is available).

   If there is more than one completion, this function will invoke request_strings()
   (and subsequently reset_window()) after displaying req_msg. In any case, error
   will contain a value out of those in the enum info that start with AUTOCOMPLETE_. */

char *autocomplete(char *p, char *req_msg, const int ext, int * const error) {
	assert(p);
	int max_len = 0, min_len = INT_MAX, prefix_len = strlen(p);
	static int ac_prune = RL_PRUNE;

	*error = AUTOCOMPLETE_CANCELLED;
	if (req_list_init(&rl, (cur_buffer->opt.case_search ? strcmp : strdictcmp), ac_prune) != OK) {
		free(p);
		return NULL;
	}
	count_scanned = 0;

	search_buff(cur_buffer, p, cur_buffer->encoding, cur_buffer->opt.case_search, '\0');
	if (stop) {
		req_list_free(&rl);
		free(p);
		return NULL;
	}

	if (ext) {
		buffer *b = (buffer *)buffers.head;
		while (b->b_node.next) {
			if (b != cur_buffer) {
				search_buff(b, p, cur_buffer->encoding, cur_buffer->opt.case_search, '*');
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
		const int l = rl.entries[i].length - 1;
		if (max_len < l) max_len = l;
		if (min_len > l) min_len = l;
	}
	req_list_finalize(&rl);


	free(p);
	p = NULL;

#ifdef NE_TEST
	/* During tests, we always output the middle entry. */
	if (rl.cur_entries) {
		qsort(rl.entries, rl.cur_entries, sizeof(req_list_entry), req_list_entries_qsorter);
		p = str_dup(rl.entries[rl.cur_entries/2].string);
	}
	*error = AUTOCOMPLETE_COMPLETED;
	req_list_free(&rl);
	return p;
#endif

	if (rl.cur_entries) {
		qsort(rl.entries, rl.cur_entries, sizeof(req_list_entry), req_list_entries_qsorter);
		/* Find maximum common prefix. */
		int m = rl.entries[0].length - 1;
		encoding_type enc_0 = rl.entries[0].encoding;
		for (int i = 1; m && i < rl.cur_entries; i++) {
			encoding_type enc_i = rl.entries[i].encoding;
			int mi = max_prefix(rl.entries[0].string, enc_0, rl.entries[i].string, enc_i);
			if (mi < m) m = mi;
		}

		/* If we can output more characters than the prefix len, we do so without
		   starting the requester. */
		if (m > prefix_len) {
			p = malloc(m + 1);
			strncpy(p, rl.entries[0].string, m);
			p[m] = 0;
			*error = min_len == m ? AUTOCOMPLETE_COMPLETED : AUTOCOMPLETE_PARTIAL;
		}
		else {
			if (req_msg) print_message(req_msg);
			int result = request_strings(&rl, 0);
			ac_prune = rl.prune ? RL_PRUNE : 0; /* Preserve preferred prune option across invocations. */
			if (result != ERROR) {
				result = result >= 0 ? result : -result - 2;
				p = str_dup(rl.entries[result].string);
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
