/* Navigation functions.

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

/* The functions in this file move the cursor. They also update the screen
accordingly. There are some assumptions which are made in order to simplify
the code: the TAB size has to be less than half the number of columns; and
win_x has to be a multiple of the TAB size.

The functions themselves are very simple; unfortunately, they are the kind of
code filled up with +1 and -1 whose nature is not always obvious. Most
functions do not have a description, because their name suggests their behaviour
in an obvious way. (Yeah, right.) */





/*********************************************************************************
You stand absolutely no chance of understanding this code if you aren't intimately
familiar with this diagram.

          |<- cur_pos (in bytes)-->|
          |<- cur_char (in chars)->|

                    |< cur_x >-----|
          |< win_x >|

 ---      +----------------------------------------------+ ---      -----
  |       |                 File boundary                |  |         |
 win_y    |                                              |  |         |
  |       |                                              |  |         |
 --- ---  |         +-------------------------+          |  |         |      ----
      |   |         |       Screen boundary   |          | cur_line   |       |
    cur_y |         |                         |          |  |         |       |
      |   |         |                         |          |  |         |       |
      |   |         |                         |          |  |         |       |
     ---  |         |              @ <-Cursor |          | ---        |      ne_lines
          |         |                         |          |          num_lines |
          |         |                         |          |            |       |
          |         |                         |          |            |       |
          |         |                         |          |            |       |
          |         |                         |          |            |       |
          |         |                         |          |            |       |
          |         +-------------------------+          |            |      ----
          |                                              |            |
          +----------------------------------------------+          -----

                    |<---- ne_columns ------->|

************************************************************************************/


/* "Resyncs" cur_pos (the current character the cursor is on) with cur_x and
   win_x. It has to take into account the TAB expansion, and can cause
   left/right movement in order to properly land on a real character.  x is the
   offset from the beginning of the line after TAB expansion. resync_pos()
   assumes that tab_size < columns/2. Note that this function has to be called
   whenever the cursor is moved to a different line, keeping the x position
   constant. The only way of avoiding this problem is not supporting TABs,
   which is of course unacceptable. Note that if x_wanted is true, then the
   wanted_x position is used rather tham cur_x+win_x. */

void resync_pos(buffer * const b) {
	int64_t x = b->win_x + b->cur_x;
	if (b->x_wanted) x = b->wanted_x;

	assert(b->opt.tab_size < ne_columns / 2);

	if (x == 0) {
		b->cur_pos = b->cur_char = 0;
		return;
	}

	line_desc *ld = b->cur_line_desc;
	int64_t i, pos, width, last_char_width;
	for(i = pos = width = 0; pos < ld->line_len; pos = next_pos(ld->line, pos, b->encoding), i++) {

		if (ld->line[pos] != '\t') width += (last_char_width = get_char_width(&ld->line[pos], b->encoding));
		else width += (last_char_width = b->opt.tab_size - width % b->opt.tab_size);

		if (width == x) {
			b->cur_pos = pos + (b->encoding == ENC_UTF8 ? utf8len(ld->line[pos]) : 1);
			b->cur_char = i + 1;
			if (b->x_wanted) {

				b->x_wanted = 0;

				if (x - b->win_x < ne_columns) b->cur_x = x - b->win_x;
				else {
					b->win_x = x - ne_columns;
					b->win_x += b->opt.tab_size - b->win_x % b->opt.tab_size;
					b->cur_x = x - b->win_x;
					if (b == cur_buffer) update_window(b);
				}
			}
			return;
		}

		if (width > x) {

			b->cur_pos = pos;
			b->cur_char = i;
			width -= last_char_width;
			b->x_wanted = 1;
			b->wanted_x = x;

			if (width - b->win_x < 0) {
				/* We are on a character which is only partially on the screen
					(more precisely, its right margin is not). We shift the screen
					to the left. */
				assert(b->win_x > 0);
				b->win_x = max(0, width - ne_columns);
				b->win_x -= b->win_x % b->opt.tab_size;
				b->cur_x = width - b->win_x;
				if (b == cur_buffer) update_window(b);
			}
			else if (width - b->win_x < ne_columns) b->cur_x = width - b->win_x;
			else {
				b->win_x = width - ne_columns;
				b->win_x += b->opt.tab_size - b->win_x % b->opt.tab_size;
				b->cur_x = width - b->win_x;
				if (b == cur_buffer) update_window(b);
			}
			return;
		}
	}

	if (b->opt.free_form) {
		b->cur_pos = ld->line_len + x - width;
		b->cur_char = i + x - width;
		b->cur_x = x - b->win_x;
		b->x_wanted = 0;
	}
	else {
		b->wanted_x = x;
		move_to_eol(b);
		b->x_wanted = 1;
	}
}



