/* Input line handling.

	Copyright (C) 1993-1998 Sebastiano Vigna 
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
#include "termchar.h"

/* This is the maximum number of bytes which can be typed on the input
	line. The actual number of characters depends on the line encoding. */

#define MAX_INPUT_LINE_LEN		2048



/* Prints an input prompt in the input line. The prompt is assumed not to be
	UTF-8 encoded. A colon is postpended to the prompt. The position of the
	first character to use for input is returned. Moreover, the status bar is
	reset, so that it will be updated. */

static unsigned int print_prompt(const unsigned char * const prompt) {

	assert(prompt != NULL);

	move_cursor(ne_lines - 1, 0);

	clear_to_eol();

	standout_on();

	output_string(prompt, FALSE);
	output_string(":", FALSE);

	standout_off();

	output_string(" ", FALSE);

	reset_status_bar();

	return strlen(prompt) + 2;
}



/* Prompts the user for a yes/no answer. The prompt is assumed not to be UTF-8
	encoded. default_value has to be TRUE or FALSE. TRUE is returned if 'y' was
	typed, FALSE in any other case. Escaping is not allowed. RETURN returns the
	default value. */


int request_response(const buffer * const b, const char * const prompt, const int default_value) {

	const char response = request_char(b, prompt, default_value ? 'y' : 'n');

	if (response == 'Y') return TRUE;
	return FALSE;
}



/* Prompts the user for a single character answer. The prompt is assumed not to
	be UTF-8 encoded. default_value has to be an ISO-8859-1 character which is
	used for the default answer. The character typed by the user (upper cased)
	is returned. The default character is used if the user types RETURN. Note
	that the cursor is moved back to its current position. This offers a clear
	distinction between immediate and long inputs, and allows for interactive
	search and replace. */


char request_char(const buffer * const b, const char * const prompt, const char default_value) {

	int c;
	input_class ic;

	set_attr(0);
	print_prompt(prompt);

	if (default_value) output_char(default_value, -1, FALSE);
	move_cursor(b->cur_y, b->cur_x);

	while(TRUE) {
		do c = get_key_code(); while(c > 0xFF || (ic = CHAR_CLASS(c)) == IGNORE || ic == INVALID);

		switch(ic) {
			case ALPHA:
				return (char)localised_up_case[(unsigned char)c];

			case RETURN:
				return (char)localised_up_case[(unsigned char)default_value];

			default:
				break;
		}
	}
}



/* Requests a number, with a given prompt and default value. The prompt is
	assumed not to be UTF-8 encoded. Only nonnegative integers can be
	entered. The effects of a negative default_value are mysterious, and not
	worth an investigation. The returned value is nonnegative if something was
	entered, negative on escaping or on entering the empty string. */


int request_number(const char * const prompt, const int default_value) {

	static char t[MAX_INT_LEN], *result, *endptr;
	int n;

	if (default_value >= 0) sprintf(t, "%d", default_value);

	if (!(result = request(prompt, default_value >= 0 ? t : NULL, FALSE, 0, io_utf8))) return ABORT;
	if (!*result) return ERROR;

	n = strtol(result, &endptr, 0);

	return *endptr || n < 0 ? ERROR : n;
}



/* Requests a string. The prompt is assumed not to be UTF-8 encoded. The
	returned string is never longer than MAX_INPUT_LINE_LEN, and has been
	malloc()ed, so you can use it and then free() it. NULL is returned on
	escaping, or if entering an empty string (unless accept_null_string is TRUE,
	in which case the empty string is duplicated and
	returned). completion_allowed and prefer_utf8 work as in request(). */

char *request_string(const char * const prompt, const char * const default_string, const int accept_null_string, const int completion_allowed, const int prefer_utf8) {

	const char * const result = request(prompt, default_string, TRUE, completion_allowed, prefer_utf8);

	if (result && (*result || accept_null_string)) return str_dup(result);

	return NULL;
}

static buffer *history_buff = NULL;

