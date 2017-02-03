/* Various editing functions such as word wrap, to upper, etc.

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

/* The number of type of brackets we recognize. */
#define NUM_BRACKETS 5

/* Applies a given to_first() function to the first letter of the text starting at the cursor,
   and to_rest() to the following alphabetical letters (see the functions below). */

static int to_something(buffer *b, int (to_first)(int), int (to_rest)(int)) {
	assert_buffer(b);

	/* If we are after the end of the line, just return ERROR. */

	if (b->cur_line == b->num_lines -1 && b->cur_pos >= b->cur_line_desc->line_len) return ERROR;

	int64_t pos = b->cur_pos;
	int c;
	/* First of all, we search for the word start, if we're not over it. */
	if (pos >= b->cur_line_desc->line_len || !ne_isword(c = get_char(&b->cur_line_desc->line[pos], b->encoding), b->encoding))
		if (search_word(b, 1) != OK)
			return ERROR;

	bool changed = false;
	int64_t new_len = 0;
	pos = b->cur_pos;
	/* Then, we compute the word position extremes, length of the result (which
		may change because of casing). */
	while (pos < b->cur_line_desc->line_len && ne_isword(c = get_char(&b->cur_line_desc->line[pos], b->encoding), b->encoding)) {
		const int new_c = new_len ? to_rest(c) : to_first(c);
		changed |= (c != new_c);

		if (b->encoding == ENC_UTF8) new_len += utf8seqlen(new_c);
		else new_len++;

		pos = next_pos(b->cur_line_desc->line, pos, b->encoding);
	}

	const int64_t len = pos - b->cur_pos;
	if (!len) {
		char_right(b);
		return OK;
	}

	if (changed) {
		/* We actually perform changes only if some character was case folded. */
		char * word = malloc(new_len * sizeof *word);
		if (!word) return OUT_OF_MEMORY;

		pos = b->cur_pos;
		new_len = 0;
		/* Second pass: we actually build the transformed word. */
		while (pos < b->cur_line_desc->line_len && ne_isword(c = get_char(&b->cur_line_desc->line[pos], b->encoding), b->encoding)) {
			if (b->encoding == ENC_UTF8) new_len += utf8str(new_len ? to_rest(c) : to_first(c), word + new_len);
			else {
				word[new_len] = new_len ? to_rest(c) : to_first(c);
				new_len++;
			}

			pos = next_pos(b->cur_line_desc->line, pos, b->encoding);
		}

		start_undo_chain(b);

		delete_stream(b, b->cur_line_desc, b->cur_line, b->cur_pos, len);
		if (new_len) insert_stream(b, b->cur_line_desc, b->cur_line, b->cur_pos, word, new_len);

		free(word);

		end_undo_chain(b);
	}

	b->attr_len = -1;
	update_line(b, b->cur_y, false, false);
	if (b->syn) {
		need_attr_update = true;
		update_syntax_states(b, b->cur_y, b->cur_line_desc, NULL);
	}

	return search_word(b, 1);
}



/* These functions upper case, lower case or capitalize the word the cursor is
   on. They just call to_something(). Note the parentheses around the function
   names, which inhibit the possible macros. */

int to_upper(buffer *b) {
	return b->encoding == ENC_UTF8 ? to_something(b, (utf8toupper), (utf8toupper)) : to_something(b, (toupper), (toupper));
}

int to_lower(buffer *b) {
	return b->encoding == ENC_UTF8 ? to_something(b, (utf8tolower), (utf8tolower)) : to_something(b, (tolower), (tolower));
}

int capitalize(buffer *b) {
	return b->encoding == ENC_UTF8 ? to_something(b, (utf8toupper), (utf8tolower)) : to_something(b, (toupper), (tolower));
}





/* Finds which bracket matches the bracket under the cursor, and moves it
   there. Various error codes can be returned. */

int match_bracket(buffer *b) {
	int64_t match_line, match_pos;
	const int rc = find_matching_bracket(b, 0, b->num_lines-1, &match_line, &match_pos, NULL, NULL);
	if (rc == OK) {
		goto_line(b, match_line);
		goto_pos(b, match_pos);
		return OK;
	}
	return rc;
}

