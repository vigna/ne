/* Display handling functions with optional update delay

   Copyright (C) 1993-1998 Sebastiano Vigna
   Copyright (C) 1999-2021 Todd M. Lewis and Sebastiano Vigna

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
   along with this program; if not, see <http://www.gnu.org/licenses/>. */


#include "ne.h"
#include "support.h"
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

bool need_attr_update;

/* If window_needs_refresh, the window has to be refreshed from scratch,
   starting from first_line and ending on last_line. Update calls keeps track
   of the number of lines updated. If this number becomes greater than turbo,
   and the turbo flag is set, we enter turbo mode. */

static bool window_needs_refresh;
static int first_line, last_line, updated_lines;

/* Prevents any other update from being actually done by setting updated_lines
   to a value greater than TURBO. It is most useful when the we know that a
   great deal of update is going to happen, most of which is useless (for
   instance, when cutting clips). */

void delay_update() {
	/* During tests, we never delay updates. */
#ifndef NE_TEST
	updated_lines = TURBO + 1;
	window_needs_refresh = true;
#endif
}


/* Compares two highlight states for equality. */

int highlight_cmp(HIGHLIGHT_STATE *x, HIGHLIGHT_STATE *y) {
	return x->state == y->state && x->stack == y->stack && ! strcmp((const char *)x->saved_s, (const char *)y->saved_s);
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
		bool got_end_ld = end_ld == NULL;
		bool invalidate_attr_buf = false;
		HIGHLIGHT_STATE next_line_state = b->attr_len < 0 ? parse(b->syn, ld, ld->highlight_state, b->encoding == ENC_UTF8) : b->next_state;
		assert(b->attr_len < 0 || b->attr_len == calc_char_len(ld, ld->line_len, b->encoding));

		for(;;) {

			/* We move one row down. */
			ld = (line_desc *) ld->ld_node.next;

			/* We update lines until next_line_state is equal to our current highlight_state, but we go until
			   end_ld if it is not NULL. In any case, we bail out at the end of the file. */
			if ((highlight_cmp(&ld->highlight_state, &next_line_state) && got_end_ld) || !ld->ld_node.next) break;
			if (row >= 0) {
				row++;
				if (row < ne_lines - 1) {
					if (++updated_lines > TURBO) window_needs_refresh = true;
					if (window_needs_refresh) {
						if (row < first_line) first_line = row;
						if (row > last_line) last_line = row;
					}
					else {
						/* If row is positive, the row is visible and we are not in TURBO mode,
						   we need to update a line differentially. To do so, we store in
						   b->attr_buf the attributes of the current line *starting with the
						   current highlight_state*, which should reflect what's on screen. */
						store_attributes(b, ld);
						invalidate_attr_buf = true;
					}
				}
			}

			/* This is where we go on parsing each line, updating highlight_state and next_line_state at each step. */
			ld->highlight_state = next_line_state;
			next_line_state = parse(b->syn, ld, ld->highlight_state, b->encoding == ENC_UTF8);

			/* If we are in the visible range and window_needs_refresh is false, b->attr_buf contains the
			   current on-screen attributes, whereas attr_buf contains the new attributes, so we can
			   perform a differential update. */
			if (row >= 0 && row < ne_lines - 1 && ! window_needs_refresh)
				output_line_desc(row, 0, ld, b->win_x, ne_columns, b->opt.tab_size, true, b->encoding == ENC_UTF8, attr_buf, b->attr_buf, b->attr_len);

			if (ld == end_ld) got_end_ld = true;
		}

		if (invalidate_attr_buf) b->attr_len = -1;
		need_attr_update = false;
	}
}


/* Outputs part of a line descriptor at the given screen row and column.
   The output will start at the first character of the line with a column
   position larger than or equal to from_col, and will continue until
   num_cols have been filled (partially overflowing characters will *not*
   be output). The TABs are expanded and considered in the computation of
   the column position. from_col and num_cols are not constrained by the
   length of the string (the string is really considered as terminating
   with an infinite sequence of spaces). cleared_at_end has the same
   meaning as in update_line(). If utf8 is true, then the line content is
   considered to be UTF-8 encoded.

   If attr is not NULL, it contains a the list of attributes for the line
   descriptor; if diff is not NULL, the update is differential: we assume
   that the line is already correctly displayed with the attributes
   specified in diff. If diff_size is shorter than the current line, all
   characters without differential information will be updated. */