static void init_history(void) {
	if (!history_buff){
		history_buff = alloc_buffer(NULL);
		if (history_buff) {
			const char * const history_filename = tilde_expand("~/.ne/.history");
			clear_buffer(history_buff);
			history_buff->opt.do_undo = 0;
			history_buff->opt.auto_prefs = 0;
			load_file_in_buffer(history_buff, history_filename);
			/* The history buffer is agnostic. The actual encoding of each line is detected dynamically. */
			history_buff->encoding = ENC_8_BIT;
			change_filename(history_buff, str_dup(history_filename));
			assert_buffer(history_buff);

			/* This should be never necessary with new histories, as all lines will
				be terminated by a life feed, but it is kept for backward
				compatibility. */
			
			move_to_bof(history_buff);
			if (history_buff->cur_line_desc->line && history_buff->cur_line_desc->line_len) {
				insert_stream(	history_buff,
									history_buff->cur_line_desc,
									history_buff->cur_line,
									history_buff->cur_line_desc->line_len,
									"", 1);
			}
		}
	}
	if (history_buff) {
		move_to_bof(history_buff);
		move_to_sol(history_buff);
	}
}

void close_history(void) {
	if (history_buff) {
		if (history_buff->is_modified) {
			while(history_buff->num_lines > 500) {
				move_to_sof(history_buff);
				delete_one_line(history_buff, history_buff->cur_line_desc, history_buff->cur_line);
				assert_buffer(history_buff);
			}
			save_buffer_to_file(history_buff,NULL);
		}
		free_buffer(history_buff);
		history_buff = NULL;
	}
}

static void add_to_history(const unsigned char * const str) {
	if (!history_buff || !str || !*str) return;

	move_to_bof(history_buff);

	/* This insert_stream() takes care of adding a line, including a life
		feed at the end. */

	insert_stream(history_buff,
						history_buff->cur_line_desc,
						history_buff->cur_line,
						history_buff->cur_line_desc->line_len,
						str,
						strlen(str) + 1);
	assert_buffer(history_buff);
}

/* request() is the main function that serves request_number() and
	request_string(). Given a prompt, a default string and a boolean flag which
	establish the possibility of any alphabetical input (as opposed to digits
	only), the user can edit a string of at most MAX_INPUT_LINE_LEN characters.
	Many useful commands can be used here. The string edited by the user is
	returned, or NULL if the input was escaped, or the empty string was entered.
	Note that presently this function always returns a pointer to a static
	buffer, but this could change in the future.

	completion_allowed has three possible values: 0 means no completion, 1
	complete as a filename, 2 complete as a command followed by a filename.

	If prefer_utf8 is true, editing an ASCII line inserting an ISO-8859-1 character
	will turn it into an UTF-8 line.
	
	request() relies on a number of auxiliary functions and static data. As
	always, we would really need nested functions, but, alas, C can't cope with
	them. */

static unsigned char input_buffer[MAX_INPUT_LINE_LEN + 1];

/* The current encoding of the command line. Contrarily to buffers, the command line may (and will) move
	back to ASCII if all non-US-ASCII characters are deleted .*/

static encoding_type encoding;

/* start_x  is the first usable x position for editing;
	len      is the current raw length of the input buffer (input_buffer[len] is always a NULL);
	x        is the x position of the cursor;
	pos      is the position of the cursor inside the input buffer;
	offset   is the first displayed buffer character. 
*/

static int start_x, len, pos, x, offset;

static int input_buffer_is_ascii() {
	return is_ascii(input_buffer, len);
}

/* The following functions perform basic editing actions on the input line. */

static void input_refresh(void) {
	int i, j;
	move_cursor(ne_lines - 1, start_x);
	for(i = start_x, j = offset; j < len; i += get_char_width(&input_buffer[j], encoding), j = next_pos(input_buffer, j, encoding)) {
		if (i + get_char_width(&input_buffer[j], encoding) >= ne_columns) break;
		output_char(get_char(&input_buffer[j], encoding), 0, encoding);
	}
	clear_to_eol();
	fflush(stdout);
}


static void input_move_left(const int do_refresh) {
	if (pos > 0) {

		const int x_delta = get_char_width(&input_buffer[pos = prev_pos(input_buffer, pos, encoding)], encoding);

		assert(pos >= 0);

		if (x == start_x) {
			offset = pos;

			if (char_ins_del_ok) {
				int i, j;
				for(i = start_x, j = offset; j < len && i + get_char_width(&input_buffer[j], encoding) < ne_columns; i += get_char_width(&input_buffer[j], encoding), j = next_pos(input_buffer, j, encoding));
				if (j < len) {
					move_cursor(ne_lines - 1, i);
					delete_chars(get_char_width(&input_buffer[j], encoding));
				}
				move_cursor(ne_lines - 1, start_x);
				insert_char(get_char(&input_buffer[pos], encoding), 0, encoding);
				move_cursor(ne_lines - 1, start_x);
			}
			else if (do_refresh) input_refresh();
		}
		else x -= x_delta;
	}
}


