/* Display handling functions with optional update delay

	Copyright (C) 1993-1998 Sebastiano Vigna 
	Copyright (C) 1999-2010 Todd M. Lewis and Sebastiano Vigna

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
	02111-1307, USA. */

#include "ne.h"
#include "cm.h"
#include "termchar.h"


/* The functions in this file act as an interface between the main code and the
	raw screen updating functions of term.c. The basic idea is that one has a
	series of functions which normally just call the basic functions; however,
	if more than turbo (or lines*2, if turbo is zero) lines have been updated,
	the update stops and is delayed to the next call to refresh_window(). This
	function should be called whenever the screen has to be sync'd with its
	contents (for instance, whenever the user gets back in control). The 
	mechanism allows for fast, responsive screen updates for short operations,
	and one-in-all updates for long operations. */


#define TURBO (turbo ? turbo : ne_lines * 2)


/* If true, the current line has changed and care must be taken to update the initial state of the following lines. */

int need_attr_update;

/* If window_needs_refresh, the window has to be refreshed from scratch,
	starting from first_line and ending on last_line. Update calls keeps track
	of the number of lines updated. If this number becomes greater than turbo,
	and the turbo flag is set, we enter turbo mode. */

static int window_needs_refresh, first_line, last_line, updated_lines;

/* A fixed reference for a pointer to the value -1. */

static const int no_attr = -1;

/* Prevents any other update from being actually done by setting updated_lines
	to a value greater than TURBO. It is most useful when the we know that a
	great deal of update is going to happen, most of which is useless (for
	instance, when cutting clips). */

void delay_update() {
	/* During tests, we never delay updates. */
#ifndef NE_TEST
	updated_lines = TURBO + 1;
	window_needs_refresh = TRUE;
#endif
}
     

/* Compares two highlight states for equality. */

int highlight_cmp(HIGHLIGHT_STATE *x, HIGHLIGHT_STATE *y) {
	return x->state == y->state && x->stack == y->stack && ! strcmp(x->saved_s, y->saved_s);
}



/* Updates the initial syntax state of line descriptors starting from a given line descriptor.
If row is nonnegative, we assume that we have also to update differentially the given lines.
We assume that the line at the given line descriptor is correctly displayed, and proceed
in updating the initial state of the following lines (and possibly their on-screen rendering).

If b->syn or need_attr_update are false, this function does nothing.

The state update (and the screen update, if requested) continues until we get to a line whose
initial state concides with the final state of the previous line; in case you want to force
more lines to be updated, you can provide a non-NULL end_ld. Note that, in any case, we
update only visibile lines (albeit initial states will be updated as necessary).

This function uses the local attribute buffer: thus, after a call the local attribute buffer
could be invalidated. */

void update_syntax_states(buffer *b, int row, line_desc *ld, line_desc *end_ld) {
	
	if (b->syn && need_attr_update) {
 		int got_end_ld = end_ld == NULL;
		int invalidate_attr_buf = FALSE;
		HIGHLIGHT_STATE next_line_state = b->attr_len < 0 ? parse(b->syn, ld, ld->highlight_state, b->encoding == ENC_UTF8) : b->next_state;

		assert(b->attr_len < 0 || b->attr_len == calc_char_len(ld, b->encoding));
		
		/* We update lines until the currenct starting state is equal to next_line_state, but we go until
			end_ld if it is not NULL. In any case, we bail out at the end of the file. */
		for(;;) {

			/* We move one row down. */
			ld = (line_desc *) ld->ld_node.next;

			if ((highlight_cmp(&ld->highlight_state, &next_line_state) && got_end_ld) || !ld->ld_node.next) break;
			if (ld == end_ld) got_end_ld = TRUE;

			if (row >= 0) {
				row++;
				if (row < ne_lines - 1) {
					if (++updated_lines > TURBO) window_needs_refresh = TRUE;
					if (window_needs_refresh) {
						if (row < first_line) first_line = row;
						if (row > last_line) last_line = row;
					}
					else {
						freeze_attributes(b, ld);
						invalidate_attr_buf = TRUE;
					}
				}
			}

			ld->highlight_state = next_line_state;
			next_line_state = parse(b->syn, ld, ld->highlight_state, b->encoding == ENC_UTF8);

			if (row >= 0 && row < ne_lines - 1 && ! window_needs_refresh)
				output_line_desc(row, 0, ld, b->win_x, ne_columns, b->opt.tab_size, TRUE, b->encoding == ENC_UTF8, attr_buf, b->attr_buf, b->attr_len);
		}

		if (invalidate_attr_buf) b->attr_len = -1;
		need_attr_update = FALSE;
	}
}

