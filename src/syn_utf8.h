/* Syntax highlighting from Joe's Own Editor: UTF-8 utilities.

	Copyright (C) 2004 Joseph H. Allen
	Copyright (C) 2009-2017 Todd M. Lewis and Sebastiano Vigna

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


#ifndef _Iutf8
#define _Iutf8 1

/* UTF-8 Encoder
 *
 * c is unicode character.
 * buf is 7 byte buffer- utf-8 coded character is written to this followed by a 0 termination.
 * returns length (not including terminator).
 */

int utf8_encode PARAMS((unsigned char *buf,int c));

/* UTF-8 decoder state machine */

struct utf8_sm {
	unsigned char buf[8];	/* Record of sequence */
	int ptr;		/* Record pointer */
	int state;		/* Current state.  0 = idle, anything else is no. of chars left in sequence */
	int accu;		/* Character accumulator */
};

/* UTF-8 Decoder
 *
 * Returns 0 - 7FFFFFFF: decoded character
 *                   -1: character accepted, nothing decoded yet.
 *                   -2: incomplete sequence
 *                   -3: no sequence started, but character is between 128 - 191, 254 or 255
 */

int utf8_decode PARAMS((struct utf8_sm *utf8_sm,unsigned char c));

int utf8_decode_string PARAMS((unsigned char *s));

int utf8_decode_fwrd PARAMS((unsigned char **p,int *plen));

/* Initialize state machine */

void utf8_init PARAMS((struct utf8_sm *utf8_sm));

int unictrl PARAMS((int ucs));
int mk_wcwidth PARAMS((int wide,int c));

extern int guess_non_utf8;
extern int guess_utf8;

#endif
