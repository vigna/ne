/* main(), global initialization and global buffer functions.

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
	02111-1307, USA.  */


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

char *NO_WARRANTY_msg[] = {	PROGRAM_NAME " " VERSION ".",
										"Copyright (C) 1993-1998 Sebastiano Vigna",
										"Copyright (C) 1999-2010 Todd M. Lewis and Sebastiano Vigna",
										"",
										"This program is free software; you can redistribute it and/or modify",
										"it under the terms of the GNU General Public License as published by",
										"the Free Software Foundation; either version 2, or (at your option)",
										"any later version.",
										"",
										"This program is distributed in the hope that it will be useful,",
										"but WITHOUT ANY WARRANTY; without even the implied warranty of",
										"MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the",
										"GNU General Public License for more details.",
										"",
										"Press F1, Escape or Escape-Escape to see the menus. The shortcuts",
										"prefixed by ^ are activated by the Control key; the shortcuts prefixed",
										"by [ are activated by Control+Meta or just Meta, depending on your terminal",
										"emulator. Alternatively, just press Escape followed by a letter.",
										NULL
									};


char ARG_HELP[] = ABOUT_MSG "\n"
						"Usage: ne [options] [files]\n"
						"--help        print this message.\n"
						"--            next token is a filename.\n"
						"+[N]          move to the N-th (or the last, if N is missing) line of the first file.\n"
						"--utf8        use UTF-8 I/O.\n"
						"--no-utf8     do not use UTF-8 I/O.\n"
						"--ansi        use built-in ANSI control sequences.\n"
						"--no-ansi     do not use built-in ANSI control sequences.\n"
						"--no-config   do not read configuration files.\n"
						"--no-syntax   disable syntax-highlighting support.\n"
						"--keys FILE   use this file for keyboard configuration.\n"
						"--menus FILE  use this file for menu configuration.\n"
						"--macro FILE  exec this macro after start.\n";


/* The regular expression used to parse the locale. */

#define LOCALE_REGEX "\\(UTF-?8\\)\\|\\(ISO-?8859-?\\)\\(1?[0-9]\\)"

/* These lists contains the existing buffers, clips and macros. cur_buffer
denotes the currently displayed buffer. */

list buffers = { (node *)&buffers.tail, NULL, (node *)&buffers.head };
list clips = { (node *)&clips.tail, NULL, (node *)&clips.head };
list macros = { (node *)&macros.tail, NULL, (node *)&macros.head };

/* global prefs, only saved in ~/.ne/.default#ap if their
   current setting differs from these defaults. Make sure these
   defaults match the conditionals in prefs.c:save_prefs(). */
int req_order;
int fast_gui;
int status_bar = TRUE;
int verbose_macros = TRUE;
/* end of global prefs */

buffer *cur_buffer;
int turbo;
int do_syntax = TRUE;

/* These function live here because they access cur_buffer. new_buffer()
creates a new buffer, adds it to the buffer list, and assign it to
cur_buffer. delete_buffer() destroys cur_buffer, and makes the previous or
next buffer the current buffer, if any of the two exists. */

buffer *new_buffer(void) {
	buffer *b = alloc_buffer(cur_buffer);

	if (b) {
		clear_buffer(b);
		if (cur_buffer) 
			add(&b->b_node, &cur_buffer->b_node);
		else 
			add_head(&buffers, &b->b_node);
		
		
		
		cur_buffer = b;
	}

	return b;
}

int delete_buffer(void) {

	buffer *nb = (buffer *)cur_buffer->b_node.next, *pb = (buffer *)cur_buffer->b_node.prev;

	rem(&cur_buffer->b_node);
	free_buffer(cur_buffer);

	if (pb->b_node.prev) {
		cur_buffer = pb;
      return TRUE;
	}

	if (nb->b_node.next) {
		cur_buffer = nb;
		return TRUE;
	}

	return FALSE;
}



