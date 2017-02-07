/* Terminal control based on terminfo capabilities.
   Originally part of GNU Emacs. Vastly edited and modified for use within ne.

   Copyright (C) 1985, 1986, 1987 Free Software Foundation, Inc.
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


#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>

#ifndef TERMCAP
#include <curses.h>
#include <term.h>
#else
#include "info2cap.h"
#endif

#include <termios.h>
#include <sys/ioctl.h>

#include "term.h"
#include "ansi.h"
#include "termchar.h"
#include "cm.h"
#include "utf8.h"


/* When displaying errors about the terminal database, we try to print the
correct name. */

#ifdef TERMCAP
#define DATABASE_NAME "termcap"
#else
#define DATABASE_NAME "terminfo"
#endif

/* If true, we want the use the built-in ANSI terminal, not a real one. */

#ifdef ANSI
bool ansi = true;
#else
bool ansi = false;
#endif

/* Value is non-zero if attribute ATTR may be used with color.  ATTR
   should be one of the enumerators from enum no_color_bit, or a bit set
   built from them. */

#define MAY_USE_WITH_COLORS(ATTR) (color_ok && ! (ne_no_color_video & (ATTR)))

/* The mask values for no_color_video. */

enum no_color_bit {
	NC_STANDOUT    = 1 << 0,
	NC_UNDERLINE   = 1 << 1,
	NC_REVERSE     = 1 << 2,
	NC_BLINK       = 1 << 3,
	NC_DIM         = 1 << 4,
	NC_BOLD        = 1 << 5,
	NC_INVIS       = 1 << 6,
	NC_PROTECT     = 1 << 7,
	NC_ALT_CHARSET = 1 << 8
};

/* We use internal copies of the terminfo capabilities because we want to be
   able to use a hardwired set. */

bool ne_generic_type;

int ne_lines;
int ne_columns;
int ne_no_color_video;

char *ne_column_address;
char *ne_row_address;

char *ne_cursor_address;

char *ne_carriage_return;

char *ne_cursor_home;
char *ne_cursor_to_ll;

char *ne_cursor_right;
char *ne_cursor_down;
char *ne_cursor_left;
char *ne_cursor_up;

int ne_auto_right_margin;
int ne_eat_newline_glitch;

char *ne_clr_eos;
char *ne_clear_screen;

char *ne_bell;
char *ne_flash_screen;

char *ne_scroll_forward;
char *ne_scroll_reverse;

char *ne_enter_delete_mode;
char *ne_exit_delete_mode;
char *ne_enter_insert_mode;
char *ne_exit_insert_mode;

char *ne_enter_standout_mode;
char *ne_enter_bold_mode;
char *ne_exit_standout_mode;
int ne_magic_cookie_glitch;
bool ne_move_standout_mode;

char *ne_change_scroll_region;

char *ne_insert_line;
char *ne_parm_insert_line;
char *ne_delete_line;
char *ne_parm_delete_line;

char *ne_insert_character;
char *ne_insert_padding;
char *ne_parm_ich;

char *ne_delete_character;
char *ne_parm_dch;

bool ne_move_insert_mode;

char *ne_cursor_invisible;
char *ne_cursor_normal;

char *ne_init_1string;
char *ne_init_2string;
char *ne_init_3string;
char *ne_enter_ca_mode;
char *ne_exit_ca_mode;

char *ne_exit_attribute_mode;
char *ne_exit_alt_charset_mode;

char *ne_repeat_char;

bool ne_tilde_glitch;
bool ne_memory_below;

bool ne_has_meta_key;
char *ne_meta_on;
char *ne_meta_off;

char *ne_set_window;

char *ne_keypad_local;
char *ne_keypad_xmit;

char *ne_clr_eol;
bool ne_transparent_underline;

char *ne_set_background;
char *ne_set_foreground;

char *ne_enter_underline_mode;
char *ne_exit_underline_mode;

char *ne_enter_bold_mode;
char *ne_enter_blink_mode;
char *ne_enter_dim_mode;
char *ne_enter_reverse_mode;
char *ne_exit_attribute_mode;







/* This is the real instantiation of the cm structure used by cm.c to hold
the cursor motion strings. */

struct cm Wcm;


#define OUTPUT(a) tputs (a, ne_lines - curY, cmputc)
#define OUTPUT1(a) tputs (a, 1, cmputc)
#define OUTPUTL(a, lines) tputs (a, lines, cmputc)
#define OUTPUT_IF(a) { if (a) tputs (a, ne_lines - curY, cmputc); }
#define OUTPUT1_IF(a) { if (a) tputs (a, 1, cmputc); }



/* Terminal charateristics that higher levels want to look at.
   These are all extern'd in termchar.h */