void output_line_desc(const int row, const int col, const line_desc *ld, const int64_t from_col, const int64_t num_cols, const int tab_size, const bool cleared_at_end, const bool utf8, const uint32_t * const attr, const uint32_t * const diff, const int64_t diff_size) {
	assert(ld != NULL);
	assert(row < ne_lines - 1 && col < ne_columns);

	/* We scan the line descriptor, keeping track in curr_col of the
		current column (considering TABs) and in pos the current position in
		the line (considering UTF-8 sequences, if necessary). s is always
		ld->line + pos. The actual output screen column at any time is
		col + curr_col - from_col. */

	const char *s = ld->line;
	int64_t curr_col = 0, pos = 0, attr_pos = 0;

	while(curr_col - from_col < num_cols && pos < ld->line_len) {
		const int64_t output_col = col + curr_col - from_col;
		const int c = utf8 ? get_char(s, ENC_UTF8) : *s;
		const int c_len = utf8 ? utf8seqlen(c) : 1;

		assert(c_len >= 1);

		if (*s == '\t') {
			const int tab_width = tab_size - curr_col % tab_size;

			for(int i = 0; i < tab_width; i++)
				if (curr_col + i >= from_col && curr_col + i < from_col + num_cols) {
					move_cursor(row, output_col + i);
					output_char(' ', attr ? attr[attr_pos] : 0, false);
				}

			curr_col += tab_width;
		}
		else {
			const int c_width = output_width(c);

			if (output_col >= col) {
				if (output_col + c_width <= ne_columns) {
					if (attr) {
						/* In the case of a differential update, we output only
							characters whose attributes have changed. */
						if (!diff || diff && (attr_pos >= diff_size || diff[attr_pos] != attr[attr_pos])) {
							move_cursor(row, output_col);
							output_char(c, attr[attr_pos], utf8);
						}
					}
					else {
						move_cursor(row, output_col);
						output_char(c, 0, utf8);
					}
				}
				else {
					/* The current character is too wide: we can only output spaces
						in place of its visible part. */
					move_cursor(row, output_col);
					output_spaces(ne_columns - output_col, attr ? &attr[attr_pos] : NULL);
				}
			}
			else if (output_col + c_width > col) {
				/* The caracter is only partially displayed. We can only output spaces. */
				const int output_width = output_col + c_width - col;
				for(int i = 0; i < output_width; i++) {
					move_cursor(row, col + i);
					output_char(' ', attr ? attr[attr_pos] : 0, false);
				}
			}

			curr_col += c_width;
		}
		s += c_len;
		pos += c_len;
		attr_pos++;
	}

	/* If we have exhausted the characters in the line, we haven't still
		reached the final output column, and the line is not cleared at the
		end, since we must assume an infinite number of spaces at the end we
		must clear the line starting from the leftmost visible position. */
	if (curr_col < from_col + num_cols && ! cleared_at_end) {
		move_cursor(row, col + (curr_col - from_col <= 0 ? 0 : curr_col - from_col));
		clear_to_eol();
	}
}

/* Updates part of a line given its number, its line descriptor and a starting
   column. It can handle lines after the end of the buffer (just pass the
   tail of the line list). It checks for updated_lines bypassing TURBO, in
   which case it simply updates first_line and last_line. Note that the
   starting column is ignored in case b->syn is not NULL.

   If cleared_at_end is true, this function assumes that it doesn't have
   to clean up the rest of the line if the string is not long enough to
   fill the line.

   IF b->attr_len is nonnegative, the line is updated differentially w.r.t.
   the content of b->attr_buf. The caller is thus responsible to guarantee
   that b->attr_buf contents reflect the currently displayed attributes.

   After a call to this function with argument b->cur_line_desc,
   b->attr_buf will be updated so to contain again the currently displayed
   attributes, unless no refresh has been performed, in which case
   b->attr_len will be set to -1.
*/