static void input_move_right(const int do_refresh) {
	const int prev_pos = pos;

	if (pos < len) {
		const int x_delta = get_char_width(&input_buffer[pos], encoding);
		pos = next_pos(input_buffer, pos, encoding);

		assert(pos <= len);

		if ((x += x_delta) >= ne_columns) {
			const int shift = x - (ne_columns - 1);
			int width = 0;

			do {
				width += get_char_width(&input_buffer[offset], encoding);
				offset = next_pos(input_buffer, offset, encoding);
			} while(width < shift && offset < len);

			assert(offset < len);

			x -= width;

			if (char_ins_del_ok) {
				int i, j;
				move_cursor(ne_lines - 1, start_x);
				delete_chars(width);
				move_cursor(ne_lines - 1, x - x_delta);
				output_char(get_char(&input_buffer[prev_pos], encoding), -1, encoding);

				i = x;
				j = pos;
				while(j < len && i + (width = get_char_width(&input_buffer[j], encoding)) < ne_columns) {
					output_char(get_char(&input_buffer[j], encoding), -1, encoding);
					i += width;
					j = next_pos(input_buffer, j, encoding);
				}
			}
			else if (do_refresh) input_refresh();
		}
	}
}


static void input_move_to_sol(void) {
	if (offset == 0) {
		x = start_x;
		pos = 0;
		return;
	}

	x = start_x;
	offset = pos = 0;
	input_refresh();
}


static void input_move_to_eol(void) {
	const int width_to_end = get_string_width(input_buffer + pos, len - pos, encoding);
	int width, prev, i;

	if (x + width_to_end < ne_columns) {
		x += width_to_end;
		pos = len;
		return;
	}

	x = start_x;
	i = ne_columns - 1 - start_x;
	pos = offset = len;

	for(;;) {
		prev = prev_pos(input_buffer, offset, encoding);
		width = get_char_width(&input_buffer[prev], encoding);
		if (i - width < 0) break; 
		offset = prev;
		i -= width;
		x += width;
	}
	input_refresh();
}


static void input_next_word(void) {
	int space_skipped = FALSE;

	while(pos < len) {
		const int c = get_char(&input_buffer[pos], encoding);
		if (ne_isspace(c, encoding) || ne_ispunct(c, encoding)) space_skipped = TRUE;
		else if (space_skipped) break;
		input_move_right(FALSE);
	}
	if (x == ne_columns - 1) {
		offset = pos;
		x = start_x;
	}
	input_refresh();
}


static void input_prev_word(void) {
	int word_skipped = FALSE, c;

	while(pos > 0) {
		input_move_left(FALSE);
		c = get_char(&input_buffer[pos], encoding);
		if (!(ne_isspace(c, encoding) || ne_ispunct(c, encoding))) word_skipped = TRUE;
		else if (word_skipped) {
			input_move_right(FALSE);
			break;
		}
	}
	input_refresh();
}


static void input_paste(void) {
	const clip_desc * const cd = get_nth_clip(cur_buffer->opt.cur_clip);
	int paste_len;

	if (cd) {
		if (cd->cs->encoding != ENC_ASCII && encoding != ENC_ASCII && cd->cs->encoding != encoding) {
			alert();
			return;
		}

		paste_len = strnlen_ne(cd->cs->stream, cd->cs->len);
		if (len + paste_len > MAX_INPUT_LINE_LEN) paste_len = MAX_INPUT_LINE_LEN - len;
		memmove(&input_buffer[pos + paste_len], &input_buffer[pos], len - pos + 1);
		strncpy(&input_buffer[pos], cd->cs->stream, paste_len);
		len += paste_len;
		if (!input_buffer_is_ascii() && cd->cs->encoding != ENC_ASCII) encoding = cd->cs->encoding;
		input_refresh();
	}
}