int line_up(buffer * const b) {

	b->y_wanted = 0;
	if (b->cur_y > 0) {
		update_syntax_states(b, b->cur_y, b->cur_line_desc, NULL);
		b->cur_y--;
		b->cur_line--;
		b->cur_line_desc = (line_desc *)b->cur_line_desc->ld_node.prev;
		b->attr_len = -1;
		resync_pos(b);
		return OK;
	}
	else {
		if (b->win_y > 0) {
			update_syntax_states(b, b->cur_y, b->cur_line_desc, NULL);
			b->win_y--;
			b->cur_line--;
			b->cur_line_desc = (line_desc *)b->cur_line_desc->ld_node.prev;
			b->top_line_desc = (line_desc *)b->top_line_desc->ld_node.prev;
			b->attr_len = -1;
			if (b == cur_buffer) scroll_window(b, 0, 1);
			resync_pos(b);
			return OK;
		}
	}
	return ERROR;
}



int line_down(buffer * const b) {

	b->y_wanted = 0;
	if (b->cur_y < ne_lines - 2 && b->cur_line < b->num_lines - 1) {
		update_syntax_states(b, b->cur_y, b->cur_line_desc, NULL);
		b->cur_y++;
		b->cur_line++;
		b->attr_len = -1;
		b->cur_line_desc = (line_desc *)b->cur_line_desc->ld_node.next;
		resync_pos(b);
		return OK;
	}
	else {
		if (b->win_y < b->num_lines - ne_lines + 1) {
			update_syntax_states(b, b->cur_y, b->cur_line_desc, NULL);
			b->win_y++;
			b->cur_line++;
			b->cur_line_desc = (line_desc *)b->cur_line_desc->ld_node.next;
			b->top_line_desc = (line_desc *)b->top_line_desc->ld_node.next;
			b->attr_len = -1;
			if (b == cur_buffer) scroll_window(b, 0, -1);
			resync_pos(b);
			return OK;
		}
	}
	return ERROR;
}

/* This has to be done whenever we switch to a different buffer
 * because the screen may have been resized since the last time we
 * were here.
 */

void keep_cursor_on_screen(buffer * const b) {
	b->opt.tab_size = min(b->opt.tab_size, max(ne_columns / 2 - 1,1));
	const int shift_right = b->win_x % b->opt.tab_size;
	if (shift_right) {
		b->win_x -= shift_right;
		b->cur_x += shift_right;
	}
	if (b->cur_y > ne_lines - 2) {
		while(b->cur_y > ne_lines - 2) {
			b->cur_y--;
			b->win_y++;
			b->attr_len = -1;
			b->top_line_desc = (line_desc *)b->top_line_desc->ld_node.next;
		}
		assert(b->win_y = b->cur_line - b->cur_y);
		b->y_wanted = false;
	}

	while(b->cur_x >= ne_columns) {
		b->win_x += b->opt.tab_size;
		b->cur_x -= b->opt.tab_size;
	}
}



/* Moves win_x of n bytes to the left (n *has* to be a multiple of the current
   TAB size). It is used by char_left(). cur_x is moved, too. */

static void block_left(buffer * const b, const int n) {

	const int64_t t = b->win_x;

	assert(n <= ne_columns);
	assert(!(n % b->opt.tab_size));

	if ((b->win_x -= n) < 0) b->win_x = 0;
	b->cur_x += t - b->win_x;
	if (b == cur_buffer) update_window(b);
}