bool	line_ins_del_ok;		/* Terminal can insert and delete lines */
bool	char_ins_del_ok;		/* Terminal can insert and delete chars */
bool	scroll_region_ok;		/* Terminal supports setting the scroll window */
bool	standout_ok;			/* Terminal supports standout without magic cookies */
bool	cursor_on_off_ok;		/* Terminal can make the cursor visible or invisible */
bool	ansi_color_ok;			/* Terminal supports ANSI color */
bool	color_ok;				/* Terminal supports color */


static int	RPov;		/* Least number of chars to start a TS_repeat.
								Less wouldn't be worth. */

static bool	delete_in_insert_mode;	/* True if delete mode == insert mode */
static bool	se_is_so;					/* True if same string both enters and leaves standout mode */
static bool	esm_is_eam;					/* True if exiting standout mode turns off all attributes */

static bool	insert_mode;			/* True when in insert mode. */
static bool	standout_mode;			/* True when in standout mode. */
static bool	standout_wanted;		/* True if we should be writing in standout mode. */
static uint32_t curr_attr;			/* The current video attributes. */

/* Size of window specified by higher levels. This is the number of lines,
starting from top of screen, to participate in ins/del line operations.
Effectively it excludes the bottom lines - specified_window_size lines from
those operations.  */

int	specified_window;

/* If true, then all I/O is to be performed in UTF-8. */

bool	io_utf8;

/* Returns the output width of the given character. It is maximised with 1
 w.r.t. wcwidth(), so its result is equivalent to the width of the character
 that will be output by out(). */

int output_width(const int c) {
	const int width = wcwidth(c);
	return width > 0 ? width : 1;
}

/* Returns the output width of the given string. If s is NULL, returns len.  If
the width of the string exceeds maxWidth, modifies len so that it contains the
longest prefix of s whose width is not greater than maxWidth, and returns the
corresponding width. */

static int string_output_width(const char *s, int *len, int maxWidth, bool utf8) {
	if (s == NULL) {
		if (*len > maxWidth) *len = maxWidth;
		return *len;
	}
	else {
		int width = 0, char_width, l = *len;
		if (utf8) {
			while(l-- != 0) {
				char_width = output_width(utf8char(s));
				if (width + char_width > maxWidth) {
					*len -= l + 1;
					break;
				}
				width += char_width;
				s += utf8len(*s);
			}
		}
		else while(l-- != 0) {
			char_width = output_width(*(s++));
			if (width + char_width > maxWidth) {
				*len -= l + 1;
				break;
			}
			width += char_width;
		}
		return width;
	}
}


static int joe2color(const int joe_color) {
	if (ansi_color_ok) return joe_color & 7;

	switch(joe_color & 7) {
		case 0: return 0; /* BLACK */
		case 1: return 4; /* RED */
		case 2: return 2; /* GREEN	*/
		case 3: return 6; /* YELLOW */
		case 4: return 1; /* BLUE */
		case 5: return 5; /* MAGENTA */
		case 6: return 3; /* CYAN */
		case 7: return 7; /* WHITE */
	}

	return -1;
}


/* Sets up attributes */


#ifdef PLAIN_SET_ATTR

void set_attr(const uint32_t attr) {
	OUTPUT1(ne_exit_attribute_mode);

	if (attr & INVERSE) OUTPUT1(ne_enter_reverse_mode); 
	if (attr & BOLD) OUTPUT1(ne_enter_bold_mode);
	if (attr & UNDERLINE) OUTPUT1(ne_enter_underline_mode);
	if (attr & DIM) OUTPUT1(ne_enter_dim_mode);
	if (attr & BLINK) OUTPUT1(ne_enter_blink_mode);

	if (color_ok) {
		if (attr & FG_NOT_DEFAULT) {
			const char * const buf = tparm(ne_set_foreground , joe2color(attr >> FG_SHIFT));
			OUTPUT1(buf);
		}
		if (attr & BG_NOT_DEFAULT) {
			const char * const buf = tparm(ne_set_background , joe2color(attr >> BG_SHIFT));
			OUTPUT1(buf);
		}
	}
} 

#else