void update_line(buffer * const b, line_desc * const ld, const int row, const int64_t from_col, const bool cleared_at_end) {
	assert(ld);
	assert(row < ne_lines - 1);

	if (++updated_lines > TURBO) window_needs_refresh = true;

	if (window_needs_refresh) {
		if (row < first_line) first_line = row;
		if (row > last_line) last_line = row;
		b->attr_len = -1;
		return;
	}

	/* If the line descriptor is not valid, we just clear the line from the required position. This
	   can happen while updating a region after the last line. */
	if (! ld->ld_node.next) {
		if (! cleared_at_end) {
			move_cursor(row, from_col);
			clear_to_eol();
		}
		return;
	}

	if (b->syn) {
		const bool differential = ld == b->cur_line_desc && b->attr_len >= 0;
		HIGHLIGHT_STATE next_state = parse(b->syn, ld, ld->highlight_state, b->encoding == ENC_UTF8);
		output_line_desc(row, 0, ld, b->win_x, ne_columns, b->opt.tab_size, cleared_at_end, b->encoding == ENC_UTF8, attr_buf, differential ? b->attr_buf : NULL, differential ? b->attr_len : 0);

		if (ld == b->cur_line_desc) {
			/* If we updated current line, we update the local attribute buffer. */
			b->next_state = next_state;
			ensure_attr_buf(b, attr_len);
			// This test is necessary to avoid warnings from -fsanitize
			if (b->attr_len = attr_len) memcpy(b->attr_buf, attr_buf, attr_len * sizeof *b->attr_buf);
		}
	}
	else output_line_desc(row, from_col, ld, from_col + b->win_x, ne_columns - from_col, b->opt.tab_size, cleared_at_end, b->encoding == ENC_UTF8, NULL, NULL, 0);
}




/* Updates the text window between given lines. If doit is not true and the
   number of lines that have been updated bypasses TURBO, the update is not
   done. Rather, first_line, last_line and window_needs_refresh record that
   some refresh is needed, and from where it should be done. Setting doit to
   true forces a real update. Generally, doit should be false. */

void update_window_lines(buffer * const b, line_desc * ld, const int start_line, const int end_line, const bool doit) {
	if ((updated_lines += (end_line - start_line + 1)) > TURBO && !doit) window_needs_refresh = true;

	if (start_line < first_line) first_line = start_line;
	if (last_line < end_line) last_line = end_line;

	if (window_needs_refresh && ! doit) return;

	assert(first_line >= start_line);
	if (first_line != start_line) for(uint64_t i = first_line - start_line; i-- != 0; ) ld = (line_desc *)ld->ld_node.next;
	assert_line_desc(ld, b->encoding);

	int i;
	for(i = first_line; i <= last_line && i + b->win_y < b->num_lines; i++) {
		assert(ld->ld_node.next != NULL);
		if (b->syn) parse(b->syn, ld, ld->highlight_state, b->encoding == ENC_UTF8);
		output_line_desc(i, 0, ld, b->win_x, ne_columns, b->opt.tab_size, false, b->encoding == ENC_UTF8, b->syn ? attr_buf : NULL, NULL, 0);
		ld = (line_desc *)ld->ld_node.next;
	}

	for(; i <= last_line; i++) {
		move_cursor(i, 0);
		clear_to_eol();
	}

	window_needs_refresh = false;
	first_line = ne_lines;
	last_line = -1;
}


/* Like update_window_lines(), but it updates the whole window, and never forces
   an update. */

void update_window(buffer * const b) {
	update_window_lines(b, b->top_line_desc, 0, ne_lines - 2, false);
}

/* Updates the current line, the following syntax states if necessary,
   and finally updates all following lines. All operations are preceded by
   a call to delay_update(). This is mainly written to fix the screen
   state after a block operation. */