int find_matching_bracket(buffer *b, const int64_t min_line, int64_t max_line, int64_t *match_line, int64_t *match_pos, int *c, line_desc ** match_ld) {

	static unsigned char bracket_table[NUM_BRACKETS][2] = { { '(', ')' },
																			  { '[', ']' },
																			  { '{', '}' },
																			  { '<', '>' },
																			  { '`', '\'' } };

	line_desc *ld = b->cur_line_desc;

	if (b->cur_pos >= ld->line_len) return NOT_ON_A_BRACKET;

	int i, j, dir;
	for(i = 0; i < NUM_BRACKETS; i++) {
		for(j = 0; j < 2; j++)
			if (ld->line[b->cur_pos] == bracket_table[i][j]) break;
		if (j < 2) break;
	}

	if (i == NUM_BRACKETS && j == 2) return NOT_ON_A_BRACKET;

	if (j) dir = -1;
	else dir = 1;

	int n = 0;
	int64_t pos = b->cur_pos, y = b->cur_line;

	while(ld->ld_node.next && ld->ld_node.prev && y >= min_line && y <= max_line) {

		if (pos >= 0) {
			char * const line = ld->line;
			while(pos >= 0 && pos < ld->line_len) {

				if (line[pos] == bracket_table[i][j]) n++;
				else if (line[pos] == bracket_table[i][1 - j]) n--;

				if (n == 0) {
					*match_line = y;
					*match_pos  = pos;
					if (c) *c = line[pos];
					if (match_ld) *match_ld = ld;
					return OK;
				}
				if (dir > 0) pos = next_pos(line, pos, b->encoding);
				else pos = prev_pos(line, pos, b->encoding);
			}
		}

		pos = -1;

		if (dir == 1) {
			ld = (line_desc *)ld->ld_node.next;
			if (ld->ld_node.next && ld->line) pos = 0;
			y++;
		}
		else {
			ld = (line_desc *)ld->ld_node.prev;
			if (ld->ld_node.prev && ld->line) pos = ld->line_len - 1;
			y--;
		}
	}

	return CANT_FIND_BRACKET;
}



/* Breaks a line at the first possible position before the current cursor
   position (i.e., at a tab or at a space). The space is deleted, and a
   new line is inserted. The cursor is repositioned coherently. The number
   of characters existing on the new line is returned, or ERROR if no word
   wrap was possible. */

int word_wrap(buffer * const b) {
	const int64_t len = b->cur_line_desc->line_len;
	char * const line = b->cur_line_desc->line;
	int64_t cur_pos, pos, first_pos;

	if (!(cur_pos = pos = b->cur_pos)) return ERROR;

	/* Find the first possible position we could break a line on. */
	first_pos = 0;

	/* Skip leading white space */
	while (first_pos < len && ne_isspace(get_char(&line[first_pos], b->encoding), b->encoding)) first_pos = next_pos(line, first_pos, b->encoding);

	/* Skip non_space after leading white space */
	while (first_pos < len && !ne_isspace(get_char(&line[first_pos], b->encoding), b->encoding)) first_pos = next_pos(line, first_pos, b->encoding);

	/* Now we know that the line shouldn't be broken before &line[first_pos]. */

	/* Start from the other end now and find a candidate space to break the line on.*/
	while((pos = prev_pos(line, pos, b->encoding)) && !ne_isspace(get_char(&line[pos], b->encoding), b->encoding));

	if (! pos || pos < first_pos) return ERROR;

	start_undo_chain(b);

	delete_one_char(b, b->cur_line_desc, b->cur_line, pos);
	insert_one_line(b, b->cur_line_desc, b->cur_line, pos);

	end_undo_chain(b);

	return b->cur_pos - pos - 1;
}



/* This experimental alternative to word wrapping sets a bookmark, calls
   paragraph(), then returns to the bookmark (which may have moved due to
   insertions/deletions).  The number of characters existing on the new line is
   returned, or ERROR if no word wrap was possible. */

