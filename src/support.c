/* Miscellaneous support functions.

   Copyright (C) 1993-1998 Sebastiano Vigna 
   Copyright (C) 1999-2016 Todd M. Lewis and Sebastiano Vigna

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
#include "cm.h"
#include <signal.h>
#include <pwd.h>

/* Some systems do not define _POSIX_VDISABLE. We try to establish a reasonable value. */

#ifndef _POSIX_VDISABLE
#define _POSIX_VDISABLE 0
#endif

/* Returns a pointer to the global ne directory if the environment variable
   NE_GLOBAL_DIR is set.  If it isn't set, then GLOBALDIR is returned. */

const char *get_global_dir(void) {
	char *ne_global_dir;
	ne_global_dir = getenv("NE_GLOBAL_DIR");
	if (!ne_global_dir) ne_global_dir = GLOBALDIR;
	return tilde_expand(ne_global_dir);
}



/* Some UNIXes allow "getcwd(NULL, size)" and will allocate the buffer for
   you when your first parm is NULL.  This is not really legal, so we've put a
   front end onto getcwd() called ne_getcwd() that allocates the buffer for you
   first. */

char *ne_getcwd(const int bufsize) {
	char *result = malloc(bufsize);
	if (result) result = getcwd(result, bufsize);
	return result;
}


/* is_migrated() tells whether the specified file is currently migrated.  A
   migrated file is one which is not actually on-line but is out on tape or
   other media. In general we don't want to try to load a migrated file in an
   interactive program because the delay (waiting for a tape mount, etc.)  is
   most annoying and frustrating.  On systems which don't support hierarchical
   storage, non-zero length files always take up at least one disk block, so
   this should never be an issue for them. */

#if defined _CONVEX_SOURCE
/* The Convex system uses CDVM, and can call cvxstat to see if a file is
   migrated. */
#include <sys/dmon.h>
bool is_migrated(const char * const name) {
	struct cvxstat st;
	if (cvxstat(name, &st, sizeof(struct cvxstat)) == 0)
		if (st.st_dmonflags & IMIGRATED) return true;

	return false;
}
#elif defined ZERO_STAT_MIG_TEST
/* Some systems which support hierarchical storage will report a non-zero file
   size but zero blocks used.  (Since the file is on tape rather than disc, it's
   using no disc blocks.)  If this describes the behaviour of your system,
   define ZERO_STAT_MIG_TEST when building ne. */
bool is_migrated(const char * const name) {
	struct stat statbuf;

	if ((stat(tilde_expand(name), &statbuf) == 0) &&
		  (statbuf.st_size > 0) &&
		  (statbuf.st_blocks == 0))
		return true;
	else return false;
  }
#else

/* Most systems have no hierarchical storage facility and need never concern
   themselves with this problem. For these systems, is_migrated() will always be
   false. */
bool is_migrated(const char * const name) {
	return false;
}
#endif  


bool is_directory(const char * const name) {
	struct stat statbuf;

	return stat(tilde_expand(name), &statbuf) == 0 && S_ISDIR(statbuf.st_mode);
}

/* Returns the mtime of the named file, or 0 on error.
   If a file's mtime actually is 0, then this system probably doesn't keep
   mtimes, so we can't distinguish that case,
   but we probably don't care either, as that's also our sentinel value
   for buffers which have not been saved. */

unsigned long file_mod_time(const char *filename) {
	static struct stat statbuf;
	if ( stat(filename, &statbuf) ) return false;
	return statbuf.st_mtime;
}

/* Reads data from a file descriptors much as read() does, but never reads
   more than 1GiB in a single read(), ignores interruptions (EINTR) and
   tries again in case of EAGAIN errors. */

int64_t read_safely(const int fh, void * const buf, const int64_t len) {
	for(int64_t done = 0; done < len; ) {
		/* We read one GiB at a time, lest some OS complains. */
		const int to_do = min(len - done, 1 << 30);
		const int64_t t = read(fh, (char *)buf + done, to_do);
		if (t < 0) {
			if (errno == EINTR) continue;
			if (errno == EAGAIN) { /* We try again, but wait for a second. */
				sleep(1);
				continue;
			}
			return t;
		}
		if (t == 0) return done;
		done += t;
	}
	
	return len;
}