void update_syntax_states_delay(buffer *b, line_desc *start_ld, line_desc *end_ld) {
	delay_update();

	if (b->syn) {
		b->attr_len = -1;
		need_attr_update = true;
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

void update_deleted_char(buffer * const b, const int c, const int a, line_desc * const ld, int64_t pos, int64_t attr_pos, const int line, const int x) {
	if (++updated_lines > TURBO) window_needs_refresh = true;

	if (window_needs_refresh) {
		if (line < first_line) first_line = line;
		if (line > last_line) last_line = line;
		b->attr_len = -1;
		return;
	}

	if (b->syn) {
		assert(b->attr_len >= 0);
		assert(b->attr_len > attr_pos);
		memmove(b->attr_buf + attr_pos, b->attr_buf + attr_pos + 1, (--b->attr_len - attr_pos) * sizeof *b->attr_buf);
	}

	if (pos > ld->line_len || (pos == ld->line_len && ((c == '\t' || c == ' ') && !a))) return;

	move_cursor(line, x);

	const int c_width = c == '\t' ? b->opt.tab_size - x % b->opt.tab_size : output_width(c);

	if (!char_ins_del_ok) {
		update_line(b, ld, line, x, false);
		return;
	}

	/* Now we search for a visible TAB. If none is found, we just delete c_width
	characters and update the end of the line. Note that since the character has
	been already deleted, pos is the position of the character *after* the one
	just deleted. */

	bool tab_found = false;
	for(int64_t i = x + c_width, j = pos, curr_attr_pos = attr_pos; i < ne_columns && j < ld->line_len; i += get_char_width(&ld->line[j], b->encoding), j = next_pos(ld->line, j, b->encoding), curr_attr_pos++) {

		if (ld->line[j] == '\t') {

			/* This is the previous width of the TAB we found. */
			const int tab_width = b->opt.tab_size - i % b->opt.tab_size;

			if (c_width + tab_width > b->opt.tab_size) {
				/* In this case we cannot enlarge the TAB we found so to compensate
					for the deletion of c_width columns. Thus, we must delete
					c_width characters, but also reduce by b->opt.tab_size - c_width
					the TAB (plus update b->opt.tab_size characters at the end of the line). */
				delete_chars(c_width);

				move_cursor(line, i - c_width);
				delete_chars(b->opt.tab_size - c_width);

				update_line(b, ld, line, ne_columns - b->opt.tab_size, true);
			}
			else {
				/* In this case, instead, we just shift the piece of text between
					our current position and the TAB. Note that this is slower than
					inserting and deleting, but MUCH nicer to see. */
				output_chars(&ld->line[pos], b->syn ? &b->attr_buf[attr_pos] : NULL, j - pos, b->encoding == ENC_UTF8);
				output_spaces(c_width, b->syn ? &b->attr_buf[curr_attr_pos] : NULL);
			}
			tab_found = true;
			break;
		}
	}

	/* No TAB was found. We just delete the character and fill the end of the line. */
	if (!tab_found) {
		delete_chars(c_width);
		update_line(b, ld, line, ne_columns - c_width, true);
	}
}



/* See comments for update_deleted_char(). */

void update_inserted_char(buffer * const b, const int c, line_desc * const ld, const int64_t pos, const int64_t attr_pos, const int line, const int x) {
	assert(pos < ld->line_len);

	if (++updated_lines > TURBO) window_needs_refresh = true;

	if (window_needs_refresh) {
		if (line < first_line) first_line = line;
		if (line > last_line) last_line = line;
		b->attr_len = -1;
		return;
	}

	const uint32_t * const attr = b->syn ? &attr_buf[attr_pos] : NULL;

	if (b->syn) {
		assert(b->attr_len >= 0);
		/*fprintf(stderr, "+b->attr_len: %d calc_char_len: %d pos: %d ld->line_len %d attr_pos: %d\n", b->attr_len, calc_char_len(ld, ld->line_len, b->encoding), pos, ld->line_len, attr_pos);*/
		assert(b->attr_len + 1 == calc_char_len(ld, ld->line_len, b->encoding));
		/* We update the stored attribute vector. */
		ensure_attr_buf(b, b->attr_len + 1);
		memmove(b->attr_buf + attr_pos + 1, b->attr_buf + attr_pos, (b->attr_len++ - attr_pos) * sizeof *b->attr_buf );
		b->attr_buf[attr_pos] = *attr;
	}

	move_cursor(line, x);

	const int c_len = b->encoding == ENC_UTF8 ? utf8seqlen(c) : 1;
	const int c_width = c == '\t' ? b->opt.tab_size - x % b->opt.tab_size : output_width(c);

	if (pos + c_len == ld->line_len) {
		/* We are the last character on the line. We simply output ourselves. */
	   if (c != '\t') output_char(c, attr ? *attr : -1, b->encoding == ENC_UTF8);
		else output_spaces(c_width, attr);
		return;
	}

	if (!char_ins_del_ok) {
		update_line(b, ld, line, x, false);
		return;
	}

	/* We search for the first TAB on the line. If there is none, we have just
		to insert our characters. */

	for(int64_t i = x + c_width, j = pos + c_len; i < ne_columns && j < ld->line_len; i += get_char_width(&ld->line[j], b->encoding), j = next_pos(ld->line, j, b->encoding)) {
		if (ld->line[j] == '\t') {

			const int tab_width = b->opt.tab_size - (i - c_width) % b->opt.tab_size;

			if (tab_width > c_width) {
				if (c == '\t') output_spaces(c_width, attr);
				else output_char(c, attr ? *attr : -1, b->encoding == ENC_UTF8);
				output_chars(&ld->line[pos + c_len], attr, j - (pos + c_len), b->encoding == ENC_UTF8);
			}
			else {
				if (c == '\t') insert_chars(NULL, attr, c_width, false);
				else insert_char(c, attr ? *attr : -1, b->encoding == ENC_UTF8);
				move_cursor(line, i);
				insert_chars(NULL, attr, b->opt.tab_size - c_width, false);
			}
			return;
		}
	}

	if (c == '\t') insert_chars(NULL, attr, c_width, false);
	else insert_char(c, attr ? *attr : -1, b->encoding == ENC_UTF8);


}

/* See comments for update_deleted_char(). */

void update_overwritten_char(buffer * const b, const int old_char, const int new_char, line_desc * const ld, int64_t pos, const int64_t attr_pos, const int line, const int x) {
	assert(ld != NULL);
	assert(pos < ld->line_len);

	if (++updated_lines > TURBO) window_needs_refresh = true;

	if (window_needs_refresh) {
		if (line < first_line) first_line = line;
		if (line > last_line) last_line = line;
		b->attr_len = -1;
		return;
	}

	const uint32_t * const attr = b->syn ? &attr_buf[attr_pos] : NULL;

	if (b->syn) {
		/* fprintf(stderr, "-b->attr_len: %d calc_char_len: %d pos: %d ld->line_len %d attr_pos: %d\n", b->attr_len, calc_char_len(ld, ld->line_len, b->encoding), pos, ld->line_len, attr_pos);*/
		assert(b->attr_len + 1 == calc_char_len(ld, ld->line_len, b->encoding) || b->attr_len == calc_char_len(ld, ld->line_len, b->encoding));
		assert(attr_pos <= b->attr_len);
		if (attr_pos == b->attr_len) ensure_attr_buf(b, ++b->attr_len);
		b->attr_buf[attr_pos] = *attr;
	}

	const int old_width = old_char == '\t' ? b->opt.tab_size - x % b->opt.tab_size : output_width(old_char);
	const int new_width = new_char == '\t' ? b->opt.tab_size - x % b->opt.tab_size : output_width(new_char);

	move_cursor(line, x);

	if (old_width == new_width) {
		/* The character did not change its width (the easy case). */
		if (old_char != new_char) {
			if (new_char == '\t') output_spaces(old_width, attr);
			else output_char(new_char, attr ? *attr : -1, b->encoding == ENC_UTF8);
		}
		return;
	}

	if (!char_ins_del_ok) {
		update_line(b, ld, line, x, false);
		return;
	}

	if (new_width < old_width) {
		/* The character has been shrunk by width_delta. */
		const int width_delta = old_width - new_width;

		/* We search for the first TAB on the line. If there is none, we have
			just to delete width_delta characters, and update the last width_delta
			characters on the screen. */
		pos = next_pos(ld->line, pos, b->encoding);

		for(int64_t i = x + old_width, j = pos, curr_attr_pos = attr_pos; i < ne_columns && j < ld->line_len; i += get_char_width(&ld->line[j], b->encoding), j = next_pos(ld->line, j, b->encoding), curr_attr_pos++) {

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
					else output_char(new_char, attr ? *attr : -1, b->encoding == ENC_UTF8);
					output_chars(&ld->line[pos], attr, j - pos, b->encoding == ENC_UTF8);
					output_spaces(width_delta, b->syn ? &b->attr_buf[curr_attr_pos] : NULL);
				}
				else {
					if (new_char == '\t') output_spaces(new_width, attr);
					else output_char(new_char, attr ? *attr : -1, b->encoding == ENC_UTF8);
					delete_chars(width_delta);

					if (width_delta != b->opt.tab_size) {
						move_cursor(line, i - width_delta);
						delete_chars(b->opt.tab_size - width_delta);
					}

					update_line(b, ld, line, ne_columns - b->opt.tab_size, true);
				}
				return;
			}
		}

		delete_chars(width_delta);
		if (new_char == '\t') output_spaces(new_width, attr);
		else output_char(new_char, attr ? *attr : -1, b->encoding == ENC_UTF8);
		update_line(b, ld, line, ne_columns - width_delta, true);
	}
	else {
		/* The character has been enlarged by width_delta. */
		const int width_delta = new_width - old_width;

		/* We search for the first TAB on the line. If there is none, we have
			just to insert width_delta characters. */
		pos = next_pos(ld->line, pos, b->encoding);

		for(int64_t i = x + old_width, j = pos; i < ne_columns && j < ld->line_len; i += get_char_width(&ld->line[j], b->encoding), j = next_pos(ld->line, j, b->encoding)) {

			if (ld->line[j] == '\t') {
				const int tab_width = b->opt.tab_size - i % b->opt.tab_size;

				/* We found a TAB. Previously, this TAB was tab_width character
				wide. If width_delta is smaller than tab_width, the enlargement can
				be absorbed by the TAB we found: we just print the new text between
				the original position and i. Otherwise, we insert width_delta
				spaces, and then update the width of the TAB we found. */

				if (width_delta < tab_width) {
					if (new_char == '\t') output_spaces(new_width, attr);
					else output_char(new_char, attr ? *attr : -1, b->encoding == ENC_UTF8);
					output_chars(&ld->line[pos], attr, j - pos, b->encoding == ENC_UTF8);
				}
				else {
					insert_chars(NULL, attr, width_delta, false);
					if (new_char == '\t') output_spaces(new_width, attr);
					else output_char(new_char, attr ? *attr : -1, b->encoding == ENC_UTF8);
					move_cursor(line, i + width_delta);
					insert_chars(NULL, attr, b->opt.tab_size - (i + width_delta) % b->opt.tab_size - tab_width, false);
				}
				return;
			}
		}

		insert_chars(NULL, attr, width_delta, false);
		if (new_char == '\t') output_spaces(new_width, attr);
		else output_char(new_char, attr ? *attr : -1, b->encoding == ENC_UTF8);
	}
}




