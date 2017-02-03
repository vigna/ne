/* Hardwired ANSI terminal control sequences.

   Copyright (C) 2001-2017 Todd M. Lewis and Sebastiano Vigna

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
#ifdef TERMCAP
#include "info2cap.h"
#endif

/* Pokes into the terminfo capability strings the ANSI definitions.
   This is good for situation in which no database is available.

   The keyboard is already fixed in keys.c, because key_may_set() will
   poke into the keyboard capabilities all ANSI keyboard sequences.
*/

void setup_ansi_term(void) {

#ifdef TERMCAP
	ne_cursor_address = "\x1b[%i%d;%dH";
	ne_set_background = "\x1b[4%dm";
	ne_set_foreground = "\x1b[3%dm";
#else
	ne_cursor_address = "\x1b[%i%p1%d;%p2%dH";
	ne_set_background = "\x1b[4%p1%dm";
	ne_set_foreground = "\x1b[3%p1%dm";
#endif
	ne_enter_bold_mode = "\x1b[1m";
	ne_enter_underline_mode = "\x1b[4m";
	ne_enter_blink_mode = "\x1b[5m";
	ne_lines = 25;
	ne_columns = 80;
	ne_carriage_return = "\xd";
	ne_cursor_home = "\x1b[H";
	ne_cursor_right = "\x1b[C";
	ne_cursor_down = "\x1b[B";
	ne_cursor_left = "\x1b[D";
	ne_cursor_up = "\x1b[A";
	ne_auto_right_margin = 1;
	ne_eat_newline_glitch = 0;
	ne_clr_eos = "\x1b[J";
	ne_clear_screen = "\x1b[H\x1b[J";
	ne_bell = "\x7";
	ne_scroll_forward = "\xa";
	ne_enter_standout_mode = "\x1b[7m";
	ne_exit_standout_mode = ne_exit_attribute_mode = "\x1b[m";
	ne_magic_cookie_glitch = -1;
	ne_move_standout_mode = 0;
	ne_insert_line = "\x1b[L";
	ne_delete_line = "\x1b[M";
	ne_delete_character = "\x1b[P";
	ne_move_insert_mode = 1;
	ne_exit_alt_charset_mode = "\x1b[10m";
	ne_tilde_glitch = 0;
	ne_memory_below = 0;
	ne_has_meta_key = 0;
	ne_clr_eol = "\x1b[K";
	ne_transparent_underline = 0;
	ne_no_color_video = 3;
	ansi_color_ok = true;
}
