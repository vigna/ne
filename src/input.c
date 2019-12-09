/* Input line handling.

   Copyright (C) 1993-1998 Sebastiano Vigna
   Copyright (C) 1999-2019 Todd M. Lewis and Sebastiano Vigna

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
#include "termchar.h"
#include <dirent.h>

/* This is the maximum number of bytes which can be typed on the input
   line. The actual number of characters depends on the line encoding. */

#define MAX_INPUT_LINE_LEN  2048

/* request() is the main function that serves request_number() and
   request_string(). Given a prompt, a default string and a boolean flag which
   establish the possibility of any alphabetical input (as opposed to digits
   only), the user can edit a string of at most MAX_INPUT_LINE_LEN characters.
   Many useful commands can be used here. The string edited by the user is
   returned, or NULL if the input was escaped, or the empty string was entered.
   Note that presently this function always returns a pointer to a static
   buffer, but this could change in the future.

   completion_type has several possible values:
    0 COMPLETE_NONE   means no completion,
    1 COMPLETE_FILE   complete as a filename,
    2                 complete as a command followed by a filename, (not implemented?)
    3 COMPLETE_SYNTAX complete as a recognized syntax name.

   If prefer_utf8 is true, editing an ASCII line inserting an ISO-8859-1 character
   will turn it into an UTF-8 line.

   request() relies on a number of auxiliary functions and static data. As
   always, we would really need nested functions, but, alas, C can't cope with
   them. */

typedef struct {
	int start_x;             /* first usable screen x position for editing */
	int len;                 /* current raw length of input buffer; buf[len] is always a NULL */
	int x;                   /* screen x position of the cursor */
	int pos;                 /* position of the cursor inside the input buffer */
	int offset;              /* first displayed buffer character */
	encoding_type encoding;  /* current encoding of the buffer */
	char buf[MAX_INPUT_LINE_LEN + 1]; /* input buffer itself */
} input_buf;

static input_buf ib;        /* our main input buffer */

/* Unlike ne's document buffers, the command line may (and will) move
   back to ASCII if all non-US-ASCII characters are deleted .*/

/* Prints an input prompt in the input line. The prompt is assumed not to be
   UTF-8 encoded. A colon is postpended to the prompt. The position of the
   first character to use for input is returned. Moreover, the status bar is
   reset, so that it will be updated. */

static unsigned int print_prompt(const char * const prompt, const bool show_help) {
	static bool help_shown;

	static const char *prior_prompt;
	assert(prompt != NULL || prior_prompt);

	resume_status_bar = (void (*)(const char *message))&input_and_prompt_refresh;
	if (prompt) prior_prompt = prompt;

	move_cursor(ne_lines - 1, 0);

	clear_to_eol();

	standout_on();
	set_attr(0);
	output_string(prior_prompt, false);

	if (show_help && !help_shown) {
		help_shown = true;
		output_string(info_msg[LONG_INPUT_HELP], false);
		ib.start_x = strlen(info_msg[LONG_INPUT_HELP]);
	} else ib.start_x = 0;
	output_string(":", false);

	standout_off();

	output_string(" ", false);

	reset_status_bar();

	return ib.start_x += strlen(prior_prompt) + 2;
}



/* Prompts the user for a yes/no answer. The prompt is assumed not to be UTF-8
   encoded. default_value has to be true or false. true is returned if 'y' was
   typed, false in any other case. Escaping is not allowed. RETURN returns the
   default value. */


bool request_response(const buffer * const b, const char * const prompt, const bool default_value) {
	return request_char(b, prompt, default_value ? 'y' : 'n') == 'Y' ? true : false;
}



/* Prompts the user for a single character answer. The prompt is assumed not to
   be UTF-8 encoded. default_value has to be an ISO-8859-1 character which is
   used for the default answer. The character typed by the user (upper cased)
   is returned. The default character is used if the user types RETURN. Note
   that the cursor is moved back to its current position. This offers a clear
   distinction between immediate and long inputs, and allows for interactive
   search and replace.

   We can get away with the INVALID_CHAR (window resizing) and SUSPEND_A/resume
   code only because this is only ever called in regular editing mode, not from
   requesters or command input.  */