int char_left(buffer * const b) {

	line_desc *ld = b->cur_line_desc;

	assert(ld != NULL);
	assert_line_desc(ld, b->encoding);

	b->x_wanted = 0;
	b->y_wanted = 0;

	if (b->cur_pos > 0) {

		int disp = ld->line && b->cur_pos <= ld->line_len ? get_char_width(&ld->line[prev_pos(ld->line, b->cur_pos, b->encoding)], b->encoding) : 1;

		if (b->cur_pos <= ld->line_len && ld->line[b->cur_pos - 1] == '\t')
			disp = b->opt.tab_size - calc_width(ld, b->cur_pos - 1, b->opt.tab_size, b->encoding) % b->opt.tab_size;

		if (b->cur_x < disp) block_left(b, b->opt.tab_size * 2);
		b->cur_x -= disp;

		/* If the buffer is UTF-8 encoded, we move back until we find a sequence initiator. */
		b->cur_pos = b->cur_pos > ld->line_len ? b->cur_pos - 1 : prev_pos(ld->line, b->cur_pos, b->encoding);
		b->cur_char--;
		return OK;
	}
	else if (b->cur_line > 0) {
		line_up(b);
		move_to_eol(b);
		return OK;
	}
	return ERROR;
}


/* Same as block_left(), but to the right. */

static void block_right(buffer * const b, const int n) {

	assert(n <= ne_columns);
	assert(!(n % b->opt.tab_size));

	b->win_x += n;
	b->cur_x -= n;
	if (b == cur_buffer) update_window(b);
}

int char_right(buffer * const b) {

	const line_desc * const ld = b->cur_line_desc;
	int disp = ld->line && b->cur_pos < ld->line_len ? get_char_width(&ld->line[b->cur_pos], b->encoding) : 1;

	assert(ld != NULL);
	assert_line_desc(ld, b->encoding);

	if (ld->line && b->cur_pos < ld->line_len && ld->line[b->cur_pos] == '\t')
		disp = b->opt.tab_size - calc_width(ld, b->cur_pos, b->opt.tab_size, b->encoding) % b->opt.tab_size;

	b->x_wanted = 0;
	b->y_wanted = 0;

	if (b->cur_pos == ld->line_len && !b->opt.free_form) {
		if (!ld->ld_node.next->next) return ERROR;
		move_to_sol(b);
		line_down(b);
		return OK;
	}

	b->cur_x += disp;
	b->cur_pos = b->cur_pos >= ld->line_len ? b->cur_pos + 1 : next_pos(ld->line, b->cur_pos, b->encoding);
	b->cur_char++;

	/* If the current x position would be beyond the right screen margin, or if the same happens
		for the character we are currently over, we shift the screen to the right. */
	if (b->cur_x >= ne_columns || ld->line && b->cur_pos < ld->line_len && b->cur_x + get_char_width(&ld->line[b->cur_pos], b->encoding) > ne_columns) block_right(b, b->opt.tab_size * 2);
	return OK;
}



int prev_page(buffer * const b) {

	b->y_wanted = 0;

	if (b->cur_y > 0) {
		update_syntax_states(b, b->cur_y, b->cur_line_desc, NULL);
		b->cur_line -= b->cur_y;
		b->cur_y = 0;
		b->cur_line_desc = b->top_line_desc;
		b->attr_len = -1;
		resync_pos(b);
		return OK;
	}

	if (b->win_y == 0) return ERROR;

	update_syntax_states(b, -1, b->cur_line_desc, NULL);
	b->attr_len = -1;

	if ((b->win_y -= ne_lines - 2)<0) b->win_y = 0;

	line_desc *ld_top = b->top_line_desc;
	line_desc *ld_cur = b->cur_line_desc;

	for(int i = 0; i < ne_lines - 2 && ld_top->ld_node.prev->prev; i++) {
		ld_top = (line_desc *)ld_top->ld_node.prev;
		ld_cur = (line_desc *)ld_cur->ld_node.prev;
		b->cur_line--;
	}

	b->top_line_desc = ld_top;
	b->cur_line_desc = ld_cur;

	if (b == cur_buffer) update_window(b);
	resync_pos(b);
	return ERROR;
}