/* Check a named file's mtime relative to a buffer's stored mtime.
   Note that stat errors are treated like 0 mtime, which also is the value
   for new buffers. Return values:
     true:  if file's non-zero modification time differs from the buffer's mtime,
     false: everything else, including if the files mtime couldn't be checked
            (for example: possibly no file or couldn't stat).
   Uses filename from the buffer unless passed a name. */

bool buffer_file_modified(const buffer *b, const char *name) {

	assert_buffer(b);

#ifdef NE_TEST
	return false; /* During tests, assume all buffers' files are safe to replace. */
#endif

	if (name == NULL) name = b->filename;

	if (!name) return false;

	name = tilde_expand(name);

	unsigned long fmtime = file_mod_time(name);
	if (fmtime && fmtime != b->mtime) return true;
	return false;
}


/* Returns a pointer to a tilde-expanded version of the string pointed to by
   filename. The string should not be free()ed, since it is tracked
   locally. Note that this function can return the same pointer which is
   passed, in case no tilde expansion has to be performed. */

const char *tilde_expand(const char * filename) {

	static char *expanded_filename;

	if (!filename) return NULL;

	if (filename[0] != '~') return filename;

	char *home_dir;

	if (filename[1] == '/') {
		home_dir = getenv("HOME");
		if (!home_dir) return filename;
		filename++;
	}
	else {
		const char *s;
		char *t;

		s = filename + 1;

		while(*s && *s != '/') s++;

		struct passwd *passwd = NULL;

		if (t = malloc(s - filename)) {

			memcpy(t, filename + 1, s - filename - 1);
			t[s - filename - 1] = 0;

			passwd = getpwnam(t);

			free(t);
		}

		if (!passwd) return filename;

		filename = s;
		home_dir = passwd->pw_dir;
	}

	char * const p = realloc(expanded_filename, strlen(filename) + strlen(home_dir) + 1);
	if (p) {
		strcat(strcpy(expanded_filename = p, home_dir), filename);
		return expanded_filename;
	}

	return filename;
}



/* Given a pathname, returns a pointer to the real file name (i.e., the pointer
   points inside the string passed). */

const char *file_part(const char * const pathname) {
	if (!pathname) return NULL;
	const char * p = pathname + strlen(pathname);
	while(p > pathname && *(p - 1) != '/') p--;
	return p;
}


/* Duplicates a string. */

char *str_dup(const char * const s) {
	if (!s) return NULL;

	const int64_t len = strlen(s);
	char * const dup = malloc(len + 1);
	memcpy(dup, s, len + 1);
	return dup;
}

/* Tries to compute the length as a string of the given pointer,
   but stops after n characters (returning n). */

int64_t strnlen_ne(const char *s, int64_t n) {
	const char * const p = s;
	while(n-- != 0) if (!*(s++)) return s - p - 1;
	return s - p;
}

/* Compares strings for equality, but accepts NULLs. */

bool same_str(const char *p, const char *q) {
	if (p == q) return true;
	if (p == NULL || q == NULL) return false;
	return strcmp(p, q) == 0;
}

/* Computes the length of the maximal common prefix of s and t. */

int max_prefix(const char * const s, const char * const t) {
	int i;
	for(i = 0; s[i] && t[i] && s[i] == t[i]; i++);
	return i;
}


/* Returns true if the first string is a prefix of the second one. */

bool is_prefix(const char * const p, const char * const s) {
	int i;
	for(i = 0; p[i] && s[i] && p[i] == s[i]; i++);
	return !p[i];
}


/* The following *cmpp() functions are suitable for use with qsort().
   They front comparison functions with "normal" (i.e., like strcmp())
   calling conventions. */

/* A string pointer comparison function for qsort(). */

int strcmpp(const void *a, const void *b) {
	return strcmp(*(const char **)a, *(const char **)b);
}

/* Another comparison for qsort, this one does dictionary order. */

int strdictcmpp(const void *a, const void *b)	{
	return strdictcmp(*(const char **)a, *(const char **)b);
}

int strdictcmp(const char *a, const char *b)	{
	const int ci = strcasecmp(a, b);
	if (ci) return ci;
	return strcmp(a, b);
}

/* A filename comparison function for qsort(). It makes "../" the first string, "./" the second string
   and then orders lexicographically. */

int filenamecmpp(const void *a, const void *b) {
	return filenamecmp(*(const char **)a, *(const char **)b);
}