char request_char(const buffer * const b, const char * const prompt, const char default_value) {
	print_prompt(prompt, false);

	if (default_value) output_char(default_value, 0, false);
	move_cursor(b->cur_y, b->cur_x);

	while(true) {
		int c;
		input_class ic;
		do c = get_key_code(); while(c > 0xFF || (ic = CHAR_CLASS(c)) == IGNORE);

		if (window_changed_size) {
			window_changed_size = false;
			reset_window();
			keep_cursor_on_screen((buffer * const)b);
			refresh_window((buffer *)b);
			print_prompt(NULL, false);
			if (default_value) output_char(default_value, 0, false);
			move_cursor(b->cur_y, b->cur_x);
		}

		if (c == INVALID_CHAR) continue; /* Window resizing. */

		switch(ic) {
			case ALPHA:
				return (char)localised_up_case[(unsigned char)c];

			case RETURN:
				return (char)localised_up_case[(unsigned char)default_value];

			case COMMAND:
				if (c < 0) c = -c - 1;
				const int a = parse_command_line(key_binding[c], NULL, NULL, false);
				if (a >= 0) {
					switch(a) {

					case SUSPEND_A:
						stop_ne();
					case REFRESH_A:
						reset_window();
						keep_cursor_on_screen((buffer * const)b);
						refresh_window((buffer *)b);
						print_prompt(NULL, false);
						if (default_value) output_char(default_value, 0, false);
						move_cursor(b->cur_y, b->cur_x);
						break;
					}
				}
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


int64_t request_number(const buffer * const b, const char * const prompt, const int64_t default_value) {
	static char t[MAX_INT_LEN];

	if (default_value >= 0) sprintf(t, "%" PRId64, default_value);

	char * const result = request(b, prompt, default_value >= 0 ? t : NULL, false, 0, io_utf8);
	if (!result) return ABORT;
	if (!*result) return ERROR;

	char *endptr;
	const int64_t n = strtoll(result, &endptr, 0);

	return *endptr || n < 0 ? ERROR : n;
}



/* Requests a string. The prompt is assumed not to be UTF-8 encoded. The
   returned string is never longer than MAX_INPUT_LINE_LEN, and has been
   malloc()ed, so you can use it and then free() it. NULL is returned on
   escaping, or if entering an empty string (unless accept_null_string is true,
   in which case the empty string is duplicated and
   returned). completion_type and prefer_utf8 work as in request(). */

char *request_string(const buffer * const b, const char * const prompt, const char * const default_string, const bool accept_null_string, const int completion_type, const bool prefer_utf8) {

	const char * const result = request(b, prompt, default_string, true, completion_type, prefer_utf8);

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
			   be terminated by a newline, but it is kept for backward compatibility. */

			move_to_bof(history_buff);
			if (history_buff->cur_line_desc->line && history_buff->cur_line_desc->line_len) {
				insert_stream(history_buff,
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
			save_buffer_to_file(history_buff, NULL);
		}
		free_buffer(history_buff);
		history_buff = NULL;
	}
}

static void add_to_history(const char * const str) {
	if (!history_buff || !str || !*str) return;

	move_to_bof(history_buff);

	/* This insert_stream() takes care of adding a line, including a line-feed
		at the end. */

	insert_stream(history_buff,
					  history_buff->cur_line_desc,
					  history_buff->cur_line,
					  history_buff->cur_line_desc->line_len,
					  str,
					  strlen(str) + 1);
	assert_buffer(history_buff);
}

static int input_buffer_is_ascii() {
	return is_ascii(ib.buf, ib.len);
}

/* The following functions perform basic editing actions on the input line. */

static void input_refresh(void) {
	move_cursor(ne_lines - 1, ib.start_x);
	ib.x = ib.start_x + ib.pos - ib.offset;
	for(int i = ib.start_x, j = ib.offset; j < ib.len; i += get_char_width(&ib.buf[j], ib.encoding), j = next_pos(ib.buf, j, ib.encoding)) {
		if (i + get_char_width(&ib.buf[j], ib.encoding) >= ne_columns) break;
		output_char(get_char(&ib.buf[j], ib.encoding), 0, ib.encoding);
	}
	clear_to_eol();
	fflush(stdout);
}

void input_and_prompt_refresh() {
	print_prompt(NULL, false);
	input_refresh();
}

static void input_autocomplete(void) {
	int dx = 0, prefix_pos = ib.pos;
	char *p;

	/* find a usable prefix */
	if (prefix_pos && prefix_pos <= ib.len) {
		prefix_pos = prev_pos(ib.buf, prefix_pos, ib.encoding);
		dx--;
		while (prefix_pos && ne_isword(get_char(&ib.buf[prefix_pos], ib.encoding), ib.encoding)) {
			dx--;
			prefix_pos = prev_pos(ib.buf, prefix_pos, ib.encoding);
		}
		if (! ne_isword(get_char(&ib.buf[prefix_pos], ib.encoding), ib.encoding)) {
			dx++;
			prefix_pos = next_pos(ib.buf, prefix_pos, ib.encoding);
		}
		p = malloc(ib.pos - prefix_pos + 1);
		if (!p) {
			alert(); /* OUT_OF_MEMORY */
			return;
		}
		strncpy(p, &ib.buf[prefix_pos], ib.pos - prefix_pos);
	}
	else p = malloc(1); /* no prefix left of the cursor; we'll give an empty one. */
	p[ib.pos - prefix_pos] = 0;

	int ac_err;
	if (p = autocomplete(p, NULL, true, &ac_err)) {
		encoding_type ac_encoding = detect_encoding(p, strlen(p));
		if (ac_encoding != ENC_ASCII && ib.encoding != ENC_ASCII && ac_encoding != ib.encoding) {
			free(p);
			alert();
		} else {
			ib.encoding = ac_encoding;

			if (prefix_pos < ib.pos) {
				memmove(&ib.buf[prefix_pos], &ib.buf[ib.pos], ib.len - ib.pos + 1);
				ib.len -= ib.pos - prefix_pos;
				ib.pos = prefix_pos;
			}
			int ac_len = strlen(p);
			if (ac_len + ib.len >= MAX_INPUT_LINE_LEN) ac_len = MAX_INPUT_LINE_LEN - ib.len;
			memmove(&ib.buf[ib.pos + ac_len], &ib.buf[ib.pos], ib.len - ib.pos + 1);
			memmove(&ib.buf[ib.pos], p, ac_len);
			ib.len += ac_len;
			while (ac_len > 0) {
				const int cw = get_char_width(&ib.buf[ib.pos], ib.encoding);
				ib.pos = next_pos(ib.buf, ib.pos, ib.encoding);
				ac_len -= cw;
				dx++;
			}
			free(p);
			ib.x += dx;
			if (ib.x >= ne_columns) {
				dx = ib.x - ne_columns + 1;
				while (dx--) {
					ib.offset = next_pos(ib.buf, ib.offset, ib.encoding);
				}
				ib.x = ne_columns - 1;
			}
		}
	}
	if (ac_err == AUTOCOMPLETE_COMPLETED || ac_err == AUTOCOMPLETE_CANCELLED) {
		do_action(cur_buffer, REFRESH_A, 0, NULL);
		refresh_window(cur_buffer);
		print_prompt(NULL, false);
	}
	input_refresh();
}


static void input_move_left(const bool do_refresh) {
	if (ib.pos > 0) {

		const int x_delta = get_char_width(&ib.buf[ib.pos = prev_pos(ib.buf, ib.pos, ib.encoding)], ib.encoding);

		assert(ib.pos >= 0);

		if (ib.x == ib.start_x) {
			ib.offset = ib.pos;

			if (char_ins_del_ok) {
				int i, j;
				for(i = ib.start_x, j = ib.offset; j < ib.len && i + get_char_width(&ib.buf[j], ib.encoding) < ne_columns; i += get_char_width(&ib.buf[j], ib.encoding), j = next_pos(ib.buf, j, ib.encoding));
				if (j < ib.len) {
					move_cursor(ne_lines - 1, i);
					delete_chars(get_char_width(&ib.buf[j], ib.encoding));
				}
				move_cursor(ne_lines - 1, ib.start_x);
				insert_char(get_char(&ib.buf[ib.pos], ib.encoding), 0, ib.encoding);
				move_cursor(ne_lines - 1, ib.start_x);
			}
			else if (do_refresh) input_refresh();
		}
		else ib.x -= x_delta;
	}
}


static void input_move_right(const bool do_refresh) {
	const int prev_pos = ib.pos;

	if (ib.pos < ib.len) {
		const int x_delta = get_char_width(&ib.buf[ib.pos], ib.encoding);
		ib.pos = next_pos(ib.buf, ib.pos, ib.encoding);

		assert(ib.pos <= ib.len);

		if ((ib.x += x_delta) >= ne_columns) {
			const int shift = ib.x - (ne_columns - 1);
			int width = 0;

			do {
				width += get_char_width(&ib.buf[ib.offset], ib.encoding);
				ib.offset = next_pos(ib.buf, ib.offset, ib.encoding);
			} while(width < shift && ib.offset < ib.len);

			assert(ib.offset < ib.len);

			ib.x -= width;

			if (char_ins_del_ok) {
				int i, j;
				move_cursor(ne_lines - 1, ib.start_x);
				delete_chars(width);
				move_cursor(ne_lines - 1, ib.x - x_delta);
				output_char(get_char(&ib.buf[prev_pos], ib.encoding), 0, ib.encoding);

				i = ib.x;
				j = ib.pos;
				while(j < ib.len && i + (width = get_char_width(&ib.buf[j], ib.encoding)) < ne_columns) {
					output_char(get_char(&ib.buf[j], ib.encoding), 0, ib.encoding);
					i += width;
					j = next_pos(ib.buf, j, ib.encoding);
				}
			}
			else if (do_refresh) input_refresh();
		}
	}
}


static void input_move_to_sol(void) {
	if (ib.offset == 0) {
		ib.x = ib.start_x;
		ib.pos = 0;
		return;
	}

	ib.x = ib.start_x;
	ib.offset = ib.pos = 0;
	input_refresh();
}


static void input_move_to_eol(void) {
	const int width_to_end = get_string_width(ib.buf + ib.pos, ib.len - ib.pos, ib.encoding);

	if (ib.x + width_to_end < ne_columns) {
		ib.x += width_to_end;
		ib.pos = ib.len;
		return;
	}

	ib.x = ib.start_x;
	ib.pos = ib.offset = ib.len;
	int i = ne_columns - 1 - ib.start_x;
	for(;;) {
		const int prev = prev_pos(ib.buf, ib.offset, ib.encoding);
		const int width = get_char_width(&ib.buf[prev], ib.encoding);
		if (i - width < 0) break;
		ib.offset = prev;
		i -= width;
		ib.x += width;
	}
	input_refresh();
}


static void input_next_word(void) {
	bool space_skipped = false;

	while(ib.pos < ib.len) {
		const int c = get_char(&ib.buf[ib.pos], ib.encoding);
		if (!ne_isword(c, ib.encoding)) space_skipped = true;
		else if (space_skipped) break;
		input_move_right(false);
	}
	if (ib.x == ne_columns - 1) {
		ib.offset = ib.pos;
		ib.x = ib.start_x;
	}
	input_refresh();
}


static void input_prev_word(void) {
	bool word_skipped = false;

	while(ib.pos > 0) {
		input_move_left(false);
		const int c = get_char(&ib.buf[ib.pos], ib.encoding);
		if (ne_isword(c, ib.encoding)) word_skipped = true;
		else if (word_skipped) {
			input_move_right(false);
			break;
		}
	}
	input_refresh();
}

/* Pastes the passed string into the input buffer, or the nth clip if str is NULL. */
static void input_paste(char *str) {
	const clip_desc * const cd = get_nth_clip(cur_buffer->opt.cur_clip);

	if (str || cd) {
		if (cd && cd->cs->encoding != ENC_ASCII && ib.encoding != ENC_ASCII && cd->cs->encoding != ib.encoding) {
			alert();
			return;
		}

		int paste_len = str ? strlen(str) : strnlen_ne(cd->cs->stream, cd->cs->len);
		if (ib.len + paste_len > MAX_INPUT_LINE_LEN) paste_len = MAX_INPUT_LINE_LEN - ib.len;
		memmove(&ib.buf[ib.pos + paste_len], &ib.buf[ib.pos], ib.len - ib.pos + 1);
		strncpy(&ib.buf[ib.pos], str ? str : cd->cs->stream, paste_len);
		ib.len += paste_len;
		if (!input_buffer_is_ascii() && cd->cs->encoding != ENC_ASCII) ib.encoding = cd->cs->encoding;
		input_refresh();
	}
}

static int request_history(void) {
	req_list rl;
	int i = -1;
	char *tmpstr;
	if (!history_buff) return -1;
	line_desc *ld = (line_desc *)history_buff->line_desc_list.tail_pred;

	if (ld->ld_node.prev && req_list_init(&rl, NULL, true, false, '\0')==OK) {
		while (ld->ld_node.prev) {
			if (ld->line_len) {
				tmpstr = strntmp(ld->line, ld->line_len);
				req_list_add(&rl, tmpstr, false);
			}
			ld = (line_desc *)ld->ld_node.prev;
		}
		rl.ignore_tab = false;
		rl.prune = true;
		rl.find_quits = true;
		req_list_finalize(&rl);
		i = request_strings(&rl, 0);
		if (i != ERROR) {
			int selection = i >= 0 ? i : -i - 2;
			if (i >= 0) {
				strncpy(ib.buf, rl.entries[selection], MAX_INPUT_LINE_LEN);
				ib.len = strlen(ib.buf);
				ib.encoding = detect_encoding(ib.buf, ib.len);
				input_move_to_sol();
			} else input_paste(rl.entries[selection]);
		}
		req_list_free(&rl);
	}
	strntmp(NULL, -1);
	return i >= 0 && i < history_buff->num_lines;
}

/* The filename completion function. Returns NULL if no file matches start_prefix,
   or the longest prefix common to all files extending start_prefix. */

static char *complete_filename(const char *start_prefix) {

	/* This might be NULL if the current directory has been unlinked, or it is not readable.
	   In that case, we end up moving to the completion directory. */
	char * const cur_dir_name = ne_getcwd(CUR_DIR_MAX_SIZE);

	char * const dir_name = str_dup(start_prefix);
	if (dir_name) {
		char * const p = (char *)file_part(dir_name);
		*p = 0;
		if (p != dir_name && chdir(tilde_expand(dir_name)) == -1) {
			free(dir_name);
			return NULL;
		}
	}

	start_prefix = file_part(start_prefix);
	bool is_dir, unique = true;
	char *cur_prefix = NULL;
	DIR * const d = opendir(CURDIR);

	if (d) {
		for(struct dirent * de; !stop && (de = readdir(d)); ) {
			if (is_prefix(start_prefix, de->d_name))
				if (cur_prefix) {
					cur_prefix[max_prefix(cur_prefix, de->d_name)] = 0;
					unique = false;
				}
				else {
					cur_prefix = str_dup(de->d_name);
					is_dir = is_directory(de->d_name);
				}
		}

		closedir(d);
	}

	char * result = NULL;

	if (cur_prefix) {
		result = malloc(strlen(dir_name) + strlen(cur_prefix) + 2);
		strcat(strcat(strcpy(result, dir_name), cur_prefix), unique && is_dir ? "/" : "");
	}

	if (cur_dir_name != NULL) {
		chdir(cur_dir_name);
		free(cur_dir_name);
	}
	free(dir_name);
	free(cur_prefix);

	return result;
}


char *request(const buffer * const b, const char *prompt, const char * const default_string, const bool alpha_allowed, const int completion_type, const bool prefer_utf8) {

	ib.buf[ib.pos = ib.len = ib.offset = 0] = 0;
	ib.encoding = ENC_ASCII;
	ib.x = ib.start_x = print_prompt(prompt, true);

	init_history();

	if (default_string) {
		strncpy(ib.buf, default_string, MAX_INPUT_LINE_LEN);
		ib.len = strlen(ib.buf);
		ib.encoding = detect_encoding(ib.buf, ib.len);
		input_refresh();
	}

	bool first_char_typed = true, last_char_completion = false, selection = false;

	while(true) {

		assert(ib.buf[ib.len] == 0);

		move_cursor(ne_lines - 1, ib.x);

		int c;
		input_class ic;
		do c = get_key_code(); while((ic = CHAR_CLASS(c)) == IGNORE);

		if (window_changed_size) {
			window_changed_size = false;
			reset_window();
			keep_cursor_on_screen((buffer * const)b);
			refresh_window((buffer *)b);
			input_and_prompt_refresh();
		}

		if (c == INVALID_CHAR) continue; /* Window resizing. */

		/* ISO 10646 characters *above 256* can be added only to UTF-8 lines, or ASCII lines (making them, of course, UTF-8). */
		if (ic == ALPHA && c > 0xFF && ib.encoding != ENC_ASCII && ib.encoding != ENC_UTF8) ic = INVALID;

		if (ic != TAB) last_char_completion = false;
		if (ic == TAB && !completion_type) ic = ALPHA;

		switch(ic) {

		case INVALID:
			alert();
			break;

		case ALPHA:

			if (first_char_typed) {
				ib.buf[ib.len = 0] = 0;
				clear_to_eol();
			}

			if (ib.encoding == ENC_ASCII && c > 0x7F) ib.encoding = prefer_utf8 || c > 0xFF ? ENC_UTF8 : ENC_8_BIT;
			int c_len = ib.encoding == ENC_UTF8 ? utf8seqlen(c) : 1;
			int c_width = output_width(c);
			assert(c_len > 0);

			if (ib.len <= MAX_INPUT_LINE_LEN - c_len && (alpha_allowed || (c < 0x100 && isxdigit(c)) || c=='X' || c=='x')) {

				memmove(&ib.buf[ib.pos + c_len], &ib.buf[ib.pos], ib.len - ib.pos + 1);

				if (c_len == 1) ib.buf[ib.pos] = c;
				else utf8str(c, &ib.buf[ib.pos]);

				ib.len += c_len;

				move_cursor(ne_lines - 1, ib.x);

				if (ib.x < ne_columns - c_width) {
					if (ib.pos == ib.len - c_len) output_char(c, 0, ib.encoding);
					else if (char_ins_del_ok) insert_char(c, 0, ib.encoding);
					else input_refresh();
				}

				input_move_right(true);
			}
			break;

		case RETURN:
			selection = true;
			break;

		case TAB:
			if (completion_type == COMPLETE_FILE || completion_type == COMPLETE_SYNTAX) {
				bool quoted = false;
				char *prefix, *completion, *p;
				if (ib.len && ib.buf[ib.len - 1] == '"') {
					ib.buf[ib.len - 1] = 0;
					if (prefix = strrchr(ib.buf, '"')) {
						quoted = true;
						prefix++;
					}
					else ib.buf[ib.len - 1] = '"';
				}

				if (!quoted) {
					prefix = strrchr(ib.buf, ' ');
					if (prefix) prefix++;
					else prefix = ib.buf;
				}

				if (last_char_completion || completion_type == COMPLETE_SYNTAX) {
					if (completion_type == COMPLETE_FILE )
						completion = p = request_files(prefix, true);
					else
						completion = p = request_syntax(prefix, true);
					reset_window();
					if (completion) {
						if (*completion) selection = true;
						else completion++;
					}
				}
				else {
					if (completion_type == COMPLETE_FILE )
						completion = p = complete_filename(prefix);
					else
						completion = p = request_syntax(prefix, true);
					last_char_completion = true;
					if (!completion) alert();
				}

				if (completion && (prefix - ib.buf) + strlen(completion) + 1 < MAX_INPUT_LINE_LEN) {
					const encoding_type completion_encoding = detect_encoding(completion, strlen(completion));
					if (ib.encoding == ENC_ASCII || completion_encoding == ENC_ASCII || ib.encoding == completion_encoding) {
						strcpy(prefix, completion);
						if (quoted) strcat(prefix, "\"");
						ib.len = strlen(ib.buf);
						ib.pos = ib.offset = 0;
						ib.x = ib.start_x;
						if (ib.encoding == ENC_ASCII) ib.encoding = completion_encoding;
						input_move_to_eol();
						if (quoted) input_move_left(false);
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
			const int a = parse_command_line(key_binding[c], NULL, NULL, false);
			if (a >= 0) {
				switch(a) {

				case SUSPEND_A:
					stop_ne();
					reset_window();
					keep_cursor_on_screen((buffer * const)b);
					refresh_window((buffer *)b);
					input_and_prompt_refresh();
					break;

				case FIND_A:
			      if (first_char_typed) {
				      ib.buf[ib.len = 0] = 0;
				      clear_to_eol();
			      }
					request_history();
					reset_window();
					keep_cursor_on_screen((buffer * const)b);
					refresh_window((buffer *)b);
					input_and_prompt_refresh();
					break;

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

						if (first_char_typed == true
							 && a == LINEUP_A
							 && history_buff->cur_line_desc->line
							 && !strncmp(history_buff->cur_line_desc->line, ib.buf, history_buff->cur_line_desc->line_len))
							line_up(history_buff);

						if (history_buff->cur_line_desc->line) {
							strncpy(ib.buf,
									  history_buff->cur_line_desc->line,
									  min(history_buff->cur_line_desc->line_len, MAX_INPUT_LINE_LEN));
							ib.buf[min(history_buff->cur_line_desc->line_len, MAX_INPUT_LINE_LEN)] = 0;
							ib.len = strlen(ib.buf);
							ib.encoding = detect_encoding(ib.buf, ib.len);
						}
						else {
							ib.buf[ib.len = 0] = 0;
							ib.encoding = ENC_ASCII;
						}

						ib.x      = ib.start_x;
						ib.pos    = 0;
						ib.offset = 0;
						input_refresh();
					}
					break;

				case MOVELEFT_A:
					input_move_left(true);
					break;

				case MOVERIGHT_A:
					input_move_right(true);
					break;

				case BACKSPACE_A:
					if (ib.pos == 0) break;
					input_move_left(true);

				case DELETECHAR_A:
					if (ib.len > 0 && ib.pos < ib.len) {
						int c_len = ib.encoding == ENC_UTF8 ? utf8len(ib.buf[ib.pos]) : 1;
						int c_width = get_char_width(&ib.buf[ib.pos], ib.encoding);
						memmove(&ib.buf[ib.pos], &ib.buf[ib.pos + c_len], ib.len - ib.pos - c_len + 1);
						ib.len -= c_len;
						if (input_buffer_is_ascii()) ib.encoding = ENC_ASCII;

						if (char_ins_del_ok) {
							int i, j;

							move_cursor(ne_lines - 1, ib.x);
							delete_chars(c_width);

							for(i = ib.x, j = ib.pos; j < ib.len && i + get_char_width(&ib.buf[j], ib.encoding) < ne_columns - c_width; i += get_char_width(&ib.buf[j], ib.encoding), j = next_pos(ib.buf, j, ib.encoding));

							if (j < ib.len) {
								move_cursor(ne_lines - 1, i);
								while(j < ib.len && i + get_char_width(&ib.buf[j], ib.encoding) < ne_columns) {
									output_char(get_char(&ib.buf[j], ib.encoding), 0, ib.encoding);
									i += get_char_width(&ib.buf[j], ib.encoding);
									j = next_pos(ib.buf, j, ib.encoding);
								}
							}
						}
						else input_refresh();
					}
					break;

				case DELETELINE_A:
					move_cursor(ne_lines - 1, ib.start_x);
					clear_to_eol();
					ib.buf[ib.len = ib.pos = ib.offset = 0] = 0;
					ib.encoding = ENC_ASCII;
					ib.x = ib.start_x;
					break;

				case DELETEEOL_A:
					ib.buf[ib.len = ib.pos] = 0;
					clear_to_eol();
					if (input_buffer_is_ascii()) ib.encoding = ENC_ASCII;
					break;

				case MOVEINCUP_A:
					if (ib.x != ib.start_x) {
						ib.pos = ib.offset;
						ib.x = ib.start_x;
						break;
					}

				case MOVESOL_A:
					input_move_to_sol();
					break;

				case MOVEINCDOWN_A: {
					int i, j;
					for(i = ib.x, j = ib.pos; j < ib.len && i + get_char_width(&ib.buf[j], ib.encoding) < ne_columns; i += get_char_width(&ib.buf[j], ib.encoding), j = next_pos(ib.buf, j, ib.encoding));
					if (j != ib.pos && j < ib.len) {
						ib.pos = j;
						ib.x = i;
						break;
					}
				}

				case MOVEEOL_A:
					input_move_to_eol();
					break;

				case TOGGLESEOL_A:
				case TOGGLESEOF_A:
					if (ib.pos != 0) input_move_to_sol();
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
					input_paste(NULL);
					break;

				case AUTOCOMPLETE_A:
					input_autocomplete();
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
			assert(ib.buf[ib.len] == 0);
			if (history_buff->num_lines == 0 || ib.len != last->line_len || strncmp(ib.buf, last->line, last->line_len)) add_to_history(ib.buf);
			return ib.buf;
		}

		first_char_typed = false;
	}
}
