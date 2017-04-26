/* Search/replace functions (with and without regular expressions).

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
#include "regex.h"
#include "support.h"

/* This is the initial allocation size for regex.library. */

#define START_BUFFER_SIZE 4096

/* A boolean recording whether the last replace was for an empty string
   (of course, this can happen only with regular expressions). */

bool last_replace_empty_match;

/* This array is used both by the Boyer-Moore algorithm and by the regex
library. It is updated if b->find_string_changed != search_serial_num (which 
should be the case the first time the string is searched for). */

static unsigned int d[256];

/* Track static search compilation data by incremented serial counter.
   Compared with b->find_string_changed, which gets set to 1 when the buffer
   wants to force a recompile. We never set search_serial_num to 0 or 1. If
   search_serial_num != b->find_string_changed we know either the last
   compilation was for a different buffer, a different string for this
   buffer, or this is the first compilation. */

static unsigned int search_serial_num = 2;

/* This macro upper cases a character or not, depending on the boolean
sense_case. It is used in find(). Note that the argument *MUST* be unsigned. */

#define CONV(c) (sense_case ? c : up_case[c])


/* This vector is a translation table for the regex library which maps
lower case characters to upper case characters. It's normally adjusted
on startup according to the current locale. */

unsigned char localised_up_case[256];

/* This vector is a translation table for the regex library which maps
ASCII lower case characters to upper case characters. */

const unsigned char ascii_up_case[256] = {
	0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F,
	0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0x1A, 0x1B, 0x1C, 0x1D, 0x1E, 0x1F,
	0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28, 0x29, 0x2A, 0x2B, 0x2C, 0x2D, 0x2E, 0x2F,
	0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x3A, 0x3B, 0x3C, 0x3D, 0x3E, 0x3F,
	0x40, 0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48, 0x49, 0x4A, 0x4B, 0x4C, 0x4D, 0x4E, 0x4F,
	0x50, 0x51, 0x52, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58, 0x59, 0x5A, 0x5B, 0x5C, 0x5D, 0x5E, 0x5F,
	0x60, 0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48, 0x49, 0x4A, 0x4B, 0x4C, 0x4D, 0x4E, 0x4F,
	0x50, 0x51, 0x52, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58, 0x59, 0x5A, 0x7B, 0x7C, 0x7D, 0x7E, 0x7F,
	0x80, 0x81, 0x82, 0x83, 0x84, 0x85, 0x86, 0x87, 0x88, 0x89, 0x8A, 0x8B, 0x8C, 0x8D, 0x8E, 0x8F,
	0x90, 0x91, 0x92, 0x93, 0x94, 0x95, 0x96, 0x97, 0x98, 0x99, 0x9A, 0x9B, 0x9C, 0x9D, 0x9E, 0x9F,
	0xA0, 0xA1, 0xA2, 0xA3, 0xA4, 0xA5, 0xA6, 0xA7, 0xA8, 0xA9, 0xAA, 0xAB, 0xAC, 0xAD, 0xAE, 0xAF,
	0xB0, 0xB1, 0xB2, 0xB3, 0xB4, 0xB5, 0xB6, 0xB7, 0xB8, 0xB9, 0xBA, 0xBB, 0xBC, 0xBD, 0xBE, 0xBF,
	0xC0, 0xC1, 0xC2, 0xC3, 0xC4, 0xC5, 0xC6, 0xC7, 0xC8, 0xC9, 0xCA, 0xCB, 0xCC, 0xCD, 0xCE, 0xCF,
	0xD0, 0xD1, 0xD2, 0xD3, 0xD4, 0xD5, 0xD6, 0xD7, 0xD8, 0xD9, 0xDA, 0xDB, 0xDC, 0xDD, 0xDE, 0xDF,
	0xE0, 0xE1, 0xE2, 0xE3, 0xE4, 0xE5, 0xE6, 0xE7, 0xE8, 0xE9, 0xEA, 0xEB, 0xEC, 0xED, 0xEE, 0xEF,
	0xF0, 0xF1, 0xF2, 0xF3, 0xF4, 0xF5, 0xF6, 0xF7, 0xF8, 0xF9, 0xFA, 0xFB, 0xFC, 0xFD, 0xFE, 0xFF
};