void set_attr(const uint32_t attr) {
	bool attr_reset = false;

	/* If we have to set a different subset of attributes, or if we have to
		set to the default at least one of the colors
		(background/foreground) we must necessarily reset all attributes. */
	if ((curr_attr & AT_MASK) != (attr & AT_MASK) 
			|| (!(attr & FG_NOT_DEFAULT) && (curr_attr & FG_NOT_DEFAULT)) 
			|| (!(attr & BG_NOT_DEFAULT) && (curr_attr & BG_NOT_DEFAULT))) {

		OUTPUT1_IF(ne_exit_attribute_mode)
		attr_reset = true;

		if ((attr & INVERSE) && MAY_USE_WITH_COLORS(NC_REVERSE)) OUTPUT1_IF(ne_enter_reverse_mode)
		if ((attr & BOLD) && MAY_USE_WITH_COLORS(NC_BOLD)) OUTPUT1_IF(ne_enter_bold_mode)
		if ((attr & UNDERLINE) && MAY_USE_WITH_COLORS(NC_UNDERLINE)) OUTPUT1_IF(ne_enter_underline_mode)
		if ((attr & DIM) && MAY_USE_WITH_COLORS(NC_DIM)) OUTPUT1_IF(ne_enter_dim_mode)
		if ((attr & BLINK) && MAY_USE_WITH_COLORS(NC_BLINK)) OUTPUT1_IF(ne_enter_blink_mode)
	}

	if (color_ok) {
		/* Colors must be set if attributes have been reset and the required
			color is not default, or in any case if the color has changed. */

		if (attr_reset && (attr & FG_NOT_DEFAULT) || (attr & FG_MASK) != (curr_attr & FG_MASK)) {
			if (attr & FG_NOT_DEFAULT) {
				const char * const buf = tparm(ne_set_foreground , joe2color(attr >> FG_SHIFT));
				OUTPUT1(buf);
			}
		}
		if (attr_reset && (attr & BG_NOT_DEFAULT) || (attr & BG_MASK) != (curr_attr & BG_MASK)) {
			if (attr & BG_NOT_DEFAULT) {
				const char * const buf = tparm(ne_set_background , joe2color(attr >> BG_SHIFT));
				OUTPUT1(buf);
			}
		}
	}

	curr_attr = attr;
}

#endif

static void turn_off_standout(void) {
	OUTPUT1(ne_exit_standout_mode);
	/* We exiting standout mode deletes all attributes, we update curr_attr. */
	if (esm_is_eam) curr_attr = 0;
	standout_mode = false;
}

static void standout_if_wanted(void) {
	if (standout_mode != standout_wanted) {
		if (standout_wanted) {
			OUTPUT1(ne_enter_standout_mode);
			standout_mode = true;
		}
		else turn_off_standout();
	}
}

/* These functions are called on all terminals in order to handle highlighting,
   but do nothing on terminals with a magic cookie (or without standout).  */

void standout_on (void) {
	if (standout_ok) standout_wanted = true;
}

void standout_off (void) {
	standout_wanted = false;
}


/* Depending on the value of io_utf8, this function will do a simple putchar(),
   or a series of putchar() that expand the given character in UTF-8 encoding. 
   If attr is -1, no attribute will be set. */

static void out(int c, const uint32_t attr) {
	uint32_t add_attr = 0;

	/* PORTABILITY PROBLEM: this code is responsible for filtering nonprintable
		characters. On systems with a wider system character set, it could be
		redefined, for instance, in order to allow characters between 128 and 160 to
		be printed. Currently, it returns '?' on all control characters (and
		non-ISO-8859-1 characters, if io_utf8 is false), space on 160, and the
		obvious capital letter for control characters below 32. */

	if (c >= 127 && c < 160) {
		c = '?';
		add_attr = INVERSE;
	}

	if (c == 160) {
		c = ' ';
		add_attr = INVERSE;
	}

	if (c < ' ') {
		c += '@';
		add_attr = INVERSE;
	}

	if (c > 0xFF && !io_utf8) {
		c = '?';
		add_attr = INVERSE;
	}

	/* If io_utf8 is off, we consider all characters in the range of ISO-8859-x
	encoding schemes as printable. */

	if (io_utf8 && wcwidth(c) <= 0) {
		c = '?';
		add_attr = INVERSE;
	}

	if (attr != -1) set_attr(attr | add_attr);

	if (io_utf8) {
		if (c < 0x80) putchar(c); /* ASCII */
		else if (c < 0x800) {
			putchar(0xC0 | (c >> 6));
			putchar(0x80 | (c >> 0) & 0x3F);
		}
		else if (c < 0x10000) {
			putchar(0xE0 | (c >> 12));
			putchar(0x80 | (c >> 6) & 0x3F);
			putchar(0x80 | (c >> 0) & 0x3F);
		}
		else if (c < 0x200000) {
			putchar(0xF0 | (c >> 18));
			putchar(0x80 | (c >> 12) & 0x3F);
			putchar(0x80 | (c >> 6) & 0x3F);
			putchar(0x80 | (c >> 0) & 0x3F);
		}
		else if (c < 0x4000000) {
			putchar(0xF8 | (c >> 24));
			putchar(0x80 | (c >> 18) & 0x3F);
			putchar(0x80 | (c >> 12) & 0x3F);
			putchar(0x80 | (c >> 6) & 0x3F);
			putchar(0x80 | (c >> 0) & 0x3F);
		}
		else {
			putchar(0xFC | (c >> 30));
			putchar(0x80 | (c >> 24) & 0x3F);
			putchar(0x80 | (c >> 18) & 0x3F);
			putchar(0x80 | (c >> 12) & 0x3F);
			putchar(0x80 | (c >> 6) & 0x3F);
			putchar(0x80 | (c >> 0) & 0x3F);
		}
	}
	else putchar(c);
}