int word_wrap2(buffer * const b) {
	static char avcmd[16];

	if (b->cur_pos > b->cur_line_desc->line_len) return OK;

	bool non_blank_added = false;
	int avshift;
	char * line = b->cur_line_desc->line;
	int64_t pos, original_line;

	/* If the char to our left is a space, we need to insert
		a non-space to attach our WORDWRAP_BOOKMARK to because
		spaces at the split point get removed, which effectively
		leaves our bookmark on the current line. */
	delay_update();
	pos = prev_pos(line, b->cur_pos, b->encoding);
	if (pos >= 0 && (non_blank_added = ne_isspace(get_char(&line[pos], b->encoding), b->encoding))) {
		start_undo_chain(b);
		insert_one_char(b, b->cur_line_desc, b->cur_line, b->cur_pos, 'X');
		line = b->cur_line_desc->line;
		goto_pos(b, next_pos(line, b->cur_pos, b->encoding));
	}
	b->bookmark[WORDWRAP_BOOKMARK].pos	= b->cur_pos;
	b->bookmark[WORDWRAP_BOOKMARK].line  = original_line = b->cur_line;
	b->bookmark[WORDWRAP_BOOKMARK].cur_y = b->cur_y;
	b->bookmark_mask |= (1 << WORDWRAP_BOOKMARK);
	paragraph(b);
	goto_line(b, b->bookmark[WORDWRAP_BOOKMARK].line);
	goto_pos( b, b->bookmark[WORDWRAP_BOOKMARK].pos);
	line = b->cur_line_desc->line;
	b->bookmark[WORDWRAP_BOOKMARK].cur_y += b->bookmark[WORDWRAP_BOOKMARK].line - original_line;
	if (avshift = b->cur_y - b->bookmark[WORDWRAP_BOOKMARK].cur_y) {
		snprintf(avcmd, 16, "%c%d", avshift > 0 ? 'T' :'B', avshift > 0 ? avshift : -avshift);
		adjust_view(b,avcmd);
	}
	b->bookmark_mask &= ~(1 << WORDWRAP_BOOKMARK);
	if (non_blank_added) {
		goto_pos(b, prev_pos(b->cur_line_desc->line, b->cur_pos, b->encoding));
		delete_one_char(b, b->cur_line_desc, b->cur_line, b->cur_pos);
		end_undo_chain(b);
	}
	return stop ? STOPPED : OK;
}

/* These functions reformat a paragraph while preserving appropriate
leading US-ASCII white space. The strategy is as follows:

  1. Establish appropriate leading space. This will be taken from the line
	  following the current line if it is non-blank. Otherwise it will be
	  taken from the current line. Save a copy of it for later as space[].

  1.1 If the leading non-blank (the part after space[]) is not an alphanumeric,
	  then we take it as a comment initiator and preserve it for later as spots[].

  2. Start an undo chain.

  3. Trim trailing space off the current line.

  4. while the current line is too long (i.e., needs to be split:
		 4.1  Find the split point
		 4.2  Remove any space at the split point
		 4.3  Split the line at the split point.  (We are done with this line)
		 4.4  Make the new line the current line
		 4.5  Insert the space[] stream we saved in step 1.
		 4.5.1 Insert the spots[] stream we saved from step 1.1.
		 4.6  Trim trailing space off the end of the line.

  5. If the _following_ line is part of this paragraph (i.e., its first
	  non-blank character is in the correct position):
		 5.1  Add a space to the end of the current line.
		 5.2  Delete this line's leading white space.
		 5.3  If the leading non-blank character matches the first character
				of spots[], remove it any any subsequent non-alphanumeric.
		 5.2  Copy following line's data starting with the first
				non-blank to the end of the current line.
		 5.3  Remove the following line.
		 5.4  Goto step 3.

  6. end the undo chain.

  7. Free space[].

  8. and refresh the screen

  9. move to the next non-blank after the current line. (We have to do
	  this so that commands like "Paragraph 5" will do 5 paragraphs
	  instead of only three.)

*/

