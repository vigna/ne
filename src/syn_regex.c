/* Syntax highlighting from Joe's Own Editor: Regular expression subroutines.

	Copyright (C) 1992 Joseph H. Allen
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


#define PARAMS(protos) protos
#include "syn_types.h"
#include "syn_utf8.h"

int escape(int utf8, unsigned char **a, int *b)
{
	int c;
	unsigned char *s = *a;
	int l = *b;

	if (*s == '\\' && l >= 2) {
		++s; --l;
		switch (*s) {
		case 'n':
			c = 10;
			++s; --l;
			break;
		case 't':
			c = 9;
			++s; --l;
			break;
		case 'a':
			c = 7;
			++s; --l;
			break;
		case 'b':
			c = 8;
			++s; --l;
			break;
		case 'f':
			c = 12;
			++s; --l;
			break;
		case 'e':
			c = 27;
			++s; --l;
			break;
		case 'r':
			c = 13;
			++s; --l;
			break;
		case '8':
			c = 8;
			++s; --l;
			break;
		case '9':
			c = 9;
			++s; --l;
			break;
		case '0':
		case '1':
		case '2':
		case '3':
		case '4':
		case '5':
		case '6':
		case '7':
			c = *s - '0';
			++s; --l;
			if (l > 0 && *s >= '0' && *s <= '7') {
				c = c * 8 + s[1] - '0';
				++s; --l;
			}
			if (l > 0 && *s >= '0' && *s <= '7') {
				c = c * 8 + s[1] - '0';
				++s; --l;
			}
			break;
		case 'x':
		case 'X':
			c = 0;
			++s; --l;
			if (l > 0 && *s >= '0' && *s <= '9') {
				c = c * 16 + *s - '0';
				++s; --l;
			} else if (l > 0 && *s >= 'A' && *s <= 'F') {
				c = c * 16 + *s - 'A' + 10;
				++s; --l;
			} else if (l > 0 && *s >= 'a' && *s <= 'f') {
				c = c * 16 + *s - 'a' + 10;
				++s; --l;
			}

			if (l > 0 && *s >= '0' && *s <= '9') {
				c = c * 16 + *s - '0';
				++s; --l;
			} else if (l > 0 && *s >= 'A' && *s <= 'F') {
				c = c * 16 + *s - 'A' + 10;
				++s; --l;
			} else if (l > 0 && *s >= 'a' && *s <= 'f') {
				c = c * 16 + *s - 'a' + 10;
				++s; --l;
			}
			break;
		default:
			if (utf8)
				c = utf8_decode_fwrd(&s, &l);
			else {
				c = *s++;
				--l;
			}
			break;
		}
	} else if (utf8) {
		c = utf8_decode_fwrd(&s,&l);
	} else {
		c = *s++;
		--l;
	}
	*a = s;
	*b = l;
	return c;
}


/*
static int brack(int utf8,unsigned char **a, int *la, int c)
{
	int inverse = 0;
	int flag = 0;
	unsigned char *s = *a;
	int l = *la;

	if (!l)
		return 0;
	if (*s == '^' || *s == '*') {
		inverse = 1;
		++s;
		--l;
	}
	if (l && *s == ']') {
		++s;
		--l;
		if (c == ']')
			flag = 1;
	}
	while (l)
		if (*s == ']') {
			++s;
			--l;
			break;
		} else {
			int cl, cr;

			cl = escape(utf8, &s, &l);

			if (l >= 2 && s[0] == '-' && s[1] != ']') {
				--l;
				++s;
				cr = escape(utf8, &s, &l);
				if (c >= cl && c <= cr)
					flag = 1;
			} else if (c == cl)
				flag = 1;
		}
	*a = s;
	*la = l;
	if (inverse)
		return !flag;
	else
		return flag;
}
*/


/*
static void savec(int utf8,unsigned char **pieces, int n, int c)
{
	unsigned char buf[16];
	int len;
	unsigned char *s = NULL;

	if (utf8)
		len = utf8_encode(buf,c);
	else {
		buf[0] = c;
		len = 1;
	}

	if (pieces[n])
		vsrm(pieces[n]);
	s = vsncpy(s, 0, buf, len);
	pieces[n] = s;
}
*/




#define MAX_REGEX_SAVED 16384 /* Largest regex string we will save */


/*
static void saves(unsigned char **pieces, int n, P *p, long int szz)
{
	if (szz > MAX_REGEX_SAVED)
		pieces[n] = vstrunc(pieces[n], 0);
	else {
		pieces[n] = vstrunc(pieces[n], (int) szz);
		brmem(p, pieces[n], (int) szz);
	}
}
*/



/* Returns -1 (NO_MORE_DATA) for end of file.
 * Returns -2 if we skipped a special sequence and didn't take the character
 * after it (this happens for "strings").
 * Otherwise returns character after sequence (character will be >=0).
 */

/*
static int skip_special(P *p)
{
	int to, s;

	switch (s = pgetc(p)) {
	case '"':
		do {
			if ((s = pgetc(p)) == '\\') {
				pgetc(p);
				s = pgetc(p);
			}
		} while (s != NO_MORE_DATA && s != '"');
		if (s == '"')
			return -2;
		break;
	case '\'':
		if ((s = pgetc(p)) == '\\') {
			s = pgetc(p);
			s = pgetc(p);
		}
		if (s == '\'')
			return -2;
		if ((s = pgetc(p)) == '\'')
			return -2;
		if ((s = pgetc(p)) == '\'')
			return -2;
		break;
	case '[':
		to = ']';
		goto skip;
	case '(':
		to = ')';
		goto skip;
	case '{':
		to = '}';
skip:
		do {
			s = skip_special(p);
		} while (s != to && s != NO_MORE_DATA);
		if (s == to)
			return -2;
		break;
	case '/':
		s = pgetc(p);
		if (s == '*')
			do {
				s = pgetc(p);
				while (s == '*')
					if ((s = pgetc(p)) == '/')
						return -2;
			} while (s != NO_MORE_DATA);
		else if (s != NO_MORE_DATA)
			s = prgetc(p);
		else
			s = '/';
		break;
	}
	return s;
}
*/