/* Rings a bell or flashes the screen. If the service is not available, the
   other one is tried. */


void ring_bell(void) {
	OUTPUT1_IF (ne_bell ? ne_bell : ne_flash_screen);
}


void do_flash(void) {
	OUTPUT1_IF (ne_flash_screen ? ne_flash_screen : ne_bell);
}



/* Sets correctly the scroll region (first line is line 0). This function
   assumes scroll_region_ok == true.  The cursor position is lost, as from the
   terminfo specs. */

static void set_scroll_region (const int start, const int stop) {

	assert(scroll_region_ok);

	/* Both control string have line range 0 to lines-1 */

	char *buf;
	if (ne_change_scroll_region) buf = tparm (ne_change_scroll_region, start, stop);
	else buf = tparm (ne_set_window, start, stop, 0, ne_columns - 1);

	OUTPUT1(buf);
	losecursor();
}


static void turn_on_insert (void) {
	if (!insert_mode) OUTPUT1(ne_enter_insert_mode);
	insert_mode = true;
}


static void turn_off_insert (void) {
	if (insert_mode) OUTPUT1(ne_exit_insert_mode);
	insert_mode = false;
}



/* Prepares the terminal for interactive I/O. It
   initializes the terminal, puts standout in a known state,
   prepares the cursor address mode, and
   activates the keypad and the meta key. */

void set_terminal_modes(void) {

	/* Note that presently we do not support if and iprog, the program
	and the file which should be used, if present, to initialize the
	terminal. */

	OUTPUT1_IF(ne_exit_attribute_mode);
	OUTPUT1_IF(ne_exit_alt_charset_mode);
	OUTPUT1_IF(ne_exit_standout_mode);
	OUTPUT1_IF(ne_enter_ca_mode);
	OUTPUT1_IF(ne_keypad_xmit);

	if (ne_has_meta_key) OUTPUT1_IF(ne_meta_on);
   turn_off_standout();
	losecursor();
}


/* Puts again the terminal in its normal state. */

void reset_terminal_modes (void) {

	OUTPUT1_IF(ne_exit_attribute_mode);
	OUTPUT1_IF(ne_exit_alt_charset_mode);
	turn_off_standout();
	OUTPUT1_IF(ne_keypad_local);
	OUTPUT1_IF(ne_exit_ca_mode);
}


/* Sets the variable specified_window. Following line insert/delete operations
   will be limited to lines 0 to (size-1). */

void set_terminal_window(const int size) {

	specified_window = size ? size : ne_lines;

}


/* These functions are the external interface to cursor on/off strings. */

void cursor_on (void) {
	if (cursor_on_off_ok) OUTPUT1(ne_cursor_normal);
}


void cursor_off (void) {
	if (cursor_on_off_ok) OUTPUT1(ne_cursor_invisible);
}


/* Move to absolute position, specified origin 0 */

void move_cursor (const int row, const int col) {
	if (curY == row && curX == col) return;
	if (!ne_move_standout_mode) turn_off_standout();
	if (!ne_move_insert_mode) turn_off_insert ();
	cmgoto (row, col);
}


/* Clears from the cursor position to the end of line. It assumes that the line
   is already clear starting at column first_unused_hpos. Note that the cursor
   may be moved, on terminals lacking a `ce' string.  */

void clear_end_of_line(const int first_unused_hpos) {
	if (curX >= first_unused_hpos) return;

	if (curr_attr & BG_NOT_DEFAULT) set_attr(0);
	if (ne_clr_eol) OUTPUT1 (ne_clr_eol);
	else {
		/* We have to do it the hard way. */
		turn_off_insert ();
		for (int i = curX; i < first_unused_hpos; i++) putchar (' ');
		cmplus (first_unused_hpos - curX);
	}
}


/* Shorthand; use this if you don't know anything about the state
of the line. */

void clear_to_eol(void) {
	clear_end_of_line(ne_columns);
}


/* Clears from the cursor position to the end of screen */

void clear_to_end (void) {

	if (ne_clr_eos) OUTPUT(ne_clr_eos);
	else {
		for (int i = curY; i < ne_lines; i++) {
			move_cursor (i, 0);
			clear_to_eol();
		}
	}
}


