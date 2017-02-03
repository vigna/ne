/* terminfo emulation through GNU termcap code.

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
#include "termcap.h"
#include <stdarg.h>


/* This is the number of characters reserved to strings obtained through tparam(). They
*have* to be enough, because otherwise the capability string will be silently truncated. */

#define TPARAM_BUF_LEN 2048

char *key_up;
char *key_down;
char *key_left;
char *key_right;

char *key_home;
char *key_end;

char *key_npage;
char *key_ppage;

char *key_sf;
char *key_sr;

char *key_eol;
char *key_eos;
char *key_backspace;
char *key_dl;
char *key_il;
char *key_dc;
char *key_ic;
char *key_eic;
char *key_clear;

char *key_a1;
char *key_a3;
char *key_b2;
char *key_c1;
char *key_c3;

char *key_catab;
char *key_ctab;
char *key_stab;

char *key_f0;
char *key_f1;
char *key_f2;
char *key_f3;
char *key_f4;
char *key_f5;
char *key_f6;
char *key_f7;
char *key_f8;
char *key_f9;
char *key_f10;

char *key_f11;
char *key_f12;
char *key_f13;
char *key_f14;
char *key_f15;
char *key_f16;
char *key_f17;
char *key_f18;
char *key_f19;
char *key_f20;
char *key_f21;
char *key_f22;
char *key_f23;
char *key_f24;
char *key_f25;
char *key_f26;
char *key_f27;
char *key_f28;
char *key_f29;
char *key_f30;
char *key_f31;
char *key_f32;
char *key_f33;
char *key_f34;
char *key_f35;
char *key_f36;
char *key_f37;
char *key_f38;
char *key_f39;
char *key_f40;
char *key_f41;
char *key_f42;
char *key_f43;
char *key_f44;
char *key_f45;
char *key_f46;
char *key_f47;
char *key_f48;
char *key_f49;
char *key_f50;
char *key_f51;
char *key_f52;
char *key_f53;
char *key_f54;
char *key_f55;
char *key_f56;
char *key_f57;
char *key_f58;
char *key_f59;
char *key_f60;
char *key_f61;
char *key_f62;
char *key_f63;



/* The tparm() emulation. Note that we have to use a static buffer (because
tparm() does it). Thus, instantiated strings longer than TPARAM_BUF_LEN will
be copied in tparam_buffer and silently truncated. It should never happen
with reasonable values for TPARAM_BUF_LEN, though. */

char *tparm(const char * const cap_string,...) {
	static char tparam_buffer[TPARAM_BUF_LEN];

	va_list ap;
	int arg[4];

	va_start(ap, cap_string);
	for(int i = 0; i < 4; i++) arg[i] = va_arg(ap, int);
	va_end(ap);

	char * const p = tparam(cap_string, tparam_buffer, TPARAM_BUF_LEN, arg[0], arg[1], arg[2], arg[3]);

	if (p != tparam_buffer) {
		memcpy(tparam_buffer, p, TPARAM_BUF_LEN - 1);
		free(p);
	}

	return tparam_buffer;
}


/* This is a real fake. We already know all the parameters. */

