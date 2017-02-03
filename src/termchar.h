/* extern's of flags describing terminal's characteristics.

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


/** #define's from Joe's sources for colors and attributes. Please keep in sync. */

#include <stdbool.h>

#define INVERSE		 256
#define UNDERLINE	 512
#define BOLD		1024
#define BLINK		2048
#define DIM		4096
#define AT_MASK		(INVERSE+UNDERLINE+BOLD+BLINK+DIM)

#define BG_SHIFT	13
#define BG_VALUE	(255<<BG_SHIFT)
#define BG_NOT_DEFAULT	(256<<BG_SHIFT)
#define BG_MASK		(511<<BG_SHIFT)

#define BG_DEFAULT	(0<<BG_SHIFT)

/* #define BG_COLOR(color)	(BG_NOT_DEFAULT^(color)<<BG_SHIFT) */
#define BG_COLOR(color)	(color)

#define BG_BLACK	(BG_NOT_DEFAULT|(0<<BG_SHIFT))
#define BG_RED		(BG_NOT_DEFAULT|(1<<BG_SHIFT))
#define BG_GREEN	(BG_NOT_DEFAULT|(2<<BG_SHIFT))
#define BG_YELLOW	(BG_NOT_DEFAULT|(3<<BG_SHIFT))
#define BG_BLUE		(BG_NOT_DEFAULT|(4<<BG_SHIFT))
#define BG_MAGENTA	(BG_NOT_DEFAULT|(5<<BG_SHIFT))
#define BG_CYAN		(BG_NOT_DEFAULT|(6<<BG_SHIFT))
#define BG_WHITE	(BG_NOT_DEFAULT|(7<<BG_SHIFT))
#define BG_BBLACK	(BG_NOT_DEFAULT|(8<<BG_SHIFT))
#define BG_BRED		(BG_NOT_DEFAULT|(9<<BG_SHIFT))
#define BG_BGREEN	(BG_NOT_DEFAULT|(10<<BG_SHIFT))
#define BG_BYELLOW	(BG_NOT_DEFAULT|(11<<BG_SHIFT))
#define BG_BBLUE	(BG_NOT_DEFAULT|(12<<BG_SHIFT))
#define BG_BMAGENTA	(BG_NOT_DEFAULT|(13<<BG_SHIFT))
#define BG_BCYAN	(BG_NOT_DEFAULT|(14<<BG_SHIFT))
#define BG_BWHITE	(BG_NOT_DEFAULT|(15<<BG_SHIFT))

#define FG_SHIFT	22
#define FG_VALUE	(255<<FG_SHIFT)
#define FG_NOT_DEFAULT	(256<<FG_SHIFT)
#define FG_MASK		(511<<FG_SHIFT)

#define FG_DEFAULT	(0<<FG_SHIFT)
#define FG_BWHITE	(FG_NOT_DEFAULT|(15<<FG_SHIFT))
#define FG_BCYAN	(FG_NOT_DEFAULT|(14<<FG_SHIFT))
#define FG_BMAGENTA	(FG_NOT_DEFAULT|(13<<FG_SHIFT))
#define FG_BBLUE	(FG_NOT_DEFAULT|(12<<FG_SHIFT))
#define FG_BYELLOW	(FG_NOT_DEFAULT|(11<<FG_SHIFT))
#define FG_BGREEN	(FG_NOT_DEFAULT|(10<<FG_SHIFT))
#define FG_BRED		(FG_NOT_DEFAULT|(9<<FG_SHIFT))
#define FG_BBLACK	(FG_NOT_DEFAULT|(8<<FG_SHIFT))
#define FG_WHITE	(FG_NOT_DEFAULT|(7<<FG_SHIFT))
#define FG_CYAN		(FG_NOT_DEFAULT|(6<<FG_SHIFT))
#define FG_MAGENTA	(FG_NOT_DEFAULT|(5<<FG_SHIFT))
#define FG_BLUE		(FG_NOT_DEFAULT|(4<<FG_SHIFT))
#define FG_YELLOW	(FG_NOT_DEFAULT|(3<<FG_SHIFT))
#define FG_GREEN	(FG_NOT_DEFAULT|(2<<FG_SHIFT))
#define FG_RED		(FG_NOT_DEFAULT|(1<<FG_SHIFT))
#define FG_BLACK	(FG_NOT_DEFAULT|(0<<FG_SHIFT))


extern bool line_ins_del_ok;			/* Terminal can insert and delete lines */

extern bool char_ins_del_ok;			/* Terminal can insert and delete chars */

extern bool scroll_region_ok;			/* Terminal supports setting the scroll window */

extern bool standout_ok;					/* Terminal supports standout without magic cookies */

extern bool cursor_on_off_ok;			/* Terminal can make the cursor visible or invisible */

extern bool ansi_color_ok;				/* Terminal supports ANSI color */

extern bool color_ok;						/* Terminal supports color */

extern bool ne_generic_type;

extern int ne_lines;
extern int ne_columns;
extern int ne_no_color_video;

extern char *ne_column_address;
extern char *ne_row_address;

extern char *ne_cursor_address;

extern char *ne_carriage_return;

extern char *ne_cursor_home;
extern char *ne_cursor_to_ll;

extern char *ne_cursor_right;
extern char *ne_cursor_down;
extern char *ne_cursor_left;
extern char *ne_cursor_up;

extern int ne_auto_right_margin;
extern int ne_eat_newline_glitch;

extern char *ne_clr_eos;
extern char *ne_clear_screen;

extern char *ne_bell;
extern char *ne_flash_screen;

extern char *ne_scroll_forward;
extern char *ne_scroll_reverse;

extern char *ne_enter_delete_mode;
extern char *ne_exit_delete_mode;
extern char *ne_enter_insert_mode;
extern char *ne_exit_insert_mode;

extern char *ne_enter_standout_mode;
extern char *ne_enter_bold_mode;
extern char *ne_enter_blink_mode;
extern char *ne_enter_underline_mode;
extern char *ne_exit_standout_mode;
extern int ne_magic_cookie_glitch;
extern bool ne_move_standout_mode;

extern char *ne_change_scroll_region;

extern char *ne_insert_line;
extern char *ne_parm_insert_line;
extern char *ne_delete_line;
extern char *ne_parm_delete_line;

extern char *ne_insert_character;
extern char *ne_insert_padding;
extern char *ne_parm_ich;

extern char *ne_delete_character;
extern char *ne_parm_dch;

extern bool ne_move_insert_mode;

extern char *ne_cursor_invisible;
extern char *ne_cursor_normal;

extern char *ne_set_background;
extern char *ne_set_foreground;

extern char *ne_init_1string;
extern char *ne_init_2string;
extern char *ne_init_3string;
extern char *ne_enter_ca_mode;
extern char *ne_exit_ca_mode;

extern char *ne_exit_attribute_mode;
extern char *ne_exit_alt_charset_mode;

extern char *ne_repeat_char;

extern bool ne_tilde_glitch;
extern bool ne_memory_below;

extern bool ne_has_meta_key;
extern char *ne_meta_on;
extern char *ne_meta_off;

extern char *ne_set_window;

extern char *ne_keypad_local;
extern char *ne_keypad_xmit;

extern char *ne_clr_eol;
extern bool ne_transparent_underline;

extern bool io_utf8;
