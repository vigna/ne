/* terminfo emulation through GNU termcap definitions.

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


#include "termcap.h"

#define ERR -1

extern char *tparm(const char *cap_string,...);
extern int setupterm(const char *, int, const int *);

extern char *key_up;
extern char *key_down;
extern char *key_left;
extern char *key_right;

extern char *key_home;
extern char *key_end;

extern char *key_npage;
extern char *key_ppage;

extern char *key_sf;
extern char *key_sr;


	/* Editing keys */

extern char *key_eol;
extern char *key_eos;
extern char *key_backspace;
extern char *key_dl;
extern char *key_il;
extern char *key_dc;
extern char *key_ic;
extern char *key_eic;
extern char *key_clear;


	/* Keypad keys */

extern char *key_a1;
extern char *key_a3;
extern char *key_b2;
extern char *key_c1;
extern char *key_c3;


	/* Tab keys (never used in the standard configuration) */

extern char *key_catab;
extern char *key_ctab;
extern char *key_stab;


	/* Function keys */

extern char *key_f0;
extern char *key_f1;
extern char *key_f2;
extern char *key_f3;
extern char *key_f4;
extern char *key_f5;
extern char *key_f6;
extern char *key_f7;
extern char *key_f8;
extern char *key_f9;
extern char *key_f10;

extern char *key_f11;
extern char *key_f12;
extern char *key_f13;
extern char *key_f14;
extern char *key_f15;
extern char *key_f16;
extern char *key_f17;
extern char *key_f18;
extern char *key_f19;
extern char *key_f20;
extern char *key_f21;
extern char *key_f22;
extern char *key_f23;
extern char *key_f24;
extern char *key_f25;
extern char *key_f26;
extern char *key_f27;
extern char *key_f28;
extern char *key_f29;
extern char *key_f30;
extern char *key_f31;
extern char *key_f32;
extern char *key_f33;
extern char *key_f34;
extern char *key_f35;
extern char *key_f36;
extern char *key_f37;
extern char *key_f38;
extern char *key_f39;
extern char *key_f40;
extern char *key_f41;
extern char *key_f42;
extern char *key_f43;
extern char *key_f44;
extern char *key_f45;
extern char *key_f46;
extern char *key_f47;
extern char *key_f48;
extern char *key_f49;
extern char *key_f50;
extern char *key_f51;
extern char *key_f52;
extern char *key_f53;
extern char *key_f54;
extern char *key_f55;
extern char *key_f56;
extern char *key_f57;
extern char *key_f58;
extern char *key_f59;
extern char *key_f60;
extern char *key_f61;
extern char *key_f62;
extern char *key_f63;


/* These functions are no-ops in our termcap emulation. */

#define resetterm()
#define fixterm()
