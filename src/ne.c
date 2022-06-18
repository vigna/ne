/* main(), global initialization and global buffer functions.

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
   along with this program; if not, see <http://www.gnu.org/licenses/>.  */


#include "regex.h"

#include "ne.h"

#include "version.h"
#include "termchar.h"

#include <signal.h>
#include <limits.h>
#include <locale.h>

/* This is the array containing the "NO WARRANTY" message, which is displayed
   when ne is called without any specific file name or macro to execute. The
   message disappears as soon as any key is typed. */

char *NO_WARRANTY_msg[] = {   PROGRAM_NAME " " VERSION ".",
										"Copyright (C) 1993-1998 Sebastiano Vigna",
										"Copyright (C) 1999-2021 Todd M. Lewis and Sebastiano Vigna",
										"",
										"This program is free software; you can redistribute it and/or modify it under",
										"the terms of the GNU General Public License as published by the Free Software",
										"Foundation; either version 3 of the License, or (at your option) any later",
										"version.",
										"",
										"This program is distributed in the hope that it will be useful, but WITHOUT ANY",
										"WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A",
										"PARTICULAR PURPOSE.  See the GNU General Public License for more details.",
										"",
										"You should have received a copy of the GNU General Public License along with",
										"this program; if not, see <http://www.gnu.org/licenses/>.",
										"",
										"Press F1, Escape-Escape or Escape to see the menus. The shortcuts prefixed by ^",
										"are activated by the Control key; the shortcuts prefixed by [ are activated by",
										"Control+Meta or just Meta, depending on your terminal emulator. Alternatively,",
										"just press Escape followed by a letter.",
										"",
										"ne home page: http://ne.di.unimi.it/  GitHub repo: https://github.com/vigna/ne/",
										"Discuss ne at http://groups.google.com/group/niceeditor/",
										NULL
									};


char ARG_HELP[] = ABOUT_MSG "\n"
						"Usage: ne [options] [files]\n"
						"--help        print this message. [-h]\n"
						"--           *next token is a filename.\n"
						"+[N[,M]]     *move to last or N-th line, first or M-th column of next named file.\n"
						"--binary     *load the next file in binary mode.\n"
						"--read-only  *load the next file in read-only mode. [--readonly|--ro]\n"
						"--utf8        use UTF-8 I/O.\n"
						"--no-utf8     do not use UTF-8 I/O.\n"
						"--ansi        use built-in ANSI control sequences.\n"
						"--no-ansi     do not use built-in ANSI control sequences. [--noansi]\n"
						"--no-config   do not read configuration files. [--noconfig]\n"
						"--no-syntax   disable syntax-highlighting support.\n"
						"--prefs EXT   set autoprefs for the provided extension before loading the first file.\n"
						"--keys FILE   use this file for keyboard configuration.\n"
						"--menus FILE  use this file for menu configuration.\n"
						"--macro FILE  exec this macro after start.\n\n"
						"             *These options may appear multiple times.\n";


/* The regular expression used to parse the locale. */

#define LOCALE_REGEX "\\(UTF-?8\\)\\|\\(ISO-?8859-?\\)\\(1?[0-9]\\)"

/* These lists contain the existing buffers, clips and macros.
   cur_buffer denotes the currently displayed buffer. */

list buffers = { (node *)&buffers.tail, NULL, (node *)&buffers.head };
list clips = { (node *)&clips.tail, NULL, (node *)&clips.head };
char_stream *recording_macro;
bool executing_macro;
/* global prefs, only saved in ~/.ne/.default#ap if their
   current settings differ from these defaults. Make sure these
   defaults match the conditionals in prefs.c:save_prefs(). */
#ifndef ALTPAGING
bool req_order;
#else
bool req_order = true;
#endif
bool fast_gui;
bool status_bar = true;
bool verbose_macros = true;
/* end of global prefs */

buffer *cur_buffer;
int turbo;
bool do_syntax = true;

/* Whether we are currently displaying an about message. */
static bool displaying_info;

/* These functions live here because they access cur_buffer. new_buffer()
   creates a new buffer, adds it to the buffer list, and assign it to
   cur_buffer. delete_buffer() destroys cur_buffer, and makes the previous or
   next buffer the current buffer, if any of the two exists. */

buffer *new_buffer(void) {
	buffer *b = alloc_buffer(cur_buffer);

	if (b) {
		clear_buffer(b);
		if (cur_buffer) add(&b->b_node, &cur_buffer->b_node);
		else add_head(&buffers, &b->b_node);
		cur_buffer = b;
	}

	return b;
}