/* Outputs part of a line descriptor at the given row and column. The output
	will start at the first character with a column position larger than or
	equal to from_col, and will continue until num_cols have been filled
	(partially overflowing characters will *not* be output).  The TABs are
	expanded and considered in the computation of the column position. from_col
	and num_cols are not constrained by the length of the string (the string is
	really considered as terminating with an infinite sequence of
	spaces). cleared_at_end has the same meaning as in update_partial_line(). If
	utf8 is TRUE, then the line content is considered to be UTF-8 encoded. 

	If attr is not NULL, it contains a the list of attributes for the line descriptor;
	if diff is not NULL, the update is differential: we assume that the line is
   already correctly displayed with the attributes specified in diff. If diff_size is shorter
   than the current line, all characters without differential information will be updated. */

void output_line_desc(const int row, const int col, line_desc *ld, const int from_col, const int num_cols, const int tab_size, const int cleared_at_end, const int utf8, const int * const attr, const int * const diff, const int diff_size) {
	
	int curr_col, pos, attr_pos, cursor_moved = FALSE, c, c_len;
	const unsigned char *s = ld->line;
	
	assert(ld != NULL);
	assert(row < ne_lines - 1 && col < ne_columns);

	/* We scan the line descriptor, keeping track in i of the current column
		(considering TABs) and in pos the current position in the line
		(considering UTF-8 sequences, if necessary). s is always ld->line +
		pos. */
		
	curr_col = pos = attr_pos = 0;
	while(curr_col < from_col + num_cols && pos < ld->line_len) {
		c = utf8 ? get_char(s, ENC_UTF8) : *s;
		c_len = utf8 ? utf8seqlen(c) : 1;

		assert(c_len >= 1);

		if (*s == '\t') {
			const int tab_width = tab_size - curr_col % tab_size;
			int i;

			for(i = 0; i < tab_width; i++)
				if (curr_col + i >= from_col && 
				curr_col + i < from_col + num_cols) {
					if (!cursor_moved) {
						cursor_moved = TRUE;
						move_cursor(row, col);
					}
					output_char(' ', attr ? attr[attr_pos] : 0, FALSE);
				}
			curr_col += tab_width;
		}
		else {
			const int c_width = output_width(c);

			if ((curr_col >= from_col || curr_col + c_width > from_col && col - (from_col - curr_col) >= 0) && curr_col < from_col + num_cols) {
				if (!cursor_moved) {
					cursor_moved = TRUE;
					move_cursor(row, col - (curr_col < from_col ? from_col - curr_col : 0));
					if (curr_col > from_col) output_spaces(curr_col - from_col, attr ? &attr[attr_pos] : NULL);
				}
				if (curX + c_width <= ne_columns) {
					if (attr) {
						if (!diff || diff && (attr_pos >= diff_size || diff[attr_pos] != attr[attr_pos])) output_char(c, attr[attr_pos], utf8);
						else move_cursor(row, curX + c_width);
					}
					else output_char(c, 0, utf8);
				}
				else output_spaces(ne_columns - curX, attr ? &attr[attr_pos] : NULL);
			}
			curr_col += c_width;
		}
		s += c_len;
		pos += c_len;
		attr_pos++;
	}

	if (curr_col < from_col + num_cols && ! cleared_at_end) {
		if (! cursor_moved) {
			cursor_moved = TRUE;
			move_cursor(row, col);
		}
		clear_to_eol();
	}
}