/* Performs a search for the given pattern with a simplified Boyer-Moore
   algorithm starting at the given position, in the given direction, skipping a
   possible match at the current cursor position if skip_first is true. The
   search direction depends on b->opt.search_back. If pattern is NULL, it is
   fetched from b->find_string. In this case, b->find_string_changed is
   checked, and, if equal to search_serial_num, the string is not recompiled. Please check to
   set b->find_string_changed = 1 to force a recompile whenever a new string is set in
   b->find_string. The cursor is moved on the occurrence position if a match is
   found. */

int find(buffer * const b, const char *pattern, const bool skip_first, bool wrap_once) {

	bool recompile_string;

	if (!pattern) {
		pattern = b->find_string;
		recompile_string = b->find_string_changed != search_serial_num || b->last_was_regexp;
	}
	else recompile_string = true;

	if (recompile_string) {
		b->find_string_changed = 0;
		search_serial_num = ((search_serial_num & ~1) + 2)|2;
	}

	const int m = strlen(pattern);
	if (!pattern || !m) return ERROR;

	if (recompile_string) for(int i = 0; i < sizeof d / sizeof *d; i++) d[i] = m;

	const unsigned char * const up_case = b->encoding == ENC_UTF8 ? ascii_up_case : localised_up_case;
	const bool sense_case = (b->opt.case_search != 0);
	line_desc *ld = b->cur_line_desc;
	int64_t y = b->cur_line;
	stop = false;

	if (! b->opt.search_back) {

		if (recompile_string) {
			for(int i = 0; i < m - 1; i++) d[CONV((unsigned char)pattern[i])] = m - i-1;
			b->find_string_changed = search_serial_num;
		}

		char * p = ld->line + b->cur_pos + m - 1 + (skip_first ? 1 : 0);
		const unsigned char first_char = CONV((unsigned char)pattern[m - 1]);
		int64_t wrap_lines_left = b->num_lines + 1;

		while(y < b->num_lines && !stop && wrap_lines_left--) {

			assert(ld->ld_node.next != NULL);

			if (ld->line_len >= m) {

				while((p - ld->line) < ld->line_len) {
					const unsigned char c = CONV((unsigned char)*p);
					if (c != first_char) p += d[c];
					else {
						int i;
						for (i = 1; i < m; i++)
							if (CONV((unsigned char)*(p - i)) != CONV((unsigned char)pattern[m - i-1])) {
								p += d[c];
								break;
							}
						if (i == m) {
							goto_line_pos(b, y, (p - ld->line) - m + 1);
							return OK;
						}
					}
				}
			}

			ld = (line_desc *)ld->ld_node.next;
			if (ld->ld_node.next) p = ld->line + m-1;
			else if (wrap_once) {
				wrap_once = false;
				ld = b->top_line_desc;
				p = ld->line + m-1;
				y = -1;
			}
			y++;
		}
	}
	else {

		if (recompile_string) {
			for(int i = m - 1; i > 0; i--) d[CONV((unsigned char)pattern[i])] = i;
			b->find_string_changed = search_serial_num;
		}

		char * p = ld->line + (b->cur_pos > ld->line_len - m ? ld->line_len - m : b->cur_pos + (skip_first ? -1 : 0));
		const unsigned char first_char = CONV((unsigned char)pattern[0]);
		int64_t wrap_lines_left = b->num_lines + 1;

		while(y >= 0 && !stop && wrap_lines_left--) {

			assert(ld->ld_node.prev != NULL);

			if (ld->line_len >= m) {

				while((p - ld->line) >= 0) {

					const unsigned char c = CONV((unsigned char)*p);
					if (c != first_char) p -= d[c];
					else {
						int i;
						for (i = 1; i < m; i++)
							if (CONV((unsigned char)*(p + i)) != CONV((unsigned char)pattern[i])) {
								p -= d[c];
								break;
							}
						if (i == m) {
							goto_line_pos(b, y, p - ld->line);
							return OK;
						}
					}
				}
			}

			ld = (line_desc *)ld->ld_node.prev;
			if (ld->ld_node.prev) p = ld->line + ld->line_len - m;
			else if (wrap_once) {
				wrap_once = false;
				ld = (line_desc *)b->line_desc_list.tail_pred;
				p = ld->line + ld->line_len - m;
				y = b->num_lines;
			}
			y--;
		}
	}

	return stop ? STOPPED : NOT_FOUND;
}



/* Replaces n characters with the given string at the current cursor position,
   and then moves it to the end of the string. */

