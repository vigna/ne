/* Miscellaneous inline support functions.

   Copyright (C) 1993-1998 Sebastiano Vigna
   Copyright (C) 1999-2025 Todd M. Lewis and Sebastiano Vigna

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


/* Returns the position of the character after the one pointed by pos in s. If
   s is NULL, just returns pos + 1. If encoding is UTF8 it uses utf8len() to
   move forward. */

static int64_t inline next_pos(const char * const ss, const int64_t pos, const encoding_type encoding) {
	const unsigned char *s = (const unsigned char *)ss;
	assert(encoding != ENC_UTF8 || s == NULL || utf8len(s[pos]) > 0);
	if (s == NULL) return pos + 1;
	if (encoding == ENC_UTF8) return pos + utf8len(s[pos]);
	else return pos + 1;
}

/* Returns the position of the character before the one pointed by pos in s.
   If s is NULL, just returns pos + 1. If encoding is UTF-8 moves back until
   the upper two bits are 10. If pos is 0, this function returns -1. */

static int64_t inline prev_pos(const char * const s, int64_t pos, const encoding_type encoding) {
	assert(pos >= 0);
	if (pos == 0) return -1;
	if (s == NULL) return pos - 1;
	if (encoding == ENC_UTF8) {
		while((s[--pos] & 0xC0) == 0x80 && pos > 0);
		return pos;
	}
	else return pos - 1;
}

/* Returns the width of the ISO 10646 character represented by the sequence of bytes
   starting at s, using the provided encoding. */

static int inline get_char_width(const char * const s, const encoding_type encoding) {
	assert(s != NULL);
	return *(unsigned char *)s < 128 ? 1 : encoding == ENC_UTF8 ? output_width(utf8char(s)) : output_width(*(unsigned char *)s);
}

/* Computes the TAB-expanded width of a line descriptor up to a certain
   position. The position can be greater than the line length, the usual
   convention of infinite expansion via spaces being in place. */

static int64_t inline calc_width(const line_desc * const ld, const int64_t n, const int tab_size, const encoding_type encoding) {

	int64_t width = 0;
	for(int64_t pos = 0; pos < n; pos = pos < ld->line_len ? next_pos(ld->line, pos, encoding) : pos + 1) {
		if (pos >= ld->line_len) width++;
		else if (ld->line[pos] != '\t') width += get_char_width(&ld->line[pos], encoding);
		else width += tab_size - width % tab_size;
	}

	return width;
}

/* Computes the TAB-expanded width of a line descriptor up to a certain
   position. The position can be greater than the line length, the usual
   convention of infinite expansion via spaces being in place. An initial
   hint can be provided in the form of a known position and a corresponding
   known width. */

static int64_t inline calc_width_hint(const line_desc * const ld, const int64_t n, const int tab_size, const encoding_type encoding, const int64_t cur_pos, const int64_t cur_width) {
	if (cur_pos < n) {
		int64_t width = cur_width;
		for(int64_t pos = cur_pos; pos < n; pos = pos < ld->line_len ? next_pos(ld->line, pos, encoding) : pos + 1) {
			if (pos >= ld->line_len) width++;
			else if (ld->line[pos] != '\t') width += get_char_width(&ld->line[pos], encoding);
			else width += tab_size - width % tab_size;
		}
		return width;
	}
	else return calc_width(ld, n, tab_size, encoding);
}


/* Computes character length of a line descriptor up to a given position. */

static int64_t inline calc_char_len(const line_desc * const ld, const int64_t n, const encoding_type encoding) {
	int64_t len = 0;
	for(int64_t pos = 0; pos < n; pos = next_pos(ld->line, pos, encoding), len++);
	return len;
}


/* Given a column, the index of the byte "containing" that position is
   given, that is, calc_width(index) > n, and index is minimum with this
   property. If the width of the line is smaller than the given column, the
   line length is returned. */