/* Updates part of a line given its number and a starting column. It can handle
	lines with no associated line descriptor (such as lines after the end of the
	buffer). It checks for updated_lines bypassing TURBO. if cleared_at_end is
	TRUE, this function assumes that it doesn't have to clean up the rest of the
	line if the string is not long enough to fill the line. 

	If differential is nonzero, the line is update differentially w.r.t.
	the content of b->attr_buf. The caller is thus responsible to guarantee
	that b->attr_buf contents reflect the currently displayed characters. 

	Note that this function guarantees that it will call parse() on the
	specified line.

	Returns the line descriptor corresponding to row, or NULL if the
	row is beyond the end of text. */

line_desc *update_partial_line(buffer * const b, const int row, const int from_col, const int cleared_at_end, const int differential) {
	
	int i;
	line_desc *ld = b->top_line_desc;

	assert(row < ne_lines - 1); 
	assert_line_desc(ld, b->encoding);

	if (++updated_lines > TURBO) window_needs_refresh = TRUE;

	if (window_needs_refresh) {
		if (row < first_line) first_line = row;
		if (row > last_line) last_line = row;
	}

	for(i = 0; i < row && ld->ld_node.next; i++) ld = (line_desc *)ld->ld_node.next;

	if (! ld->ld_node.next) {
		move_cursor(row, from_col);
		clear_to_eol();
		return NULL;
	}
	else if (b->syn) parse(b->syn, ld, ld->highlight_state, b->encoding == ENC_UTF8);

	if (! window_needs_refresh) {
		assert(b->syn || ! differential);
		assert(b->attr_len >= 0 || ! differential);
		output_line_desc(row, from_col, ld, from_col + b->win_x, ne_columns - from_col, b->opt.tab_size, cleared_at_end, b->encoding == ENC_UTF8, b->syn ? attr_buf : NULL, differential ? b->attr_buf : NULL, differential ? b->attr_len : 0);
	}
	return ld;
}



/* Similar to the previous call, but updates the whole line. If the updated line is the current line,
	we update the local attribute buffer. */

void update_line(buffer * const b, const int n, const int differential) {
	line_desc * const ld = update_partial_line(b, n, 0, FALSE, differential);
	if (b->syn && ld == b->cur_line_desc) {
		/* If we updated the entire current line, we update the local attribute buffer. */
		b->next_state = parse(b->syn, ld, ld->highlight_state, b->encoding == ENC_UTF8);	
		ensure_attr_buf(b, attr_len);	
		memcpy(b->attr_buf, attr_buf, (b->attr_len = attr_len) * sizeof *b->attr_buf);
	}
}




/* Updates the text window between given lines. If doit is not true and the
	number of lines that have been updated bypasses TURBO, the update is not
	done. Rather, first_line, last_line and window_needs_refresh record that
	some refresh is needed, and from where it should be done. Setting doit to
	TRUE forces a real update. Generally, doit should be FALSE. */

void update_window_lines(buffer * const b, const int start_line, const int end_line, const int doit) {
	
	int i;
	line_desc *ld = b->top_line_desc;

	assert_line_desc(ld, b->encoding);

	if ((updated_lines += (end_line - start_line + 1)) > TURBO && !doit) window_needs_refresh = TRUE;

	if (start_line < first_line) first_line = start_line;
	if (last_line < end_line) last_line = end_line;

	if (window_needs_refresh && ! doit) return;
		
	for(i = 0; i <= last_line && i + b->win_y < b->num_lines; i++) {
		assert(ld->ld_node.next != NULL);

		if (i >= first_line) {
			if (b->syn) parse(b->syn, ld, ld->highlight_state, b->encoding == ENC_UTF8);
			output_line_desc(i, 0, ld, b->win_x, ne_columns, b->opt.tab_size, FALSE, b->encoding == ENC_UTF8, b->syn ? attr_buf : NULL, NULL, 0);
		}
		ld = (line_desc *)ld->ld_node.next;
	}

	for(; i <= last_line; i++) {
		move_cursor(i, 0);
		clear_to_eol();
	}

	window_needs_refresh = FALSE;
	first_line = ne_lines;
	last_line = -1;
}