/* Clears the entire screen */

void clear_entire_screen (void) {

	if (ne_clear_screen) {
		OUTPUTL(ne_clear_screen, ne_lines);
		cmat (0, 0);
	}
	else {
		move_cursor (0, 0);
		clear_to_end();
	}
}


/* Outputs raw_len characters pointed at by string, attributed as
   indicated by a corresponding vector of attributes, which can be NULL,
   in which case no attribute will be set. The characters will be
   truncated to the end of the current line. Passing a NULL for string
   results in outputting spaces. A len of 0 causes no action. If utf8 is
   true, the string is UTF-8 encoded. */
  
void output_chars(const char *string, const uint32_t *attr, const int raw_len, const bool utf8) {
	if (raw_len == 0) return;

	turn_off_insert();
	standout_if_wanted();

	/* If the string is UTF-8 encoded, compute its real length. */
	int len = utf8 && string != NULL ? utf8strlen(string, raw_len) : raw_len;

	/* If the width of the string exceeds the remaining columns, we reduce
		len. Moreover, we don't dare write in last column of bottom line, if
		AutoWrap, since that would scroll the whole screen on some terminals. */

	cmplus(string_output_width(string, &len, ne_columns - curX - (AutoWrap && curY == ne_lines - 1), utf8));

	if (string == NULL) {
		for(int i = 0; i < len; i++) {
			/* When outputting spaces, it's only the first attribute that's used. */
			if (attr) set_attr(*attr);
			putchar(' ');
		}
		return;
	}

	if (!ne_transparent_underline && !ne_tilde_glitch) {
		for(int i = 0; i < len; i++) {
			if (utf8) {
				const int c = utf8char(string);
				string += utf8len(*string);
				out(c, attr ? attr[i] : -1);
			}
			else {
				const int c = (unsigned char)*string++;
				out(c, attr ? attr[i] : -1);
			}
		}
	} 
	else
		for(int i = 0; i < len; i++) {
			if (attr) set_attr(attr[i]);
			int c = utf8 ? utf8char(string) : (unsigned char)*string;

			if (c == '_' && ne_transparent_underline) {
				putchar (' ');
				OUTPUT1(Left);
			}

			if (ne_tilde_glitch && c == '~') c = '`';

			out(c, attr ? attr[i] : -1);
			string += utf8 ? utf8len(*string) : 1;
		}
}



/* Outputs a NULL-terminated string without setting attributes. */

void output_string(const char * const s, const bool utf8) {
	assert(s != NULL);
	output_chars(s, NULL, strlen(s), utf8);
}


/* Outputs a single ISO 10646 character with a given set of attributes. If
   attr == -1, no attribute is set. */

void output_char(const int c, const uint32_t attr, const bool utf8) {
	static char t[8];

	if (utf8) {
		memset(t, 0, sizeof t);
		utf8str(c, t);
	}
	else {
		t[0] = c;
		t[1] = 0;
	}

	assert(c != 0);

	output_chars(t, attr != -1 ? &attr : NULL, strlen(t), utf8);
}


/* Outputs spaces. */

void output_spaces(const int n, const uint32_t * const attr) {
	output_chars(NULL, attr, n, false);
}

/* Same as output_chars(), but inserts instead. */

void insert_chars(const char * start, const uint32_t * const attr, const int raw_len, const bool utf8) {
	if (raw_len == 0) return;

	standout_if_wanted();

	/* If the string is non-NULL and UTF-8 encoded, compute its real length. */
	int len = utf8 && start != NULL ? utf8strlen(start, raw_len) : raw_len;

	if (ne_parm_ich) {
		int width = 0;

		if (start != NULL) {
			if (utf8) for(int i = 0; i < raw_len; i += utf8len(start[i])) width += output_width(utf8char(start + i));
			else for(int i = 0; i < raw_len; i++) width += output_width(start[i]);
		}
		else width = len;

		const char * const buf = tparm (ne_parm_ich, width);
		OUTPUT1 (buf);

		if (start) output_chars(start, attr, raw_len, utf8);

		return;
	}

	turn_on_insert ();

	/* If the width of the string exceeds the remaining columns, we reduce
		len. Moreovero, we don't dare to write in the last column of the
		bottom line, if AutoWrap, since that would scroll the whole screen
		on some terminals. */

	cmplus(string_output_width(start, &len, ne_columns - curX - (AutoWrap && curY == ne_lines - 1), utf8));

	if (!ne_transparent_underline && !ne_tilde_glitch && start
		  && ne_insert_padding == NULL && ne_insert_character == NULL) {
		for(int i = 0; i < len; i++) {
			if (attr) set_attr(attr[i]);
			int c;
			if (utf8) {
				c = utf8char(start);
				start += utf8len(*start);
			}
			else c = (unsigned char)*start++;
			out(c, attr ? attr[i] : -1);
		}
	}
	else
		for(int i = 0; i < len; i++) {

			OUTPUT1_IF (ne_insert_character);

			if (!start) {
				/* When outputting spaces, it's only the first attribute that's used. */
				out(' ', attr ? *attr : -1);
			}
			else {
				if (attr) set_attr(attr[i]);
				int c;
				if (utf8) {
					c = utf8char(start);
					start += utf8len(*start);
				}
				else c = (unsigned char)*start++;

				if (ne_tilde_glitch && c == '~') c = '`';

				out(c, attr ? attr[i] : -1);
			}

			OUTPUT1_IF(ne_insert_padding);
		}
}