int next_page(buffer * const b) {
	b->y_wanted = 0;

	if (b->cur_y < ne_lines - 2) {
		update_syntax_states(b, b->cur_y, b->cur_line_desc, NULL);
		line_desc *ld_cur;
		if (b->win_y >= b->num_lines - (ne_lines - 1)) {
			ld_cur = b->top_line_desc;
			int i;
			for(i = 0; i < ne_lines - 2 && ld_cur->ld_node.next->next; i++)
				ld_cur = (line_desc *)ld_cur->ld_node.next;
			b->cur_line += (i - b->cur_y);
			b->cur_y = i;
		}
		else {
			b->cur_line += (ne_lines - 2 - b->cur_y);
			b->cur_y = ne_lines - 2;
			ld_cur = b->top_line_desc;
			for(int i = 0; i < ne_lines - 2; i++)
			  ld_cur = (line_desc *)ld_cur->ld_node.next;
		}
		b->attr_len = -1;
		b->cur_line_desc = ld_cur;
		resync_pos(b);
		return OK;
	}

	if (b->win_y >= b->num_lines - (ne_lines - 1)) return ERROR;

	update_syntax_states(b, -1, b->cur_line_desc, NULL);
	b->attr_len = -1;

	int disp = ne_lines - 2;

	if (b->win_y + disp > b->num_lines - (ne_lines - 1))
		disp = b->num_lines - (ne_lines - 1) - b->win_y;

	b->win_y += disp;
	b->cur_line += disp;

	line_desc *ld_top = b->top_line_desc;
	line_desc *ld_cur = b->cur_line_desc;

	for(int i = 0; i < disp && ld_top->ld_node.next->next; i++) {
		ld_top = (line_desc *)ld_top->ld_node.next;
		ld_cur = (line_desc *)ld_cur->ld_node.next;
	}

	b->top_line_desc = ld_top;
	b->cur_line_desc = ld_cur;

	if (b == cur_buffer) update_window(b);
	resync_pos(b);
	return OK;
}

int page_up(buffer * const b) {
	/* Already on the top line? */
	if (b->cur_line == 0) return OK;

	update_syntax_states(b, -1, b->cur_line_desc, NULL);
	b->attr_len = -1;

	if (!b->y_wanted) {
		b->y_wanted = true;
		b->wanted_y = b->cur_line;
		b->wanted_cur_y = b->cur_y;
	}

	for (int i = 0; i < ne_lines - 2; i++) {
		b->wanted_y--; /* We want to move up */

		/* Can we move up? */
		if (b->wanted_y >= 0 /* We aren't yet off the top */
				&& b->wanted_y < b->num_lines - 1) { /* we aren't still past the end */
			b->cur_line_desc = (line_desc *)b->cur_line_desc->ld_node.prev;
			b->cur_line--;
		}

		/* Should we shift the view up? */
		if (b->win_y > 0 /* We aren't already at the top */
				&& b->win_y + b->wanted_cur_y > b->wanted_y) { /* Gap between virtual cursor and TOS is to small */
			b->top_line_desc = (line_desc *)b->top_line_desc->ld_node.prev;
			b->win_y--;
	 	}
	}

	b->cur_y = b->cur_line - b->win_y;

	keep_cursor_on_screen(b);
	if (b == cur_buffer) update_window(b);
	resync_pos(b);
	return OK;
}