static int64_t inline calc_pos(const line_desc * const ld, const int64_t col, const int tab_size, const encoding_type encoding) {
	int c_width;
	int64_t pos = 0;
	for(int64_t width = 0; pos < ld->line_len && width + (c_width = get_char_width(&ld->line[pos], encoding)) <= col; pos = next_pos(ld->line, pos, encoding)) {
		if (ld->line[pos] != '\t') width += c_width;
		else width += tab_size - width % tab_size;
	}

	return pos;
}

/* Given a column, the index of the byte "containing" that position is
   given, that is, calc_width(index) > n, and index is minimum with this
   property. If the width of the line is smaller than the given column, the
   line is extended with spaces. */

static int64_t inline calc_virt_pos(const line_desc * const ld, const int64_t col, const int tab_size, const encoding_type encoding) {
	int c_width;
	int64_t pos, width;
	for(pos = width = 0; pos < ld->line_len && width + (c_width = get_char_width(&ld->line[pos], encoding)) <= col; pos = next_pos(ld->line, pos, encoding)) {
		if (ld->line[pos] != '\t') width += c_width;
		else width += tab_size - width % tab_size;
	}

	assert(pos <= ld->line_len);
	assert(pos == ld->line_len || width == col);
	if (pos == ld->line_len) pos += col - width;

	return pos;
}

/* Returns true if the specified character is an US-ASCII whitespace character. */

static bool inline isasciispace(const int c) {
	return c < 0x80 && isspace(c);
}

/* Returns true if the specified character is an US-ASCII alphabetic character. */

static bool inline isasciialpha(const int c) {
	return c < 0x80 && isalpha(c);
}

/* Returns true if the specified block of text is US-ASCII. */

static bool inline is_ascii(const char * const ss, int len) {
	const unsigned char * const s = (const unsigned char *)ss;
	while(len-- != 0) if (s[len] >= 0x80) return false;
	return true;
}


/* Returns toupper() of the given character, if it is US-ASCII, the character itself, otherwise. */

static int inline asciitoupper(const int c) {
	return c < 0x80 ? toupper(c) : c;
}

/* Returns tolower() of the given character, if it is US-ASCII, the character itself, otherwise. */

static int inline asciitolower(const int c) {
	return c < 0x80 ? tolower(c) : c;
}

/* Returns the ISO 10646 character represented by the sequence of bytes
   starting at s, using the provided encoding. */

static int inline get_char(const char * const s, const encoding_type encoding) {
	if (encoding == ENC_UTF8) return utf8char(s);
	else return *(unsigned char *)s;
}

/* Returns the width of the first len characters of s, using the provided
   encoding. If s is NULL, returns len. */

static int inline get_string_width(const char * const s, const int64_t len, const encoding_type encoding) {
	if (s == NULL) return len;
	int64_t width = 0;
	for(int64_t pos = 0; pos < len; pos = next_pos(s, pos, encoding)) width += get_char_width(s + pos, encoding);
	return width;
}

/* Returns whether the given character is a punctuation character. This function is
   compiled differently depending on whether wide-character function support is inhibited. */

static bool inline ne_ispunct(const int c, const int encoding) {
#ifdef NOWCHAR
	return encoding != ENC_UTF8 ? ispunct(c) : c < 0x80 ? ispunct(c) : false;
#else
	return encoding != ENC_UTF8 ? ispunct(c) : iswpunct(c);
#endif
}

/* Returns whether the given character is whitespace. This function is
   compiled differently depending on whether wide-character function support is inhibited. */

static bool inline ne_isspace(const int c, const int encoding) {
#ifdef NOWCHAR
	return encoding != ENC_UTF8 ? isspace(c) : c < 0x80 ? isspace(c) : false;
#else
	return encoding != ENC_UTF8 ? isspace(c) : iswspace(c);
#endif
}

/* Returns whether the given character is a "word" character.
   Word characters are '_' plus any non-punctuation or space.

   TODO: implement a way for users to specify their own word characters.
   For now, hardcode '_'.  */

static bool inline ne_isword(const int c, const int encoding) {
	return c == '_' || !(c == '\0' || ne_isspace(c, encoding) || ne_ispunct(c, encoding));
}