/* Like update_window_lines(), but it updates the whole window, and never forces
	an update. */

void update_window(buffer * const b) {
	update_window_lines(b, 0, ne_lines - 2, FALSE);
}

/* Updates the current line, the following syntax states if necessary,
	and finally updates all following lines. All operations are preceded by
	a call to delay_update(). This is mainly written to fix the screen
	state after a block operation. */

void update_syntax_and_lines(buffer *b, line_desc *start_ld, line_desc *end_ld) {
	delay_update();

	if (b->syn) {
		b->attr_len = -1;
		need_attr_update = TRUE;
		update_syntax_states(b, -1, start_ld, end_ld);
	}
}

/* The following functions update a character on the screen. Three operations
	are supported---insertion, deletion, overwriting. The semantics is a bit
	involved.  Essentially, they should be called *immediately* after the
	modification has been done. They assume that the video is perfectly up to
	date, and that only the given modification has been performed. Moreover,
	in case of syntax highlighting, the functions must update the local
	attribute buffer so that it reflects the current status of the line. In
	particular, no update of the rest of the line must be performed for
	syntactic reasons (it will be handled by the caller). Thus, for
	instance, update_inserted_char() assumes that ld->line[pos] contains
	the inserted character. The tough part is expanding/contracting the
	tabs following the modified position in such a way to maintain them
	consistent. Moreover, a lot of special cases are considered and
	optimized (such as writing a space at the end of a line). TURBO is
	taken into consideration. */

void update_deleted_char(buffer * const b, const int c, const line_desc * const ld, int pos, int attr_pos, const int line, const int x) {

	int i, j, c_width, tab_width, tab_found = FALSE, curr_attr_pos;

	if (b->syn) {
		assert(b->attr_len >= 0);
		assert(b->attr_len - 1 == calc_char_len(ld, b->encoding));
		memmove(b->attr_buf + attr_pos, b->attr_buf + attr_pos + 1, (--b->attr_len - attr_pos) * sizeof *b->attr_buf);
	}

	if (++updated_lines > TURBO) window_needs_refresh = TRUE;

	if (window_needs_refresh) {
		if (line < first_line) first_line = line;
		if (line > last_line) last_line = line;
		return;
	}

	if (pos > ld->line_len || (pos == ld->line_len && (c == '\t' || c == ' '))) return;

	move_cursor(line, x);

	if (c == '\t') c_width = b->opt.tab_size - x % b->opt.tab_size;
	else c_width = output_width(c);

	if (!char_ins_del_ok) {
		/* Argh! We can't insert or delete! Just update the rest of the line. */
		if (b->syn) update_line(b, line, FALSE);
		else update_partial_line(b, line, x, FALSE, FALSE);
		return;
	}

	/* Now we search for a visible TAB. If none is found, we just delete c_width
	characters and update the end of the line. Note that since the character has
	been already deleted, pos is the position of the character *after* the one
	just deleted. */

	for(i = x + c_width, j = pos, curr_attr_pos = attr_pos; i < ne_columns && j < ld->line_len; i += get_char_width(&ld->line[j], b->encoding), j = next_pos(ld->line, j, b->encoding), curr_attr_pos++) {

		if (ld->line[j] == '\t') {

			/* This is the previous width of the TAB we found. */
			tab_width = b->opt.tab_size - i % b->opt.tab_size;

			if (c_width + tab_width > b->opt.tab_size) {
				/* In this case we cannot enlarge the TAB we found so to compensate
					for the deletion of c_width columns. Thus, we must delete
					c_width characters, but also reduce by b->opt.tab_size - c_width
					the TAB (plus update b->opt.tab_size characters at the end of the line). */
				delete_chars(c_width);

				move_cursor(line, i - c_width);
				delete_chars(b->opt.tab_size - c_width);

				update_partial_line(b, line, ne_columns - b->opt.tab_size, TRUE, FALSE);
			}
			else {
				/* In this case, instead, we just shift the piece of text between
					our current position and the TAB. Note that this is slower than
					inserting and deleting, but MUCH nicer to see. */
				output_chars(&ld->line[pos], b->syn ? &b->attr_buf[attr_pos] : NULL, j - pos, b->encoding == ENC_UTF8);
				output_spaces(c_width, b->syn ? &b->attr_buf[curr_attr_pos] : NULL);
			}
			tab_found = TRUE;
			break;
		}
	}
	
	/* No TAB was found. We just delete the character and fill the end of the line. */
	if (!tab_found) {
		delete_chars(c_width);
		update_partial_line(b, line, ne_columns - c_width, TRUE, FALSE);
	}
}