char *request(const char *prompt, const char * const default_string, const int alpha_allowed, const int completion_allowed, const int prefer_utf8) {

	action a;
	input_class ic;
	int c, c_len, c_width, first_char_typed = TRUE, last_char_completion = FALSE, selection = FALSE, quoted;
	unsigned char *completion, *prefix, *p;

	set_attr(0);

	input_buffer[pos = len = offset = 0] = 0;
	encoding = ENC_ASCII;
	x = start_x = print_prompt(prompt);

	init_history();
	
	if (default_string) {
		strncpy(input_buffer, default_string, MAX_INPUT_LINE_LEN);
		len = strlen(input_buffer);
		encoding = detect_encoding(input_buffer, len);
		input_refresh();
	}

	while(TRUE) {

		assert(input_buffer[len] == 0);

		move_cursor(ne_lines - 1, x);

		do c = get_key_code(); while((ic = CHAR_CLASS(c)) == IGNORE);

		/* ISO 10646 characters *above 256* can be added only to UTF-8 lines, or ASCII lines (making them, of course, UTF-8). */
		if (ic == ALPHA && c > 0xFF && encoding != ENC_ASCII && encoding != ENC_UTF8) ic = INVALID;

		if (ic != TAB) last_char_completion = FALSE;
		if (ic == TAB && !completion_allowed) ic = ALPHA;

		switch(ic) {

		case INVALID:
			alert();
			break;
					
		case ALPHA:

			if (first_char_typed) {
				input_buffer[len = 0] = 0;
				clear_to_eol();
			}
			
			if (encoding == ENC_ASCII && c > 0x7F) encoding = prefer_utf8 || c > 0xFF ? ENC_UTF8 : ENC_8_BIT;
			c_len = encoding == ENC_UTF8 ? utf8seqlen(c) : 1;
			c_width = output_width(c);
			assert(c_len > 0);

			if (len <= MAX_INPUT_LINE_LEN - c_len && (alpha_allowed || (c < 0x100 && isxdigit(c)) || c=='X' || c=='x')) {
				
				memmove(&input_buffer[pos + c_len], &input_buffer[pos], len - pos + 1);

				if (c_len == 1) input_buffer[pos] = c;
				else utf8str(c, &input_buffer[pos]);

				len += c_len;
				
				move_cursor(ne_lines - 1, x);

				if (x < ne_columns - c_width) {
					if (pos == len - c_len) output_char(c, -1, encoding);
					else if (char_ins_del_ok) insert_char(c, -1, encoding);
					else input_refresh();
				}

				input_move_right(TRUE);
			}
			break;

		case RETURN:
			selection = TRUE;
			break;


		case TAB:
			if (completion_allowed) {
				quoted = FALSE;
				if (len && input_buffer[len - 1] == '"') {
					input_buffer[len - 1] = 0;
					if (prefix = strrchr(input_buffer, '"')) {
						quoted = TRUE;
						prefix++;
					}
					else input_buffer[len - 1] = '"';
				}

				if (!quoted) {
					prefix = strrchr(input_buffer, ' ');
					if (prefix) prefix++;
					else prefix = input_buffer;
				}

				if (last_char_completion) {
					completion = p = request_files(prefix, TRUE);
					reset_window();
					if (completion) {
						if (*completion) selection = TRUE;
						else completion++;
					}
				}
				else {
					completion = p = complete(prefix);
					last_char_completion = TRUE;
					if (!completion) alert();
				}

				if (completion && (prefix - input_buffer) + strlen(completion) + 1 < MAX_INPUT_LINE_LEN) {
					const encoding_type completion_encoding = detect_encoding(completion, strlen(completion));
					if (encoding == ENC_ASCII || completion_encoding == ENC_ASCII || encoding == completion_encoding) {
						strcpy(prefix, completion);
						if (quoted) strcat(prefix, "\"");
						len = strlen(input_buffer);
						pos = offset = 0;
						x = start_x;
						if (encoding == ENC_ASCII) encoding = completion_encoding;
						input_move_to_eol();
						if (quoted) input_move_left(FALSE);
						input_refresh();
					}
					else alert();
				}
				else if (quoted) strcat(prefix, "\"");

				free(p);
			}
			break;
					

		case COMMAND:
			if (c < 0) c = -c - 1;
			if ((a = parse_command_line(key_binding[c], NULL, NULL, FALSE))>=0) {
				switch(a) {

				case LINEUP_A:
				case LINEDOWN_A:
				case MOVESOF_A:
				case MOVEEOF_A:
				case PAGEUP_A:
				case PAGEDOWN_A:
				case NEXTPAGE_A:
				case PREVPAGE_A:
					if (history_buff) {
						switch(a) {
						case LINEUP_A: line_up(history_buff); break;
						case LINEDOWN_A: line_down(history_buff); break;
						case MOVESOF_A: move_to_sof(history_buff); break;
						case MOVEEOF_A: move_to_bof(history_buff); break;
						case PAGEUP_A:
						case PREVPAGE_A: prev_page(history_buff); break;
						case PAGEDOWN_A:
						case NEXTPAGE_A: next_page(history_buff); break;
						}

						/* In some cases, the default displayed on the command line will be the same as the 
							first history item. In that case we skip it. */

						if (first_char_typed == TRUE 
							 && a == LINEUP_A 
							 && history_buff->cur_line_desc->line 
							 && !strncmp(history_buff->cur_line_desc->line, input_buffer, history_buff->cur_line_desc->line_len))
							line_up(history_buff);
						
						if (history_buff->cur_line_desc->line) {
							strncpy(input_buffer,
									  history_buff->cur_line_desc->line,
									  min(history_buff->cur_line_desc->line_len,MAX_INPUT_LINE_LEN));
							input_buffer[min(history_buff->cur_line_desc->line_len,MAX_INPUT_LINE_LEN)] = 0;
							len    = strlen(input_buffer);
							encoding = detect_encoding(input_buffer, len);
						}
						else {
							input_buffer[len = 0] = 0;
							encoding = ENC_ASCII;
						}
						
						x      = start_x;
						pos    = 0;
						offset = 0;
						input_refresh();
					}
					break;

				case MOVELEFT_A:
					input_move_left(TRUE);
					break;

				case MOVERIGHT_A:
					input_move_right(TRUE);
					break;

				case BACKSPACE_A:
					if (pos == 0) break;
					input_move_left(TRUE);

				case DELETECHAR_A:
					if (len > 0 && pos < len) {
						c_len = encoding == ENC_UTF8 ? utf8len(input_buffer[pos]) : 1;
						c_width = get_char_width(&input_buffer[pos], encoding);
						memmove(&input_buffer[pos], &input_buffer[pos + c_len], len - pos - c_len + 1);
						len -= c_len;
						if (input_buffer_is_ascii()) encoding = ENC_ASCII;

						if (char_ins_del_ok) {
							int i, j;

							move_cursor(ne_lines - 1, x);
							delete_chars(c_width);

							for(i = x, j = pos; j < len && i + get_char_width(&input_buffer[j], encoding) < ne_columns - c_width; i += get_char_width(&input_buffer[j], encoding), j = next_pos(input_buffer, j, encoding));

							if (j < len) {
								move_cursor(ne_lines - 1, i);
								while(j < len && i + get_char_width(&input_buffer[j], encoding) < ne_columns) {
									output_char(get_char(&input_buffer[j], encoding), -1, encoding);
									i += get_char_width(&input_buffer[j], encoding);
									j = next_pos(input_buffer, j, encoding);
								}
							}
						}
						else input_refresh();
					}
					break;

				case DELETELINE_A:
					move_cursor(ne_lines - 1, start_x);
					clear_to_eol();
					input_buffer[len = pos = offset = 0] = 0;
					encoding = ENC_ASCII;
					x = start_x;
					break;
 
				case DELETEEOL_A:
					input_buffer[len = pos] = 0;
					clear_to_eol();
					if (input_buffer_is_ascii()) encoding = ENC_ASCII;
					break;

				case MOVEINCUP_A:
					if (x != start_x) {
						pos = offset;
						x = start_x;
						break;
					}

				case MOVESOL_A:
					input_move_to_sol();
					break;

				case MOVEINCDOWN_A: {
					int i, j;
					for(i = x, j = pos; j < len && i + get_char_width(&input_buffer[j], encoding) < ne_columns; i += get_char_width(&input_buffer[j], encoding), j = next_pos(input_buffer, j, encoding));
					if (j != pos && j < len) {
						pos = j;
						x = i;
						break;
					}
				}

				case MOVEEOL_A:
					input_move_to_eol();
					break;

				case TOGGLESEOL_A:
				case TOGGLESEOF_A:
					if (pos != 0) input_move_to_sol();
					else input_move_to_eol();
					break;

				case PREVWORD_A:
					input_prev_word();
					break;

				case NEXTWORD_A:
					input_next_word();
					break;

				case REFRESH_A:
					input_refresh();
					break;

				case PASTE_A:
					input_paste();
					break;

				case ESCAPE_A:
					return NULL;

				default:
					break;
				}
			}
			break;

		default:
			break;
		}

		if (selection) {
			const line_desc * const last = (line_desc *)history_buff->line_desc_list.tail_pred->prev;
			assert(input_buffer[len] == 0);
			if (history_buff->num_lines == 0 || len != last->line_len || strncmp(input_buffer, last->line, last->line_len)) add_to_history(input_buffer);
			return input_buffer;
		}

		first_char_typed = FALSE;
	}
}