/* Inserts a single ISO 10646 character. If attr == -1, no attribute is set. */

void insert_char(const int c, const uint32_t attr, const bool utf8) {
	static char t[8];

	if (utf8) {
		memset(t, 0, sizeof t);
		utf8str(c, t);
	}
	else {
		t[0] = c;
		t[1] = 0;
	}

	assert(c != 0);

	insert_chars(t, attr == -1 ? NULL : &attr, strlen(t), utf8);
}

/* Deletes n characters at the current cursor position. */

void delete_chars (int n) {
	if (n == 0) return;

	standout_if_wanted();
	if (delete_in_insert_mode) turn_on_insert();
	else {
		turn_off_insert();
		OUTPUT1_IF(ne_enter_delete_mode);
	}

	if (ne_parm_dch) {
		const char * const buf = tparm(ne_parm_dch, n);
		OUTPUT1(buf);
	}
	else while(n-- != 0) OUTPUT1(ne_delete_character);

	if (!delete_in_insert_mode) OUTPUT_IF(ne_exit_delete_mode);
}


/* This internal function will do an insertion or deletion
for n lines, given a parametrized and/or a one-line capability
for that purpose. */

static void do_multi_ins_del(char * const multi, const char * const single, int n) {
	if (multi) {
		const char * const buf = tparm(multi, n);
		OUTPUT(buf);
	}
	else while(n-- != 0) OUTPUT(single);
}


/* Inserts n lines at vertical position vpos. If n is negative, it deletes -n
   lines. specified_window is taken into account. This function assumes
   line_ins_del_ok == true. Returns true if an insertion/deletion actually happened. */

int ins_del_lines (const int vpos, const int n) {

	int i = n > 0 ? n : -n;

	assert(line_ins_del_ok);
	assert(i != 0);
	assert(vpos < specified_window);

	if (scroll_region_ok && vpos + i >= specified_window) return false;

	if (!ne_memory_below && vpos + i >= ne_lines) return false;

	standout_if_wanted();

	if (scroll_region_ok) {
		if (specified_window != ne_lines) set_scroll_region(vpos, specified_window - 1);

		if (n < 0) {
			move_cursor(specified_window - 1, 0);
			while (i-- != 0) OUTPUTL(ne_scroll_forward, specified_window - vpos + 1);
		}
		else {
			move_cursor(vpos, 0);
			while (i-- != 0) OUTPUTL(ne_scroll_reverse, specified_window - vpos + 1);
		}

		if (specified_window != ne_lines) set_scroll_region(0, ne_lines - 1);
	}
	else {
		if (n > 0) {
			if (specified_window != ne_lines) {
				move_cursor(specified_window - i, 0);
				do_multi_ins_del(ne_parm_delete_line, ne_delete_line, i);
			}

			move_cursor(vpos, 0);
			do_multi_ins_del(ne_parm_insert_line, ne_insert_line, i);
		}
		else {
			move_cursor(vpos, 0);
			do_multi_ins_del(ne_parm_delete_line, ne_delete_line, i);

			if (specified_window != ne_lines) {
				move_cursor(specified_window - i, 0);
				do_multi_ins_del(ne_parm_insert_line, ne_insert_line, i);
			}
			else if (ne_memory_below) {
				move_cursor(ne_lines + n, 0);
				clear_to_end ();
			}
		}
	}

	return true;
}


extern int cost;		/* In cm.c */
extern int evalcost(int);


/* Performs the cursor motion cost setup, and sets the variable RPov to the
   number of characters (with padding) which are really output when repeating
   one character. RPov is disable using UTF-8 I/O. */

static void calculate_costs (void) {

	if (ne_repeat_char) {

		char *const buf = tparm(ne_repeat_char, ' ', 1);

		cost = 0;
		tputs(buf, 1, evalcost);

		RPov = cost + 1;
	}
	else RPov = ne_columns * 2;

	cmcostinit();
}