/* See comments for update_deleted_char(). */

void update_inserted_char(buffer * const b, const int c, const line_desc * const ld, const int pos, const int attr_pos, const int line, const int x) {

	int i, j, c_width, c_len;
	const int * const attr = b->syn ? &attr_buf[attr_pos] : NULL;

	assert(pos < ld->line_len);

	if (b->syn) {
		assert(b->attr_len >= 0);
		/*fprintf(stderr, "+b->attr_len: %d calc_char_len: %d pos: %d ld->line_len %d attr_pos: %d\n", b->attr_len,calc_char_len(ld, b->encoding), pos, ld->line_len, attr_pos);*/
		assert(b->attr_len + 1 == calc_char_len(ld, b->encoding));
		/* We update the stored attribute vector. */
		ensure_attr_buf(b, b->attr_len + 1);	
		memmove(b->attr_buf + attr_pos + 1, b->attr_buf + attr_pos, (b->attr_len++ - attr_pos) * sizeof *b->attr_buf );
		b->attr_buf[attr_pos] = *attr;
	}

	if (++updated_lines > TURBO) window_needs_refresh = TRUE;

	if (window_needs_refresh) {
		if (line < first_line) first_line = line;
		if (line > last_line) last_line = line;
		return;
	}

	move_cursor(line, x);

	c_len = b->encoding == ENC_UTF8 ? utf8seqlen(c) : 1;
	if (c == '\t') c_width = b->opt.tab_size - x % b->opt.tab_size;
	else c_width = output_width(c);

	if (pos + c_len == ld->line_len) {
		/* We are the last character on the line. We simply output ourselves. */
	   if (c != '\t') output_char(c, attr ? *attr : -1, b->encoding == ENC_UTF8);
		else output_spaces(c_width, attr);
		return;
	}

	if (!char_ins_del_ok) {
		update_partial_line(b, line, x, FALSE, FALSE);
		return;
	}

	/* We search for the first TAB on the line. If there is none, we have just
		to insert our characters. */

	for(i = x + c_width, j = pos + c_len; i < ne_columns && j < ld->line_len; i += get_char_width(&ld->line[j], b->encoding), j = next_pos(ld->line, j, b->encoding)) {
		if (ld->line[j] == '\t') {

			const int tab_width = b->opt.tab_size - (i - c_width) % b->opt.tab_size;

			if (tab_width > c_width) {
				if (c == '\t') output_spaces(c_width, attr);
				else output_char(c, attr ? *attr : -1, b->encoding == ENC_UTF8);
				output_chars(&ld->line[pos + c_len], attr, j - (pos + c_len), b->encoding == ENC_UTF8);
			}
			else {
				if (c == '\t') insert_chars(NULL, attr, c_width, FALSE);
				else insert_char(c, attr ? *attr : -1, b->encoding == ENC_UTF8);
				move_cursor(line, i);
				insert_chars(NULL, attr, b->opt.tab_size - c_width, FALSE);
			}
			return;
		}
	}

	if (c == '\t') insert_chars(NULL, attr, c_width, FALSE);
	else insert_char(c, attr ? *attr : -1, b->encoding == ENC_UTF8);
	
	
}

/* See comments for update_deleted_char(). */