int replace(buffer * const b, const int n, const char * const string) {

	int64_t len;

	assert(string != NULL);

	last_replace_empty_match = false;

	len = strlen(string);

	start_undo_chain(b);

	delete_stream(b, b->cur_line_desc, b->cur_line, b->cur_pos, n);

	if (len) insert_stream(b, b->cur_line_desc, b->cur_line, b->cur_pos, string, len);

	end_undo_chain(b);

	if (! b->opt.search_back) goto_pos(b, b->cur_pos + len);

	return OK;
}



/* The following variables are used by regex. In particular, re_reg holds
the stard/end of the extended replacement registers. */

static struct re_pattern_buffer re_pb;
static struct re_registers re_reg;

/* This string is used to replace the dot in UTF-8 searches. It will match only
 whole UTF-8 sequences. */

#define UTF8DOT "([\x01-\x7F\xC0-\xFF][\x80-\xBF]*)"

/* This string is prefixed to a complemented character class to force matches
   against UTF-8 non-US-ASCII characters.  It will match any UTF-8 sequence of
   length at least two, besides all characters expressed by the character
   class. Note that a closing ] and a closing ) must be appended. */

#define UTF8COMP "([\xC0-\xFF][\x80-\xBF]+|[^"

/* This string is used to replace non-word-constituents (\W) in UTF-8
 searches. It will match only whole UTF-8 sequences of non-word-constituent
 characters. */

#define UTF8NONWORD "([\x01-\x1E\x20-\x2F\x3A-\x40\x5B-\x60\x7B-\x7F]|[\xC0-\xFF][\x80-\xBF]+)"

/* In UTF-8 text, the numbering of a parenthesised group may differ from the
   "official" one, due to the usage of parenthesis in UTF8DOT, UT8COMP and
   UTF8NONWORD. This array records for each user-invoked group the
   corresponding (usually larger) regex group. The group may be larger than
   RE_NREGS, in which case there is no way to recover it. */

static int map_group[RE_NREGS];
static int use_map_group;

/* Works exactly like find(), but uses the regex library instead. */