static char *pa_spots = NULL;    /* Where we keep leading non-alphanumerics */
static int pa_spots_pos;         /* How long pa_spots is in chars */

/* save_spots() is like save_space(), but it preserves the string of non-alphanumerics
   immediately follow where save_space() left off. */

static int save_spots(line_desc * const ld, const int pos, const encoding_type encoding) {
	if (pa_spots) free(pa_spots);
	pa_spots  = NULL;
	pa_spots_pos = 0;

	if (!ld->line || ld->line_len <= pos) return 0; /* No data on this line. */

	while(pos + pa_spots_pos < ld->line_len && isparaspot(ld->line[pos+pa_spots_pos]))
		pa_spots_pos = next_pos(ld->line, pos+pa_spots_pos, encoding) - pos;
	if (pa_spots_pos) {
		if ((pa_spots = malloc(pa_spots_pos)))	memcpy(pa_spots, ld->line+pos, pa_spots_pos);
		else pa_spots_pos = 0;
	}
	return pa_spots_pos > 0;
}

static char   *pa_space;     /* Where we keep space for paragraph left offsets */
static int64_t pa_space_len; /* How long pa_space is when tabs are expanded */
static int64_t pa_space_pos; /* How long pa_space is without expanding tabs */

/* save_space() sets pa_space, pa_space_len, and pa_space_pos to reflect the
   space on the left end of the line ld refers to in the context of the given
   tab size. If the line contains only space then it is treated identically to
   an empty line, in which case save_space() returns 0 and pa_space,
   pa_space_len, and pa_space_pos are cleared. Otherwise it returns 1. The
   string pa_space points to is not null-terminated, so be careful how you use it. */

static int save_space(line_desc * const ld, const int tab_size, const encoding_type encoding) {
	if (pa_space) free(pa_space);
	pa_space  = NULL;
	pa_space_len = 0;
	pa_space_pos = 0;

	if (!ld->line) return 0; /* No data on this line. */

	int64_t pos = 0;
	while(pos < ld->line_len && isasciispace(ld->line[pos])) pos = next_pos(ld->line, pos, encoding);

	if (pos == ld->line_len) return 0; /* Blank lines don't count. */

	pa_space_pos = pos;
	pa_space_len = calc_width(ld, pos, tab_size, encoding);

	if (pos == 0) {
		pa_space = NULL;
		save_spots(ld, pos, encoding);
		return 1;
	}

	if ((pa_space = malloc(pos))) {
		memcpy(pa_space, ld->line, pos);
		save_spots(ld, pos, encoding);
		return 1;
	}

	return 0;
}


/* trim_trailing_space() removes spaces from the end of the line referred to by
   the line_desc ld. The int line is necessary if you want to be able to undo
   later. */

static void trim_trailing_space(buffer * const b, line_desc *ld, const int64_t line, const encoding_type encoding) {
	if (!ld->line) return;
	int64_t pos = ld->line_len;
	while (pos > 0 && isasciispace(ld->line[pos - 1])) pos = prev_pos(ld->line, pos, encoding);
	if (pos >= 0 && pos < ld->line_len) delete_stream(b, ld, line, pos, ld->line_len - pos);
}

/* is_part_of_paragraph() determines if the line ld refers to could be
   considered part of a paragraph based on its leading spaces compared to
   pa_space_len. If they are the same, is_part_of_paragraph() returns 1, and
   *first_non_blank is set to the position of the first non-blank character on
   the line. Otherwise, *first_non_blank is -1 and is_part_of_paragraph()
   returns 0. */

static int is_part_of_paragraph(const line_desc * const ld, const int tab_size, int64_t * const first_non_blank, const encoding_type encoding) {
	*first_non_blank = -1;
	if (!ld->line) return 0;

	int64_t pos = 0;
	while (pos < ld->line_len && isasciispace(ld->line[pos])) pos = next_pos(ld->line, pos, encoding);
	if (pos < ld->line_len && calc_width(ld, pos, tab_size, encoding) == pa_space_len) {
		*first_non_blank = pos;
		return 1;
	}
	return 0;
}