void update_overwritten_char(buffer * const b, const int old_char, const int new_char, const line_desc * const ld, int pos, const int attr_pos, const int line, const int x) {

	int i, j, new_width, old_width, curr_attr_pos;
	const int * const attr = b->syn ? &attr_buf[attr_pos] : &no_attr;

	assert(ld != NULL);
	assert(pos < ld->line_len);

	if (b->syn) {
		/* fprintf(stderr, "-b->attr_len: %d calc_char_len: %d pos: %d ld->line_len %d attr_pos: %d\n", b->attr_len,calc_char_len(ld, b->encoding), pos, ld->line_len, attr_pos);*/
		assert(b->attr_len + 1 == calc_char_len(ld, b->encoding) || b->attr_len == calc_char_len(ld, b->encoding));
		assert(attr_pos <= b->attr_len);
		if (attr_pos == b->attr_len) ensure_attr_buf(b, ++b->attr_len);
		b->attr_buf[attr_pos] = *attr;
	}

	if (++updated_lines > TURBO) window_needs_refresh = TRUE;

	if (window_needs_refresh) {
		if (line < first_line) first_line = line;
		if (line > last_line) last_line = line;
		return;
	}

	if (old_char == '\t') old_width = b->opt.tab_size - x % b->opt.tab_size;
	else old_width = output_width(old_char);

	if (new_char == '\t') new_width = b->opt.tab_size - x % b->opt.tab_size;
	else new_width = output_width(new_char);

	move_cursor(line, x);

	if (old_width == new_width) {
		/* The character did not change its width (the easy case). */
		if (old_char != new_char) {
			if (new_char == '\t') output_spaces(old_width, attr);
			else output_char(new_char, *attr, b->encoding == ENC_UTF8);
		}
		return;
	}

	if (!char_ins_del_ok) {
		update_partial_line(b, line, x, FALSE, FALSE);
		return;
	}

	if (new_width < old_width) {
		/* The character has been shrunk by width_delta. */
		const int width_delta = old_width - new_width;

		/* We search for the first TAB on the line. If there is none, we have
			just to delete width_delta characters, and update the last width_delta
			characters on the screen. */
		pos = next_pos(ld->line, pos, b->encoding);

		for(i = x + old_width, j = pos, curr_attr_pos = attr_pos; i < ne_columns && j < ld->line_len; i += get_char_width(&ld->line[j], b->encoding), j = next_pos(ld->line, j, b->encoding), curr_attr_pos++) {

			if (ld->line[j] == '\t') {
				const int tab_width = b->opt.tab_size - i % b->opt.tab_size;

				/* We found a TAB. Previously, this TAB was tab_width character
				wide. If width_delta + tab_width does not exceed the width of a
				TAB, we just add width_delta characters to the expansion of the
				curren TAB. Otherwise, we first delete width_delta
				characters. Then, if width_delta was not a full TAB we delete the
				remaining characters from the TAB we found. */

				if (width_delta + tab_width <= b->opt.tab_size) {
					if (new_char == '\t') output_spaces(new_width, attr);
					else output_char(new_char, *attr, b->encoding == ENC_UTF8);
					output_chars(&ld->line[pos], attr, j - pos, b->encoding == ENC_UTF8);
					output_spaces(width_delta, b->syn ? &b->attr_buf[curr_attr_pos] : NULL);
				}
				else {
					if (new_char == '\t') output_spaces(new_width, attr);
					else output_char(new_char, *attr, b->encoding == ENC_UTF8);
					delete_chars(width_delta);

					if (width_delta != b->opt.tab_size) {
						move_cursor(line, i - width_delta);
						delete_chars(b->opt.tab_size - width_delta);
					}

					update_partial_line(b, line, ne_columns - b->opt.tab_size, TRUE, FALSE);
				}
				return;
			}
		}

		delete_chars(width_delta);
		if (new_char == '\t') output_spaces(new_width, attr);
		else output_char(new_char, *attr, b->encoding == ENC_UTF8);
		update_partial_line(b, line, ne_columns - width_delta, TRUE, FALSE);
	}
	else {
		/* The character has been enlarged by width_delta. */
		const int width_delta = new_width - old_width;

		/* We search for the first TAB on the line. If there is none, we have
			just to insert width_delta characters. */
		pos = next_pos(ld->line, pos, b->encoding);

		for(i = x + old_width, j = pos; i < ne_columns && j < ld->line_len; i += get_char_width(&ld->line[j], b->encoding), j = next_pos(ld->line, j, b->encoding)) {

			if (ld->line[j] == '\t') {
				const int tab_width = b->opt.tab_size - i % b->opt.tab_size;

				/* We found a TAB. Previously, this TAB was tab_width character
				wide. If width_delta is smaller than tab_width, the enlargement can
				be absorbed by the TAB we found: we just print the new text between
				the original position and i. Otherwise, we insert width_delta
				spaces, and then update the width of the TAB we found. */

				if (width_delta < tab_width) {
					if (new_char == '\t') output_spaces(new_width, attr);
					else output_char(new_char, *attr, b->encoding == ENC_UTF8);
					output_chars(&ld->line[pos], attr, j - pos, b->encoding == ENC_UTF8);
				}
				else {
					insert_chars(NULL, attr, width_delta, FALSE);
					if (new_char == '\t') output_spaces(new_width, attr);
					else output_char(new_char, *attr, b->encoding == ENC_UTF8);
					move_cursor(line, i + width_delta);
					insert_chars(NULL, attr, b->opt.tab_size - (i + width_delta) % b->opt.tab_size - tab_width, FALSE);
				}
				return;
			}
		}

		insert_chars(NULL, attr, width_delta, FALSE);
		if (new_char == '\t') output_spaces(new_width, attr);
		else output_char(new_char, *attr, b->encoding == ENC_UTF8);
	}
}