/* Resets the terminal status, updating the whole window and resetting the
   status bar. It *never* does any real update; it is just used to mark that
   the window and the status bar have to be completely rebuilt. */


void reset_window(void) {
	window_needs_refresh = true;
	first_line = 0;
	last_line = ne_lines - 2;
	reset_status_bar();
}



/* Forces the screen update. It should be called whenever the user has to
   interact, so that he is presented with a correctly updated display. */

void refresh_window(buffer * const b) {
	if (window_needs_refresh) {
		line_desc *ld = b->top_line_desc;
		for(int i = first_line; i-- != 0 && (line_desc *)ld->ld_node.next;) ld = (line_desc *)ld->ld_node.next;
		if (ld->ld_node.next) update_window_lines(b, ld, first_line, last_line, true);
		highlight_mark(b, true);
		updated_lines = 0;
	}
}



/* Scrolls a region starting at a given line upward (n == -1) or downward
(n == 1). TURBO is checked. */

void scroll_window(buffer * const b, line_desc * const ld, const int line, const int n) {
	assert(n == -1 || n == 1);
	assert(line >= 0);
	assert(line < ne_lines);

	if (line_ins_del_ok) {
		if (updated_lines++ > TURBO || window_needs_refresh) {
			window_needs_refresh = true;
			if (first_line > line) first_line = line;
			last_line = ne_lines - 2;
			return;
		}
	}
	else {
		/* Argh! We can't insert or delete lines. The only chance is
		rewriting the last lines of the screen. */

		update_window_lines(b, ld, line, ne_lines - 2, false);
		return;
	}

	if (n > 0) update_line(b, ld, line, 0, ins_del_lines(line, 1));
	else {
		line_desc * last_line_ld = b->top_line_desc;
		for(int i = ne_lines - 2; i-- != 0 && last_line_ld->ld_node.next;) last_line_ld = (line_desc *)last_line_ld->ld_node.next;
		update_line(b, last_line_ld, ne_lines - 2, 0, ins_del_lines(line, -1));
	}
}