/* The main() function. It is responsible for argument parsing, calling
some terminal and signal initialization functions, and entering the
event loop. */

int main(int argc, char **argv) {
	
	input_class ic;
	int i, c, no_config = FALSE, displaying_info = FALSE, first_line = 0;
	char *macro_name = NULL, *key_bindings_name = NULL, *menu_conf_name = NULL;

	clip_desc *cd;
	char *skiplist, *locale;

	if (!(skiplist = calloc(argc, 1)))  /* We need this many flags. */
	  exit(1);                             /* This would be bad.       */

	locale = setlocale(LC_ALL, "");
	for(i = 0; i < 256; i++) localised_up_case[i] = toupper(i);

	if (locale) {
		struct re_pattern_buffer re_pb;
		struct re_registers re_reg;
		memset(&re_pb, 0, sizeof re_pb);
		memset(&re_reg, 0, sizeof re_reg);

		re_pb.translate = (char *)localised_up_case;
		re_compile_pattern(LOCALE_REGEX, strlen(LOCALE_REGEX), &re_pb);
		if (re_search(&re_pb, locale, strlen(locale), 0, strlen(locale), &re_reg) >= 0) {
			if (re_reg.start[1] >= 0) io_utf8 = TRUE;
		}
		free(re_reg.start);
		free(re_reg.end);
	}

	for(i = 1; i < argc; i++) {

		/* Special arguments start with two dashes. If we find one, we
		cancel its entry in argv[], so that it will be skipped when opening
		the specified files. The only exception is +N for skipping to the
		N-th line. */

		if (argv[i][0] == '-' && argv[i][1] == '-') {
			if (!argv[i][2]) skiplist[i++] = 1; /* You can use "--" to force the next token to be a filename */
			else if (!strcmp(&argv[i][2], "help" "\0" VERSION_STRING)) {
				puts(ARG_HELP);
				exit(0);
			}
			else if (!strcmp(&argv[i][2], "noconfig") || !strcmp(&argv[i][2], "no-config")) {
				no_config = TRUE;
				skiplist[i] = 1; /* argv[i] = NULL; */
			}
			else if (!strcmp(&argv[i][2], "noansi") || !strcmp(&argv[i][2], "no-ansi")) {
				ansi = FALSE;
				skiplist[i] = 1; /* argv[i] = NULL; */
			}
			else if (!strcmp(&argv[i][2], "no-syntax")) {
				do_syntax = FALSE;
				skiplist[i] = 1; /* argv[i] = NULL; */
			}
			else if (!strcmp(&argv[i][2], "ansi")) {
				ansi = TRUE;
				skiplist[i] = 1; /* argv[i] = NULL; */
			}
			else if (!strcmp(&argv[i][2], "utf8")) {
				io_utf8 = TRUE;
				skiplist[i] = 1; /* argv[i] = NULL; */
			}
			else if (!strcmp(&argv[i][2], "no-utf8")) {
				io_utf8 = FALSE;
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
		else if (argv[i][0] == '+') {
			first_line = argv[i][1] ? strtol(argv[i]+1, NULL, 10) : INT_MAX;
			if (errno || first_line < 0) first_line = 0;
			skiplist[i] = 1; /* argv[i] = NULL; */
		}
	}

	/* Unless --noconfig was specified, we try to configure the
	menus and the keyboard. Note that these functions can exit() on error. */

	if (!no_config) {
		get_menu_configuration(menu_conf_name);
		get_key_bindings(key_bindings_name);
	}

	/* If we cannot even create a buffer, better go... */

	if (!new_buffer()) exit(1);

	clear_buffer(cur_buffer);

	/* The INT_MAX clip always exists, and it is used by the Through command. */

	if (!(cd = alloc_clip_desc(INT_MAX, 0))) exit(1);

	add_head(&clips, &cd->cd_node);

	/* General terminfo and cursor motion initalization. From here onwards,
	we cannot exit() lightly. */

	term_init();

	/* We will be always using the last line for the status bar. */

	set_terminal_window(ne_lines-1);

	/* We read in all the key capabilities. */

	read_key_capabilities();

	/* Some initializations of other modules... */

	re_set_syntax(
		/* RE_CHAR_CLASSES		|  I would *love* character classes, but they do not seem to work... 8^( */
		RE_CONTEXT_INDEP_ANCHORS |
		RE_CONTEXT_INDEP_OPS	| RE_HAT_LISTS_NOT_NEWLINE |
		RE_NEWLINE_ALT			| RE_NO_BK_PARENS				|
		RE_NO_BK_VBAR			| RE_NO_EMPTY_RANGES
	);

	/* The terminal is prepared for interactive I/O. */

	set_interactive_mode();

	clear_entire_screen();


#ifndef _AMIGA

	/* This function sets fatal_code() as signal interrupt handler
	for all the dangerous signals (SIGILL, SIGSEGV etc.). */

	set_fatal_code();

#endif

	load_auto_prefs(cur_buffer, DEF_PREFS_NAME);

	if (argc > 1) {

		/* The first file opened does not need a NEWDOC_A action. Note that
		file loading can be interrupted (wildcarding can sometimes produce
		unwanted results). */

		int first_file = TRUE;

		stop = FALSE;

		for(i = 1; i < argc && !stop; i++) {
			if (argv[i] && !skiplist[i]) {
				if (!first_file) do_action(cur_buffer, NEWDOC_A, -1, NULL);
				else first_file = FALSE;

				do_action(cur_buffer, OPEN_A, 0, str_dup(argv[i]));
			}
		}

		free(skiplist);

		/* This call makes current the first specified file. It is called
		only if more than one buffer exist. */

		if (get_nth_buffer(1)) do_action(cur_buffer, NEXTDOC_A, -1, NULL);
		if (first_line) do_action(cur_buffer, GOTOLINE_A, first_line, NULL);
	}

	/* Note that we do not need to update the display. clear_entire_screen() is
	ok if no file was loaded; otherwise, OPEN_A will call update_window(). */

	refresh_window(cur_buffer);

	if (macro_name) do_action(cur_buffer, MACRO_A, -1, str_dup(macro_name));
	else if (argc == 1) {

		/* If there is no file to load, and no macro to execute, we display
		the "NO WARRANTY" message. */

		displaying_info = TRUE;

		for(i = 0; NO_WARRANTY_msg[i]; i++) {
			if (i == ne_lines - 1) break;
			move_cursor(i, 0);
			output_string(NO_WARRANTY_msg[i], FALSE);
		}
		if (++i < ne_lines - 1) {
			move_cursor(i, 0);
			if (exists_gprefs_dir()) {
				output_string("Global Directory: ", FALSE);
				output_string(exists_gprefs_dir(), FALSE);
			}
			else {
				output_string("Global directory \"", FALSE);
				output_string(get_global_dir(), FALSE);
				output_string("\" not found!", FALSE);
			}
		}
	}

	while(TRUE) {

		/* If we are displaying the "NO WARRANTY" info, we should not refresh the
		window now */

		if (!displaying_info) refresh_window(cur_buffer);
	
		draw_status_bar();

		move_cursor(cur_buffer->cur_y, cur_buffer->cur_x);

		c = get_key_code();
		ic = CHAR_CLASS(c);

		if (window_changed_size) {
			print_error(do_action(cur_buffer, REFRESH_A, 0, NULL));
			window_changed_size = FALSE;
		}

		if (displaying_info) {
			displaying_info = FALSE;

			for(i = 0; i < sizeof NO_WARRANTY_msg / sizeof *NO_WARRANTY_msg + 2; i++) {
				if (i == ne_lines - 1) break;
				move_cursor(i, 0);
				clear_to_eol();
			}

			fflush(stdout);
		}

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