/* Resets the terminal status, updating the whole window and resetting the
	status bar. It *never* does any real update; it is just used to mark that
	the window and the status bar have to be completely rebuilt. */


void reset_window(void) {
	window_needs_refresh = TRUE;
	first_line = 0;
	last_line = ne_lines - 2;
	reset_status_bar();
}



/* Forces the screen update. It should be called whenever the user has to
	interact, so that he is presented with a correctly updated display. */

void refresh_window(buffer * const b) {
	if (window_needs_refresh) update_window_lines(b, first_line, last_line, TRUE);
	updated_lines = 0;
}



/* Scrolls a region starting at a given line upward (n == -1) or downward (n ==
	-1). TURBO is checked. */

void scroll_window(buffer * const b, const int line, const int n) {
	assert(n == -1 || n == 1);
	assert(line >= 0);
	assert(line < ne_lines);

	if (line_ins_del_ok) {
		if (updated_lines++ > TURBO || window_needs_refresh) {
			window_needs_refresh = TRUE;
			if (first_line > line) first_line = line;
			last_line = ne_lines - 2;
			return;
		}
	}
	else {
		/* Argh! We can't insert or delete lines. The only chance is
		rewriting the last lines of the screen. */

		update_window_lines(b, line, ne_lines - 2, FALSE);
		return;
	}

	if (n > 0) {
		ins_del_lines(line, 1);
		update_line(b, line, FALSE);
	}
	else {
		ins_del_lines(line, -1);
		update_line(b, ne_lines - 2, FALSE);
	}
}



/* Computes the attributes of the given line and stores them into the
attribute buffer. Note that if you call this function on a line different
from the current line, you must take care of invalidating the attribute
buffer afterwards (b->attr_len = -1). */

HIGHLIGHT_STATE freeze_attributes(buffer *b, line_desc *ld) {
	b->next_state = parse(b->syn, ld, ld->highlight_state, b->encoding == ENC_UTF8);	
	ensure_attr_buf(b, attr_len);	
	memcpy(b->attr_buf, attr_buf, (b->attr_len = attr_len) * sizeof *b->attr_buf);
	return b->next_state;
}