int page_down(buffer * const b) {
	/* Already on the bottom line? */
	if (b->cur_line == b->num_lines - 1) return OK;

	update_syntax_states(b, -1, b->cur_line_desc, NULL);
	b->attr_len = -1;

	if (!b->y_wanted) {
		b->y_wanted = true;
		b->wanted_y = b->cur_line;
		b->wanted_cur_y = b->cur_y;
	}

	const int disp = ne_lines - 2;
	const int shift_view = (b->win_y + disp < b->num_lines); /* can't already see the last line */

	for (int i = 0; i < disp; i++) {
		b->wanted_y++; /* We want to move down */

		/* Can we move down? */
		if (b->wanted_y > 0 /* We aren't still above the top  */
				&&  b->wanted_y < b->num_lines) { /* we aren't yet to the end */
			b->cur_line_desc = (line_desc *)b->cur_line_desc->ld_node.next;
			b->cur_line++;
		}

		/* Should we shift the view down? */
		if (shift_view /* already decided we should */
				&& b->wanted_y - b->wanted_cur_y >  b->win_y) { /* Gap between virtual cursor and TOS is to big */
			b->top_line_desc = (line_desc *)b->top_line_desc->ld_node.next;
			b->win_y++;
	 	}
	}

	b->cur_y = b->cur_line - b->win_y;

	keep_cursor_on_screen(b);
	if (b == cur_buffer) update_window(b);
	resync_pos(b);
	return OK;
}

int move_tos(buffer * const b) {

	b->y_wanted = 0;

	if (b->cur_y > 0) {
		update_syntax_states(b, b->cur_y, b->cur_line_desc, NULL);
		b->cur_line -= b->cur_y;
		b->cur_y = 0;
		b->cur_line_desc = b->top_line_desc;
		b->attr_len = -1;
		resync_pos(b);
	}
	return OK;
}

int move_bos(buffer * const b) {

	b->y_wanted = 0;

	if (b->cur_y < ne_lines - 2) {
		update_syntax_states(b, b->cur_y, b->cur_line_desc, NULL);
		b->attr_len = -1;
		line_desc *ld_cur;
		if (b->win_y >= b->num_lines - (ne_lines - 1)) {
			ld_cur = b->top_line_desc;
			int i;
			for(i = 0; i < ne_lines - 2  &&	ld_cur->ld_node.next->next; i++)
				ld_cur = (line_desc *)ld_cur->ld_node.next;
			b->cur_line += (i - b->cur_y);
			b->cur_y = i;
		}
		else {
			b->cur_line += (ne_lines - 2 - b->cur_y);
			b->cur_y = ne_lines - 2;
			ld_cur = b->top_line_desc;
			for(int i = 0; i < ne_lines - 2; i++)
			  ld_cur = (line_desc *)ld_cur->ld_node.next;
		}
		b->cur_line_desc = ld_cur;
		resync_pos(b);
	}
	return OK;
}

/* adjust_view() never moves the cursor. It is only concerned with shifting
   win_x, cur_x, win_y and cur_y -- the variables which control which part
   of the file is visible in the terminal window. */