/* paragraph() reformats a paragraph following the current parameters for
   right_margin (a value of 0 forces the use of the full screen width).  On
   completion the cursor is positioned either:

  * on the first non-blank character after the paragraph if there is one, or

  * on a blank line following the paragraph if there is one, or

  * on the last line of the paragraph.

  paragraph() returns OK unless the cursor ends up on the last line of the
  file, in which case it returns ERROR. */

int paragraph(buffer * const b) {
	line_desc *ld = b->cur_line_desc, *start_line_desc = ld;

	if (!ld->line) return line_down(b);

	/** Step 1 **/
	if (!(
			 (ld->ld_node.next->next &&
				save_space((line_desc *)ld->ld_node.next, b->opt.tab_size, b->encoding)
				)
			 || save_space(ld, b->opt.tab_size, b->encoding)
			)
		 ) return line_down(b);


	/** Step 2 **/
	start_undo_chain(b);

	/* This useless insertion and deletion of a single character ensures
		that the text isn't shifted way over to the left after an undo. */
	int64_t line = b->cur_line;
	insert_one_char(b, ld, line, 0, ' ');
	delete_stream(b, ld, line, 0, 1);

	const int right_margin = b->opt.right_margin ? b->opt.right_margin : ne_columns;
	bool done = false, skip;
	int64_t pos;
	do {
		/** Step 3 **/
		trim_trailing_space(b, ld, line, b->encoding);

		/** Step 4 **/
		while (!stop && !done && calc_width(ld, ld->line_len, b->opt.tab_size, b->encoding) > right_margin) {
			int64_t spaces;
			int64_t split_pos;
			bool did_split;

			/** 4.1  Find the split point **/

			pos = 0;	/* Skip past leading spaces... */
			while(pos < ld->line_len && isasciispace(ld->line[pos]))
			  pos = next_pos(ld->line, pos, b->encoding);
						  /* ...and the invariants if any. */
			while(pos < ld->line_len && isparaspot(ld->line[pos]))
			  pos = next_pos(ld->line, pos, b->encoding);

			did_split = false;
			split_pos = spaces = 0;

			while (pos < ld->line_len &&
					(calc_width(ld, pos, b->opt.tab_size, b->encoding) < right_margin ||
					  ! split_pos)) {
				if (isasciispace(ld->line[pos])) {
					split_pos = pos;
					spaces = 0;
					while (pos < ld->line_len && isasciispace(ld->line[pos])) {
						pos = next_pos(ld->line, pos, b->encoding);
						spaces++;
					}
				}
				else pos = next_pos(ld->line, pos, b->encoding);
			}

			if (split_pos) {
				/** 4.2  Remove any space at the split point. **/
				if (spaces) delete_stream(b, ld, line, split_pos, spaces);
				/** 4.3  Split the line at the split point.  (We are done with this line) **/
				insert_one_line(b, ld, line, split_pos);
				did_split = true;
			}

			/** 4.4  Make the (new?) next line the current line **/
			if (ld->ld_node.next->next) {
				ld = (line_desc *)ld->ld_node.next;
				line++;

				/** 4.5  Insert the pa_space[] stream we saved in step 1. Note that  **/
				/** we only want to do this if this line is the result of a split,	**/
				/** which is true if did_split is true.                              **/

				if (did_split) {
					if (pa_space && pa_space_len && pa_space_pos)
						insert_stream(b, ld, line, 0, pa_space, pa_space_pos);
					/** 4.5.1 Insert the pa_spots[] stream if there is one. **/
					if (pa_spots && pa_spots_pos)
						insert_stream(b, ld, line, pa_space_pos, pa_spots, pa_spots_pos);
				}

				/** 4.6  Trim trailing space off the end of the line. **/
				trim_trailing_space(b, ld, line, b->encoding);
			}
			else done = true;
		}

		/** If the current line is just a spot (no text), we skip over it. **/
		pos = 0;   /* Skip past leading spaces... */
		skip = false;
		while(pos < ld->line_len && isasciispace(ld->line[pos]))
			pos = next_pos(ld->line, pos, b->encoding);
					  /* ...and the invariants if any. */
		while(pos < ld->line_len && isparaspot(ld->line[pos]))
			pos = next_pos(ld->line, pos, b->encoding);
		if (pos == ld->line_len) skip = true;

		/** 5. If the _following_ line is part of this paragraph (i.e., its first  **/
		/**     non-blank character is in the correct position):                   **/

		if (ld->ld_node.next->next && is_part_of_paragraph((line_desc *)ld->ld_node.next, b->opt.tab_size, &pos, b->encoding)) {
			/** If the next line is just a spot (no text), we want to skip over it  **/
			/** rather than splicing it to the current line.                        **/
			if (skip || save_spots((line_desc *)ld->ld_node.next, pos, b->encoding) && ((line_desc *)ld->ld_node.next)->line_len <= pos+pa_spots_pos) {
				/* skip to next line */
				ld = (line_desc *)ld->ld_node.next;
				line++;
			}
			else {
				/** 5.1  Add a space to the end of the current line.                    **/
				insert_one_char(b, ld, line, ld->line_len, ' ');

				/** 5.4  Move following line's data starting with the first             **/
				/** non-blank to the end of the current line.                           **/

				/**  We do this by first deleting the leading spaces                    **/
				if (pos > 0) delete_stream(b, (line_desc *)ld->ld_node.next, line + 1, 0, pos);
				/** 5.2 Cache the leading non-alphanumeric in pa_spots, then delete it,  **/
				if (save_spots((line_desc *)ld->ld_node.next, 0, b->encoding))
					delete_stream(b, (line_desc *)ld->ld_node.next, line + 1, 0, pa_spots_pos);
				/**  Finally splice the lines by deleting the newline at the end of the current line.                 **/
				delete_stream(b, ld, line, ld->line_len, 1);
			}
		}
		else done = true;
	} while (!done && !stop);

	/** Step 6 **/
	end_undo_chain(b);

	/** Step 7 **/
	if (pa_space) {
		free(pa_space);
		pa_space = NULL;
	}

	/** Step 8 **/
	if (b->syn) {
		b->attr_len = -1;
		need_attr_update = true;
		update_syntax_states(b, -1, start_line_desc, (line_desc *)ld->ld_node.next);
	}
	update_window_lines(b, b->cur_y, ne_lines - 2, false);

	/** Step 9 **/
	goto_line(b, line);
	if (stop || line_down(b) == ERROR) return stop ? STOPPED : ERROR;

	/* Try to find the first non-blank starting with this line. */
	ld = b->cur_line_desc;
	line = b->cur_line;

	do {
		if (ld->line) {
			for (pos = 0; pos < ld->line_len; pos = next_pos(ld->line, pos, b->encoding)) {
				if (!isasciispace(ld->line[pos])) {
					goto_line(b, line);
					goto_pos(b, pos);
					return ld->ld_node.next ? OK : ERROR;
				}
			}
		}
		ld = (line_desc *)ld->ld_node.next;
		line++;
	} while (ld->ld_node.next);

	return b->cur_line_desc->ld_node.next ? OK : ERROR;
}


