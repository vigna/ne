/* Various editing functions such as word wrap, to upper, etc.

	Copyright (C) 1993-1998 Sebastiano Vigna
	Copyright (C) 1999-2011 Todd M. Lewis and Sebastiano Vigna

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

/* The number of type of brackets we recognize. */

#define NUM_BRACKETS 4

/* Applies a given to_first() function to the first letter of the starting at the cursor, and to_rest() to
	the following alphabetical letter (see the functions below). */

static int to_something(buffer *b, int (to_first)(int), int (to_rest)(int)) {

	int c, new_c, x, pos = b->cur_pos, len, new_len, changed = FALSE;
	unsigned char *word;

	assert_buffer(b);

	/* If we are after the end of the line, just return ERROR. */

	if (b->cur_line == b->num_lines -1 && b->cur_pos >= b->cur_line_desc->line_len) return ERROR;

	/* First of all, we search for the word start, if we're not over it. */
	if (pos >= b->cur_line_desc->line_len || !ne_isword(c = get_char(&b->cur_line_desc->line[pos], b->encoding), b->encoding)) search_word(b, 1);
	x = b->cur_x; /* The original x position (to perform the line update). */

	pos = b->cur_pos;
	new_len = 0;
	/* Then, we compute the word position extremes, length of the result (which
		may change because of casing). */
	while (pos < b->cur_line_desc->line_len && ne_isword(c = get_char(&b->cur_line_desc->line[pos], b->encoding), b->encoding)) {
		new_c = new_len ? to_rest(c) : to_first(c);
		changed |= (c != new_c);

		if (b->encoding == ENC_UTF8) new_len += utf8seqlen(new_c);
		else new_len++;

		pos = next_pos(b->cur_line_desc->line, pos, b->encoding);
	}

	if (!(len = pos - b->cur_pos)) {
		char_right(b);
		return OK;
	}

	if (changed) {
		/* We actually perform changes only if some character was case folded. */

		if (!(word = malloc(new_len * sizeof *word))) return OUT_OF_MEMORY;

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
		insert_stream(b, b->cur_line_desc, b->cur_line, b->cur_pos, word, new_len);

		free(word);

		end_undo_chain(b);
	}

	b->attr_len = -1;
	update_line(b, b->cur_y, FALSE, FALSE);
	if (b->syn) {
		need_attr_update = TRUE;
		update_syntax_states(b, b->cur_y, b->cur_line_desc, NULL);
	}

	search_word(b, 1);

	return OK;
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

	static unsigned char bracket_table[NUM_BRACKETS][2] = { { '(',')' }, { '[',']' }, { '{','}' }, { '<','>' } };

	int i, j, n, y, dir, pos;
	line_desc *ld = b->cur_line_desc;
	const unsigned char *line;

	if (b->cur_pos >= ld->line_len) return NOT_ON_A_BRACKET;

	for(i = 0; i < NUM_BRACKETS; i++) {
		for(j = 0; j < 2; j++)
			if (ld->line[b->cur_pos] == bracket_table[i][j]) break;
		if (j < 2) break;
	}

	if (i == NUM_BRACKETS && j == 2) return NOT_ON_A_BRACKET;

	if (j) dir = -1;
	else dir = 1;

	n = 0;
	pos = b->cur_pos;
	y = b->cur_line;

	while(ld->ld_node.next && ld->ld_node.prev) {

		if (pos >= 0) {
			line = ld->line;
			while(pos >= 0 && pos < ld->line_len) {

				if (line[pos] == bracket_table[i][j]) n++;
				else if (line[pos] == bracket_table[i][1 - j]) n--;

				if (n == 0) {
					goto_line(b, y);
					goto_pos(b, pos);
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
	position (i.e., at a tab or at a space). The space is deleted, and a new
	line is inserted. The cursor is repositioned coherently.  The number of
	characters existing on the new line is returned, or ERROR if no word wrap was
	possible. */

int word_wrap(buffer * const b) {
	const int len = b->cur_line_desc->line_len;
	unsigned char * const line = b->cur_line_desc->line;
	int cur_pos, pos, first_pos;

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


/* These functions reformat a paragraph while preserving appropriate
leading US-ASCII white space. The strategy is as follows:

  1. Establish appropriate leading space. This will be taken from the line
     following the current line if it is non-blank. Otherwise it will be
     taken from the current line. Save a copy of it for later as space[].

  2. Start an undo chain.

  3. Trim trailing space off the current line.

  4. while the current line is too long (i.e., needs to be split:
       4.1  Find the split point
       4.2  Remove any space at the split point
       4.3  Split the line at the split point.  (We are done with this line)
       4.4  Make the new line the current line
       4.5  Insert the space[] stream we saved in step 1.
       4.6  Trim trailing space off the end of the line.

  5. If the _following_ line is part of this paragraph (i.e., its first
     non-blank character is in the correct position):
       5.1  Add a space to the end of the current line.
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

static char *pa_space	  = NULL;  /* Where we keep space for paragraph left offsets */
static int	pa_space_len;			/* How long pa_space is when tabs are expanded */
static int	pa_space_pos;			/* How long pa_space is without expanding tabs */

/* save_space() sets pa_space, pa_space_len, and pa_space_pos to reflect the
	space on the left end of the line ld refers to in the context of the given
	tab size. If the line contains only space then it is treated identically to
	an empty line, in which case save_space() returns 0 and pa_space,
	pa_space_len, and pa_space_pos are cleared. Otherwise it returns 1. The
	string pa_space points to is not null-terminated, so be careful how you use it. */

static int save_space(line_desc * const ld, const int tab_size, const encoding_type encoding) {
	int pos;
	if (pa_space) free(pa_space);
	pa_space	  = NULL;
	pa_space_len = 0;
	pa_space_pos = 0;

	if (!ld->line) return 0; /* No data on this line. */

	pos = 0;
	while(pos < ld->line_len && isasciispace(ld->line[pos])) pos = next_pos(ld->line, pos, encoding);

	if (pos == ld->line_len) return 0; /* Blank lines don't count. */

	pa_space_pos = pos;
	pa_space_len = calc_width(ld, pos, tab_size, encoding);

	if (pos == 0) {
		pa_space = NULL;
		return 1;
	}

	if ((pa_space = malloc(pos))) {
		memcpy(pa_space, ld->line, pos);
		return 1;
	}
	return 0;
}


/* trim_trailing_space() removes spaces from the end of the line referred to by
	the line_desc ld. The int line is necessary if you want to be able to undo
	later. */

static void trim_trailing_space(buffer * const b, line_desc *ld, const int line, const encoding_type encoding) {
	int pos;
	if (!ld->line) return;
	insert_one_char(b, ld, line, ld->line_len, ' '); /* Make sure there's a space on the end and not a UTF8
                                                       so the prev_pos() below won't go berzerk. */
	pos = ld->line_len;
	while(pos > 0 && isasciispace(ld->line[pos - 1])) pos = prev_pos(ld->line, pos, encoding);

	if (pos >= 0 && pos < ld->line_len) delete_stream(b, ld, line, pos, ld->line_len - pos);
}

/* is_part_of_paragraph() determines if the line ld refers to could be
	considered part of a paragraph based on its leading spaces compared to
	pa_space_len. If they are the same, is_part_of_paragraph() returns 1, and
	*first_non_blank is set to the position of the first non-blank character on
	the line. Otherwise, *first_non_blank is -1 and is_part_of_paragraph()
	returns 0. */

static int is_part_of_paragraph(const line_desc * const ld, const int tab_size, int * const first_non_blank, const encoding_type encoding) {
	int pos = 0;
	*first_non_blank = -1;
	if (!ld->line) return 0;
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
	int pos,
		done,
		line			= b->cur_line,
		right_margin = b->opt.right_margin ? b->opt.right_margin : ne_columns;
	line_desc *ld = b->cur_line_desc, *start_line_desc = ld;

	if (!ld->line)	return line_down(b);

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
	insert_one_char(b, ld, line, 0, ' ');
	delete_stream(b, ld, line, 0, 1);

	done = FALSE;
	do {
		/** Step 3 **/
		trim_trailing_space(b, ld, line, b->encoding);

		/** Step 4 **/
		while (!done && calc_width(ld, ld->line_len, b->opt.tab_size, b->encoding) > right_margin) {
			int spaces;
			int split_pos;
			int did_split;

			/** 4.1  Find the split point **/

			pos = 0;   /* Skip past leading spaces. */
			while(pos < ld->line_len && isasciispace(ld->line[pos]))
			  pos = next_pos(ld->line, pos, b->encoding);

			did_split = split_pos = spaces = 0;

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
				did_split = 1;
			}

			/** 4.4  Make the (new?) next line the current line **/
			if (ld->ld_node.next->next) {
				ld = (line_desc *)ld->ld_node.next;
				line++;

				/** 4.5  Insert the ps_space[] stream we saved in step 1. Note that  **/
				/** we only want to do this if this line is the result of a split,	**/
				/** which is true if did_split != 0.											**/

				if (did_split && pa_space && pa_space_len) {
					insert_stream(b, ld, line, 0, pa_space, pa_space_pos);
				}

				/** 4.6  Trim trailing space off the end of the line. **/
				trim_trailing_space(b, ld, line, b->encoding);
			}
			else done = TRUE;
		}

		/** 5. If the _following_ line is part of this paragraph (i.e., its first **/
		/**	 non-blank character is in the correct position):						 **/

		if (ld->ld_node.next->next && is_part_of_paragraph((line_desc *)ld->ld_node.next, b->opt.tab_size, &pos, b->encoding)) {
			/** 5.1  Add a space to the end of the current line.							 **/
			insert_one_char(b, ld, line, ld->line_len, ' ');

			/** 5.2  Move following line's data starting with the first					**/
			/** non-blank to the end of the current line.								**/
			/**  We do this by first deleting the leading spaces, then we				 **/
			/**  delete the newline at the end of the current line.						 **/
			if (pos > 0) delete_stream(b, (line_desc *)ld->ld_node.next, line + 1, 0, pos);
			delete_stream(b, ld, line, ld->line_len, 1);
		}
		else done = TRUE;
	} while (!done);

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
		need_attr_update = TRUE;
		update_syntax_states(b, -1, start_line_desc, (line_desc *)ld->ld_node.next);
	}
	update_window_lines(b, b->cur_y, ne_lines - 2, FALSE);

	/** Step 9 **/
	goto_line(b, line);
	if (line_down(b) == ERROR) return ERROR;

	/* Try to find the first non-blank starting with this line. */
	ld	= b->cur_line_desc;
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

	int
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

int auto_indent_line(buffer * const b, const int line, line_desc * const ld, const int up_to_col) {

	line_desc * const prev_ld = (line_desc *)ld->ld_node.prev;
	int pos = 0, col = 0, c;

	assert(prev_ld->ld_node.prev != NULL);
	assert_line_desc(prev_ld, b->encoding);

	if (prev_ld->line_len == 0) return 0;

	while(pos < prev_ld->line_len && ne_isspace(c = get_char(&prev_ld->line[pos], b->encoding), b->encoding)) {
		col += (c == '\t' ? b->opt.tab_size - col % b->opt.tab_size : 1);
		if (col > up_to_col) break;
		pos = next_pos(prev_ld->line, pos, b->encoding);
	}
	insert_stream(b, ld, line, 0, prev_ld->line, pos);
	return pos;
}
