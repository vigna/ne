/* Function prototypes for term.c

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

#include <stdint.h>
#include <stdbool.h>

int output_width(int c);
void ring_bell(void);
void do_flash(void);
void set_terminal_modes(void);
void reset_terminal_modes(void);
void set_terminal_window(int size);
void standout_on(void);
void standout_off(void);
void cursor_on(void);
void cursor_off(void);
void move_cursor(int row, int col);
void clear_end_of_line(int first_unused_hpos);
void clear_to_eol(void);
void clear_to_end(void);
void clear_entire_screen(void);
void set_attr(const uint32_t);
void output_chars(const char *string, const uint32_t *attr, int raw_len, bool utf8);
void output_string(const char *s, bool utf8);
void output_spaces(int n, const uint32_t *attr);
void output_char(int c, const uint32_t attr, const bool utf8);
void insert_chars(const char *start, const uint32_t *attr, int raw_len, bool utf8);
void insert_char(int c, const uint32_t attr, bool utf8);
void delete_chars(int n);
int ins_del_lines(int vpos, int n);
int ttysize(void);
void term_init(void);