/* Centers the current line with respect to the right_margin parameter. If the
   line (without spaces) is longer than the right margin, nothing happens. */

int center(buffer * const b) {

	line_desc * const ld = b->cur_line_desc;
	const int right_margin = b->opt.right_margin ? b->opt.right_margin : ne_columns;

	int64_t
		len,
		start_pos = 0,
		end_pos = ld->line_len;

	while(start_pos < ld->line_len && isasciispace(ld->line[start_pos])) start_pos = next_pos(ld->line, start_pos, b->encoding);
	if (start_pos == ld->line_len) return OK;
	while(isasciispace(ld->line[prev_pos(ld->line, end_pos, b->encoding)])) end_pos = prev_pos(ld->line, end_pos, b->encoding);

	len = b->encoding == ENC_UTF8 ? utf8strlen(&ld->line[start_pos], end_pos - start_pos) : end_pos - start_pos;
	if (len >= right_margin) return OK;

	start_undo_chain(b);

	delete_stream(b, ld, b->cur_line, end_pos, ld->line_len - end_pos);
	delete_stream(b, ld, b->cur_line, 0, start_pos);
	insert_spaces(b, ld, b->cur_line, 0, (right_margin - len) / 2);

	end_undo_chain(b);

	return OK;
}



/* Indents a line of the amount of whitespace present on the previous line, stopping
   at a given column (use INT_MAX for not stopping). The number of
   inserted bytes is returned. */