bool delete_buffer(void) {
	buffer *b = (buffer *)cur_buffer->b_node.next;

	rem(&cur_buffer->b_node);
	free_buffer(cur_buffer);

	if (! b->b_node.next) b = (buffer *)buffers.head;
	if (b == (buffer *)&buffers.tail) return false;

	cur_buffer = b;
	return true;
}

void about(void) {
	set_attr(0);
	clear_entire_screen();
	displaying_info = true;
	int i;
	for(i = 0; NO_WARRANTY_msg[i]; i++) {
		if (i == ne_lines - 1) break;
		move_cursor(i, 0);
		output_string(NO_WARRANTY_msg[i], false);
	}
	reset_window();

	char t[256] = ABOUT_MSG " Global directory";
	char * const gprefs_dir = exists_gprefs_dir();
	if (gprefs_dir) strncat(strncat(t, ": ", sizeof t - 1), gprefs_dir, sizeof t - 1);
	else strncat(strncat(strncat(t, " ", sizeof t - 1), get_global_dir(), sizeof t - 1), " not found!", sizeof t - 1);
	print_message(t);
}

/* The main() function. It is responsible for argument parsing, calling
   some terminal and signal initialization functions, and entering the
   event loop. */

int main(int argc, char **argv) {

	char *locale = setlocale(LC_ALL, "");
	for(int i = 0; i < 256; i++) localised_up_case[i] = toupper(i);

	if (locale) {
		struct re_pattern_buffer re_pb;
		struct re_registers re_reg;
		memset(&re_pb, 0, sizeof re_pb);
		memset(&re_reg, 0, sizeof re_reg);

		re_pb.translate = localised_up_case;
		re_compile_pattern(LOCALE_REGEX, strlen(LOCALE_REGEX), &re_pb);
		if (re_search(&re_pb, locale, strlen(locale), 0, strlen(locale), &re_reg) >= 0) {
			if (re_reg.start[1] >= 0) io_utf8 = true;
		}
		free(re_reg.start);
		free(re_reg.end);
	}

	bool no_config = false;
	char *macro_name = NULL, *key_bindings_name = NULL, *menu_conf_name = NULL, *startup_prefs_name = DEF_PREFS_NAME;

	char * const skiplist = calloc(argc, 1);
	if (!skiplist) exit(1);  /* We need this many flags. */

	for(int i = 1; i < argc; i++) {

		if (argv[i][0] == '-' && (!strcmp(&argv[i][1], "h") || !strcmp(&argv[i][1], "-help" "\0" VERSION_STRING))) {
			puts(ARG_HELP);
			exit(0);
		}

		/* Special arguments start with two dashes. If we find one, we
		   cancel its entry in argv[], so that it will be skipped when opening
		   the specified files. The only exception is +N for skipping to the
		   N-th line. */

		if (argv[i][0] == '-' && argv[i][1] == '-') {
			if (!argv[i][2]) i++; /* You can use "--" to force the next token to be a filename */
			else if (!strcmp(&argv[i][2], "noconfig") || !strcmp(&argv[i][2], "no-config")) {
				no_config = true;
				skiplist[i] = 1; /* argv[i] = NULL; */
			}
			else if (!strcmp(&argv[i][2], "noansi") || !strcmp(&argv[i][2], "no-ansi")) {
				ansi = false;
				skiplist[i] = 1; /* argv[i] = NULL; */
			}
			else if (!strcmp(&argv[i][2], "no-syntax")) {
				do_syntax = false;
				skiplist[i] = 1; /* argv[i] = NULL; */
			}
			else if (!strcmp(&argv[i][2], "prefs")) {
				if (i < argc-1) {
					startup_prefs_name = argv[i+1];
					skiplist[i] = skiplist[i+1] = 1; /* argv[i] = argv[i+1] = NULL; */
				}
			}
			else if (!strcmp(&argv[i][2], "ansi")) {
				ansi = true;
				skiplist[i] = 1; /* argv[i] = NULL; */
			}
			else if (!strcmp(&argv[i][2], "utf8")) {
				io_utf8 = true;
				skiplist[i] = 1; /* argv[i] = NULL; */
			}
			else if (!strcmp(&argv[i][2], "no-utf8")) {
				io_utf8 = false;
				skiplist[i] = 1; /* argv[i] = NULL; */
			}
			else if (!strcmp(&argv[i][2], "macro")) {
				if (i < argc-1) {
					macro_name = argv[i+1];
					skiplist[i] = skiplist[i+1] = 1; /* argv[i] = argv[i+1] = NULL; */
				}
			}
			else if (!strcmp(&argv[i][2], "keys")) {
				if (i < argc-1) {
					key_bindings_name = argv[i+1];
					skiplist[i] = skiplist[i+1] = 1; /* argv[i] = argv[i+1] = NULL; */
				}
			}
			else if (!strcmp(&argv[i][2], "menus")) {
				if (i < argc-1) {
					menu_conf_name = argv[i+1];
					skiplist[i] = skiplist[i+1] = 1; /* argv[i] = argv[i+1] = NULL; */
				}
			}
		}
	}

#ifdef NE_TEST
	{
		/* Dump the builtin menu and key bindings to compare to
		   doc/default.menus and doc/default.keys. */
		int dump_menu_config(FILE *f);
		int dump_key_config(FILE *f);
		FILE *f;

		if ((f = fopen("ne_test_dump_default_config", "w")) ) {
			dump_menu_config(f);
			dump_key_config(f);
			fclose(f);
		}
	}
#endif

	/* Unless --noconfig was specified, we try to configure the
	   menus and the keyboard. Note that these functions can exit() on error. */

	if (!no_config) {
		get_menu_configuration(menu_conf_name);
		get_key_bindings(key_bindings_name);
	}

#ifdef NE_TEST
	{
		/* Dump the builtin menu and key bindings to compare to
		   doc/default.menus and doc/default.keys. */
		int dump_menu_config(FILE *f);
		int dump_key_config(FILE *f);
		FILE *f;

		if ((f = fopen("ne_test_dump_loaded_config", "w")) ) {
			dump_menu_config(f);
			dump_key_config(f);
			fclose(f);
		}
	}
#endif

	/* If we cannot even create a buffer, better go... */

	if (!new_buffer()) exit(1);

	/* Now that key_bindings are loaded, try to fix up the
	   NOT_FOUND error_msg and the LONG_INPUT_HELP info_msg. */
	{
		char *keystroke_string, *new_msg_text;
		if ((keystroke_string = find_key_strokes(REPEATLAST_A, 1))) {
			if ((new_msg_text = malloc(39+strlen(keystroke_string)))) {
				strcat(strcat(strcpy(new_msg_text, "Not Found. (RepeatLast with "), keystroke_string), " to wrap.)");
				error_msg[NOT_FOUND] = new_msg_text;
			}
			free(keystroke_string);
		}
		if ((keystroke_string = find_key_strokes(FIND_A, 1))) {
			if ((new_msg_text = malloc(24+strlen(keystroke_string)))) {
				strcat(strcat(strcpy(new_msg_text, " (browse history with "), keystroke_string), ")");
				info_msg[LONG_INPUT_HELP] = new_msg_text;
			}
			free(keystroke_string);
		}
	}

	clear_buffer(cur_buffer);

	/* The INT_MAX clip always exists, and it is used by the Through command. */

	clip_desc * const cd = alloc_clip_desc(INT_MAX, 0);
	if (!cd) exit(1);

	add_head(&clips, &cd->cd_node);

	/* General terminfo and cursor motion initialization. From here onwards,
	   we cannot exit() lightly. */

	term_init();

	/* We will be always using the last line for the status bar. */

	set_terminal_window(ne_lines-1);

	/* We read in all the key capabilities. */

	read_key_capabilities();

	/* Some initializations of other modules... */

	re_set_syntax(
		RE_CONTEXT_INDEP_ANCHORS |
		RE_CONTEXT_INDEP_OPS     | RE_HAT_LISTS_NOT_NEWLINE |
		RE_NEWLINE_ALT           | RE_NO_BK_PARENS          |
		RE_NO_BK_VBAR            | RE_NO_EMPTY_RANGES
	);

	bool first_file = true;

	load_virtual_extensions();
	load_auto_prefs(cur_buffer, startup_prefs_name);

	buffer *stdin_buffer = NULL;
	if (!isatty(fileno(stdin))) {
		first_file = false;
		const int error = load_fd_in_buffer(cur_buffer, fileno(stdin));
		print_error(error);
		stdin_buffer = cur_buffer;

		if (!(freopen("/dev/tty", "r", stdin))) {
			fprintf(stderr, "Cannot reopen input tty\n");
			abort();
		}
	}

	/* The terminal is prepared for interactive I/O. */

	set_interactive_mode();

	clear_entire_screen();

	/* This function sets fatal_code() as signal interrupt handler
	   for all the dangerous signals (SIGILL, SIGSEGV etc.). */

	set_fatal_code();

	if (argc > 1) {

		/* The first file opened does not need a NEWDOC_A action. Note that
		   file loading can be interrupted (wildcarding can sometimes produce
		   unwanted results). */

		uint64_t first_line = 0, first_col = 0;
		bool binary = false, skip_plus = false, read_only = false;
		stop = false;

		for(int i = 1; i < argc && !stop; i++) {
			if (argv[i] && !skiplist[i]) {
				if (argv[i][0] == '+' && !skip_plus) {       /* looking for "+", or "+N" or "+N,M"  */
					uint64_t tmp_l = INT64_MAX, tmp_c = 0;
					char *d;
					errno = 0;
					if (argv[i][1]) {
						if (isdigit((unsigned char)argv[i][1])) {
							tmp_l = strtoll(argv[i]+1, &d, 10);
							if (!errno) {
								if (*d) {  /* separator between N and M */
									if (isdigit((unsigned char)d[1])) {
										tmp_c = strtoll(d+1, &d, 10);
										if (*d) errno = ERANGE;
									}
									else errno = ERANGE;
								}
							}
						}
						else errno = ERANGE;
					}
					if (!errno) {
						first_line = tmp_l;
						first_col  = tmp_c;
					}
					else {
						skip_plus = true;
						i--;
					}
				}
				else if (!strcmp(argv[i], "--binary")) {
					binary = true;
				}
				else if (!strcmp(argv[i], "--read-only") || !strcmp(argv[i], "--readonly") || !strcmp(argv[i], "--ro")) {
					read_only = true;
				}
				else {
					if (!strcmp(argv[i], "-") && stdin_buffer) {
						stdin_buffer->opt.binary = binary;
						if (read_only) stdin_buffer->opt.read_only = read_only;
						if (first_line) do_action(stdin_buffer, GOTOLINE_A, first_line, NULL);
						if (first_col)  do_action(stdin_buffer, GOTOCOLUMN_A, first_col, NULL);
						stdin_buffer = NULL;
					}
					else {
						if (!strcmp(argv[i], "--")) i++;
						if (!first_file) do_action(cur_buffer, NEWDOC_A, -1, NULL);
						else first_file = false;
						cur_buffer->opt.binary = binary;
						if (i < argc) do_action(cur_buffer, OPEN_A, 0, str_dup(argv[i]));
						if (first_line) do_action(cur_buffer, GOTOLINE_A, first_line, NULL);
						if (first_col)  do_action(cur_buffer, GOTOCOLUMN_A, first_col, NULL);
						if (read_only) cur_buffer->opt.read_only = read_only;
					}
					first_line =
					first_col  = 0;
					skip_plus  =
					binary    =
					read_only  = false;
				}
			}
		}

		free(skiplist);

		/* This call makes current the first specified file. It is called
		   only if more than one buffer exist. */

		if (get_nth_buffer(1)) do_action(cur_buffer, NEXTDOC_A, -1, NULL);

	}

	/* We delay updates. In this way the macro activity does not cause display activity. */

	#ifndef NE_TERMCAP
	if (ansi)
	#endif
	ttysize();
	reset_window();
	delay_update();

	if (macro_name) do_action(cur_buffer, MACRO_A, -1, str_dup(macro_name));
	else if (first_file) {
		/* If there is no file to load, and no macro to execute, we display
		   the "NO WARRANTY" message. */
		about();
	}

	while(true) {
		/* If we are displaying the "NO WARRANTY" info, we should not refresh the
		   window now */
		if (!displaying_info) {
			refresh_window(cur_buffer);
			if (!cur_buffer->visible_mark.shown) highlight_mark(cur_buffer, true);
			if (cur_buffer->opt.automatch) automatch_bracket(cur_buffer, true);
		}

		draw_status_bar();
		move_cursor(cur_buffer->cur_y, cur_buffer->cur_x);

		int c = get_key_code();

		if (window_changed_size) {
			print_error(do_action(cur_buffer, REFRESH_A, 0, NULL));
			window_changed_size = displaying_info = false;
			cur_buffer->automatch.shown = false;
			cur_buffer->visible_mark.shown = false;
		}

		if (c == INVALID_CHAR) continue; /* Window resizing. */
		const input_class ic = CHAR_CLASS(c);

		if (displaying_info) {
			refresh_window(cur_buffer);
			displaying_info = false;
		}

		if (cur_buffer->automatch.shown) automatch_bracket(cur_buffer, false);

		switch(ic) {
		case INVALID:
			print_error(INVALID_CHARACTER);
			break;

		case ALPHA:
			print_error(do_action(cur_buffer, INSERTCHAR_A, c, NULL));
			break;

		case TAB:
			print_error(do_action(cur_buffer, INSERTTAB_A, 1, NULL));
			break;

		case RETURN:
			print_error(do_action(cur_buffer, INSERTLINE_A, -1, NULL));
			break;

		case COMMAND:
			if (c < 0) c = -c - 1;
			if (key_binding[c]) print_error(execute_command_line(cur_buffer, key_binding[c]));
			break;

		default:
			break;
		}
	}
}