/* Gets the window size using TIOCGSIZE, TIOCGWINSZ, or LINES/COLUMNS as a
   last resort. It is called by the signal handler for SIGWINCH on systems
   that support it. Return 1 if the window size has changed. */

int ttysize(void) {
#ifdef TIOCGSIZE
	/* try using the TIOCGSIZE call, if defined */
	struct ttysize size;
	D(fprintf(stderr,"ttysize (TIOCGSIZE): CHECKING...\n");)
	if (ioctl(0, TIOCGSIZE, &size)) return 0;
	const int l = size.ts_lines;
	const int c = size.ts_cols;
#elif defined(TIOCGWINSZ)
	/* try using the TIOCGWINSZ call, if defined */
	struct winsize size;
	D(fprintf(stderr,"ttysize (TIOCGWINSZ): CHECKING...\n");)
	if (ioctl(0, TIOCGWINSZ, &size)) return 0;
	const int l = size.ws_row;
	const int c = size.ws_col;
#else
	/* As a last resort, we try to read LINES and COLUMNS, falling back to the terminal-specified size. */
	if (! getenv("LINES") || ! getenv("COLUMNS")) return 0;
	const int l = strtol(getenv("LINES"), NULL, 10);
	const int c = strtol(getenv("COLUMNS"), NULL, 10);
#endif
	D(fprintf(stderr,"ttysize:...size is (%d,%d)\n", l, c);)
	if (((ne_lines != l) || (ne_columns != c)) && l > 0 && c > 0) {
		ScreenRows = ne_lines	 = l;
		ScreenCols = ne_columns  = c;
		set_terminal_window(ne_lines - 1);
		if (scroll_region_ok) set_scroll_region(0, ne_lines - 1);
		D(fprintf(stderr,"ttysize: size changed.\n");)
		return 1;
	}

	return 0;
}


#ifndef TERMCAP

/* If we get capabilities from the database, then we copy them into our
   internal counterparts. */

void copy_caps(void) {

	ne_generic_type = generic_type;

	ne_lines = lines;
	ne_columns = columns;
	ne_no_color_video = no_color_video == -1 ? 0 : no_color_video;

	ne_column_address = column_address;
	ne_row_address = row_address;

	ne_cursor_address = cursor_address;

	ne_carriage_return = carriage_return;

	ne_cursor_home = cursor_home;
	ne_cursor_to_ll = cursor_to_ll;

	ne_cursor_right = cursor_right;
	ne_cursor_down = cursor_down;
	ne_cursor_left = cursor_left;
	ne_cursor_up = cursor_up;

	ne_auto_right_margin = auto_right_margin;
	ne_eat_newline_glitch = eat_newline_glitch;

	ne_clr_eos = clr_eos;
	ne_clear_screen = clear_screen;

	ne_bell = bell;
	ne_flash_screen = flash_screen;

	ne_scroll_forward = scroll_forward;
	ne_scroll_reverse = scroll_reverse;

	ne_enter_delete_mode = enter_delete_mode;
	ne_exit_delete_mode = exit_delete_mode;
	ne_enter_insert_mode = enter_insert_mode;
	ne_exit_insert_mode = exit_insert_mode;

	ne_enter_standout_mode = enter_standout_mode;
	ne_exit_standout_mode = exit_standout_mode;
	ne_magic_cookie_glitch = magic_cookie_glitch;
	ne_move_standout_mode = move_standout_mode;

	ne_change_scroll_region = change_scroll_region;

	ne_insert_line = insert_line;
	ne_parm_insert_line = parm_insert_line;
	ne_delete_line = delete_line;
	ne_parm_delete_line = parm_delete_line;

	ne_insert_character = insert_character;
	ne_insert_padding = insert_padding;
	ne_parm_ich = parm_ich;

	ne_delete_character = delete_character;
	ne_parm_dch = parm_dch;

	ne_move_insert_mode = move_insert_mode;

	ne_cursor_invisible = cursor_invisible;
	ne_cursor_normal = cursor_normal;

	ne_init_1string = init_1string;
	ne_init_2string = init_2string;
	ne_init_3string = init_3string;
	ne_enter_ca_mode = enter_ca_mode;
	ne_exit_ca_mode = exit_ca_mode;

	ne_exit_attribute_mode = exit_attribute_mode;
	ne_exit_alt_charset_mode = exit_alt_charset_mode;

	ne_repeat_char = repeat_char;

	ne_tilde_glitch = tilde_glitch;
	ne_memory_below = memory_below;

	ne_has_meta_key = has_meta_key;
	ne_meta_on = meta_on;
	ne_meta_off = meta_off;

	ne_set_window = set_window;

	ne_keypad_local = keypad_local;
	ne_keypad_xmit = keypad_xmit;

	ne_clr_eol = clr_eol;
	ne_transparent_underline = transparent_underline;

	if (ansi_color_ok = (set_a_foreground && set_a_background)) {
		ne_set_background = set_a_background;
		ne_set_foreground = set_a_foreground;
	}
	else {
		ne_set_background = set_background;
		ne_set_foreground = set_foreground;
	}

	ne_enter_underline_mode = enter_underline_mode;
	ne_exit_underline_mode = exit_underline_mode;

	ne_enter_bold_mode = enter_bold_mode;
	ne_enter_blink_mode = enter_blink_mode;
	ne_enter_dim_mode = enter_dim_mode;
	ne_enter_reverse_mode = enter_reverse_mode;
	ne_exit_attribute_mode = exit_attribute_mode;
}