int adjust_view(buffer * const b, const char *p) {
	b->y_wanted = 0;

	if (!p) p = "t";
	int rc = OK;

	while(*p) {
		int disp = 0;
		char *q;
		int mag = max(0,strtol(p+1, &q, 0));
		switch (*p) {
			case 't' :
			case 'T' :
				/* Shift the view so that the current line is displayed at the top. */
				disp = mag ? -min(mag,b->cur_y) : -b->cur_y;
				break;

			case 'm' :
			case 'M' :
				/* Shift the view so that the current line is displayed at the center. */
				disp = (ne_lines - 2) / 2 - b->cur_y;
				break;

			case 'b' :
			case 'B' :
				/* Shift the view so that the current line is displayed at the bottom. */
				disp = mag ? min(mag,(ne_lines -2) - b->cur_y) : (ne_lines - 2) - b->cur_y;
				break;

			case 'l' :
			case 'L' :
				/* Shift the view as far left as possible, or mag columns. */
				if (mag == 0) mag = b->cur_x;
				while (b->cur_x >= b->opt.tab_size && mag > 0) {
					b->win_x += b->opt.tab_size;
					b->cur_x -= b->opt.tab_size;
					mag      -= b->opt.tab_size;
				}
				break;

			case 'c' :
			case 'C' :
				/* Shift the view as far left as possible. This way we don't have to
					deal with figuring out which side of Middle the view started on. */
				while (b->cur_x >= b->opt.tab_size) {
					b->win_x += b->opt.tab_size;
					b->cur_x -= b->opt.tab_size;
				}
				/* Since we now know that the cursor is left of center, we can start
					to shift the view right until the cursor is centered or until we
					run out of text to shift right. */
				while (b->cur_x < (ne_columns / 2) - (ne_columns / 2) % b->opt.tab_size && b->win_x >= b->opt.tab_size) {
					b->win_x -= b->opt.tab_size;
					b->cur_x += b->opt.tab_size;
				}
				break;

			case 'r' :
			case 'R' :
				/* Shift the view as far right as possible, or mag columns. */
				if (mag == 0) mag = b->win_x;
				while (b->cur_x < ne_columns - b->opt.tab_size && b->win_x >= b->opt.tab_size && mag > 0) {
					mag      -= b->opt.tab_size;
					b->win_x -= b->opt.tab_size;
					b->cur_x += b->opt.tab_size;
				}
				break;

			default  :
				/* When we hit a character we don't recognize, we set the rc, but
					we still process other valid view displacements. */
				rc = ERROR;
				break;
		}
		if (disp > 0) {
			for(int i = 0; i < disp && b->top_line_desc->ld_node.prev->prev; i++) {
				b->win_y--;
				b->cur_y++;
				b->top_line_desc = (line_desc *)b->top_line_desc->ld_node.prev;
			}
		}
		else if (disp < 0) {
			for(int i = 0; i > disp && b->top_line_desc->ld_node.next->next; i--) {
				b->win_y++;
				b->cur_y--;
				b->top_line_desc = (line_desc *)b->top_line_desc->ld_node.next;
			}
		}
		p = q;
	}

	if (b == cur_buffer) update_window(b);
	resync_pos(b);
	return rc;
}



void goto_line(buffer * const b, const int64_t n) {
	b->y_wanted = 0;

	if (n >= b->num_lines || n == b->cur_line) return;

	line_desc *ld;

	if (n >= b->win_y && n < b->win_y + ne_lines - 1) {
		update_syntax_states(b, -1, b->cur_line_desc, NULL);
		b->attr_len = -1;
		b->cur_y = n - b->win_y;
		b->cur_line = n;
		ld = b->top_line_desc;
		for(int64_t i = 0; i < b->cur_y; i++) ld = (line_desc *)ld->ld_node.next;
		b->cur_line_desc = ld;
		resync_pos(b);
		return;
	}

	update_syntax_states(b, -1, b->cur_line_desc, NULL);
	b->attr_len = -1;

	b->win_y = n - (ne_lines - 1) / 2;

	if (b->win_y > b->num_lines - (ne_lines - 1)) b->win_y = b->num_lines - (ne_lines - 1);
	if (b->win_y < 0) b->win_y = 0;

	b->cur_y = n - b->win_y;

	ld = nth_line_desc(b, n);

	b->cur_line = n;
	b->cur_line_desc = ld;
	for(int i = 0; i < b->cur_y; i++) ld = (line_desc *)ld->ld_node.prev;
	b->top_line_desc = ld;

	if (b == cur_buffer) update_window(b);
	resync_pos(b);
}



void goto_column(buffer * const b, const int64_t n) {

	b->x_wanted = 0;
	b->y_wanted = 0;

	if (n == b->win_x + b->cur_x) return;

	if (n >= b->win_x && n < b->win_x + ne_columns) {
		b->cur_x = n - b->win_x;
		resync_pos(b);
		return;
	}

	if ((b->win_x = n - ne_columns / 2)<0) b->win_x = 0;
	b->win_x -= b->win_x % b->opt.tab_size;
	b->cur_x = n - b->win_x;

	resync_pos(b);
	if (b == cur_buffer) update_window(b);
}



/* This is like a goto_column(), but you specify a position (i.e.,
a character offset) instead. */