int find_regexp(buffer * const b, const char *regex, const bool skip_first, bool wrap_once) {

	const unsigned char * const up_case = b->encoding == ENC_UTF8 ? ascii_up_case : localised_up_case;
	bool recompile_string;

	if (!regex) {
		regex = b->find_string;
		recompile_string = b->find_string_changed != search_serial_num || !b->last_was_regexp;
	}
	else recompile_string = true;

	if (recompile_string) {
		b->find_string_changed = 0;
		search_serial_num = ((search_serial_num & ~1) + 2)|2;
	}

	if (!regex || !strlen(regex)) return ERROR;

	if (re_pb.buffer == NULL) {
		if (re_pb.buffer = malloc(START_BUFFER_SIZE)) re_pb.allocated = START_BUFFER_SIZE;
		else return OUT_OF_MEMORY;
	}

	re_pb.fastmap = (void *)d;

	/* We have to be careful: even if the search string has not changed, it
	is possible that case sensitivity has. In this case, we force recompilation. */

	if (b->opt.case_search) {
		if (re_pb.translate != 0) recompile_string = true;
		re_pb.translate = 0;
	}
	else {
		if (re_pb.translate != up_case) recompile_string = true;
		re_pb.translate = (unsigned char *)up_case;
	}

	if (recompile_string) {
		const char *actual_regex = regex;

		/* If the buffer encoding is UTF-8, we need to replace dots with UTF8DOT,
			non-word-constituents (\W) with UTF8NONWORD, and embed complemented
			character classes in UTF8COMP, so that they do not match UTF-8
			subsequences. Moreover, we must compute the remapping from the virtual
			to the actual groups caused by the new groups thus introduced. */

		if (b->encoding == ENC_UTF8) {
			const char *s;
			char *q;
			bool escape = false;
			int virtual_group = 0, real_group = 0, dots = 0, comps = 0, nonwords = 0, use_map_group = 0;

			s = regex;

			/* We first scan regex to compute the exact number of characters of
				the actual (i.e., after substitutions) regex. */

			do {
				if (!escape) {
					if (*s == '.') dots++;
					else if (*s == '[') {
						if (*(s+1) == '^') {
							comps++;
							s++;
						}

						if (*(s+1) == ']') s++; /* A literal ]. */

						/* We scan the list up to ] and check that no non-US-ASCII characters appear. */
						do if (utf8len(*(++s)) != 1) return UTF8_REGEXP_CHARACTER_CLASS_NOT_SUPPORTED; while(*s && *s != ']');
					}
					else if (*s == '\\') {
						escape = true;
						continue;
					}
				}
				else if (*s == 'W') nonwords++;
				escape = false;
			} while(*(++s));

			actual_regex = q = malloc(strlen(regex) + 1 + (strlen(UTF8DOT) - 1) * dots + (strlen(UTF8NONWORD) - 2) * nonwords + (strlen(UTF8COMP) - 1) * comps);
			if (!actual_regex) return OUT_OF_MEMORY;
			s = regex;
			escape = false;

			do {
				if (escape || *s != '.' && *s != '(' && *s != '[' && *s != '\\') {
					if (escape && *s == 'W') {
						q--;
						strcpy(q, UTF8NONWORD);
						q += strlen(UTF8NONWORD);
						real_group++;
					}
					else *(q++) = *s;
				}
				else {
					if (*s == '\\') {
						escape = true;
						*(q++) = '\\';
						continue;
					}

					if (*s == '.') {
						strcpy(q, UTF8DOT);
						q += strlen(UTF8DOT);
						real_group++;
					}
					else if (*s == '(') {
						*(q++) = '(';
						if (virtual_group < RE_NREGS - 1) {
							map_group[++virtual_group] = ++real_group;
							use_map_group = virtual_group;
						}
					}
					else if (*s == '[') {
						if (*(s+1) == '^') {
							strcpy(q, UTF8COMP);
							q += strlen(UTF8COMP);
							s++;
							if (*(s+1) == ']') *(q++) = *(++s); /* A literal ]. */
							do *(q++) = *(++s); while (*s && *s != ']');
							if (*s) *(q++) = ')';
							real_group++;
						}
						else {
							*(q++) = '[';
							if (*(s+1) == ']') *(q++) = *(++s); /* A literal ]. */
							do *(q++) = *(++s); while (*s && *s != ']');
						}
					}
				}

				escape = false;
			} while(*(s++));

			/* This assert may be false if a [ is not closed. */
			assert(strlen(actual_regex) == strlen(regex) + (strlen(UTF8DOT) - 1) * dots + (strlen(UTF8NONWORD) - 2) * nonwords + (strlen(UTF8COMP) - 1) * comps);
		}

		const char * p = re_compile_pattern(actual_regex, strlen(actual_regex), &re_pb);

		if (b->encoding == ENC_UTF8) free((void*)actual_regex);

		if (p) {
			/* Here we have a very dirty hack: since we cannot return the error of
				regex, we print it here. Which means that we access term.c's
				functions. 8^( */
			print_message(p);
			alert();
			return ERROR;
		}

	}

	b->find_string_changed = search_serial_num;

	line_desc *ld = b->cur_line_desc;
	int64_t y = b->cur_line;
	stop = false;

	if (! b->opt.search_back) {

		int64_t start_pos = b->cur_pos + (skip_first ? 1 : 0);
		int64_t wrap_lines_left = b->num_lines + 1;

		while(y < b->num_lines && !stop && wrap_lines_left--) {
			assert(ld->ld_node.next != NULL);

			int64_t pos;
			if (start_pos <= ld->line_len &&
				 (pos = re_search(&re_pb, ld->line ? ld->line : "", ld->line_len, start_pos, ld->line_len - start_pos, &re_reg)) >= 0) {
				goto_line_pos(b, y, pos);
				return OK;
			}

			ld = (line_desc *)ld->ld_node.next;
			start_pos = 0;
			y++;
			if (wrap_once && y == b->num_lines) {
				wrap_once = false;
				ld = b->top_line_desc;
				y = 0;
			}
		}
	}
	else {

		int64_t start_pos = b->cur_pos + (skip_first ? -1 : 0);
		int64_t wrap_lines_left = b->num_lines + 1;

		while(y >= 0 && !stop && wrap_lines_left--) {

			assert(ld->ld_node.prev != NULL);

			int64_t pos;
			if (start_pos >= 0 &&
				 (pos = re_search(&re_pb, ld->line ? ld->line : "", ld->line_len, start_pos, -start_pos - 1, &re_reg)) >= 0) {
				goto_line_pos(b, y, pos);
				return OK;
			}

			ld = (line_desc *)ld->ld_node.prev;
			if (ld->ld_node.prev) start_pos = ld->line_len;
			else if (wrap_once) {
				wrap_once = false;
				ld = (line_desc *)b->line_desc_list.tail_pred;
				start_pos = ld->line_len;
				y = b->num_lines;
			}
			y--;
		}
	}

	return stop ? STOPPED : NOT_FOUND;
}


