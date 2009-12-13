/* AutoComplete

   Copyright (C) 1999-2009 Todd M. Lewis and Sebastiano Vigna

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

static buffer *autocomp_buff = NULL;

static void init_autocomp(void) {
	if (!autocomp_buff){
		autocomp_buff = alloc_buffer(NULL);
		if (autocomp_buff) {
			/* const char * const autocomp_filename = tilde_expand("~/.ne/.autocomp"); */
			clear_buffer(autocomp_buff);
			autocomp_buff->opt.do_undo = 0;
			autocomp_buff->opt.auto_prefs = 0;
			/* The autocomp buffer is agnostic. The actual encoding of each line is detected dynamically. */
			autocomp_buff->encoding = ENC_8_BIT;
			/* change_filename(autocomp_buff, str_dup(autocomp_filename)); */
			assert_buffer(autocomp_buff);
		}
	}
	if (autocomp_buff) {
		move_to_bof(autocomp_buff);
		move_to_sol(autocomp_buff);
	}
}

void close_autocomp(void) {
	if (autocomp_buff) {
		/* if (autocomp_buff->is_modified) {
			save_buffer_to_file(autocomp_buff,NULL);
		}  */
		free_buffer(autocomp_buff);
		autocomp_buff = NULL;
	}
}

static void add_to_autocomp(const unsigned char * const str, int len, const unsigned char *postfix) {
	int l, r, m, t, minlen, c0, c1, i;
	line_desc *ldl, *ldr, *ldm;

	if (!autocomp_buff || !str || !*str) return;

	m = l = 0;
	ldm = ldl = (line_desc *)autocomp_buff->line_desc_list.head;
	r   = autocomp_buff->num_lines - 1;
	ldr = (line_desc *)autocomp_buff->line_desc_list.tail_pred->prev;

	for (i=0; i<(r - l)/2; i++) {
		m++;
		ldm = (line_desc *)ldm->ld_node.next;
	}

	while (l < r) {
		minlen = max(0,min(len, ldm->line_len - 2));
		c0 = (minlen > 0) ? strncasecmp(str, ldm->line, minlen) : 0;
		if (c0 < 0 || (c0 == 0 && len < ldm->line_len - 2)) {
			r = m;
			ldr = ldm; 
			m = l;
			ldm = ldl;
		   for (i = 0; i<(r - l)/2; i++) {
				m++;
				ldm = (line_desc *)ldm->ld_node.next;
			}
		} else if (c0 > 0 || (c0 == 0 && len > ldm->line_len - 2)) {
			m = l = m + 1;
			ldm = ldl = (line_desc *)ldm->ld_node.next;
		   for (i = 0; i < (r - l)/2; i++) {
				m++;
				ldm = (line_desc *)ldm->ld_node.next;
			}
		} else if (c0 == 0 && len == ldm->line_len - 2) {
			/* We've got an exact cast-insensitive match. Back up to the first such match. */
			while (m > 0 && len == ((line_desc *)ldm->ld_node.prev)->line_len - 2 && !strncasecmp(str, ((line_desc *)ldm->ld_node.prev)->line, len)) {
				m--;
				ldm = (line_desc *)ldm->ld_node.prev;
			}
			/* Scan forward for possible case-sensitive match, or insertion point. */
			while (len == ldm->line_len - 2 && (c0 = strncasecmp(str, ldm->line, len)) == 0) {
				if (c1 = strncmp(str, ldm->line, len) < 0) {
					m++;
					ldm = (line_desc *)ldm->ld_node.next;
				} else if (c1 == 0) {
					return; /* Exact match is already in the list. */
				}
			}
			l = r = m; /* We've got our insertion point. */
		}
	}

	insert_stream(autocomp_buff,
						ldm, /* line descriptor */
						m,   /* line number (for undo, which we don't need) */
						0,   /* position in the line */
						str, /* is not \0-terminated, so we really need len */
						len);
	insert_stream(autocomp_buff,
						ldm,
						m,
						len,
						postfix,
						strlen(postfix)+1);
	assert_buffer(autocomp_buff);
}