void goto_pos(buffer * const b, const int64_t pos) {
	goto_column(b, calc_width(b->cur_line_desc, pos, b->opt.tab_size, b->encoding));
}



void move_to_sol(buffer * const b) {

	b->x_wanted = 0;
	b->y_wanted = 0;

	const bool update = b->win_x && b == cur_buffer;
	b->win_x =
	b->cur_x =
	b->cur_pos =
	b->cur_char = 0;

	if (update) update_window(b);
}


void move_to_eol(buffer * const b) {
	line_desc *ld = b->cur_line_desc;

	assert(ld->ld_node.next != NULL);
	assert((ld->line != NULL) == (ld->line_len != 0));

	b->x_wanted = 0;
	b->y_wanted = 0;

	if (!ld->line) {
		move_to_sol(b);
		return;
	}

	const int64_t total_width = calc_width(ld, ld->line_len, b->opt.tab_size, b->encoding);

	if (total_width >= b->win_x && total_width < b->win_x + ne_columns) {
		/* We move to a visible position. */
		b->cur_x = total_width - b->win_x;
		b->cur_pos = ld->line_len;
		b->cur_char = calc_char_len(ld, b->encoding);
		return;
	}

	for(int64_t i = 0, pos = 0, width = 0; pos < ld->line_len; pos = next_pos(ld->line, pos, b->encoding), i++)  {
		if (ld->line[pos] != '\t') width += get_char_width(&ld->line[pos], b->encoding);
		else width += b->opt.tab_size - width % b->opt.tab_size;

		if (total_width - width < ne_columns - b->opt.tab_size) {
			int64_t t = b->win_x;
			b->win_x = width - width % b->opt.tab_size;
			b->cur_x = total_width - b->win_x;
			b->cur_pos = ld->line_len;
			b->cur_char = calc_char_len(ld, b->encoding);
			if (t != b->win_x && b == cur_buffer) update_window(b);
			return;
		}
	}

	assert(false);
}



/* Sets the variables like a move_to_sof(), but does not perform any
   update. This is required in several places. */

void reset_position_to_sof(buffer * const b) {
	b->x_wanted =
	b->y_wanted =
	b->win_x =
	b->win_y =
	b->cur_x =
	b->cur_y =
	b->cur_line =
	b->cur_pos =
	b->cur_char = 0;
	b->attr_len = -1;
	b->cur_line_desc = b->top_line_desc = (line_desc *)b->line_desc_list.head;
}



void move_to_sof(buffer * const b) {
	const bool moving = b->win_x || b->win_y;

	if (moving) update_syntax_states(b, -1, b->cur_line_desc, NULL);
	else update_syntax_states(b, b->cur_y, b->cur_line_desc, NULL);

	reset_position_to_sof(b);
	if (moving && b == cur_buffer) update_window(b);
}



void move_to_bof(buffer * const b) {

	line_desc *ld = (line_desc *)b->line_desc_list.tail_pred;
	for(int i = 0; i < ne_lines - 2 && ld->ld_node.prev->prev; i++) ld = (line_desc *)ld->ld_node.prev;

	b->x_wanted = 0;
	b->y_wanted = 0;

	const int64_t old_win_x = b->win_x, old_win_y = b->win_y;
	b->cur_line = b->num_lines - 1;
	b->win_x = 0;
	b->win_y = ld->ld_node.prev->prev ? b->num_lines - (ne_lines - 1) : b->num_lines - 1;

	if (old_win_x != b->win_x || old_win_y != b->win_y) {
		update_syntax_states(b, -1, b->cur_line_desc, NULL);
		if (b == cur_buffer) reset_window();
	}
	else update_syntax_states(b, b->cur_y, b->cur_line_desc, NULL);
	b->attr_len = -1;

	if (!ld->ld_node.prev->prev) {
		b->win_y =
		b->cur_x =
		b->cur_char =
		b->cur_pos = 0;
		b->cur_y = b->num_lines - 1;
		b->top_line_desc = (line_desc *)b->line_desc_list.head;
		b->cur_line_desc = (line_desc *)b->line_desc_list.tail_pred;
	}
	else {
		b->win_x =
		b->cur_x =
		b->cur_char =
		b->cur_pos = 0;
		b->cur_y = ne_lines - 2;
		b->top_line_desc = ld;
		b->cur_line_desc = (line_desc *)b->line_desc_list.tail_pred;
	}
}