#endif



/* This is the main terminal initialization function. It sets up Wcm,
patches here and there the terminfo database, calculates the costs, and
initializes the terminal characteristics variables. Note that this function
can exit(). */

void term_init (void) {

	int errret;

	/* First of all we initialize the terminfo database. */

	if (ansi) setup_ansi_term();
	else if (setupterm(0, 1, &errret) == ERR) {
		printf("There are problems in finding your terminal in the database.\n"
				 "Please check that the variable TERM is set correctly, and that\n"
				 "your " DATABASE_NAME " database is up to date.\n"
				 "If your terminal is ANSI-compatible, you can also try to use\n"
				 "the --ansi switch.\n");
		exit(1);
	}
#ifndef TERMCAP
	else copy_caps();
#endif

	ColPosition = ne_column_address;
	RowPosition = ne_row_address;
	AbsPosition = ne_cursor_address;
	CR = ne_carriage_return;
	Home = ne_cursor_home;
	LastLine = ne_cursor_to_ll;
	Right = ne_cursor_right;
	Down = ne_cursor_down;
	Left = ne_cursor_left;
	Up = ne_cursor_up;
	AutoWrap = ne_auto_right_margin;
	MagicWrap = ne_eat_newline_glitch;
	ScreenRows = ne_lines;
	ScreenCols = ne_columns;

	if (!ne_bell) ne_bell = "\07";

	if (!ne_scroll_forward) ne_scroll_forward = Down;
	if (!ne_scroll_reverse) ne_scroll_reverse = Up;

	if (!ansi && key_backspace && key_left && !strcmp(key_backspace, key_left)) {
		/* In case left and backspace produce the same sequence,
		we want to get key_left. */

		key_backspace = NULL;
	}

	specified_window = ne_lines;

	if (Wcm_init()) {

		/* We can't do cursor motion */

		if (ne_generic_type) {
			printf("Your terminal type is a generic terminal, not a real\n"
					 "terminal, and it lacks the ability to position the cursor.\n"
					 "Please check that the variable TERM is set correctly, and that\n"
					 "your " DATABASE_NAME " database is up to date.\n");
		}
		else {
			printf("Your terminal type is not powerful enough to run ne:\n"
					 "it lacks the ability to position the cursor.\n"
					 "Please check that the variable TERM is set correctly, and that\n"
					 "your " DATABASE_NAME "database is up to date.\n");
		}

		printf("If your terminal is ANSI-compatible, you can also try to use\n"
				 "the --ansi switch.\n");

		exit(1);
	}

	calculate_costs();

	delete_in_insert_mode
		  = ne_enter_delete_mode && ne_enter_insert_mode
		  && !strcmp (ne_enter_delete_mode, ne_enter_insert_mode);

	se_is_so = ne_enter_standout_mode && ne_exit_standout_mode
		  && !strcmp (ne_enter_standout_mode, ne_exit_standout_mode);

	esm_is_eam = ne_exit_standout_mode && ne_exit_attribute_mode
		  && !strcmp (ne_exit_standout_mode, ne_exit_attribute_mode);

	scroll_region_ok = ne_set_window || ne_change_scroll_region;

	line_ins_del_ok = (((ne_insert_line || ne_parm_insert_line)
		  && (ne_delete_line || ne_parm_delete_line))
		  || (scroll_region_ok
		  && ne_scroll_forward
		  && ne_scroll_reverse));

	char_ins_del_ok = ((ne_insert_character || ne_enter_insert_mode ||
		 ne_insert_padding || ne_parm_ich)
		  && (ne_delete_character || ne_parm_dch));

	standout_ok = (ne_enter_standout_mode && ne_exit_standout_mode && ne_magic_cookie_glitch < 0);

	cursor_on_off_ok = (ne_cursor_invisible && ne_cursor_normal);

	color_ok = (ne_set_foreground && ne_set_background);
}