/* If syntax is enabled, and b->attr_len < 0, computes the attributes
   of the current line descriptor, stores them in b->attr_buf, and
   stores in b->next_state the return value of parse(). */

void ensure_attributes(buffer *b) {
	if (! b->syn || b->attr_len >= 0) return;
	store_attributes(b, b->cur_line_desc);
}


/* Computes the attributes of the given line, stores them into b->attr_buf
   and stores in b->next_state the return value of parse(). Note that if
   you call this function on a line different from the current line, you
   must take care of invalidating the attribute buffer afterwards
   (b->attr_len = -1). */

void store_attributes(buffer *b, line_desc *ld) {
	b->next_state = parse(b->syn, ld, ld->highlight_state, b->encoding == ENC_UTF8);
	assert(calc_char_len(ld, ld->line_len, b->encoding) == attr_len);
	// This test is necessary to avoid warnings from -fsanitize
	ensure_attr_buf(b, attr_len);
	if (b->attr_len = attr_len) memcpy(b->attr_buf, attr_buf, attr_len * sizeof *b->attr_buf);
}

static uint32_t invert_boldness(uint32_t orig_attr) {
	uint32_t tmp_attr = orig_attr;
	switch (orig_attr & BG_MASK) {
		case BG_BLACK:    tmp_attr = (tmp_attr & ~BG_MASK ) | BG_BBLACK;     break;
		case BG_RED:      tmp_attr = (tmp_attr & ~BG_MASK ) | BG_BRED;       break;
		case BG_GREEN:    tmp_attr = (tmp_attr & ~BG_MASK ) | BG_BGREEN;     break;
		case BG_YELLOW:   tmp_attr = (tmp_attr & ~BG_MASK ) | BG_BYELLOW;    break;
		case BG_BLUE:     tmp_attr = (tmp_attr & ~BG_MASK ) | BG_BBLUE;      break;
		case BG_MAGENTA:  tmp_attr = (tmp_attr & ~BG_MASK ) | BG_BMAGENTA;   break;
		case BG_CYAN:     tmp_attr = (tmp_attr & ~BG_MASK ) | BG_BCYAN;      break;
		case BG_WHITE:    tmp_attr = (tmp_attr & ~BG_MASK ) | BG_BWHITE;     break;
		case BG_BBLACK:   tmp_attr = (tmp_attr & ~BG_MASK ) | BG_BLACK;      break;
		case BG_BRED:     tmp_attr = (tmp_attr & ~BG_MASK ) | BG_RED;        break;
		case BG_BGREEN:   tmp_attr = (tmp_attr & ~BG_MASK ) | BG_GREEN;      break;
		case BG_BYELLOW:  tmp_attr = (tmp_attr & ~BG_MASK ) | BG_YELLOW;     break;
		case BG_BBLUE:    tmp_attr = (tmp_attr & ~BG_MASK ) | BG_BLUE;       break;
		case BG_BMAGENTA: tmp_attr = (tmp_attr & ~BG_MASK ) | BG_MAGENTA;    break;
		case BG_BCYAN:    tmp_attr = (tmp_attr & ~BG_MASK ) | BG_CYAN;       break;
		case BG_BWHITE:   tmp_attr = (tmp_attr & ~BG_MASK ) | BG_WHITE;      break;
		default:          tmp_attr = (tmp_attr & ~BG_MASK ) | BG_BWHITE;     break;
		}

	switch (orig_attr & FG_MASK) {
		case FG_BLACK:    tmp_attr = (tmp_attr & ~FG_MASK) | FG_BBLACK;      break;
		case FG_RED:      tmp_attr = (tmp_attr & ~FG_MASK) | FG_BRED;        break;
		case FG_GREEN:    tmp_attr = (tmp_attr & ~FG_MASK) | FG_BGREEN;      break;
		case FG_YELLOW:   tmp_attr = (tmp_attr & ~FG_MASK) | FG_BYELLOW;     break;
		case FG_BLUE:     tmp_attr = (tmp_attr & ~FG_MASK) | FG_BBLUE;       break;
		case FG_MAGENTA:  tmp_attr = (tmp_attr & ~FG_MASK) | FG_BMAGENTA;    break;
		case FG_CYAN:     tmp_attr = (tmp_attr & ~FG_MASK) | FG_BCYAN;       break;
		case FG_WHITE:    tmp_attr = (tmp_attr & ~FG_MASK) | FG_BWHITE;      break;
		case FG_BBLACK:   tmp_attr = (tmp_attr & ~FG_MASK) | FG_BLACK;       break;
		case FG_BRED:     tmp_attr = (tmp_attr & ~FG_MASK) | FG_RED;         break;
		case FG_BGREEN:   tmp_attr = (tmp_attr & ~FG_MASK) | FG_GREEN;       break;
		case FG_BYELLOW:  tmp_attr = (tmp_attr & ~FG_MASK) | FG_YELLOW;      break;
		case FG_BBLUE:    tmp_attr = (tmp_attr & ~FG_MASK) | FG_BLUE;        break;
		case FG_BMAGENTA: tmp_attr = (tmp_attr & ~FG_MASK) | FG_MAGENTA;     break;
		case FG_BCYAN:    tmp_attr = (tmp_attr & ~FG_MASK) | FG_CYAN;        break;
		case FG_BWHITE:   tmp_attr = (tmp_attr & ~FG_MASK) | FG_WHITE;       break;
		default:          tmp_attr = (tmp_attr & ~FG_MASK) | FG_BBLACK;      break;
		}
	return tmp_attr;
}