void toggle_sof_eof(buffer * const b) {
	if (b->cur_line == 0 && b->cur_pos == 0) {
		delay_update();
		move_to_bof(b);
		move_to_eol(b);
	}
	else move_to_sof(b);
}



void toggle_sol_eol(buffer * const b) {

	if (b->cur_pos == 0) move_to_eol(b);
	else move_to_sol(b);
}



/* Searches for the start of the next or previous word, depending on the value
   of dir. */

int search_word(buffer * const b, const int dir) {
	assert(dir == -1 || dir == 1);

	line_desc *ld = b->cur_line_desc;

	int64_t pos = b->cur_pos;
	bool word_started = false, space_skipped = false;

	if (pos >= ld->line_len) pos = ld->line_len;
	else if (!ne_isword(get_char(&ld->line[pos], b->encoding), b->encoding)) space_skipped = true;

	if (dir < 0 || pos < ld->line_len)
		pos = (dir > 0 ? next_pos : prev_pos)(ld->line, pos, b->encoding);

	int64_t y = b->cur_line;

	while(y < b->num_lines && y >= 0) {
		while(pos < ld->line_len && pos >= 0) {
			const int c = get_char(&ld->line[pos], b->encoding);
			if (!ne_isword(c, b->encoding)) space_skipped = true;
			else word_started = true;

			if (dir > 0) {
				if (space_skipped && ne_isword(c, b->encoding)) {
					goto_line(b, y);
					goto_pos(b, pos);
					return OK;
				}
			}
			else {
				if (word_started) {
					if (!ne_isword(c, b->encoding)) {
						goto_line(b, y);
						goto_pos(b, pos + 1);
						return OK;
					}
					else if (pos == 0) {
						goto_line(b, y);
						goto_pos(b, 0);
						return OK;
					}
				}
			}
			pos = (dir > 0 ? next_pos : prev_pos)(ld->line, pos, b->encoding);
		}

		space_skipped = true;

		if (dir > 0) {
			ld = (line_desc *)ld->ld_node.next;
			y++;
			pos = 0;
		}
		else {
			ld = (line_desc *)ld->ld_node.prev;
			y--;
			if (ld->ld_node.prev) pos = prev_pos(ld->line, ld->line_len, b->encoding);
		}
	}
	return ERROR;
}



/* Moves to the character after the end of the current word. It doesn't move at
   all on US-ASCII spaces and punctuation. */

void move_to_eow(buffer * const b) {

	line_desc *ld = b->cur_line_desc;
	int64_t pos = b->cur_pos;
	if (pos >= ld->line_len || !ne_isword(get_char(&ld->line[pos], b->encoding), b->encoding)) return;

	for(pos = b->cur_pos; pos < ld->line_len; pos += b->encoding == ENC_UTF8 ? utf8len(ld->line[pos]) : 1) 
		if (!ne_isword(get_char(&ld->line[pos], b->encoding), b->encoding)) break;

	goto_pos(b, pos);
}


/* Implements Brief's "incrementale move to the end": if we are in the middle
   of a line, we mode to the end of line; otherwise, if we are in the middle of
   a page, we move to the end of the page; otherwise, if we are in the middle
   of a file we move to the end of file. */

void move_inc_down(buffer * const b) {

	if (b->cur_pos == b->cur_line_desc->line_len) {
		if (b->cur_y == ne_lines - 2) move_to_bof(b);
		else next_page(b);
	}
	move_to_eol(b);
}

/* Same as above, towards the top. */

void move_inc_up(buffer * const b) {

	if (b->cur_pos == 0) {
		if (b->cur_y == 0) move_to_sof(b);
		else prev_page(b);
	}
	else move_to_sol(b);
}