int auto_indent_line(buffer * const b, const int64_t line, line_desc * const ld, const int64_t up_to_col) {

	line_desc * const prev_ld = (line_desc *)ld->ld_node.prev;
	assert_line_desc(prev_ld, b->encoding);

	if (!prev_ld->ld_node.prev || prev_ld->line_len == 0) return 0;

	int c;
	int64_t pos = 0;	
	for(int64_t col = 0; pos < prev_ld->line_len && ne_isspace(c = get_char(&prev_ld->line[pos], b->encoding), b->encoding); ) {
		col += (c == '\t' ? b->opt.tab_size - col % b->opt.tab_size : 1);
		if (col > up_to_col) break;
		pos = next_pos(prev_ld->line, pos, b->encoding);
	}
	if (pos) insert_stream(b, ld, line, 0, prev_ld->line, pos);
	return pos;
}


/* Shift a block of lines left or right with whitespace adjustments. */

int shift(buffer * const b, char *p, char *msg, int msg_size) {
	const bool use_tabs = b->opt.tabs && b->opt.shift_tabs;
	const int64_t init_line = b->cur_line, init_pos = b->cur_pos, init_y = b->cur_y;

	line_desc *ld = NULL, *start_line_desc = NULL;
	int64_t shift_size = 1;
	char dir = '>';
	int shift_mag = b->opt.tab_size, rc = 0;

	/* Parse parm p; looks like [<|>] ### [s|t], but we allow them
	   in any order, once, with optional white space. */
	if (p) {
		int dir_b = 0, size_b = 0, st_b = 0;
		while (*p) {
			if (isasciispace(*p)) p++;
			else if (!dir_b && (dir_b = (*p == '<' || *p == '>'))) dir = *p++;
			else if (!size_b && (size_b = isdigit((unsigned char)*p))) {
				errno = 0;
				shift_size = strtoll(p, &p, 10);
				if (errno) return INVALID_SHIFT_SPECIFIED;
			} else if (!st_b && (st_b = (*p == 's' || *p == 'S'))) {
				shift_mag = 1;
				p++;
			} else if (!st_b && (st_b = (*p == 't' || *p == 'T'))) p++;
			else return INVALID_SHIFT_SPECIFIED;
		}
	}
	shift_size *= max(1, shift_mag);
	if (shift_size == 0) return INVALID_SHIFT_SPECIFIED;

	int64_t first_line = b->cur_line, last_line = b->cur_line, left_col = 0;

	if (b->marking) {
		if (b->mark_is_vertical) left_col = min(calc_width(b->cur_line_desc, b->block_start_pos, b->opt.tab_size, b->encoding),
		                                        calc_width(b->cur_line_desc, b->cur_pos,         b->opt.tab_size, b->encoding));
		first_line = min(b->block_start_line, b->cur_line);
		last_line  = max(b->block_start_line, b->cur_line);
	}

	/* If we're shifting left (dir=='<'), verify that we have sufficient white space
	   to remove on all the relevant lines before making any changes, i. */

	if (dir == '<') {
		shift_size = -shift_size; /* signed shift_size now also indicates direction. */
		for (int64_t line = first_line; !rc && line <= last_line; line++) {
			int64_t pos;
			goto_line(b, line);
			pos = calc_pos(b->cur_line_desc, left_col, b->opt.tab_size, b->encoding);
			while (pos < b->cur_line_desc->line_len &&
			       left_col - calc_width(b->cur_line_desc, pos, b->opt.tab_size, b->encoding) > shift_size) {
				if (isasciispace(b->cur_line_desc->line[pos]))
					pos = next_pos(b->cur_line_desc->line, pos, b->encoding);
				else {
					rc = INSUFFICIENT_WHITESPACE;
					break;
				}
			}
		}
	}

	if (!rc) {
		start_undo_chain(b);
		for (int64_t line = first_line; line <= last_line; line++) {
			int64_t pos, c_pos, c_col_orig, offset;
			b->attr_len = -1;
			goto_line(b,line);
			ld = b->cur_line_desc;
			if (line == first_line) start_line_desc = ld;
			pos = calc_pos(ld, left_col, b->opt.tab_size, b->encoding);
			/* If left_col is in the middle of a tab, pos will be on that tab. */
			/* whitespace adjustment strategy:
			   1. Starting from left_col, advance to the right to the first non-blank character C.
			   2. Note C's col. The desired new column is this value +/- shift_size.
			   3. Move left looking for the first tab or non-whitespace or the left_col, whichever comes first.
			      Whitespace changes all take place at that transition point.
			   4. While C's col is wrong
			        if C's col is too far to the right,
			          if we're on a space, delete it;
			          else if there's a tab to our left, delete it;
			          else we should not have started, because it's not possible!
			        if C's col is too far to the left,
			           if its needs to be beyond the next tab stop,
			             insert a tab and move right;
			           else insert a space. */
			/* 1. */
			while (pos < ld->line_len && isasciispace(ld->line[pos]))
				pos = next_pos(ld->line, pos, b->encoding);
			if (pos >= ld->line_len) continue; /* We ran off the end of the line. */
			/* line[pos] should be the first non-blank character. */ 
			/* 2. */
			c_pos = pos;
			c_col_orig = calc_width(ld, c_pos, b->opt.tab_size, b->encoding);
			/* 3. */
			while (pos && ld->line[pos-1] == ' ')
				pos = prev_pos(ld->line, pos, b->encoding);
			/* If pos is non-zero, it should be on a blank, with only blanks between here and c_pos. */
			/* 4. */
			/* offset = how_far_we_have_moved - how_far_we_want_to_move. */
			while (!stop && (offset = calc_width(ld, c_pos, b->opt.tab_size, b->encoding)-c_col_orig - shift_size)) {
				if (offset > 0) { /* still too far right; remove whitespace */
					if (ld->line[pos] == ' ') {
						delete_stream(b, ld, b->cur_line, pos, 1);
						c_pos--;
					}
					else if (pos) { /* should be a tab just to our left */
						pos = prev_pos(ld->line, pos, b->encoding); /* now we're on the tab */
						if (ld->line[pos] == '\t') {
							delete_stream(b, ld, b->cur_line, pos, 1);
							c_pos--;
						}
						else break; /* Should have been a tab. This should never happen! Give up on this line and go mangle the next one. */
					}
					else break; /* This should never happen; give up on this line and go mangle the next one. */
				}
				else if (offset < 0) { /* too far left; insert whitespace */
					char c = ' ';
					if (use_tabs && (b->opt.tab_size - calc_width(ld, pos, b->opt.tab_size, b->encoding) % b->opt.tab_size) <= -offset )
						c = '\t';
					if (insert_one_char(b, ld, b->cur_line, pos, c)) {
						break;
					}
					pos++;
					c_pos++;
				}
			}
		}
		end_undo_chain(b);
		if (b->syn) {
			b->attr_len = -1;
			need_attr_update = true;
			update_syntax_states(b, -1, start_line_desc, (line_desc *)ld->ld_node.next);
		}
		update_window_lines(b, 0, ne_lines - 2, false);
	}

	/* put the screen back where way we found it. */
	goto_line(b, init_line);
	goto_pos(b, init_pos);
	delay_update();
	const int64_t avshift = b->cur_y - init_y;
	if (avshift) {
		snprintf(msg, msg_size, "%c%" PRId64, avshift > 0 ? 'T' :'B', avshift > 0 ? avshift : -avshift);
		adjust_view(b,msg);
	}

	return rc;
}