/* (Un)highlights (depending on the value of show) the bracket matching
   the one under the cursor (if any). */

void automatch_bracket(buffer * const b, const bool show) {
	static int c;
	static uint32_t orig_attr;

	if (show) {
		int64_t match_pos, match_line;
		uint32_t tmp_attr;
		line_desc *matching_ld;
		if (find_matching_bracket(b, b->win_y, b->win_y + ne_lines - 2 >= b->num_lines - 1 ? b->num_lines - 1 : b->win_y + ne_lines - 2,
								  &match_line, &match_pos, &c, &matching_ld) == OK) {
			/* We limited find_matching_bracket()'s search to the visible lines, but not the
			visible portions of those lines. Now ensure the matching pos is within the visible window. */
			b->automatch.y = match_line - b->win_y;
			b->automatch.x = calc_width(matching_ld, match_pos, b->opt.tab_size, b->encoding) - b->win_x;
			if (b->automatch.x >= 0 && b->automatch.x < ne_columns ) {
				if (b->syn) {
					parse(b->syn, matching_ld, matching_ld->highlight_state, b->encoding == ENC_UTF8);
					orig_attr = attr_buf[calc_char_len(matching_ld, match_pos, b->encoding == ENC_UTF8)];
				} else orig_attr = -1;

				tmp_attr = orig_attr;

				if (b->opt.automatch & 1 ) /* invert boldness of FG, BG */
					tmp_attr = invert_boldness(tmp_attr);

				if (b->opt.automatch & 2 )
					tmp_attr = tmp_attr ^ INVERSE;

				if (b->opt.automatch & 4 )
					tmp_attr = tmp_attr ^ BOLD;

				if (b->opt.automatch & 8 )
					tmp_attr = tmp_attr ^ UNDERLINE;

				move_cursor(b->automatch.y, b->automatch.x);
				output_char(c, tmp_attr, b->encoding == ENC_UTF8);
				b->automatch.shown = true;
			}
		}
	} else {
		if (b->automatch.shown) {
			if (b->automatch.x >= 0 && b->automatch.x < ne_columns && b->automatch.y >= 0 && b->automatch.y < ne_lines - 1) {
				move_cursor(b->automatch.y, b->automatch.x);
				output_char(c, orig_attr, b->encoding == ENC_UTF8);
			}
			b->automatch.shown = false;
		}
	}
}

