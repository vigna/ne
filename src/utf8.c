/* UTF-8 support.

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


#include <assert.h>
#include <ctype.h>
#include "utf8.h"


/* Computes the character length of an UTF-8 encoded sequence of bytes. */

int64_t utf8strlen(const char * const s, const int64_t len) {
	int64_t i = 0, l = 0;
	while(i < len) {
		assert(utf8len(s[i]) >= 0);
		i += utf8len(s[i]);
		l++;
	}
	return l;
}


/* Returns the length of a bytes sequence encoding the given character. */

int utf8seqlen(const int c) {
	if (c < 0x80) return 1;
	if (c < 0x800) return 2;
	if (c < 0x10000) return 3;
	if (c < 0x200000) return 4;
	if (c < 0x4000000) return 5;
	return 6;
}

/* Return the Unicode characters represented by the given string, or -1 if an error occurs. */

int utf8char(const char * const ss) {
	const unsigned char * const s = (const unsigned char *)ss;
	if (s[0] < 0x80) return s[0];
	if (s[0] < 0xC0) return -1;
	if (s[0] < 0xE0) return (s[0] & 0x1F) << 6 | s[1] & 0x3F;
	if (s[0] < 0xF0) return (s[0] & 0xF) << 12 | (s[1] & 0x3F) << 6 | (s[2] & 0x3F);
	if (s[0] < 0xF8) return (s[0] & 0x7) << 18 | (s[1] & 0x3F) << 12 | (s[2] & 0x3F) << 6 | (s[3] & 0x3F);
	if (s[0] < 0xFC) return (s[0] & 0x3) << 24 | (s[1] & 0x3F) << 18 | (s[2] & 0x3F) << 12 | (s[3] & 0x3F) << 6 | (s[4] & 0x3F);
	return (s[0] & 0x1) << 30 | (s[1] & 0x3F) << 24 | (s[2] & 0x3F) << 18 | (s[3] & 0x3F) << 12 | (s[4] & 0x3F) << 6 | (s[5] & 0x3F);
}

/* Writes the UTF-8 encoding (at most 6 bytes) of the given character to the
   given string. Returns the length of the string written. */

int utf8str(const int c, char * const ss) {
	unsigned char * const s = (unsigned char *)ss;

	if (c < 0x80) {
		s[0] = c;
		return 1;
	}

	if (c < 0x800) {
		s[0] = c >> 6 | 0xC0;
		s[1] = c & 0x3F | 0x80;
		return 2;
	}

	if (c < 0x10000) {
		s[0] = c >> 12 | 0xE0;
		s[1] = c >> 6 & 0x3F | 0x80;
		s[2] = c & 0x3F | 0x80;
		return 3;
	}

	if (c < 0x200000) {
		s[0] = c >> 18 | 0xF0;
		s[1] = c >> 12 & 0x3F | 0x80;
		s[2] = c >> 6 & 0x3F | 0x80;
		s[3] = c & 0x3F | 0x80;
		return 4;
	}

	if (c < 0x4000000) {
		s[0] = c >> 24 | 0xF8;
		s[1] = c >> 18 & 0x3F | 0x80;
		s[2] = c >> 12 & 0x3F | 0x80;
		s[3] = c >> 6 & 0x3F | 0x80;
		s[4] = c & 0x3F | 0x80;
		return 5;
	}

	s[0] = c >> 30 | 0xFC;
	s[1] = c >> 24 & 0x3F | 0x80;
	s[2] = c >> 18 & 0x3F | 0x80;
	s[3] = c >> 12 & 0x3F | 0x80;
	s[4] = c >> 6 & 0x3F | 0x80;
	s[5] = c & 0x3F | 0x80;
	return 6;
}


/* Upper cases an UTF-8 character. The only point of this function is that is
   has the same prototype as toupper(). */

int utf8toupper(const int c) {
#ifdef NOWCHAR
	return c < 0x80 ? toupper(c) : c;
#else
	return towupper(c);
#endif
}

/* Lower cases an UTF-8 character. The only point of this function is that is
   has the same prototype as tolower(). */

int utf8tolower(const int c) {
#ifdef NOWCHAR
	return c < 0x80 ? tolower(c) : c;
#else
	return towlower(c);
#endif
}