/*
int pmatch(unsigned char **pieces, unsigned char *regex, int len, P *p, int n, int icase)
{
	int c, d;
	P *q = pdup(p, USTR "pmatch");
	P *o = NULL;
	int utf8 = p->b->o.charmap->type;
	struct charmap *map = p->b->o.charmap;
	struct utf8_sm sm;

	utf8_init(&sm);

	while (len) {
		if (utf8) {
			do {
				c = utf8_decode(&sm,*regex++);
				--len;
			} while (len && c<0);
			if (c<0)
				return 0;
		} else {
			c = *regex++;
			--len;
		}

		switch (c) {
		case '\\':
			if (!len--)
				goto fail;
			switch (c = *regex++) {
			case '?':
				d = pgetc(p);
				if (d == NO_MORE_DATA)
					goto fail;
				savec(utf8, pieces, n++, d);
				break;
			case 'n':
			case 'r':
			case 'a':
			case 'f':
			case 'b':
			case 't':
			case 'e':
			case 'x':
			case 'X':
			case '0':
			case '1':
			case '2':
			case '3':
			case '4':
			case '5':
			case '6':
			case '7':
			case '8':
			case '9':
				regex -= 2;
				len += 2;
				if (pgetc(p) != escape(utf8, &regex, &len))
					goto fail;
				break;
			case '*':
	
				o = pdup(p, USTR "pmatch");
				do {
					long pb = p->byte;

					if (pmatch(pieces, regex, len, p, n + 1, icase)) {
						saves(pieces, n, o, pb - o->byte);
						goto succeed;
					}
					c = pgetc(p);
				} while (c != NO_MORE_DATA && c != '\n');
				goto fail;
			case 'c':
				o = pdup(p, USTR "pmatch");
				do {
					long pb = p->byte;

					if (pmatch(pieces, regex, len, p, n + 1, icase)) {
						saves(pieces, n, o, pb - o->byte);
						goto succeed;
					}
				} while (skip_special(p) != NO_MORE_DATA);
				goto fail;
			case '[':
				d = pgetc(p);
				if (d == NO_MORE_DATA)
					goto fail;
				if (!brack(utf8, &regex, &len, d))
					goto fail;
				savec(utf8, pieces, n++, d);
				break;
			case '+':
				{
					unsigned char *oregex = regex;
					int olen = len;

					unsigned char *tregex;
					int tlen;

					int match;

					P *r = NULL;

					int d = 0;

					o = pdup(p, USTR "pmatch");

					/
					if (len >= 2 && regex[0] == '\\') {
						if (regex[1] == '[') {
							regex += 2;
							len -= 2;
							brack(utf8, &regex, &len, 0);
						} else {
							d = escape(utf8, &regex, &len);
							if (icase)
								d = joe_tolower(map,d);
						}
					} else if (utf8) {
						if ((d = utf8_decode_fwrd(&regex, &len)) < 0)
							goto done;
						else if (icase)
							d = joe_tolower(map,d);
					} else {
						if (len >= 1) {
							--len;
							d = *regex++;
							if (icase)
								d = joe_tolower(map,d);
						} else
							goto done;
					}

					/

					do {
						P *z = pdup(p, USTR "pmatch");

						if (pmatch(pieces, regex, len, p, n + 1, icase)) {
							saves(pieces, n, o, z->byte - o->byte);
							if (r)
								prm(r);
							r = pdup(p, USTR "pmatch");
						}
						pset(p, z);
						prm(z);
						c = pgetc(p);
						tregex = oregex;
						tlen = olen;
						if (*oregex == '\\') {
							if (oregex[1] == '[') {
								tregex += 2;
								tlen -= 2;
								match = brack(utf8, &tregex, &tlen, c);
							} else
								match = (d == c);
						} else {
							if(icase)
								match = (joe_tolower(map,c) == d);
							else
								match = (c == d);
						}
					} while (c != NO_MORE_DATA && match);

				      done:
					if (r) {
						pset(p, r);
						prm(r);
					}
					if (r)
						goto succeed;
					else
						goto fail;
				}
			case '^':
				if (!pisbol(p))
					goto fail;
				break;
			case '$':
				if (!piseol(p))
					goto fail;
				break;
			case '<':
				if (!pisbow(p))
					goto fail;
				break;
			case '>':
				if (!piseow(p))
					goto fail;
				break;
			case '\\':
				d = pgetc(p);
				if (d != c)
					goto fail;
				break;
			default:
				goto fail;
			}
			break;
		default:
			d = pgetc(p);
			if (icase) {
				if (joe_tolower(map,d) != joe_tolower(map,c))
					goto fail;
			} else {
				if (d != c)
					goto fail;
			}
		}
	}
succeed:
	if (o)
		prm(o);
	prm(q);
	return 1;

fail:
	if (o)
		prm(o);
	pset(p, q);
	prm(q);
	return 0;
}
*/