/* This allows regexp users to retrieve matched substrings.
   They are responsible for freeing these strings.
   i0 should be <= number of paren groups in original regex. */
char *nth_regex_substring(const line_desc *ld, int i0) {
	char *str;
	int j, i;

	if ((i = use_map_group ? map_group[i0] : i0) >= RE_NREGS) return NULL;

	if (i > 0 && i < re_reg.num_regs ) {
		if (str = malloc(re_reg.end[i] - re_reg.start[i] + 1)) {
			memcpy(str, ld->line + re_reg.start[i], re_reg.end[i] - re_reg.start[i]);
			str[re_reg.end[i] - re_reg.start[i]] = 0;
			return str;
		}
	}
	return NULL;
}


/* This allows regexp users to check whether matched substrings are nonempty. */
bool nth_regex_substring_nonempty(const line_desc *ld, int i0) {
	int i;
	if ((i = use_map_group ? map_group[i0] : i0) >= RE_NREGS) return false;
	if (i > 0 && i < re_reg.num_regs) return re_reg.start[i] != re_reg.end[i];
	return false;
}


/* Replaces a regular expression. The given string can contain \0, \1 etc. for
   the pattern matched by the i-th pair of brackets (\0 is the whole
   string). */

int replace_regexp(buffer * const b, const char * const string) {
	assert(string != NULL);

	bool reg_used = false;
	char *p, *q, *t = NULL;
	if (q = p = str_dup(string)) {

		int len = strlen(p);

		while(true) {
			while(*q && *q != '\\') q++;

			if (!*q) break;

			int i = *(q + 1) - '0';

			if (*(q + 1) == '\\') {
				memmove(q, q + 1, strlen(q + 1) + 1);
				q++;
				len--;
			}
			else if (i >= 0 && i < re_reg.num_regs && re_reg.start[i] >= 0) {
				if (b->encoding == ENC_UTF8) {
					/* In the UTF-8 case, the replacement group index must be
						mapped through map_group to recover the real group. */
					if ((i = map_group[i]) >= RE_NREGS) {
						free(p);
						return GROUP_NOT_AVAILABLE;
					}
				}
				*q++ = 0;
				*q++ = i;
				reg_used = true;
			}
			else {
				free(p);
				return WRONG_CHAR_AFTER_BACKSLASH;
			}
		}

		if (reg_used) {
			if (t = malloc(re_reg.end[0] - re_reg.start[0] + 1)) {
				memcpy(t, b->cur_line_desc->line + re_reg.start[0], re_reg.end[0] - re_reg.start[0]);
				t[re_reg.end[0] - re_reg.start[0]] = 0;
			}
			else {
				free(p);
				return OUT_OF_MEMORY;
			}
		}

		for(int i = re_reg.num_regs; i-- != 0;) {
			re_reg.end[i] -= re_reg.start[0];
			re_reg.start[i] -= re_reg.start[0];
		}

		start_undo_chain(b);

		delete_stream(b, b->cur_line_desc, b->cur_line, b->cur_pos, re_reg.end[0]);

		q = p;
		int64_t pos = 0;

		while(true) {
			if (strlen(q)) {
				insert_stream(b, b->cur_line_desc, b->cur_line, b->cur_pos + pos, q, strlen(q));
				pos += strlen(q);
			}

			q += strlen(q) + 1;

			if (q - p > len) break;

			assert(*q < RE_NREGS);

			if (re_reg.end[*(unsigned char *)q] - re_reg.start[*(unsigned char *)q]) {

				char c = t[re_reg.end[*(unsigned char *)q]];
				t[re_reg.end[*(unsigned char *)q]] = 0;

				insert_stream(b, b->cur_line_desc, b->cur_line, b->cur_pos + pos, t + re_reg.start[*(unsigned char *)q], re_reg.end[*(unsigned char *)q] - re_reg.start[*(unsigned char *)q]);

				t[re_reg.end[*(unsigned char *)q]] = c;

				pos += re_reg.end[*(unsigned char *)q] - re_reg.start[*(unsigned char *)q];
			}

			q++;
		}

		end_undo_chain(b);

		if (! b->opt.search_back) goto_pos(b, b->cur_pos + pos);

		free(t);
		free(p);
	}
	else return OUT_OF_MEMORY;

	last_replace_empty_match = re_reg.start[0] == re_reg.end[0];
	return OK;
}