void search_buff(const buffer *b, const unsigned char *p, int *max_len, int case_search, const unsigned char *postfix) {
	line_desc *ld = (line_desc *)b->line_desc_list.head, *next;
	int p_len = strlen(p);
	int l, r;
	
	while(next = (line_desc *)ld->ld_node.next) {
		l = r = 0;
		do {
			/* find left edge of word */
			while (l < ld->line_len - p_len && !ne_isword(get_char(&ld->line[l], b->encoding), b->encoding)) l += get_char_width(&ld->line[l], b->encoding);
			if    (l < ld->line_len - p_len ) {
				/* find right edge of word */
				r = l + get_char_width(&ld->line[l], b->encoding);
				while (r < ld->line_len && ne_isword(get_char(&ld->line[r], b->encoding), b->encoding)) r += get_char_width(&ld->line[r], b->encoding);
				if (r - l >= p_len && p_len == 0 || !(case_search ? strncmp : strncasecmp)(p, &ld->line[l], p_len)) {
					add_to_autocomp(&ld->line[l], r - l, postfix);
					if ( *max_len < r - l ) *max_len = r - l;
				}
				l = r;
			}
		} while (l < ld->line_len - p_len);
		
		ld = next;
	}
}

/*	autocomplete takes a prefix string (which it is expected to free()), and
	returns an expanded string (which the caller is expected to free()).

		We search through all the document buffers for any "word" sequences which
	start on a word boundary and begin with the prefix (i.e. "\bprefix\w*").

		All the unique strings are stored, sorted, in the autocomp_buff buffer.
	Those that came from the current buffer will have a space on the right end;
	others will have a '*'. These are then presented to the user by calling
	request_strings(). */

unsigned char *autocomplete(unsigned char *p) {
	unsigned char *cp;
	unsigned char **entries;
	line_desc *ld;
	buffer *b = (buffer *)buffers.head;
	int max_len = 0, num_entries, i;
	
	init_autocomp();

	search_buff(cur_buffer, p, &max_len, cur_buffer->opt.case_search, "  ");
	while (b->b_node.next) {
		if (b != cur_buffer)
			search_buff(b, p, &max_len, cur_buffer->opt.case_search, " *");
		b = (buffer *)b->b_node.next;
	}

	/*	N.B.: The delete_one_char() calls below remove the first char of the
		two-char prefixes from each line in autocomp_buff, so for example
		  "matched_word *"
		becomes
		  "matched_word*\0"
		in the char_pool. As request_strings() expect entries to be
		NULL-terminated strings, it is imperative that no other changes be
		made before the call to request_strings() so that these free characters
		in the pool are not reused. Yes, this is a bit of a hack.*/
	ld = (line_desc *)autocomp_buff->line_desc_list.head;
	i = 0;
	while (ld->ld_node.next) {
		if (ld->line_len > 2 && ld->line[ld->line_len - 2] == ' ')
			delete_one_char(autocomp_buff, ld, i, ld->line_len - 2);
		ld = (line_desc *)ld->ld_node.next;
		i++;
	}

	free(p);
	p = NULL;
	if (autocomp_buff && 
	    (num_entries = autocomp_buff->num_lines - 1 ) > 0 && 
	    (entries = malloc(sizeof(char *) * (num_entries)))) {
		ld = (line_desc *)autocomp_buff->line_desc_list.head;
		for (i=0; i < num_entries; i++) {
			entries[i] = ld->line;
			ld = (line_desc *)ld->ld_node.next;
		}
		if ((i = request_strings((const char * const *)entries, num_entries, 0, max_len, '*')) != ERROR) {
			cp = entries[i >= 0 ? i : -i - 2];
			if (p = malloc(strlen(cp) + 1)) {
				strncpy(p,cp,strlen(cp) + 1);
				if (strlen(p) > 0) p[strlen(p) - 1] = '\0'; /* Get rid of the trailing '*' or ' '. */
			}
		}
		free(entries);
	}
	close_autocomp();
	return p;
}
