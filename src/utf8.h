/* UTF-8 support prototypes.

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


int utf8char(const unsigned char *s);
int utf8strlen(const unsigned char *s, int len);
int utf8seqlen(int c);
int utf8str(int c, unsigned char *  s);
int utf8tolower(int c);
int utf8toupper(int c);

/* Computes the length of an UTF-8 sequence, given the first byte. If the byte
	is not a legal sequence start, this function returns -1. */

#define utf8len(c) \
	(((unsigned char)(c)) < 0x80 ?  1 : \
	 ((unsigned char)(c)) < 0xC0 ? -1 : \
	 ((unsigned char)(c)) < 0xE0 ?  2 : \
	 ((unsigned char)(c)) < 0xF0 ?  3 : \
	 ((unsigned char)(c)) < 0xF8 ?  4 : \
	 ((unsigned char)(c)) < 0xFC ?  5 : 6)

#ifdef NOWCHAR
#define wcwidth(x) (1)
#else
#include <wchar.h>
#include <wctype.h>
#endif