int filenamecmp(const char * s, const char * t) {
	if (strcmp(s, "../")==0) return strcmp(t, "../") == 0 ? 0 : -1;
	if (strcmp(s, "..")==0)  return strcmp(t, "..")  == 0 ? 0 : -1;
	if (strcmp(t, "../")==0) return 1;
	if (strcmp(t, "..")==0) return 1;
	if (strcmp(s, "./")==0) return strcmp(t, "./") == 0 ? 0 : -1;
	if (strcmp(s, ".")==0) return strcmp(t, ".") == 0 ? 0 : -1;
	if (strcmp(t, "./")==0) return 1;
	if (strcmp(t, ".")==0) return 1;
	return strdictcmp(s, t);
}



/* Sets the "interactive I/O mode" of the terminal. It suitably sets the mode
   bits of the termios structure, and then transmits various capability strings
   by calling set_terminal_modes(). This function assumes that the terminfo
   database has been properly initialized. The old_termios structure records
   the original state of the terminal interface. */

static struct termios termios, old_termios;

void set_interactive_mode(void) {
	tcgetattr(0, &termios);

	old_termios = termios;

	termios.c_iflag &= ~(IXON | IXOFF | ICRNL | INLCR | ISTRIP);
	termios.c_iflag |= IGNBRK;

	termios.c_oflag &= ~OPOST;

	termios.c_lflag &= ~(ISIG | ICANON | ECHO | ECHONL | IEXTEN);

	/* Cygwin's signal must be disabled, or CTRL-C won't work. There is no way
		to really change the sequences associated to signals. */

#ifndef __CYGWIN__
	termios.c_lflag |= ISIG;
#endif

	termios.c_cflag &= ~(CSIZE | PARENB);
	termios.c_cflag |= CS8;

	termios.c_cc[VTIME] = 0;
	termios.c_cc[VMIN] = 1;

	/* Now we keep the kernel from intercepting any keyboard input in order
		to turn it into a signal. Note that some signals, such as dsusp on BSD,
		are not trackable here. They have to be disabled through suitable
		commands (for instance, `stty dsusp ^-'). */

	termios.c_cc[VSUSP] = _POSIX_VDISABLE;
	termios.c_cc[VQUIT] = _POSIX_VDISABLE;
	termios.c_cc[VKILL] = _POSIX_VDISABLE;

	/* Control-\ is the stop control sequence for ne. */

	termios.c_cc[VINTR] = '\\'-'@';

	tcsetattr(0, TCSADRAIN, &termios);

	/* SIGINT is used for the interrupt character. */

	signal(SIGINT, set_stop);

	/* We do not want to be stopped if we did not generate the signal */

	signal(SIGTSTP, SIG_IGN);

#ifdef SIGWINCH
	siginterrupt(SIGWINCH, 1);
	signal(SIGWINCH, handle_winch);
#endif
	/* This ensures that a physical read will be performed at each getchar(). */

	setbuf(stdin, NULL);

	/* We enable the keypad, cursor addressing, etc. */

	set_terminal_modes();

}



/* Undoes the work of the previous function, in reverse order. It assumes the
   old_termios has been filled with the old termios structure. */

void unset_interactive_mode(void) {

	/* We move the cursor on the last line, clear it, and output a CR, so that
		the kernel can track the cursor position. Note that clear_to_eol() can
		move the cursor. */

	losecursor();
	move_cursor(ne_lines - 1, 0);
	clear_to_eol();
	move_cursor(ne_lines - 1, 0);

	/* Now we disable the keypad, cursor addressing, etc. fflush() guarantees
		that tcsetattr() won't clip part of the capability strings output by
		reset_terminal_modes(). */

	reset_terminal_modes();
	putchar('\r');
	fflush(stdout);

	/* Now we restore all the flags in the termios structure to the state they
		were before us. */

	tcsetattr(0, TCSADRAIN, &old_termios);

#ifdef SIGWINCH
	signal(SIGWINCH, SIG_IGN);
#endif
	signal(SIGINT, SIG_DFL);
	signal(SIGTSTP, SIG_DFL);
}



/* Computes the TAB-expanded width of a line descriptor up to a certain
   position. The position can be greater than the line length, the usual
   convention of infinite expansion via spaces being in place. */