int setupterm(const char * const dummy1, const int dunmmy2, const int * const dummy3) {
	char * const term_name = getenv("TERM"), *s;
	if (!term_name) return ERR;

	int c = tgetent(NULL, term_name);

	if (c != 1) return ERR;

	/*	if (c == -1) {
		printf("Could not access the termcap database.\n");
		exit(1);
	}
	else if (c == 0) {
		printf("There is no terminal named %s.\n", term_name);
		exit(1);
		}*/

	struct termios termios;
	tcgetattr(0, &termios);

	switch(cfgetospeed(&termios)) {

		case B50:
			ospeed = 1;
			break;

		case B75:
			ospeed = 2;
			break;

		case B110:
			ospeed = 3;
			break;

		case B134:
			ospeed = 4;
			break;

		case B150:
			ospeed = 5;
			break;

		case B200:
			ospeed = 6;
			break;

		case B300:
			ospeed = 7;
			break;

		case B600:
			ospeed = 8;
			break;

		case B1200:
			ospeed = 9;
			break;

		case B1800:
			ospeed = 10;
			break;

		case B2400:
			ospeed = 11;
			break;

		case B4800:
			ospeed = 12;
			break;

		case B9600:
			ospeed = 13;
			break;

		case B19200:
			ospeed = 14;
			break;
	}

	if (ospeed == 0) ospeed = 15;

	if (s = tgetstr("pc", NULL)) PC = *s;

	ne_generic_type = tgetflag("gn");

	if ((ne_lines = tgetnum("li")) <= 0) ne_lines = 25;
	if ((ne_columns = tgetnum("co")) <= 0) ne_columns = 80;

	int l = c = 0;
	if (s = getenv("LINES")) l = atoi(s);
	if (s = getenv("COLUMNS")) c = atoi(s);
	if (l > 0 && c > 0) {
		ne_lines = l;
		ne_columns = c;
	}

	ne_column_address = tgetstr("ch", NULL);
	ne_row_address = tgetstr("cv", NULL);

	ne_cursor_address = tgetstr("cm", NULL);

	ne_carriage_return = tgetstr("cr", NULL);

	ne_cursor_home = tgetstr("ho", NULL);
	ne_cursor_to_ll = tgetstr("ll", NULL);

	ne_cursor_right = tgetstr("nd", NULL);

	ne_cursor_down = tgetstr("do", NULL);
	ne_cursor_left = tgetstr("le", NULL);
	ne_cursor_up = tgetstr("up", NULL);

	ne_auto_right_margin = tgetflag("am");
	ne_eat_newline_glitch = tgetflag("xn");

	ne_clr_eos = tgetstr("cd", NULL);
	ne_clear_screen = tgetstr("cl", NULL);

	ne_bell = tgetstr("bl", NULL);
	ne_flash_screen = tgetstr("vb", NULL);

	ne_scroll_forward = tgetstr("sf", NULL);
	ne_scroll_reverse = tgetstr("sr", NULL);

	ne_enter_delete_mode = tgetstr("dm", NULL);
	ne_exit_delete_mode = tgetstr("ed", NULL);
	ne_enter_insert_mode = tgetstr("im", NULL);
	ne_exit_insert_mode = tgetstr("ei", NULL);

	ne_enter_standout_mode = tgetstr("so", NULL);
	ne_exit_standout_mode = tgetstr("se", NULL);
	ne_magic_cookie_glitch = tgetnum("sg");
	ne_move_standout_mode = tgetflag("ms");

	ne_change_scroll_region = tgetstr("cs", NULL);

	ne_insert_line = tgetstr("al", NULL);
	ne_parm_insert_line = tgetstr("AL", NULL);
	ne_delete_line = tgetstr("dl", NULL);
	ne_parm_delete_line = tgetstr("DL", NULL);

	ne_insert_character = tgetstr("ic", NULL);
	ne_parm_ich = tgetstr("IC", NULL);

	ne_insert_padding = tgetstr("ip", NULL);

	ne_delete_character = tgetstr("dc", NULL);
	ne_parm_dch = tgetstr("DC", NULL);

	ne_move_insert_mode = tgetflag("mi");

	ne_cursor_invisible = tgetstr("vi", NULL);
	ne_cursor_normal = tgetstr("ve", NULL);

	ne_init_1string = tgetstr("i1", NULL);
	ne_init_2string = tgetstr("is", NULL);
	ne_init_3string = tgetstr("i3", NULL);

	ne_enter_ca_mode = tgetstr("ti", NULL);
	ne_exit_ca_mode = tgetstr("te", NULL);

	ne_exit_attribute_mode = tgetstr("me", NULL);
	ne_exit_alt_charset_mode = tgetstr("ae", NULL);

	ne_repeat_char = tgetstr("rp", NULL);

	ne_tilde_glitch = tgetflag("hz");
	ne_memory_below = tgetflag("db");

	ne_has_meta_key = tgetflag("km");
	ne_meta_on = tgetstr("mm", NULL);
	ne_meta_off = tgetstr("mo", NULL);

	ne_set_window = tgetstr("wi", NULL);

	ne_keypad_local = tgetstr("ke", NULL);
	ne_keypad_xmit = tgetstr("ks", NULL);

	ne_clr_eol = tgetstr("ce", NULL);
	ne_transparent_underline = tgetflag("ul");

	/* TODO: add strings */

	key_up = tgetstr("ku", NULL);
	key_down = tgetstr("kd", NULL);
	key_left = tgetstr("kl", NULL);
	key_right = tgetstr("kr", NULL);

	key_home = tgetstr("kh", NULL);
	key_end = tgetstr("@7", NULL);

	key_npage = tgetstr("kN", NULL);
	key_ppage = tgetstr("kP", NULL);

	key_sf = tgetstr("kF", NULL);
	key_sr = tgetstr("kR", NULL);


	/* Editing keys */

	key_eol = tgetstr("kE", NULL);
	key_eos = tgetstr("kS", NULL);
	key_backspace = tgetstr("kb", NULL);
	key_dl = tgetstr("kL", NULL);
	key_il = tgetstr("kA", NULL);
	key_dc = tgetstr("kD", NULL);
	key_ic = tgetstr("kI", NULL);
	key_eic = tgetstr("kM", NULL);
	key_clear = tgetstr("kC", NULL);


	/* Keypad keys */

	key_a1 = tgetstr("K1", NULL);
	key_a3 = tgetstr("K2", NULL);
	key_b2 = tgetstr("K3", NULL);
	key_c1 = tgetstr("K4", NULL);
	key_c3 = tgetstr("K5", NULL);


	/* Tab keys (never used in the standard configuration) */

	key_catab = tgetstr("ka", NULL);
	key_ctab = tgetstr("kt", NULL);
	key_stab = tgetstr("kT", NULL);


	/* Function keys */

	key_f0 = key_f10 = tgetstr("k0", NULL);
	key_f1 = tgetstr("k1", NULL);
	key_f2 = tgetstr("k2", NULL);
	key_f3 = tgetstr("k3", NULL);
	key_f4 = tgetstr("k4", NULL);
	key_f5 = tgetstr("k5", NULL);
	key_f6 = tgetstr("k6", NULL);
	key_f7 = tgetstr("k7", NULL);
	key_f8 = tgetstr("k8", NULL);
	key_f9 = tgetstr("k9", NULL);

	return 0;
}
