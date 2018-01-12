/* Main command processing loop.

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
#include "support.h"
#include "version.h"
#include <limits.h>

/* ne's temporary file name template for the THROUGH command. */

#define NE_TMP ".ne-tmp.XXXXXX"

/* Turns an unspecified integer argument (-1) to 1. This
is what most commands require. */

#define NORMALIZE(x)  { x = (x)<0 ? 1 : (x); }

/* The length of the static message buffer. It must be larger by a factor
   of about three of the maximum screen width as UTF-8 encoded characters
   might take several characters per screen position. */

#define MAX_MESSAGE_SIZE (1024)

/* Here, given a mask represent a user flag and an integer i, we do as follows:
   i < 0 : toggle flag;
   i = 0 : clear flag;
   i > 0 : set flag;
*/

#define SET_USER_FLAG(b,i,x) {\
	if ((i)<0) (b)->x = !(b)->x;\
	else (b)->x = ((i) != 0);\
}
#define SET_GLOBAL_FLAG(c,f) {\
	if ((c)<0) (f) = !(f);\
	else (f) = ((c) != 0);\
}


/* Converts a non-positive result from request_number() to OK if the
   function was aborted or not-a-number error if an invalid number was read. */

#define NUMERIC_ERROR(c) ((c) == ABORT ? OK : NOT_A_NUMBER)


/* Do we want RepealLast to wrap on the next invocation? Upon NOT_FOUND from
   search/replace functions, set this to 2. do_action() reduces toward 0.*/

static int perform_wrap;


/* This is the dispatcher of all actions that have some effect on the text.

   The arguments are an action to be executed, a possible integer parameter and
   a possible string parameter. -1 and NULL are, respectively, reserved values
   meaning "no argument". For most operations, the integer argument is the
   number of repetitions. When an on/off choice is required, nonzero means on,
   zero means off, no argument means toggle.

   If there is a string argument (i.e. p != NULL), it is assumed that the
   action will consume p -- it ends up being free()d or stored
   somewhere. Though efficient, this has lead to some memory leaks (can you
   find them?). */

int do_action_wrapped(buffer *b, action a, int64_t c, char *p);
int do_action(buffer *b, action a, int64_t c, char *p) {
	static int da_depth = 0;
	static FILE *log;
	
	if (!log) log = fopen("/tmp/ne-actions.log","a");
	if (log) {
		fprintf(log,"%p%2d %ld,%ld(%ld) %s %ld '%s'\n",
		             b, da_depth++,
		                    b->cur_line,
		                        b->cur_pos,
		                           b->cur_char,
		                                command_names[a],
		                                   c,   p ? p : "<null>");
		fflush(log);
	}
	int rc = do_action_wrapped(b, a, c, p);
	da_depth--;
	return rc;
}