int64_t calc_width(const line_desc * const ld, const int64_t n, const int tab_size, const encoding_type encoding) {

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

int64_t calc_width_hint(const line_desc * const ld, const int64_t n, const int tab_size, const encoding_type encoding, const int64_t cur_pos, const int64_t cur_width) {
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

/* Computes character length of a line descriptor. */

int64_t calc_char_len(const line_desc * const ld, const encoding_type encoding) {
	int64_t len = 0;
	for(int64_t pos = 0; pos < ld->line_len; pos = next_pos(ld->line, pos, encoding), len++);
	return len;
}


/* Given a column, the index of the character "containing" that position is
   given, that is, calc_width(index) > n, and index is minimum with this
   property. If the width of the line is smaller than the given column, the
   line length is returned. */

int64_t calc_pos(const line_desc * const ld, const int64_t col, const int tab_size, const encoding_type encoding) {
	int c_width;
	int64_t pos = 0;
	for(int64_t width = 0; pos < ld->line_len && width + (c_width = get_char_width(&ld->line[pos], encoding)) <= col; pos = next_pos(ld->line, pos, encoding)) {
		if (ld->line[pos] != '\t') width += c_width;
		else width += tab_size - width % tab_size;
	}

	return pos;
}

/* Returns true if the specified character is invariant on the left edge of re-wrapped paragraphs */
/* This will require a re-think for leading non-space invariants. For now, always assume false. */
bool isparaspot(const int c) {
	/* 
	char *spots = "%*#/>\t ";
	char *p = spots;
	while (*p) {
		if (*p++ == c) return true;
	} */
	return false;
}

/* Returns true if the specified character is an US-ASCII whitespace character. */

bool isasciispace(const int c) {
	return c < 0x80 && isspace(c);
}

/* Returns true if the specified character is an US-ASCII alphabetic character. */

bool isasciialpha(const int c) {
	return c < 0x80 && isalpha(c);
}

/* Returns true if the specified block of text is US-ASCII. */

bool is_ascii(const char * const ss, int len) {
	const unsigned char * const s = (const unsigned char *)ss;
	while(len-- != 0) if (s[len] >= 0x80) return false;
	return true;
}


/* Returns toupper() of the given character, if it is US-ASCII, the character itself, otherwise. */

int asciitoupper(const int c) {
	return c < 0x80 ? toupper(c) : c;
}

/* Returns tolower() of the given character, if it is US-ASCII, the character itself, otherwise. */

int asciitolower(const int c) {
	return c < 0x80 ? tolower(c) : c;
}

/* Detects (heuristically) the encoding of a piece of text. */

encoding_type detect_encoding(const char *ss, const int64_t len) {
	if (len == 0) return ENC_ASCII;

	const unsigned char *s = (const unsigned char *)ss;
	bool is_ascii = true;
	const unsigned char * const t = s + len;

	do {
		if (*s >= 0x80) { /* On US-ASCII text, we never enter here. */
			is_ascii = false; /* Once we get here, we are either 8-bit or UTF-8. */
			int l = utf8len(*s);
			if (l == -1) return ENC_8_BIT;
			else if (l > 1) {
				if (s + l > t) return ENC_8_BIT;
				else {
					/* We check for redundant representations. */
					if (l == 2) {
						if (!(*s & 0x1E)) return ENC_8_BIT;
					}
					else if (!(*s & (1 << 7 - l) - 1) && !(*(s + 1) & ((1 << l - 2) - 1) << 8 - l)) return ENC_8_BIT;
					while(--l != 0) if ((*(++s) & 0xC0) != 0x80) return ENC_8_BIT;
				}
			}
		}
	} while(++s < t);

	return is_ascii ? ENC_ASCII : ENC_UTF8;
}

/* Returns the position of the character after the one pointed by pos in s. If
   s is NULL, just returns pos + 1. If encoding is UTF8 it uses utf8len() to
   move forward. */

int64_t next_pos(const char * const ss, const int64_t pos, const encoding_type encoding) {
	const unsigned char *s = (const unsigned char *)ss;
	assert(encoding != ENC_UTF8 || s == NULL || utf8len(s[pos]) > 0);
	if (s == NULL) return pos + 1;
	if (encoding == ENC_UTF8) return pos + utf8len(s[pos]);
	else return pos + 1;
}

/* Returns the position of the character before the one pointed by pos in s.
   If s is NULL, just returns pos + 1. If encoding is UTF-8 moves back until
   the upper two bits are 10. If pos is 0, this function returns -1. */

int64_t prev_pos(const char * const s, int64_t pos, const encoding_type encoding) {
	assert(pos >= 0);
	if (pos == 0) return -1;
	if (s == NULL) return pos - 1;
	if (encoding == ENC_UTF8) {
		while((s[--pos] & 0xC0) == 0x80 && pos > 0);
		return pos;
	}
	else return pos - 1;
}


/* Returns the ISO 10646 character represented by the sequence of bytes
   starting at s, using the provided encoding. */

int get_char(const char * const s, const encoding_type encoding) {
	if (encoding == ENC_UTF8) return utf8char(s);
	else return *(unsigned char *)s;
}

/* Returns the width of the ISO 10646 character represented by the sequence of bytes
   starting at s, using the provided encoding. */

int get_char_width(const char * const s, const encoding_type encoding) {
	assert(s != NULL);
	return encoding == ENC_UTF8 ? output_width(utf8char(s)) : output_width(*(unsigned char *)s);
}

/* Returns the width of the first len characters of s, using the provided
   encoding. If s is NULL, returns len. */

int get_string_width(const char * const s, const int64_t len, const encoding_type encoding) {
	if (s == NULL) return len;
	int64_t width = 0;
	for(int64_t pos = 0; pos < len; pos = next_pos(s, pos, encoding)) width += get_char_width(s + pos, encoding);
	return width;
}

/* Returns whether the given character is a punctuation character. This function is 
   compiled differently depending on whether wide-character function support is inhibited. */

bool ne_ispunct(const int c, const int encoding) {
#ifdef NOWCHAR
	return encoding != ENC_UTF8 ? ispunct(c) : c < 0x80 ? ispunct(c) : false;
#else
	return encoding != ENC_UTF8 ? ispunct(c) : iswpunct(c);
#endif
}

/* Returns whether the given character is whitespace. This function is 
   compiled differently depending on whether wide-character function support is inhibited. */

bool ne_isspace(const int c, const int encoding) {
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

bool ne_isword(const int c, const int encoding) {
	return c == '_' || !(ne_isspace(c, encoding) || ne_ispunct(c, encoding));
}

/* Returns a copy of the ne_isword() string to the left of the cursor, or an empty
   string if none is found. Consumer is responsible for seeing that the string is freed.
   *prefix_pos is the offset from the beginning of the current line where the prefix starts. */

int context_prefix(const buffer *b, char **p, int64_t *prefix_pos) {

	*prefix_pos = b->cur_pos;
	if (*prefix_pos && *prefix_pos <= b->cur_line_desc->line_len) {
		*prefix_pos = prev_pos(b->cur_line_desc->line, *prefix_pos, b->encoding);
		while (*prefix_pos && ne_isword(get_char(&b->cur_line_desc->line[*prefix_pos], b->encoding), b->encoding))
			*prefix_pos = prev_pos(b->cur_line_desc->line, *prefix_pos, b->encoding);
		if (! ne_isword(get_char(&b->cur_line_desc->line[*prefix_pos], b->encoding), b->encoding))
			*prefix_pos = next_pos(b->cur_line_desc->line, *prefix_pos, b->encoding);
		*p = malloc(b->cur_pos + 1 - *prefix_pos);
		if (!*p) return OUT_OF_MEMORY;
		strncpy(*p, &b->cur_line_desc->line[*prefix_pos], b->cur_pos - *prefix_pos);
	} 
	else *p = malloc(1); /* no prefix left of the cursor; we'll give an empty one. */

	(*p)[b->cur_pos - *prefix_pos] = 0;
	return OK;
}

/* Given a buffer, return a string like
   "1,3-5,7,9" indicating which bookmarks are set. */
const char *cur_bookmarks_string(const buffer *b) {
	int bits = b->bookmark_mask;
	static char str[16];
	char *s = str;
	int i;
  
	memset(str,0,16);
	for (i=0, bits &= 0x03ff; i<10 && bits; i++, bits >>= 1) {
		if ( bits & 1 ) {
			*(s++) = '0' + i;
			if ( (bits & 0x07 ) == 0x07 ) *(s++) = '-';
			else *(s++) = ',';
			while ( (bits & 0x07) == 0x07 ) {
				bits >>= 1;
				i++;
			}
		}
	}
	if (s > str) *(--s) = '\0';
	return str;
}