void highlight_mark(buffer * const b, const bool show) {
	static int c;
	static uint32_t orig_attr;

	if (show && !fast_gui) {
		uint32_t tmp_attr;
		if (b->marking && b->opt.automatch) {
			b->visible_mark.y = b->block_start_line - b->win_y;
			if (b->visible_mark.y >= 0 && b->visible_mark.y < ne_lines - 1) {
				line_desc *ld = nth_line_desc(b, b->block_start_line);
				b->visible_mark.x = calc_width(ld, b->block_start_pos, b->opt.tab_size, b->encoding) - b->win_x;
				if (b->visible_mark.x >= 0 && b->visible_mark.x < ne_columns) {
					move_cursor(b->visible_mark.y, b->visible_mark.x);
					if (b->syn && b->block_start_pos < ld->line_len) {
						parse(b->syn, ld, ld->highlight_state, b->encoding == ENC_UTF8);
						orig_attr = attr_buf[b->block_start_pos];
					} else orig_attr = 0;
					if (b->block_start_pos < ld->line_len && ld->line[b->block_start_pos] != '\t') {
						c = get_char( &ld->line[b->block_start_pos], b->encoding);
					} else c = ' ';

					tmp_attr = orig_attr;
					unsigned int mark_style = (b->opt.automatch == 0 || b->opt.automatch == 0x0f ) ? 1+2 : b->opt.automatch ^ 0x07;
					if (mark_style & 1 ) /* invert boldness of FG, BG */
						tmp_attr = invert_boldness(tmp_attr);

					if (mark_style & 2 )
						tmp_attr = tmp_attr ^ INVERSE;

					if (mark_style & 4 )
						tmp_attr = tmp_attr ^ BOLD;

					if (mark_style & 8 )
						tmp_attr = tmp_attr ^ UNDERLINE;

					output_char(c, tmp_attr, b->encoding == ENC_UTF8);
					b->visible_mark.shown = true;
				}
			}
		}
	} else {
		if (b->visible_mark.shown) {
			if (b->visible_mark.x >= 0 && b->visible_mark.x < ne_columns && b->visible_mark.y >= 0 && b->visible_mark.y < ne_lines - 1) {
				move_cursor(b->visible_mark.y, b->visible_mark.x);
				output_char(c, orig_attr, b->encoding == ENC_UTF8);
			}
			b->visible_mark.shown = false;
		}
	}
}