int do_action_wrapped(buffer *b, action a, int64_t c, char *p) {
	static char msg[MAX_MESSAGE_SIZE];
	line_desc *next_ld;
	HIGHLIGHT_STATE next_line_state;
	int error = OK, recording;
	int64_t col;
	char *q;

	assert(b->cur_pos >= 0);
	assert_buffer(b);
	assert_buffer_content(b);
	assert(b->encoding != ENC_UTF8 || b->cur_pos >= b->cur_line_desc->line_len || utf8len(b->cur_line_desc->line[b->cur_pos]) > 0);
	assert(b != cur_buffer || b->cur_x < ne_columns);
	assert(b != cur_buffer || b->cur_y < ne_lines - 1);
#ifndef NDEBUG
	if (b->syn && b->attr_len != -1) {
		HIGHLIGHT_STATE next_state = parse(b->syn, b->cur_line_desc, b->cur_line_desc->highlight_state, b->encoding == ENC_UTF8);
		assert(attr_len == b->attr_len);
		assert(memcmp(attr_buf, b->attr_buf, attr_len) == 0);
		assert(memcmp(&next_state, &b->next_state, sizeof next_state) == 0);
	}
#endif

	stop = false;

	if (b->recording) record_action(b->cur_macro, a, c, p, verbose_macros);

	if (perform_wrap > 0) perform_wrap--;

	switch(a) {

	case EXIT_A:
		if (save_all_modified_buffers()) {
			print_error(CANT_SAVE_EXIT_SUSPENDED);
			return ERROR;
		}
		else {
			close_history();
			unset_interactive_mode();
			exit(0);
		}
		return OK;

	case SAVEALL_A:
		if (save_all_modified_buffers()) {
			print_error(CANT_SAVE_ALL);
			return ERROR;
		}
		else print_message(info_msg[MODIFIED_SAVED]);
		return OK;

	case PUSHPREFS_A:
		NORMALIZE(c);
		for (int64_t i = 0; i < c && !(error = push_prefs(b)) && !stop; i++);
		return stop ? STOPPED : error ;

	case POPPREFS_A:
		NORMALIZE(c);
		for (int64_t i = 0; i < c && !(error = pop_prefs(b)) && !stop; i++);
		return stop ? STOPPED : error ;

	case QUIT_A:
		if (modified_buffers() && !request_response(b, info_msg[SOME_DOCUMENTS_ARE_NOT_SAVED], false)) return ERROR;
		close_history();
		unset_interactive_mode();
		exit(0);

	case LINEUP_A:
		NORMALIZE(c);
		for(int64_t i = 0; i < c && !(error = line_up(b)) && !stop; i++);
		return stop ? STOPPED : error;

	case LINEDOWN_A:
		NORMALIZE(c);
		for(int64_t i = 0; i < c && !(error = line_down(b)) && !stop; i++);
		return stop ? STOPPED : error;

	case PREVPAGE_A:
		NORMALIZE(c);
		for(int64_t i = 0; i < c && !(error = prev_page(b)) && !stop; i++);
		return stop ? STOPPED : error;

	case NEXTPAGE_A:
		NORMALIZE(c);
		for(int64_t i = 0; i < c && !(error = next_page(b)) && !stop; i++);
		return stop ? STOPPED : error;

	case MOVELEFT_A:
		NORMALIZE(c);
		for(int64_t i = 0; i < c && !(error = char_left(b)) && !stop; i++);
		return stop ? STOPPED : error;

	case MOVERIGHT_A:
		NORMALIZE(c);
		for(int64_t i = 0; i < c && !(error = char_right(b)) && !stop; i++);
		return stop ? STOPPED : error;

	case MOVESOL_A:
		move_to_sol(b);
		return OK;

	case MOVEEOL_A:
		move_to_eol(b);
		return OK;

	case MOVESOF_A:
		move_to_sof(b);
		return OK;

	case MOVEEOF_A:
		delay_update();
		move_to_bof(b);
		move_to_eol(b);
		return OK;

	case PAGEUP_A:
		NORMALIZE(c);
		for(int64_t i = 0; i < c && !(error = page_up(b)) && !stop; i++);
		return stop ? STOPPED : error;

	case PAGEDOWN_A:
		NORMALIZE(c);
		for(int64_t i = 0; i < c && !(error = page_down(b)) && !stop; i++);
		return stop ? STOPPED : error;

	case MOVETOS_A:
		error = move_tos(b);
		return error;

	case MOVEBOS_A:
		error = move_bos(b);
		return error;

	case ADJUSTVIEW_A:
		NORMALIZE(c);
		error = adjust_view(b, p);
		if (p) free(p);
		return error;

	case TOGGLESEOF_A:
		toggle_sof_eof(b);
		return OK;

	case TOGGLESEOL_A:
		toggle_sol_eol(b);
		return OK;

	case NEXTWORD_A:
		NORMALIZE(c);
		for(int64_t i = 0; i < c && !(error = search_word(b, 1)) && !stop; i++);
		return stop ? STOPPED : error;

	case PREVWORD_A:
		NORMALIZE(c);
		for(int64_t i = 0; i < c && !(error = search_word(b, -1)) && !stop; i++);
		return stop ? STOPPED : error;

	case DELETENEXTWORD_A:
	case DELETEPREVWORD_A:
		recording = b->recording;
		b->recording = 0;
		NORMALIZE(c);
		delay_update();
		start_undo_chain(b);
		for(int64_t i = 0; i < c && !error && !stop; i++) {
			int marking_t = b->marking;
			int mark_is_vertical_t = b->mark_is_vertical;
			b->bookmark[WORDWRAP_BOOKMARK].pos = b->block_start_pos;
			b->bookmark[WORDWRAP_BOOKMARK].line = b->block_start_line;
			b->bookmark_mask |= (1 << WORDWRAP_BOOKMARK);

			b->marking = 1;
			b->mark_is_vertical = 0;
			b->block_start_line = b->cur_line;
			b->block_start_pos = b->cur_pos;

			if(!(error = do_action(b, a == DELETENEXTWORD_A ? NEXTWORD_A : PREVWORD_A, 1, NULL))) {
				if (!(error = erase_block(b))) update_window_lines(b, b->cur_line_desc, b->cur_y, ne_lines - 2, false);
			}
			b->bookmark_mask &= ~(1 << WORDWRAP_BOOKMARK);
			b->block_start_pos = b->bookmark[WORDWRAP_BOOKMARK].pos;
			b->block_start_line = b->bookmark[WORDWRAP_BOOKMARK].line;
			b->marking = marking_t;
			b->mark_is_vertical = mark_is_vertical_t;
		}
		end_undo_chain(b);
		b->recording = recording;
		return stop ? STOPPED : error;

	case MOVEEOW_A:
		move_to_eow(b);
		return OK;

	case MOVEINCUP_A:
		move_inc_up(b);
		return OK;

	case MOVEINCDOWN_A:
		move_inc_down(b);
		return OK;

	case UNSETBOOKMARK_A:
		if (p && p[0]=='*' && !p[1]) { /* Special parm "*" for UNSETBOOKMARK_A */
			b->bookmark_mask = b->cur_bookmark = 0;
			print_message("All BookMarks cleared.");
			free(p);
			return OK;
		} /* Intentionally fall through to regular BOOKMARK parm parsing. */

	case SETBOOKMARK_A:
	case GOTOBOOKMARK_A:
		{
			bool relative = false;
			/* *p can be  "", "-", "0".."9", "+1","-1", for which, respectively, */
			/*  c becomes  0, AB,  0 .. 9,  next,prev. Anything else is out of range. */
			if (p) {
				if (p[0] == '?') {
					free(p);
					snprintf(msg, MAX_MESSAGE_SIZE, "Cur Bookmarks: [%s] %s (0-9, -1, +1, or '-')", cur_bookmarks_string(b), a==SETBOOKMARK_A?"SetBookmark":"GotoBookmark");
					p = request_string(b, msg, NULL, true, COMPLETE_NONE, b->encoding == ENC_UTF8 || b->encoding == ENC_ASCII && b->opt.utf8auto);
					if (!p) {
						return INVALID_BOOKMARK_DESIGNATION;
					}
				}
			}
			if (p) {
				if ((p[0]=='+' || p[0]=='-') && p[1]=='1') {
					if (b->cur_bookmark<0 || b->cur_bookmark>MAX_USER_BOOKMARK) b->cur_bookmark = 0;
					int i;
					for(i = 0; i<=MAX_USER_BOOKMARK; i++) {
						b->cur_bookmark = (b->cur_bookmark+MAX_USER_BOOKMARK+1+(p[0]=='+'?1:-1))%(MAX_USER_BOOKMARK+1);
						if ((a==SETBOOKMARK_A?~b->bookmark_mask:b->bookmark_mask) & (1<<b->cur_bookmark)) {
							c = b->cur_bookmark;
							relative = true;
							break;
						}
					}
					if (i==MAX_USER_BOOKMARK+1) {
						free(p);
						switch (a) {
						case SETBOOKMARK_A:
							return NO_UNSET_BOOKMARKS_TO_SET;
						case GOTOBOOKMARK_A:
							return NO_SET_BOOKMARKS_TO_GOTO;
						default:
							return NO_SET_BOOKMARKS_TO_UNSET;
						}
					}
				} else if (p[0]) {
					if (!p[1]) {
						if (*p == '-') c = AUTO_BOOKMARK;
						else c = *p - '0';
					}
					else c = -1;
				}
				else c = 0;
				free(p);
				if (c < 0 || c > AUTO_BOOKMARK) return INVALID_BOOKMARK_DESIGNATION;
			}
			else
				c = 0;
			switch(a) {
			case SETBOOKMARK_A:
				b->bookmark[c].pos = b->cur_pos;
				b->bookmark[c].line = b->cur_line;
				b->bookmark[c].cur_y = b->cur_y;
				b->bookmark_mask |= (1 << c);
				b->cur_bookmark = c;
				snprintf(msg, MAX_MESSAGE_SIZE, "Bookmark %c set", c <= MAX_USER_BOOKMARK?'0' + (int)c : '-');
				print_message(msg);
				break;
			case UNSETBOOKMARK_A:
				if (! (b->bookmark_mask & (1 << c)))
					return BOOKMARK_NOT_SET;
				b->bookmark_mask &= ~(1 << c);
				snprintf(msg, MAX_MESSAGE_SIZE, "Bookmark %c unset", c <= MAX_USER_BOOKMARK?'0' + (int)c : '-');
				print_message(msg);
				break;
			case GOTOBOOKMARK_A:
				if (! (b->bookmark_mask & (1 << c))) return BOOKMARK_NOT_SET;
				else {
					const int64_t prev_line = b->cur_line;
					const int64_t prev_pos = b->cur_pos;
					const int cur_y = b->cur_y;
					b->cur_bookmark = c;
					int avshift;
					delay_update();
					goto_line_pos(b, b->bookmark[c].line, b->bookmark[c].pos);
					if (avshift = b->cur_y - b->bookmark[c].cur_y) {
						snprintf(msg, MAX_MESSAGE_SIZE, "%c%d", avshift > 0 ? 'T' :'B', avshift > 0 ? avshift : -avshift);
						adjust_view(b, msg);
					}
					b->bookmark[AUTO_BOOKMARK].line = prev_line;
					b->bookmark[AUTO_BOOKMARK].pos = prev_pos;
					b->bookmark[AUTO_BOOKMARK].cur_y = cur_y;
					b->bookmark_mask |= 1<<AUTO_BOOKMARK;
					if (relative) {
						snprintf(msg, MAX_MESSAGE_SIZE, "At Bookmark %c", c <= MAX_USER_BOOKMARK?'0' + (int)c : '-');
						print_message(msg);
					}
				}
			default: ; /* Can't happen */
			}
			return OK;
		}

	case GOTOLINE_A:
		if (c < 0 && (c = request_number(b, "Line", b->cur_line + 1)) < 0) return NUMERIC_ERROR(c);
		if (c == 0 || c > b->num_lines) c = b->num_lines;
		goto_line(b, --c);
		return OK;

	case GOTOCOLUMN_A:
		if (c < 0 && (c = request_number(b, "Column", b->cur_x + b->win_x + 1)) < 0) return NUMERIC_ERROR(c);
		goto_column(b, c ? --c : 0);
		return OK;

	case INSERTSTRING_A:
		/* Since we are going to call another action, we do not want to record this insertion twice. */
		recording= b->recording;
		b->recording = 0;
		error = ERROR;
		if (p || (p = request_string(b, "String", NULL, false, COMPLETE_NONE, b->encoding == ENC_UTF8 || b->encoding == ENC_ASCII && b->opt.utf8auto))) {
			encoding_type encoding = detect_encoding(p, strlen(p));
			error = OK;
			start_undo_chain(b);
			/* We cannot rely on encoding promotion done by INSERTCHAR_A, because it could work
			   just for part of the string if UTF-8 auto-detection is not enabled. */
			if (b->encoding == ENC_ASCII || encoding == ENC_ASCII || (b->encoding == encoding)) {
				if (b->encoding == ENC_ASCII) b->encoding = encoding;
				for(int64_t pos = 0; p[pos] && error == OK; pos = next_pos(p, pos, encoding)) error = do_action(b, INSERTCHAR_A, get_char(&p[pos], encoding), NULL);
			}
			else error = INVALID_STRING;
			end_undo_chain(b);
			free(p);
		}
		b->recording = recording;
		return error;

	case TABS_A:
		SET_USER_FLAG(b, c, opt.tabs);
		return OK;

	case DELTABS_A:
		SET_USER_FLAG(b, c, opt.del_tabs);
		return OK;

	case SHIFTTABS_A:
		SET_USER_FLAG(b, c, opt.shift_tabs);
		return OK;

	case AUTOMATCHBRACKET_A:
		if (c < 0 && (c = request_number(b, "Match mode (sum of 0:none, 1:brightness, 2:inverse, 4:bold, 8:underline)", b->opt.automatch)) < 0 || c > 15) return ((c) == ABORT ? OK : INVALID_MATCH_MODE);
		b->opt.automatch = c;
		return OK;

	case INSERTTAB_A:
		recording = b->recording;
		b->recording = 0;
		NORMALIZE(c);
		start_undo_chain(b);
		if (b->opt.tabs) {
			while (c-- > 0) {
				error = do_action(b, INSERTCHAR_A, '\t', NULL);
			}
		}
		else {
			while (c-- > 0) {
				do {
					error = do_action(b, INSERTCHAR_A, ' ', NULL);
				} while (b->opt.tab_size && (b->win_x + b->cur_x) % b->opt.tab_size);
			}
		}
		end_undo_chain(b);
		b->recording = recording;
		return error;

	case INSERTCHAR_A: {
		static int last_inserted_char = ' ';
		int deleted_char, old_char;

		if (b->opt.read_only) return DOCUMENT_IS_READ_ONLY;

		if ((c < 0 || c > MAX_UTF_8) && ((c = request_number(b, "Char Code", last_inserted_char)) < 0 || c > MAX_UTF_8)) return NUMERIC_ERROR(c);
		if (c == 0) return CANT_INSERT_0;

		if (b->encoding == ENC_ASCII) {
			if (c > 0xFF) b->encoding = ENC_UTF8;
			else if (c > 0x7F) b->encoding = b->opt.utf8auto ? ENC_UTF8 : ENC_8_BIT;
		}
		if (c > 0xFF && b->encoding == ENC_8_BIT) return INVALID_CHARACTER;

		last_inserted_char = c;

		old_char = b->cur_pos < b->cur_line_desc->line_len ? get_char(&b->cur_line_desc->line[b->cur_pos], b->encoding) : 0;

		ensure_attributes(b);
		start_undo_chain(b);

		if (deleted_char = !b->opt.insert && b->cur_pos < b->cur_line_desc->line_len) delete_one_char(b, b->cur_line_desc, b->cur_line, b->cur_pos);
		if (b->cur_pos > b->cur_line_desc->line_len) {
			/* We insert spaces to reach the insertion position. */
			insert_spaces(b, b->cur_line_desc, b->cur_line, b->cur_line_desc->line_len, b->cur_pos - b->cur_line_desc->line_len);
			if (b->syn) update_line(b, b->cur_line_desc, b->cur_y, 0, true);
		}

		insert_one_char(b, b->cur_line_desc, b->cur_line, b->cur_pos, c);

		need_attr_update = true;

		/* At this point the line has been modified: note that if we are in overwrite mode and write a character
		   at or beyond the length of the current line, we are actually doing an insertion. */

		if (!deleted_char) update_inserted_char(b, c, b->cur_line_desc, b->cur_pos, b->cur_char, b->cur_y, b->cur_x);
		else update_overwritten_char(b, old_char, c, b->cur_line_desc, b->cur_pos, b->cur_char, b->cur_y, b->cur_x);

		char_right(b);

		/* Note the use of ne_columns-1. This avoids a double horizontal scrolling each time a
		   word wrap happens with b->opt.right_margin = 0. */

		error = b->opt.word_wrap && b->win_x + b->cur_x >= (b->opt.right_margin ? b->opt.right_margin : ne_columns - 1) ? word_wrap(b) : ERROR;

		if (error == ERROR) {
			assert_buffer_content(b);
			/* No word wrap. */
			if (b->syn) update_line(b, b->cur_line_desc, b->cur_y, 0, true);
			assert_buffer_content(b);
		}
		else {
			/* Fixes in case of word wrapping. */
			const bool wont_scroll = b->win_x == 0;
			int64_t a = 0;
			update_line(b, b->cur_line_desc, b->cur_y, calc_width(b->cur_line_desc, b->cur_line_desc->line_len, b->opt.tab_size, b->encoding) - b->win_x, false);

			need_attr_update = false;
			/* Poke the correct state into the next line. */
			if (b->syn) ((line_desc *)b->cur_line_desc->ld_node.next)->highlight_state = b->next_state;

			if (b->opt.auto_indent) a = auto_indent_line(b, b->cur_line + 1, (line_desc *)b->cur_line_desc->ld_node.next, INT_MAX);
			move_to_sol(b);
			line_down(b);
			goto_pos(b, error + a);

			if (wont_scroll) {
				if (b->cur_line == b->num_lines - 1) update_line(b, b->cur_line_desc, b->cur_y, 0, false);
				else scroll_window(b, b->cur_line_desc, b->cur_y, 1);
			}

			need_attr_update = true;
			assert_buffer_content(b);
		}

		end_undo_chain(b);
		return OK;
	}


	case BACKSPACE_A:
	case DELETECHAR_A:
		if (b->opt.read_only) return DOCUMENT_IS_READ_ONLY;
		NORMALIZE(c);
		start_undo_chain(b);
		for(int64_t i = 0; i < c && !stop; i++) {
			if (a == BACKSPACE_A) {
				if (b->cur_pos == 0) {
					if (b->cur_line == 0) {
						/* Start of buffer. We just return an error. */
						end_undo_chain(b);
						return ERROR;
					}
					/* We turn a backspace at the start of a line into a delete
					   at the end of the previous line. */
					char_left(b);
				}
				else {
					if (b->opt.del_tabs && (b->win_x + b->cur_x) % b->opt.tab_size == 0
						&& (b->cur_pos > b->cur_line_desc->line_len || b->cur_line_desc->line[b->cur_pos - 1] == ' ')) {
						/* We are deleting one or more spaces from a tabbing position. We go left until the
						   previous tabbing, or when spaces end. */
						int64_t back = 1;
						while((b->win_x + b->cur_x - back) % b->opt.tab_size != 0
							&& (b->cur_pos - back > b->cur_line_desc->line_len || b->cur_line_desc->line[b->cur_pos - back - 1] == ' ')) back++;
						goto_pos(b, b->cur_pos - back);
					}
					else char_left(b);
					/* If we are not over text, we are in free form mode; the backspace
					   is turned into moving to the left. */
					if (b->cur_pos >= b->cur_line_desc->line_len) continue;
				}
			}

			/* From here, we just implement a delete. */

			if (b->opt.del_tabs && b->cur_pos < b->cur_line_desc->line_len && b->cur_line_desc->line[b->cur_pos] == ' ' &&
				((b->win_x + b->cur_x) % b->opt.tab_size == 0 || b->cur_line_desc->line[b->cur_pos - 1] != ' ')) {
				col = 0;
				do col++; while((b->win_x + b->cur_x + col) % b->opt.tab_size != 0
									 && b->cur_pos + col < b->cur_line_desc->line_len
									 && b->cur_line_desc->line[b->cur_pos + col] == ' ');
				/* We are positioned at the start of the block of col spaces. If there is at most
				   one character to delete, we can just go on. Otherwise, we replace the block with a
				   TAB, doing some magick to keep everything in sync. */
				if (col > 1 && (b->win_x + b->cur_x + col) % b->opt.tab_size == 0) {
					if (b->syn) {
						ensure_attributes(b);
						memmove(b->attr_buf + b->cur_char + 1, b->attr_buf + b->cur_char + col, b->attr_len - (b->cur_char + col));
						b->attr_buf[b->cur_char] = -1;
						b->attr_len -= (col - 1);
					}
					delete_stream(b, b->cur_line_desc, b->cur_line, b->cur_pos, col);
					insert_one_char(b, b->cur_line_desc, b->cur_line, b->cur_pos, '\t');
				}
			}

			if (b->cur_pos > b->cur_line_desc->line_len) {
				col = b->win_x + b->cur_x;
				/* We are not over text; we must be in FreeForm mode.
				   We're deleting past the end of the line, so if we aren't on the last line
				   we need to pad this line with space up to col, then fall through to the
				   delete_one_char() below. */
				if (b->cur_line_desc->ld_node.next->next == NULL) continue;
				if (b->cur_line_desc->line_len == 0) {
					auto_indent_line(b, b->cur_line, b->cur_line_desc, col);
					resync_pos(b);
				}
				/* We need spaces if the line was not empty, or if we were sitting in the middle of a TAB. */
				insert_spaces(b, b->cur_line_desc, b->cur_line, b->cur_line_desc->line_len, col - calc_width(b->cur_line_desc, b->cur_line_desc->line_len, b->opt.tab_size, b->encoding));
				if (b->syn) store_attributes(b, b->cur_line_desc);
			}

			ensure_attributes(b);

			if (b->cur_pos < b->cur_line_desc->line_len) {
				/* Deletion inside a line. */
				const int old_char = b->encoding == ENC_UTF8 ? utf8char(&b->cur_line_desc->line[b->cur_pos]) : b->cur_line_desc->line[b->cur_pos];
				const uint32_t old_attr = b->syn ? b->attr_buf[b->cur_pos] : 0;
				if (b->syn) {
					/* Invalidate attrs beyond the right window edge. */
					const int64_t right_char = calc_char_len(b->cur_line_desc, calc_pos(b->cur_line_desc, b->win_x + ne_columns, b->opt.tab_size, b->encoding), b->encoding);
					if (right_char < b->attr_len) b->attr_len = right_char;
				}
				delete_one_char(b, b->cur_line_desc, b->cur_line, b->cur_pos);

				update_deleted_char(b, old_char, old_attr, b->cur_line_desc, b->cur_pos, b->cur_char, b->cur_y, b->cur_x);
				if (b->syn) update_line(b, b->cur_line_desc, b->cur_y, 0, true);
			}
			else {
				/* Here we handle the case in which two lines are joined. Note that if the first line is empty,
				   it is just deleted by delete_one_char(), so we must store its initial state and restore
				   it after the deletion. */
				if (b->syn && b->cur_pos == 0) next_line_state = b->cur_line_desc->highlight_state;
				delete_one_char(b, b->cur_line_desc, b->cur_line, b->cur_pos);
				if (b->syn && b->cur_pos == 0) b->cur_line_desc->highlight_state = next_line_state;

				update_line(b, b->cur_line_desc, b->cur_y, b->cur_x, true);

				if (b->cur_y < ne_lines - 2) scroll_window(b, (line_desc *)b->cur_line_desc->ld_node.next, b->cur_y + 1, -1);
			}
		}
		need_attr_update = true;
		end_undo_chain(b);
		return error ? error : stop ? STOPPED : 0;

	case INSERTLINE_A:
		if (b->opt.read_only) return DOCUMENT_IS_READ_ONLY;
		NORMALIZE(c);
		/* There SHOULD be a start_undo_chain(b) here, but in practice we've become used to using
		   a single Undo after an InsertLine to remove the autoindent space. Therefore, this sillyness.
		   If we had a real DeleteSOL command, we wouldn't need the Undo to undo InsertLine in two steps.
		   There could also be extant macros out there that expect Undo to revert autoindent and
		   InsertLine in two steps. So, again, this sillyness with excessive start_ and end_undo_chain. */
		for(int64_t i = 0; i < c && !stop; i++) {
			start_undo_chain(b);
			if (b->win_x == 0) ensure_attributes(b);

			if (insert_one_line(b, b->cur_line_desc, b->cur_line, b->cur_pos > b->cur_line_desc->line_len ? b->cur_line_desc->line_len : b->cur_pos) == OK) {
				end_undo_chain(b);
				if (b->win_x) {
					int64_t a = -1;
					/* If b->win_x is nonzero, the move_to_sol() call will
					   refresh the entire video, so we shouldn't do anything. However, we
					   must poke into the next line initial state the correct state. */
					if (b->syn) {
						b->attr_len = -1;
						ensure_attributes(b);
						((line_desc *)b->cur_line_desc->ld_node.next)->highlight_state = b->next_state;
					}

					if (b->opt.auto_indent) {
						start_undo_chain(b);
						a = auto_indent_line(b, b->cur_line + 1, (line_desc *)b->cur_line_desc->ld_node.next, INT_MAX);
						end_undo_chain(b);
					}

					move_to_sol(b);
					line_down(b);
					if (a != -1) goto_pos(b, a);
				}
				else {
					int64_t a = -1;
					update_line(b, b->cur_line_desc, b->cur_y, b->cur_x, false);
					/* We need to avoid updates until we fix the next line. */
					need_attr_update = false;
					/* We poke into the next line initial state the correct state. */
					if (b->syn) {
						b->attr_len = -1;
						ensure_attributes(b);
						((line_desc *)b->cur_line_desc->ld_node.next)->highlight_state = b->next_state;
					}

					if (b->opt.auto_indent) {
						start_undo_chain(b);
						a = auto_indent_line(b, b->cur_line + 1, (line_desc *)b->cur_line_desc->ld_node.next, INT_MAX);
						end_undo_chain(b);
					}

					move_to_sol(b);
					line_down(b);
					if (a != -1) goto_pos(b, a);

					if (b->cur_line == b->num_lines - 1) update_line(b, b->cur_line_desc, b->cur_y, 0, false);
					else scroll_window(b, b->cur_line_desc, b->cur_y, 1);

				}
				need_attr_update = true;
			} else end_undo_chain(b);
		}

		return stop ? STOPPED : 0;

	case DELETELINE_A:

		if (b->opt.read_only) return DOCUMENT_IS_READ_ONLY;
		NORMALIZE(c);

		col = b->win_x + b->cur_x;
		b->cur_pos = -1;
		start_undo_chain(b);
		for(int64_t i = 0; i < c && !stop; i++) {
			if (error = delete_one_line(b, b->cur_line_desc, b->cur_line)) break;
			scroll_window(b, b->cur_line_desc, b->cur_y, -1);
		}
		end_undo_chain(b);
		if (b->syn) {
			b->attr_len = -1;
			update_line(b, b->cur_line_desc, b->cur_y, 0, false);
			need_attr_update = true;
		}
		goto_column(b, col);

		return stop ? STOPPED : error;

	case UNDELLINE_A:

		if (b->opt.read_only) return DOCUMENT_IS_READ_ONLY;

		NORMALIZE(c);

		next_ld = (line_desc *)b->cur_line_desc->ld_node.next;

		start_undo_chain(b);
		for(int64_t i = 0; i < c && !stop; i++) {
			/* This is a bit tricky. First of all, if we are undeleting for
			   the first time and the local attribute buffer is not valid
			   we fill it. */
			if (i == 0) ensure_attributes(b);
			if (error = undelete_line(b)) break;
			if (i == 0) {
				if (b->syn) {
					/* Now the only valid part of the local attribute buffer is before b->cur_char. */
					if (b->cur_char < b->attr_len) b->attr_len = b->cur_char;
					update_line(b, b->cur_line_desc, b->cur_y, b->cur_x, false);
					next_line_state = b->next_state;
				}
				else update_line(b, b->cur_line_desc, b->cur_y, b->cur_x, false);
			}
			/* For each undeletion, we must poke into the next line its correct initial state. */
			if (b->syn) ((line_desc *)b->cur_line_desc->ld_node.next)->highlight_state = next_line_state;
			/* We actually scroll down the remaining lines, if necessary. */
			if (b->cur_y < ne_lines - 2) scroll_window(b, (line_desc *)b->cur_line_desc->ld_node.next, b->cur_y + 1, 1);
		}
		if (b->syn) {
			/* Finally, we force the update of the initial states of all following lines up to next_ld. */
			need_attr_update = true;
			update_syntax_states(b, b->cur_y, b->cur_line_desc, next_ld);
		}
		end_undo_chain(b);
		return stop ? STOPPED : error;

	case DELETEEOL_A:

		if (b->opt.read_only) return DOCUMENT_IS_READ_ONLY;
		ensure_attributes(b);
		delete_to_eol(b, b->cur_line_desc, b->cur_line, b->cur_pos);
		update_line(b, b->cur_line_desc, b->cur_y, b->cur_x, false);
		need_attr_update = true;

		return OK;

	case SAVE_A:
		p = str_dup(b->filename);

	case SAVEAS_A:
		if (p || (q = p = request_file(b, "Filename", b->filename))) {
			print_info(SAVING);

			if (buffer_file_modified(b, p) && !request_response(b, info_msg[a==SAVE_A ? FILE_HAS_BEEN_MODIFIED : FILE_ALREADY_EXISTS], false)) {
				free(p);
				return DOCUMENT_NOT_SAVED;
			}
			error = save_buffer_to_file(b, p);

			if (!print_error(error)) {
				const bool load_syntax = b->filename == NULL || ! same_str(extension(p), extension(b->filename));
				change_filename(b, p);
				if (load_syntax) {
					b->syn = NULL; /* So that autoprefs will load the right syntax. */
					load_auto_prefs(b, NULL); /* Will get extension from the name, or virtual extension. */
					reset_syntax_states(b);
					reset_window();
				}
				print_info(SAVED);
			}
			else {
				free(p);
				return ERROR;
			}
		}
		b->undo.last_save_step = b->undo.cur_step;
		return OK;

	case KEYCODE_A:
		if (c >= NUM_KEYS) c = -1;
		if (c < 0) {
			print_message(info_msg[PRESS_A_KEY]);
			do c = get_key_code(); while(c == INVALID_CHAR || c > 0xFF || CHAR_CLASS(c) == IGNORE);
		}
		col = (c < 0) ? -c-1 : c;
		snprintf(msg, MAX_MESSAGE_SIZE, "Key Code: 0x%02x,  Input Class: %s,  Assigned Command: %s", (int)col, input_class_names[CHAR_CLASS(c)],
		         (key_binding[col] && key_binding[col][0]) ? key_binding[col] : "(none)" );
		print_message(msg);
		return OK;

	case CLEAR_A:
		if ((b->is_modified) && !request_response(b, info_msg[THIS_DOCUMENT_NOT_SAVED], false)) return ERROR;
		clear_buffer(b);
		reset_window();
		return OK;

	case OPENNEW_A:
		b = new_buffer();
		reset_window();

	case OPEN_A:
		if ((b->is_modified) && !request_response(b, info_msg[THIS_DOCUMENT_NOT_SAVED], false)) {
			if (a == OPENNEW_A) do_action(b, CLOSEDOC_A, 1, NULL);
			return ERROR;
		}

		if (p || (p = request_file(b, "Filename", b->filename))) {
			static bool dprompt = 0; /* Set to true if we ever respond 'yes' to the prompt. */

			buffer *dup = get_buffer_named(p);

			/* 'c' -- flag meaning "Don't prompt if we've ever responded 'yes'." */
			if (!dup || dup == b || (dprompt && !c) || (dprompt = request_response(b, info_msg[SAME_NAME], false))) {
				error = load_file_in_buffer(b, p);
				if (error != FILE_IS_MIGRATED
					&& error != FILE_IS_DIRECTORY
					&& error != IO_ERROR
					&& error != FILE_IS_TOO_LARGE
					&& error != OUT_OF_MEMORY
					&& error != OUT_OF_MEMORY_DISK_FULL) {
					change_filename(b, p);
					b->syn = NULL; /* So that autoprefs will load the right syntax. */
					if (b->opt.auto_prefs) {
						if (b->allocated_chars - b->free_chars <= MAX_SYNTAX_SIZE) {
							if (load_auto_prefs(b, NULL) == HAS_NO_EXTENSION)
								load_auto_prefs(b, DEF_PREFS_NAME);
							reset_syntax_states(b);
						}
						else if (error == OK) error = FILE_TOO_LARGE_SYNTAX_HIGHLIGHTING_DISABLED;
					}
				}
				print_error(error);
				reset_window();
				return OK;
			}
			free(p);
		}
		if (a == OPENNEW_A) do_action(b, CLOSEDOC_A, 1, NULL);
		return ERROR;

	case ABOUT_A:
		about();
		return OK;

	case REFRESH_A:
		clear_entire_screen();
		ttysize();
		keep_cursor_on_screen(cur_buffer);
		reset_window();
		return OK;

	case FIND_A:
	case FINDREGEXP_A:
		if (p || (p = request_string(b, a == FIND_A ? "Find" : "Find RegExp", b->find_string, false, COMPLETE_NONE, b->encoding == ENC_UTF8 || b->encoding == ENC_ASCII && b->opt.utf8auto))) {

			const encoding_type encoding = detect_encoding(p, strlen(p));

			if (encoding != ENC_ASCII && b->encoding != ENC_ASCII && encoding != b->encoding) {
				free(p);
				return INCOMPATIBLE_SEARCH_STRING_ENCODING;
			}

			free(b->find_string);
			b->find_string = p;
			b->find_string_changed = 1;
			print_error(error = (a == FIND_A ? find : find_regexp)(b, NULL, false, false));
			if (error == NOT_FOUND) perform_wrap = 2;
			b->last_was_replace = 0;
			b->last_was_regexp = (a == FINDREGEXP_A);
		}

		return error ? ERROR : 0;

	case REPLACE_A:
	case REPLACEONCE_A:
	case REPLACEALL_A:

		if (b->opt.read_only) {
			free(p);
			return DOCUMENT_IS_READ_ONLY;
		}

		if ((q = b->find_string) || (q = request_string(b, b->last_was_regexp ? "Find RegExp" : "Find", NULL, false, COMPLETE_NONE, b->encoding == ENC_UTF8 || b->encoding == ENC_ASCII && b->opt.utf8auto))) {

			const encoding_type search_encoding = detect_encoding(q, strlen(q));

			if (search_encoding != ENC_ASCII && b->encoding != ENC_ASCII && search_encoding != b->encoding) {
				free(p);
				free(q);
				return INCOMPATIBLE_SEARCH_STRING_ENCODING;
			}

			if (q != b->find_string) {
				free(b->find_string);
				b->find_string = q;
				b->find_string_changed = 1;
			}

			if (p || (p = request_string(b, b->last_was_regexp ? "Replace RegExp" : "Replace", b->replace_string, true, COMPLETE_NONE, b->encoding == ENC_UTF8 || b->encoding == ENC_ASCII && b->opt.utf8auto))) {
				const encoding_type replace_encoding = detect_encoding(p, strlen(p));
				bool first_search = true;
				int64_t num_replace = 0;

				if (replace_encoding != ENC_ASCII && b->encoding != ENC_ASCII && replace_encoding != b->encoding ||
					search_encoding != ENC_ASCII && replace_encoding != ENC_ASCII && search_encoding != replace_encoding) {
					free(p);
					return INCOMPATIBLE_REPLACE_STRING_ENCODING;
				}

				c = 0;
				b->last_was_replace = 1;

				free(b->replace_string);
				b->replace_string = p;

				if (a == REPLACEALL_A) start_undo_chain(b);

				while(!stop &&
						!(error = (b->last_was_regexp ? find_regexp : find)(b, NULL, !first_search && a != REPLACEALL_A && c != 'A' && c != 'Y', false))) {

					if (c != 'A' && a != REPLACEALL_A && a != REPLACEONCE_A) {
						refresh_window(b);
						c = request_char(b, b->opt.search_back ? "Replace (Yes/No/Last/All/Quit/Forward)" : "Replace (Yes/No/Last/All/Quit/Backward)", 'n');
						if (c == 'Q') break;
						if (c == 'A') start_undo_chain(b);
					}

					if (c == 'A' || c == 'Y' || c == 'L' || a == REPLACEONCE_A || a == REPLACEALL_A) {
						/* We delay buffer encoding promotion until it is really necessary. */
						if (b->encoding == ENC_ASCII) b->encoding = replace_encoding;
						const int64_t cur_char = b->cur_char;
						const int cur_x = b->cur_x;

						if (b->last_was_regexp) error = replace_regexp(b, p);
						else error = replace(b, strlen(b->find_string), p);

						if (!error) {
							if (cur_char < b->attr_len) b->attr_len = cur_char;
							update_line(b, b->cur_line_desc, b->cur_y, cur_x, false);
							if (b->syn) {
								need_attr_update = true;
								update_syntax_states(b, b->cur_y, b->cur_line_desc, NULL);
							}

							num_replace++;

							if (last_replace_empty_match)
								if (b->opt.search_back) error = char_left(cur_buffer);
								else error = char_right(cur_buffer);
						}

						if (print_error(error)) {
							if (a == REPLACEALL_A || c == 'A') end_undo_chain(b);
							return ERROR;
						}
					}

					if (c == 'B' && !(b->opt.search_back) || c == 'F' && (b->opt.search_back)) {
						b->opt.search_back = !b->opt.search_back;
						b->find_string_changed = 1;
					}

					if (a == REPLACEONCE_A || c == 'L') break;

					first_search = false;
				}

				if (a == REPLACEALL_A || c == 'A') end_undo_chain(b);

				if (num_replace) {
					snprintf(msg, MAX_MESSAGE_SIZE, "%" PRId64 " replacement%s made.%s", num_replace, num_replace > 1 ? "s" : "", error == NOT_FOUND ? strchr(error_msg[NOT_FOUND], '(')-1 :"");
					print_message(msg);
				}
				if (stop) error = STOPPED;
				if (error == STOPPED) reset_window();
				if (error == NOT_FOUND) perform_wrap = 2;

				if (error && ((c != 'A' && a != REPLACEALL_A || first_search) || error != NOT_FOUND)) {
					print_error(error);
					return ERROR;
				}
				return OK;
			}
		}
		return ERROR;

	case REPEATLAST_A:
		if (b->opt.read_only && b->last_was_replace) return DOCUMENT_IS_READ_ONLY;
		if (!b->find_string) return NO_SEARCH_STRING;
		if ((b->last_was_replace) && !b->replace_string) return NO_REPLACE_STRING;

		const encoding_type search_encoding = detect_encoding(b->find_string, strlen(b->find_string));
		if (search_encoding != ENC_ASCII && b->encoding != ENC_ASCII && search_encoding != b->encoding) return INCOMPATIBLE_SEARCH_STRING_ENCODING;
		if (b->last_was_replace) {
			const encoding_type replace_encoding = detect_encoding(b->replace_string, strlen(b->replace_string));
			if (replace_encoding != ENC_ASCII && b->encoding != ENC_ASCII && replace_encoding != b->encoding ||
				 search_encoding != ENC_ASCII && replace_encoding != ENC_ASCII && search_encoding != replace_encoding)
				return INCOMPATIBLE_REPLACE_STRING_ENCODING;
		}

		NORMALIZE(c);

		error = OK;
		int64_t num_replace = 0;
		start_undo_chain(b);
		for (int64_t i = 0; i < c && ! stop && ! (error = (b->last_was_regexp ? find_regexp : find)(b, NULL, !b->last_was_replace, perform_wrap > 0)); i++)
			if (b->last_was_replace) {
				const int64_t cur_char = b->cur_char;
				const int cur_x = b->cur_x;

				if (b->last_was_regexp) error = replace_regexp(b, b->replace_string);
				else error = replace(b, strlen(b->find_string), b->replace_string);

				if (! error) {
					if (cur_char < b->attr_len) b->attr_len = cur_char;
					update_line(b, b->cur_line_desc, b->cur_y, cur_x, false);
					if (b->syn) {
						need_attr_update = true;
						update_syntax_states(b, b->cur_y, b->cur_line_desc, NULL);
					}

					num_replace++;

					if (last_replace_empty_match)
						if (b->opt.search_back) error = char_left(cur_buffer);
						else error = char_right(cur_buffer);
				}

				if (error) break;
			}

		end_undo_chain(b);
		if (num_replace) {
			snprintf(msg, MAX_MESSAGE_SIZE, "%" PRId64 " replacement%s made.%s", num_replace, num_replace > 1 ? "s" : "", error == NOT_FOUND ? strchr(error_msg[NOT_FOUND], '(')-1 :"");
			print_message(msg);
		}
		if (stop) error = STOPPED;
		if (error == STOPPED) reset_window();
		if (error == NOT_FOUND) perform_wrap = 2;
		return num_replace && error ? ERROR : error;

	case MATCHBRACKET_A:
		return print_error(match_bracket(b)) ? ERROR : 0;

	case ALERT_A:
		alert();
		return OK;

	case BEEP_A:
		ring_bell();
		return OK;

	case FLASH_A:
		do_flash();
		return OK;

	case ESCAPETIME_A:
		if (c < 0 && (c = request_number(b, "Timeout (1/10s)", -1))<0) return NUMERIC_ERROR(c);
		if (c < 256) {
			set_escape_time(c);
			return OK;
		}
		else return ESCAPE_TIME_OUT_OF_RANGE;

	case TABSIZE_A:
		if (c < 0 && (c = request_number(b, "TAB Size", b->opt.tab_size))<=0) return NUMERIC_ERROR(c);
		if (c < ne_columns / 2) {
			const int64_t pos = b->cur_pos;
			move_to_sol(b);
			b->opt.tab_size = c;
			goto_pos(b, pos);
			reset_window();
			return OK;
		}
		return TAB_SIZE_OUT_OF_RANGE;

	case TURBO_A:
		if ((int)c < 0 && (int)(c = request_number(b, "Turbo Threshold", turbo)) < 0) return NUMERIC_ERROR(c);
		turbo = c;
		return OK;

	case CLIPNUMBER_A:
		if ((int)c < 0 && (int)(c = request_number(b, "Clip Number", b->opt.cur_clip)) < 0) return NUMERIC_ERROR(c);
		b->opt.cur_clip = c;
		return OK;

	case RIGHTMARGIN_A:
		if ((int)c < 0 && (int)(c = request_number(b, "Right Margin", b->opt.right_margin)) < 0) return NUMERIC_ERROR(c);
		b->opt.right_margin = c;
		return OK;

	case FREEFORM_A:
		SET_USER_FLAG(b, c, opt.free_form);
		return OK;

	case PRESERVECR_A:
		SET_USER_FLAG(b, c, opt.preserve_cr);
		return OK;

	case CRLF_A:
		SET_USER_FLAG(b, c, is_CRLF);
		return OK;

	case VISUALBELL_A:
		SET_USER_FLAG(b, c, opt.visual_bell);
		return OK;

	case STATUSBAR_A:
		SET_GLOBAL_FLAG(c, status_bar);
		reset_status_bar();
		return OK;

	case HEXCODE_A:
		SET_USER_FLAG(b, c, opt.hex_code);
		reset_status_bar();
		return OK;

	case FASTGUI_A:
		SET_GLOBAL_FLAG(c, fast_gui);
		reset_status_bar();
		return OK;

	case INSERT_A:
		SET_USER_FLAG(b, c, opt.insert);
		return OK;

	case WORDWRAP_A:
		SET_USER_FLAG(b, c, opt.word_wrap);
		return OK;

	case AUTOINDENT_A:
		SET_USER_FLAG(b, c, opt.auto_indent);
		return OK;

	case VERBOSEMACROS_A:
		SET_GLOBAL_FLAG(c, verbose_macros);
		return OK;

	case AUTOPREFS_A:
		SET_USER_FLAG(b, c, opt.auto_prefs);
		return OK;

	case BINARY_A:
		SET_USER_FLAG(b, c, opt.binary);
		return OK;

	case NOFILEREQ_A:
		SET_USER_FLAG(b, c, opt.no_file_req);
		return OK;

	case REQUESTORDER_A:
		SET_GLOBAL_FLAG(c, req_order);
		return OK;

	case UTF8AUTO_A:
		SET_USER_FLAG(b, c, opt.utf8auto);
		return OK;

	case UTF8_A: {
		const encoding_type old_encoding = b->encoding, encoding = detect_buffer_encoding(b);

		if (c < 0 && b->encoding != ENC_UTF8 || c > 0) {
			if (encoding == ENC_ASCII || encoding == ENC_UTF8) b->encoding = ENC_UTF8;
			else return BUFFER_IS_NOT_UTF8;
		}
		else b->encoding = encoding == ENC_ASCII ? ENC_ASCII : ENC_8_BIT;
		if (old_encoding != b->encoding) {
			reset_syntax_states(b);
			reset_undo_buffer(&b->undo);
		}
		b->attr_len = -1;
		need_attr_update = false;
		move_to_sol(b);
		reset_window();
		return OK;
	}

	case MODIFIED_A:
		SET_USER_FLAG(b, c, is_modified);
		return OK;

	case UTF8IO_A:
		if (c < 0) io_utf8 = ! io_utf8;
		else io_utf8 = c != 0;
		reset_window();
		return OK;

	case DOUNDO_A:
		SET_USER_FLAG(b, c, opt.do_undo);
		if (!(b->opt.do_undo)) {
			reset_undo_buffer(&b->undo);
			b->atomic_undo = 0;
		}
		return OK;

	case READONLY_A:
		SET_USER_FLAG(b, c, opt.read_only);
		return OK;

	case CASESEARCH_A:
		SET_USER_FLAG(b, c, opt.case_search);
		b->find_string_changed = 1;
		return OK;

	case SEARCHBACK_A:
		SET_USER_FLAG(b, c, opt.search_back);
		b->find_string_changed = 1;
		return OK;

	case ATOMICUNDO_A:
		if (b->opt.do_undo) {
			/* set c to the desired b->link_undos */
			if (!p) {
				c = b->link_undos ? b->link_undos - 1 : 1;
			} else if (p[0]=='0') {
				c = 0;
			} else if (p[0]=='-') {
				c = b->link_undos ? b->link_undos - 1 : 0;
			} else if (p[0]=='+' || p[0]=='1') {  /* Kindly allow undocumented "AtomicUndo 1" also. */
				c = b->link_undos + 1;
			} else {
				free(p);
				return INVALID_LEVEL;
			}
			while(c > b->link_undos) start_undo_chain(b);
			while(c < b->link_undos) end_undo_chain(b);
			b->atomic_undo = (c > 0) ? 1 : 0;
			snprintf(msg, MAX_MESSAGE_SIZE, "AtomicUndo level: %" PRId64, c);
			print_message(msg);
			if (p) free(p);
			return OK;
		} else {
			if (p) free(p);
			return UNDO_NOT_ENABLED;
		}

	case RECORD_A:
		recording = b->recording;
		SET_USER_FLAG(b, c, recording);
		if (b->recording && !recording) {
			b->cur_macro = reset_stream(b->cur_macro);
			print_message(info_msg[STARTING_MACRO_RECORDING]);
		}
		else if (!b->recording && recording) print_message(info_msg[MACRO_RECORDING_COMPLETED]);
		return OK;

	case PLAY_A:
		if (!b->recording && !b->executing_internal_macro) {
			if (c < 0 && (c = request_number(b, "Times", 1))<=0) return NUMERIC_ERROR(c);
			b->executing_internal_macro = 1;
			for(int64_t i = 0; i < c && !(error = play_macro(b, b->cur_macro)); i++);
			b->executing_internal_macro = 0;
			return print_error(error) ? ERROR : 0;
		}
		else return ERROR;

	case SAVEMACRO_A:
		if (p || (p = request_file(b, "Macro Name", NULL))) {
			print_info(SAVING);
			optimize_macro(b->cur_macro, verbose_macros);
			if ((error = print_error(save_stream(b->cur_macro, p, b->is_CRLF, false))) == OK) print_info(SAVED);
			free(p);
			return error ? ERROR : 0;
		}
		return ERROR;

	case OPENMACRO_A:
		if (p || (p = request_file(b, "Macro Name", NULL))) {
			char_stream *cs;
			cs = load_stream(b->cur_macro, p, false, false);
			if (cs) b->cur_macro = cs;
			free(p);
			return cs ? 0 : ERROR;
		}
		return ERROR;

	case MACRO_A:
		if (p || (p = request_file(b, "Macro Name", NULL))) {
			error = print_error(execute_macro(b, p));
			free(p);
			return error ? ERROR : 0;
		}
		return ERROR;

	case UNLOADMACROS_A:
		unload_macros();
		return OK;

	case NEWDOC_A:
		new_buffer();
		reset_window();
		return OK;

	case CLOSEDOC_A:
		if ((b->is_modified) && !request_response(b, info_msg[THIS_DOCUMENT_NOT_SAVED], false)) return ERROR;
		if (!delete_buffer()) {
			close_history();
			unset_interactive_mode();
			exit(0);
		}
		keep_cursor_on_screen(cur_buffer);
		reset_window();

		/* We always return ERROR after a buffer has been deleted. Otherwise,
		   the calling routines (and macros) could work on an unexisting buffer. */

		return ERROR;

	case NEXTDOC_A: /* Was NEXT_BUFFER: */
		if (b->b_node.next->next) cur_buffer = (buffer *)b->b_node.next;
		else cur_buffer = (buffer *)buffers.head;
		keep_cursor_on_screen(cur_buffer);
		reset_window();
		need_attr_update = false;
		b->attr_len = -1;
		return OK;

	case PREVDOC_A:
		if (b->b_node.prev->prev) cur_buffer = (buffer *)b->b_node.prev;
		else cur_buffer = (buffer *)buffers.tail_pred;
		keep_cursor_on_screen(cur_buffer);
		reset_window();
		need_attr_update = false;
		b->attr_len = -1;
		return OK;

	case SELECTDOC_A: ;
		const int n = request_document();
		if (n < 0 || !(b = get_nth_buffer(n))) return ERROR;
		cur_buffer = b;
		keep_cursor_on_screen(cur_buffer);
		reset_window();
		need_attr_update = false;
		b->attr_len = -1;
		return OK;

	case MARK_A:
	case MARKVERT_A:
		if (c < 0) c = 1;
		SET_USER_FLAG(b, c, marking);
		if (!b->marking) return(OK);
		print_message(info_msg[a==MARK_A ? BLOCK_START_MARKED : VERTICAL_BLOCK_START_MARKED]);
		b->mark_is_vertical = (a == MARKVERT_A);
		b->block_start_line = b->cur_line;
		b->block_start_pos = b->cur_pos;
		return OK;

	case CUT_A:
		if (b->opt.read_only) return DOCUMENT_IS_READ_ONLY;

	case COPY_A:
		if (!(error = print_error((b->mark_is_vertical ? copy_vert_to_clip : copy_to_clip)(b, (int)c < 0 ? b->opt.cur_clip : c, a == CUT_A)))) {
			b->marking = 0;
			update_window_lines(b, b->cur_line_desc, b->cur_y, ne_lines - 2, false);
		}
		return error ? ERROR : 0;

	case ERASE_A:
		if (b->opt.read_only) return DOCUMENT_IS_READ_ONLY;
		if (!(error = print_error((b->mark_is_vertical ? erase_vert_block : erase_block)(b)))) {
			b->marking = 0;
			update_window_lines(b, b->cur_line_desc, b->cur_y, ne_lines - 2, false);
		}
		return OK;

	case PASTE_A:
	case PASTEVERT_A:
		if (b->opt.read_only) return DOCUMENT_IS_READ_ONLY;
		if (!(error = print_error((a == PASTE_A ? paste_to_buffer : paste_vert_to_buffer)(b, (int)c < 0 ? b->opt.cur_clip : c)))) update_window_lines(b, b->cur_line_desc, b->cur_y, ne_lines - 2, false);
		assert_buffer_content(b);
		return error ? ERROR : 0;

	case GOTOMARK_A:
		if (b->marking) {
			delay_update();
			goto_line_pos(b, b->block_start_line, b->block_start_pos);
			return OK;
		}
		print_error(MARK_BLOCK_FIRST);
		return ERROR;

	case OPENCLIP_A:
		if (p || (p = request_file(b, "Clip Name", NULL))) {
			error = print_error(load_clip(b->opt.cur_clip, p, b->opt.preserve_cr, b->opt.binary));
			free(p);
			return error ? ERROR : 0;
		}
		return ERROR;

	case SAVECLIP_A:
		if (p || (p = request_file(b, "Clip Name", NULL))) {
			print_info(SAVING);
			if ((error = print_error(save_clip(b->opt.cur_clip, p, b->is_CRLF, b->opt.binary))) == OK) print_info(SAVED);
			free(p);
			return error ? ERROR : 0;
		}
		return ERROR;

	case EXEC_A:
		if (p || (p = request_string(b, "Command", b->command_line, false, COMPLETE_FILE, b->encoding == ENC_UTF8 || b->encoding == ENC_ASCII && b->opt.utf8auto))) {
			free(b->command_line);
			b->command_line = p;
			return print_error(execute_command_line(b, p)) ? ERROR : 0;
		}
		return ERROR;

	case NAMECONVERT_A:
		q = NULL;
		if (b->filename && (p = ne_getcwd(CUR_DIR_MAX_SIZE))) {
			if (b->filename[0] == '/' && (c < 1))
				q = relative_file_path(b->filename, p);
			else if (b->filename[0] != '/' && (c != 0))
				q = absolute_file_path(b->filename, p);
			if (q) {
				change_filename(b, q);
				reset_status_bar();
			}
			free(p);
		}
		return OK;

	case SYSTEM_A:
		if (p || (p = request_string(b, "Shell command", NULL, false, COMPLETE_FILE, b->encoding == ENC_UTF8 || b->encoding == ENC_ASCII && b->opt.utf8auto))) {

			unset_interactive_mode();
			if (system(p)) error = EXTERNAL_COMMAND_ERROR;
			set_interactive_mode();

			free(p);
			ttysize();
			keep_cursor_on_screen(cur_buffer);
			reset_window();
			return print_error(error) ? ERROR : OK;
		}
		return ERROR;

	case THROUGH_A:

		if (b->opt.read_only) return DOCUMENT_IS_READ_ONLY;

		if (!b->marking) b->mark_is_vertical = 0;

		if (p || (p = request_string(b, "Filter", NULL, false, COMPLETE_FILE, b->encoding == ENC_UTF8 || b->encoding == ENC_ASCII && b->opt.utf8auto))) {
			int fin = -1, fout = -1;

			char tmpnam1[strlen(P_tmpdir)+strlen(NE_TMP)+2], tmpnam2[strlen(P_tmpdir)+strlen(NE_TMP)+2], *command;

			strcat(strcat(strcpy(tmpnam1, P_tmpdir), "/"), NE_TMP);
			strcat(strcat(strcpy(tmpnam2, P_tmpdir), "/"), NE_TMP);
			if ((fin = mkstemp(tmpnam1)) != -1) close(fin);
			if ((fout = mkstemp(tmpnam2)) != -1) close(fout);
			if (fin != -1 && fout != -1) {

				realloc_clip_desc(get_nth_clip(INT_MAX), INT_MAX, 0);

				if (!b->marking || !(error = (b->mark_is_vertical ? copy_vert_to_clip : copy_to_clip)(b, INT_MAX, false))) {

					if (!(error = save_clip(INT_MAX, tmpnam1, b->is_CRLF, b->opt.binary))) {
						if (command = malloc(strlen(p) + strlen(tmpnam1) + strlen(tmpnam2) + 16)) {

							strcat(strcat(strcat(strcat(strcat(strcpy(command, "( "), p), " ) <"), tmpnam1), " >"), tmpnam2);

							unset_interactive_mode();
							if (system(command)) error = EXTERNAL_COMMAND_ERROR;
							set_interactive_mode();

							if (!error) {

								if (!(error = load_clip(INT_MAX, tmpnam2, b->opt.preserve_cr, b->opt.binary))) {

									start_undo_chain(b);

									if (b->marking) (b->mark_is_vertical ? erase_vert_block : erase_block)(b);
									error = (b->mark_is_vertical ? paste_vert_to_buffer : paste_to_buffer)(b, INT_MAX);
									end_undo_chain(b);

									b->marking = 0;

									realloc_clip_desc(get_nth_clip(INT_MAX), INT_MAX, 0);
								}
							}
							free(command);
						}
						else error = OUT_OF_MEMORY;
					}
				}
			}
			else error = CANT_OPEN_TEMPORARY_FILE;

			remove(tmpnam1);
			remove(tmpnam2);

			ttysize();
			keep_cursor_on_screen(cur_buffer);
			reset_window();
			free(p);
			return print_error(error) ? ERROR : OK;
		}
		return ERROR;

	case TOUPPER_A:
		if (b->opt.read_only) return DOCUMENT_IS_READ_ONLY;
		NORMALIZE(c);
		start_undo_chain(b);
		for(int64_t i = 0; i < c && !(error = to_upper(b)) && !stop; i++);
		end_undo_chain(b);
		if (stop) error = STOPPED;
		return print_error(error) ? ERROR : 0;

	case TOLOWER_A:
		if (b->opt.read_only) return DOCUMENT_IS_READ_ONLY;
		NORMALIZE(c);
		start_undo_chain(b);
		for(int64_t i = 0; i < c && !(error = to_lower(b)) && !stop; i++);
		end_undo_chain(b);
		if (stop) error = STOPPED;
		return print_error(error) ? ERROR : 0;

	case CAPITALIZE_A:
		if (b->opt.read_only) return DOCUMENT_IS_READ_ONLY;
		NORMALIZE(c);
		start_undo_chain(b);
		for(int64_t i = 0; i < c && !(error = capitalize(b)) && !stop; i++);
		end_undo_chain(b);
		if (stop) error = STOPPED;
		return print_error(error) ? ERROR : 0;


	case CENTER_A:
		if (b->opt.read_only) return DOCUMENT_IS_READ_ONLY;
		NORMALIZE(c);
		start_undo_chain(b);
		for(int64_t i = 0; i < c && !(error = center(b)) && !stop; i++) {
			need_attr_update = true;
			b->attr_len = -1;
			update_line(b, b->cur_line_desc, b->cur_y, 0, false);
			move_to_sol(b);
			if (line_down(b) != OK) break;
		}
		end_undo_chain(b);

		if (stop) error = STOPPED;
		return print_error(error) ? ERROR : 0;

	case PARAGRAPH_A:
		if (b->opt.read_only) return DOCUMENT_IS_READ_ONLY;
		NORMALIZE(c);
		for(int64_t i = 0; i < c && !(error = paragraph(b)) && !stop; i++);
		if (stop) error = STOPPED;
		if (error == STOPPED) reset_window();
		return print_error(error) ? ERROR : 0;

	case SHIFT_A:
		if (b->opt.read_only) return DOCUMENT_IS_READ_ONLY;
		error = shift(b, p, &msg[0], MAX_MESSAGE_SIZE);
		if (stop) error = STOPPED;
		if (p) free(p);
		return print_error(error) ? ERROR : 0;

	case LOADPREFS_A:
		if (p || (p = request_file(b, "Prefs Name", NULL))) {
			error = print_error(load_prefs(b, p));
			free(p);
			return error ? ERROR : OK;
		}
		return ERROR;

	case SAVEPREFS_A:
		if (p || (p = request_file(b, "Prefs Name", NULL))) {
			error = print_error(save_prefs(b, p));
			free(p);
			return error ? ERROR : OK;
		}
		return ERROR;

	case LOADAUTOPREFS_A:
		return print_error(load_auto_prefs(b, NULL)) ? ERROR : OK;

	case SAVEAUTOPREFS_A:
		return print_error(save_auto_prefs(b, NULL)) ? ERROR : OK;

	case SAVEDEFPREFS_A:
		return print_error(save_auto_prefs(b, DEF_PREFS_NAME)) ? ERROR : OK;

	case SYNTAX_A:
		if (!do_syntax) return SYNTAX_NOT_ENABLED;
		if (p || (p = request_string(b, "Syntax",  b->syn ? (const char *)b->syn->name : NULL, true, COMPLETE_SYNTAX, b->encoding == ENC_UTF8 || b->encoding == ENC_ASCII && b->opt.utf8auto))) {
			if (!strcmp(p, "*")) b->syn = NULL;
			else error = print_error(load_syntax_by_name(b, p));
			reset_window();
			free(p);
			return error ? ERROR : OK;
		}
		return ERROR;

	case ESCAPE_A:
		handle_menus();
		return OK;

	case UNDO_A:
		if (b->opt.read_only) return DOCUMENT_IS_READ_ONLY;
		if (!b->opt.do_undo) return UNDO_NOT_ENABLED;

		NORMALIZE(c);
		delay_update();

		if (b->atomic_undo) {
			b->atomic_undo = 0;
			while (b->link_undos) end_undo_chain(b);
			print_message("AtomicUndo level: 0");
		}

		for(int64_t i = 0; i < c && !(error = undo(b)) && !stop; i++);
		if (stop) error = STOPPED;
		b->is_modified = b->undo.cur_step != b->undo.last_save_step;
		update_window(b);
		return print_error(error) ? ERROR : 0;

	case REDO_A:
		if (b->opt.read_only) return DOCUMENT_IS_READ_ONLY;
		if (!b->opt.do_undo) return UNDO_NOT_ENABLED;

		NORMALIZE(c);
		delay_update();

		for(int64_t i = 0; i < c && !(error = redo(b)) && !stop; i++);

		if (stop) error = STOPPED;
		b->is_modified = b->undo.cur_step != b->undo.last_save_step;
		update_window(b);
		return print_error(error) ? ERROR : 0;

	case FLAGS_A:
		help("FLAGS");
		reset_window();
		return OK;

	case HELP_A:
		help(p);
		if (p) free(p);
		reset_window();
		return OK;

	case SUSPEND_A:
		stop_ne();
		reset_window();
		keep_cursor_on_screen(cur_buffer);
		return OK;

	case AUTOCOMPLETE_A:
		/* Since we are going to call other actions (INSERTSTRING_A and DELETEPREVWORD_A),
		   we do not want to record this insertion twice. Also, we are counting on
		   INSERTSTRING_A to handle character encoding issues. */
		recording = b->recording;

		int64_t pos = b->cur_pos;

		if (!p) { /* no prefix given; find one left of the cursor. */
			if (context_prefix(b, &p, &pos)) return OUT_OF_MEMORY;
		}

		snprintf(msg, MAX_MESSAGE_SIZE, "AutoComplete: prefix \"%s\"", p);

		int e;
		if (p = autocomplete(p, msg, true, &e)) {
			b->recording = 0;
			start_undo_chain(b);
			if (pos >= b->cur_pos || (error = do_action(b, DELETEPREVWORD_A, 1, NULL)) == OK) error = do_action(b, INSERTSTRING_A, 0, p);
			end_undo_chain(b);
			b->recording = recording;
			print_message(info_msg[e]);
		}
		else if (stop) error = STOPPED;
		else if (e == AUTOCOMPLETE_NO_MATCH) print_message(info_msg[AUTOCOMPLETE_NO_MATCH]);

		return print_error(error) ? ERROR : 0;

	default:
		if (p) free(p);
		return OK;
	}
}
