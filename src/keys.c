/* Terminfo database scanning and keyboard escape sequence matching functions.

   Copyright (C) 1993-1998 Sebastiano Vigna 
   Copyright (C) 1999-2020 Todd M. Lewis and Sebastiano Vigna

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
#include "support.h"
#include <time.h>
#include <signal.h>
#include <errno.h>
#include <string.h>

/* The keywords used in the configuration files. */

#define KEY_KEYWORD "KEY"
#define SEQ_KEYWORD "SEQ"

/* The name of the key bindings file. */

#define KEY_BINDINGS_NAME ".keys"

/* Maximum number of key definitions from terminfo plus others
   we may get from keys file -- i.e. key_may_set(). */

#define MAX_TERM_KEY 512

/* Size of the keyboard input buffer. */

#define KBD_BUF_SIZE 512

/* This structure describes a key in the terminfo database. These structures
are ordered with respect to the string field to optimize their scanning. The
order is *inverted* w.r.t. strcmp(). */


typedef struct {
	const char *string;
	int code;
	config_source source;
} term_key;


static term_key key[MAX_TERM_KEY];
static int num_keys;

/* Function to pass to qsort for sorting the key capabilities array. */

static int keycmp(const void *t1, const void *t2) {
	return -strcmp(((term_key *)t1)->string, ((term_key *)t2)->string);
}


/* Search the key capability s in the ordered capability vector; if found at
position pos return -pos-1 (i.e., always a negative number), otherwise return
the correct place for insertion of s */

int binsearch(const char * const s) {
	if (num_keys) {
		int l = 0, m = 0, r = num_keys - 1;

		while(l <= r) {
			m = (l + r) / 2;
			if (is_prefix(s, key[m].string)) return -m - 1;
			if (strcmp(key[m].string, s) > 0) l = m + 1;
			else if (strcmp(key[m].string, s) < 0) r = m - 1;
		}

		return l;
	}
	else return 0;
}

#ifdef NE_TEST
void escstrf(FILE *f, const char *p) {
	fprintf(f, "\"");
	while (*p) {
		if (*p == '"') fprintf(f, "\\\"");
		else if (isprint(*p)) fprintf(f, "%c", *p);
		else fprintf(f, "\\x%02x", *p);
		p++;
	}
	fprintf(f,"\"");
}


void dump_key_config(FILE *f) {
	int keynum;

   config_source prev_source = CONFIG_SOURCE_COUNT;
   
	for (keynum = 0; keynum < NUM_KEYS; keynum++) {
		if (key_binding[keynum] && key_binding[keynum][0]) {
			for (int seqnum = 0; seqnum < num_keys; seqnum++) {
				if (key[seqnum].code == keynum) {
					if (prev_source != key[seqnum].source) {
						fprintf(f, "\n# config source: %s\n", config_source_names[key_binding_source[keynum]]);
						prev_source = key[seqnum].source;
					}
					fprintf(f,"%s%s\t", key[seqnum].source == BUILT_IN ? "# " : "", SEQ_KEYWORD, config_source_names[key[seqnum].source]);
					escstrf(f, key[seqnum].string);
					fprintf(f,"\t%x\n", keynum);
				}
			}
			if (prev_source != key_binding_source[keynum]) {
				fprintf(f, "\n# config source: %s\n", config_source_names[key_binding_source[keynum]]);
				prev_source = key_binding_source[keynum];
			}
			fprintf(f, "%s%s\t%4x\t%s\n", key_binding_source[keynum] == BUILT_IN ? "# " : "", KEY_KEYWORD, keynum, key_binding[keynum] );
		}
	}
}
#endif



#ifdef DEBUGPRINTF
void dump_keys(void) {
	 for (int i = 0; i < num_keys; i++) {
		char *p = key[i].string;
		fprintf(stderr, "%3d: \"", i);
		while (*p) {
			if (isprint(*p)) fprintf(stderr, "%c", *p);
			else fprintf(stderr, "\\x%02x", *p);
			p++;
		}
		fprintf (stderr,"\"\t-> %d  (%s)\n", key[i].code, config_source_names[key[i].source]);
	}
}

#endif


/* Sets the first free position in the key capabilities array to the cap_string
   capability, and increment the first free position counter.
*/

static void key_set(const char * const cap_string, const int code, config_source source) {
	if (!cap_string) return;

	key[num_keys].string = cap_string;
	key[num_keys].code = code;
	key[num_keys].source = source;
	num_keys++;

}



/* key_may_set() maps a key capability string to a key code number.
   It assumes the array is already sorted, and it keeps it that way.
   If the code number is positive and the cap_string is already in
   the key map, no mapping is done. If the code number is negative
   and the cap_string is already in the key vector, the matching code
   is replaced with the positive counterpart of the code passed in.
      This is part of the horrible hack to make cursor and
   function keys work on numerous terminals which have broken terminfo
   and termcap entries, or for weak terminal emulators which happen to
   produce well-known sequences. Returns
	   > 0 on success,
	   ==0 if table is full (or no cap_string supplied)
	   < 0 if string was already defined.   
*/
int key_may_set(const char * const cap_string, int code, config_source source) {
	int pos = 0;

	if (num_keys >= MAX_TERM_KEY - 1) return 0;
	if (!cap_string || (pos = binsearch(cap_string)) < 0) {
		if (code < 0) {
			key[-pos-1].code = -code - 1;
			key[-pos-1].source = source;
		}
		return pos;
	}
   if (code < 0) code = -code - 1;
	memmove(key + pos + 1, key + pos, (num_keys - pos) * sizeof *key);
	key[pos].string = cap_string;
	key[pos].code = code;
	key[pos].source = source;
	num_keys++;
	assert(num_keys < MAX_TERM_KEY);
	return pos+1;
}



/* Here we scan the terminfo database and build a term_key structure for
each key available. num_keys records the number of entries. The array is
sorted in reverse order with respect to string field (this optimizes the
comparisons, assuming that usually almost all control sequences start with a
character smaller than ' ', while the characters typed by the user are
almost always greater than or equal to ' '). */

extern const char meta_prefixed[128][3];

void read_key_capabilities(void) {
	if (!ansi) {
		/* Cursor movement keys */

		key_set(key_up,    NE_KEY_UP, BUILT_IN);
		key_set(key_down,  NE_KEY_DOWN, BUILT_IN);
		key_set(key_left,  NE_KEY_LEFT, BUILT_IN);
		key_set(key_right, NE_KEY_RIGHT, BUILT_IN);
		key_set(key_home,  NE_KEY_HOME, BUILT_IN);
		key_set(key_end,   NE_KEY_END, BUILT_IN);
		key_set(key_npage, NE_KEY_NPAGE, BUILT_IN);
		key_set(key_ppage, NE_KEY_PPAGE, BUILT_IN);
		key_set(key_sf,    NE_KEY_SCROLL_FORWARD, BUILT_IN);
		key_set(key_sr,    NE_KEY_SCROLL_REVERSE, BUILT_IN);


	/* Editing keys */

		key_set(key_eol,   NE_KEY_CLEAR_TO_EOL, BUILT_IN);
		key_set(key_eos,   NE_KEY_CLEAR_TO_EOS, BUILT_IN);
		key_set(key_backspace, NE_KEY_BACKSPACE, BUILT_IN);
		key_set(key_dl,    NE_KEY_DELETE_LINE, BUILT_IN);
		key_set(key_il,    NE_KEY_INSERT_LINE, BUILT_IN);
		key_set(key_dc,    NE_KEY_DELETE_CHAR, BUILT_IN);
		key_set(key_ic,    NE_KEY_INSERT_CHAR, BUILT_IN);
		key_set(key_eic,   NE_KEY_EXIT_INSERT_CHAR, BUILT_IN);
		key_set(key_clear, NE_KEY_CLEAR, BUILT_IN);


		/* Keypad keys */

		key_set(key_a1, NE_KEY_A1, BUILT_IN);
		key_set(key_a3, NE_KEY_A3, BUILT_IN);
		key_set(key_b2, NE_KEY_B2, BUILT_IN);
		key_set(key_c1, NE_KEY_C1, BUILT_IN);
		key_set(key_c3, NE_KEY_C3, BUILT_IN);


		/* Tab keys (never used in the standard configuration) */

		key_set(key_catab, NE_KEY_CLEAR_ALL_TABS, BUILT_IN);
		key_set(key_ctab,  NE_KEY_CLEAR_TAB, BUILT_IN);
		key_set(key_stab,  NE_KEY_SET_TAB, BUILT_IN);


		/* Function keys */

		key_set(key_f0,  NE_KEY_F(0), BUILT_IN);
		key_set(key_f1,  NE_KEY_F(1), BUILT_IN);
		key_set(key_f2,  NE_KEY_F(2), BUILT_IN);
		key_set(key_f3,  NE_KEY_F(3), BUILT_IN);
		key_set(key_f4,  NE_KEY_F(4), BUILT_IN);
		key_set(key_f5,  NE_KEY_F(5), BUILT_IN);
		key_set(key_f6,  NE_KEY_F(6), BUILT_IN);
		key_set(key_f7,  NE_KEY_F(7), BUILT_IN);
		key_set(key_f8,  NE_KEY_F(8), BUILT_IN);
		key_set(key_f9,  NE_KEY_F(9), BUILT_IN);
		key_set(key_f10, NE_KEY_F(10), BUILT_IN);
		key_set(key_f11, NE_KEY_F(11), BUILT_IN);
		key_set(key_f12, NE_KEY_F(12), BUILT_IN);
		key_set(key_f13, NE_KEY_F(13), BUILT_IN);
		key_set(key_f14, NE_KEY_F(14), BUILT_IN);
		key_set(key_f15, NE_KEY_F(15), BUILT_IN);
		key_set(key_f16, NE_KEY_F(16), BUILT_IN);
		key_set(key_f17, NE_KEY_F(17), BUILT_IN);
		key_set(key_f18, NE_KEY_F(18), BUILT_IN);
		key_set(key_f19, NE_KEY_F(19), BUILT_IN);
		key_set(key_f20, NE_KEY_F(20), BUILT_IN);
		key_set(key_f21, NE_KEY_F(21), BUILT_IN);
		key_set(key_f22, NE_KEY_F(22), BUILT_IN);
		key_set(key_f23, NE_KEY_F(23), BUILT_IN);
		key_set(key_f24, NE_KEY_F(24), BUILT_IN);
		key_set(key_f25, NE_KEY_F(25), BUILT_IN);
		key_set(key_f26, NE_KEY_F(26), BUILT_IN);
		key_set(key_f27, NE_KEY_F(27), BUILT_IN);
		key_set(key_f28, NE_KEY_F(28), BUILT_IN);
		key_set(key_f29, NE_KEY_F(29), BUILT_IN);
		key_set(key_f30, NE_KEY_F(30), BUILT_IN);
		key_set(key_f31, NE_KEY_F(31), BUILT_IN);
		key_set(key_f32, NE_KEY_F(32), BUILT_IN);
		key_set(key_f33, NE_KEY_F(33), BUILT_IN);
		key_set(key_f34, NE_KEY_F(34), BUILT_IN);
		key_set(key_f35, NE_KEY_F(35), BUILT_IN);
		key_set(key_f36, NE_KEY_F(36), BUILT_IN);
		key_set(key_f37, NE_KEY_F(37), BUILT_IN);
		key_set(key_f38, NE_KEY_F(38), BUILT_IN);
		key_set(key_f39, NE_KEY_F(39), BUILT_IN);
		key_set(key_f40, NE_KEY_F(40), BUILT_IN);
		key_set(key_f41, NE_KEY_F(41), BUILT_IN);
		key_set(key_f42, NE_KEY_F(42), BUILT_IN);
		key_set(key_f43, NE_KEY_F(43), BUILT_IN);
		key_set(key_f44, NE_KEY_F(44), BUILT_IN);
		key_set(key_f45, NE_KEY_F(45), BUILT_IN);
		key_set(key_f46, NE_KEY_F(46), BUILT_IN);
		key_set(key_f47, NE_KEY_F(47), BUILT_IN);
		key_set(key_f48, NE_KEY_F(48), BUILT_IN);
		key_set(key_f49, NE_KEY_F(49), BUILT_IN);
		key_set(key_f50, NE_KEY_F(50), BUILT_IN);
		key_set(key_f51, NE_KEY_F(51), BUILT_IN);
		key_set(key_f52, NE_KEY_F(52), BUILT_IN);
		key_set(key_f53, NE_KEY_F(53), BUILT_IN);
		key_set(key_f54, NE_KEY_F(54), BUILT_IN);
		key_set(key_f55, NE_KEY_F(55), BUILT_IN);
		key_set(key_f56, NE_KEY_F(56), BUILT_IN);
		key_set(key_f57, NE_KEY_F(57), BUILT_IN);
		key_set(key_f58, NE_KEY_F(58), BUILT_IN);
		key_set(key_f59, NE_KEY_F(59), BUILT_IN);
		key_set(key_f60, NE_KEY_F(60), BUILT_IN);
		key_set(key_f61, NE_KEY_F(61), BUILT_IN);
		key_set(key_f62, NE_KEY_F(62), BUILT_IN);
		key_set(key_f63, NE_KEY_F(63), BUILT_IN);
	}

	/* Fake (simulated) command key. */

	key_set("\x1B:", NE_KEY_COMMAND, BUILT_IN);

	assert(num_keys < MAX_TERM_KEY - 1);

	D(fprintf(stderr, "Got %d keys from terminfo\n", num_keys);)

	qsort(key, num_keys, sizeof(term_key), keycmp);

	/* A nice hack for common cursor movements borrowed from pico.

		Unfortunately, quite a few terminfo and termcap entries out there have
		bad values for cursor key capability strings. (The f# values are
		generally in sad shape too, but that's a much larger problem.)  However,
		certain escape sequences are quite common among large sets of terminals,
		and so we define the most common ones here.

		key_may_set() won't assign key cap strings if that sequence is already
		taken, so we shouldn't be doing too much damage if the terminfo or
		termcap happens to be correct.  */

	key_may_set("\x1b[A",	NE_KEY_UP, BUILT_IN);
	key_may_set("\x1b?x",	NE_KEY_UP, BUILT_IN);
	/*	key_may_set("\x1b" "A",	NE_KEY_UP, BUILT_IN);*/
	key_may_set("\x1bOA",	NE_KEY_UP, BUILT_IN);

	key_may_set("\x1b[B",	NE_KEY_DOWN, BUILT_IN);
	key_may_set("\x1b?r",	NE_KEY_DOWN, BUILT_IN);
	/* key_may_set("\x1b" "B",	NE_KEY_DOWN, BUILT_IN);*/
	key_may_set("\x1bOB",	NE_KEY_DOWN, BUILT_IN);

	key_may_set("\x1b[D",	NE_KEY_LEFT, BUILT_IN);
	key_may_set("\x1b?t",	NE_KEY_LEFT, BUILT_IN);
	/*key_may_set("\x1b" "D",	NE_KEY_LEFT, BUILT_IN);*/
	key_may_set("\x1bOD",	NE_KEY_LEFT, BUILT_IN);

	key_may_set("\x1b[C",	NE_KEY_RIGHT, BUILT_IN);
	key_may_set("\x1b?v",	NE_KEY_RIGHT, BUILT_IN);
	/*key_may_set("\x1b" "C",	NE_KEY_RIGHT, BUILT_IN);*/
	key_may_set("\x1bOC",	NE_KEY_RIGHT, BUILT_IN);

	key_may_set("\x1b[1~",  NE_KEY_HOME, BUILT_IN);
	key_may_set("\x1b[4~",  NE_KEY_END, BUILT_IN);
	key_may_set("\x1b[6~",  NE_KEY_NPAGE, BUILT_IN);
	key_may_set("\x1b[5~",  NE_KEY_PPAGE, BUILT_IN);
	key_may_set("\x1b[2~",  NE_KEY_INSERT_CHAR, BUILT_IN);
	key_may_set("\x1b[3~",  NE_KEY_DELETE_CHAR, BUILT_IN);

	key_may_set("\x1b[H",  NE_KEY_HOME, BUILT_IN);
	key_may_set("\x1b[L",  NE_KEY_INSERT_CHAR, BUILT_IN);

	/* gnome-terminal bizarre home/end keys */
	key_may_set("\x1bOH",  NE_KEY_HOME, BUILT_IN);
	key_may_set("\x1bOF",  NE_KEY_END, BUILT_IN);


	/* The fundamental F1 escape key has been stolen by Gnome. We replace it
		with a double escape, if possible. */

	key_may_set("\x1B\x1B", NE_KEY_F(1), BUILT_IN);


	/* More hacking. Function keys are routinely defined wrong on bazillions of
		systems. This sections codes the F1-F10 keys for vt100, xterms and PCs. I
		can't believe vendors can ship such buggy termcap/terminfo entries.  This
		also handles the case of an otherwise limited terminal emulator which
		happens to produce these sequences for function keys.  */

	/* xterm fkeys: kf1=\E[11~ kf2=\E[12~ kf3=\E[13~ kf4=\E[14~  kf5=\E[15~
						 kf6=\E[17~ kf7=\E[18~ kf8=\E[19~ kf9=\E[20~ kf10=\E[21~ kf11=\E[23~ kf12=\E[24~ */

	key_may_set("\x1b[11~",  NE_KEY_F(1), BUILT_IN);
	key_may_set("\x1b[12~",  NE_KEY_F(2), BUILT_IN);
	key_may_set("\x1b[13~",  NE_KEY_F(3), BUILT_IN);
	key_may_set("\x1b[14~",  NE_KEY_F(4), BUILT_IN);
	key_may_set("\x1b[15~",  NE_KEY_F(5), BUILT_IN);
	key_may_set("\x1b[17~",  NE_KEY_F(6), BUILT_IN);
	key_may_set("\x1b[18~",  NE_KEY_F(7), BUILT_IN);
	key_may_set("\x1b[19~",  NE_KEY_F(8), BUILT_IN);
	key_may_set("\x1b[20~",  NE_KEY_F(9), BUILT_IN);
	key_may_set("\x1b[21~", NE_KEY_F(10), BUILT_IN);
	key_may_set("\x1b[23~", NE_KEY_F(11), BUILT_IN);
	key_may_set("\x1b[24~", NE_KEY_F(12), BUILT_IN);

	/* vt100 keys:   k1=\EOP    k2=\EOQ    k3=\EOR    k4=\EOS     k5=\EOt 
                    k6=\EOu    k7=\EOv    k8=\EOl    k9=\EOw    k10=\EOy */

	key_may_set("\x1bOP",  NE_KEY_F(1), BUILT_IN);
	key_may_set("\x1bOQ",  NE_KEY_F(2), BUILT_IN);
	key_may_set("\x1bOR",  NE_KEY_F(3), BUILT_IN);
	key_may_set("\x1bOS",  NE_KEY_F(4), BUILT_IN);
	key_may_set("\x1bOt",  NE_KEY_F(5), BUILT_IN);
	key_may_set("\x1bOu",  NE_KEY_F(6), BUILT_IN);
	key_may_set("\x1bOv",  NE_KEY_F(7), BUILT_IN);
	key_may_set("\x1bOl",  NE_KEY_F(8), BUILT_IN);
	key_may_set("\x1bOw",  NE_KEY_F(9), BUILT_IN);
	key_may_set("\x1bOy", NE_KEY_F(10), BUILT_IN);

	/* pc keys:   k1=\E[[A   k2=\E[[B    k3=\E[[C    k4=\E[[D     k5=\E[[E */

	key_may_set("\x1b[[A",  NE_KEY_F(1), BUILT_IN);
	key_may_set("\x1b[[B",  NE_KEY_F(2), BUILT_IN);
	key_may_set("\x1b[[C",  NE_KEY_F(3), BUILT_IN);
	key_may_set("\x1b[[D",  NE_KEY_F(4), BUILT_IN);
	key_may_set("\x1b[[E",  NE_KEY_F(5), BUILT_IN);

	/* If at this point any sequence of the form ESC+ASCII character is free, we bind
		it to the simulated META key. */

	for(int i = 1; i < 128; i++) key_may_set(meta_prefixed[i], NE_KEY_META(i), BUILT_IN);

#ifdef DEBUGPRINTF
	dump_keys();
#endif
}




/* Sets the escape time, which is an option, but it's global to ne and it's not
   saved in autopreferences files. However, an EscapeTime command can be
   attached manually to any preferences file. */

static int escape_time = 10;

void set_escape_time(const int new_escape_time) {
	escape_time = new_escape_time;
}


/* Sets the current timeout in the termios structure relative to stdin. If the
   timeout value (in tenth of a second) is positive, VMIN is set to 0,
   otherwise to 1. */

static void set_termios_timeout(const int timeout) {
	struct termios termios;

	tcgetattr(0, &termios);

	termios.c_cc[VTIME] = timeout;
	termios.c_cc[VMIN] = timeout ? 0 : 1;

	tcsetattr(0, TCSANOW, &termios);
}


/* Reads in characters, and tries to match them with the sequences
   corresponding to special keys. Returns a positive number, denoting
   a character (possibly INVALID_CHAR), or a negative number denoting a key
   code (if x is the key code, -x-1 will be returned).

   This function tries to be highly optimized and efficient by employing a
   sorted array of strings for the terminal keys. An index keeps track of the
   key which has a partial match with the current contents of the keyboard
   buffer. As each character is input, a match is tried with the rest of the
   string. If a new character does not match, we can just increment the key
   counter (because the array is sorted). When we get out of the array, we give
   back the first char in the keyboard buffer (the next call will retry a match
   on the following chars). */


int get_key_code(void) {
	static int cur_len = 0;
	static char kbd_buffer[KBD_BUF_SIZE];

	int c, e, last_match = 0, cur_key = 0;
	bool partial_match = false, partial_is_utf8 = false;

	while(true) {

		if (cur_len) {

			/* Something is already in the buffer. last_match is the position
			we have to check. */

			while(last_match < cur_len) {
				if (last_match == 0 && io_utf8 && (unsigned char)kbd_buffer[0] >= 0x80) {
					partial_is_utf8 = true;
					last_match++;
				}
				else if (partial_is_utf8) { 	/* Our partial match is an UTF-8 sequence. */
					if ((kbd_buffer[last_match] & 0xC0) == 0x80) {
						if (utf8len(kbd_buffer[0]) == ++last_match) {
							c = utf8char(kbd_buffer);
							if (cur_len -= last_match) memmove(kbd_buffer, kbd_buffer + last_match, cur_len);
							return c == -1 ? INVALID_CHAR : c;
						}
					}
					else {
						/* A UTF-8 error. We discard the first character and try again. */
						if (--cur_len) memmove(kbd_buffer, kbd_buffer + 1, cur_len);
						partial_is_utf8 = false;
						last_match = 0;
					}
				}
				else {
					/* First easy case. We felt off the array. We return the first character
						in the buffer and restart the match. */

					if (!key[cur_key].string) {
						c = kbd_buffer[0];
						if (--cur_len) memmove(kbd_buffer, kbd_buffer + 1, cur_len);
						return (unsigned char)c;
					}

					/* Second case. We have a partial match on the first last_match
						characters. If another character matches, either the string is terminated,
						and we return the key code, or we increment the match count. */


					else if (key[cur_key].string[last_match] == kbd_buffer[last_match]) {
						if (key[cur_key].string[last_match + 1] == 0) {
							if (cur_len -= last_match + 1) memmove(kbd_buffer, kbd_buffer + last_match + 1, cur_len);

							assert(key[cur_key].code < NUM_KEYS);

							return -key[cur_key].code - 1;
						}
						else last_match++;
					}

					/* The tricky part. If there is a failed match, the order guarantees that
						no match if possible if the code of the keyboard char is greater than the code of
						the capability char. Otherwise, we check for the first capability starting
						with the current keyboard characters. */

					else {
						if (kbd_buffer[last_match] > key[cur_key].string[last_match]) {
							c = kbd_buffer[0];
							if (--cur_len) memmove(kbd_buffer, kbd_buffer + 1, cur_len);
							return (unsigned char)c;
						}
						else {
							last_match = 0;
							cur_key++;
						}
					}
				}
			}
			/* If we have a partial match, let's look at stdin for escape_time
				tenths of second. If nothing arrives, it is probably time to return
				what we got. Note that this won't work properly if the terminal has
				a key capability which is a prefix of another key capability. */

			partial_match = true;
		}

		fflush(stdout);

		if (partial_match) set_termios_timeout(escape_time);

		errno = 0;
		c = getchar();
		e = errno;

		if (partial_match) set_termios_timeout(0);

		/* This is necessary to circumvent the slightly different behaviour of getc() in Linux and BSD. */
		clearerr(stdin);

		if (c == EOF && (!partial_match || e) && e != EINTR) kill(getpid(), SIGTERM);

		partial_match = false;

		if (c != EOF) {
			if (cur_len < KBD_BUF_SIZE) kbd_buffer[cur_len++] = c;
		}
		else {
			if (cur_len) {

				/* We ran out of time. If our match was UTF-8, we discard the
					partially received UTF-8 sequence. Otherwise, we return the
					first character of the keyboard buffer.  */

				if (partial_is_utf8) cur_len = last_match = partial_is_utf8 = 0;
				else {
					c = kbd_buffer[0];
					if (--cur_len) memmove(kbd_buffer, kbd_buffer + 1, cur_len);
					return (unsigned char)c;
				}
			}
			else return INVALID_CHAR;
		}
	}
}

static void error_in_key_bindings(const int line, const char * const s) {
	fprintf(stderr, "Error in key bindings file at line %d: %s\n", line, s);
	exit(0);
}

static void get_key_bind(const char * key_bindings_name, char * (exists_prefs_func)(), config_source source) {
	if (!key_bindings_name) key_bindings_name = KEY_BINDINGS_NAME;

	char * const prefs_dir = exists_prefs_func();
	if (prefs_dir) {
		char * const key_bindings = malloc(strlen(prefs_dir) + strlen(key_bindings_name) + 1);
		if (key_bindings) {
			strcat(strcpy(key_bindings, prefs_dir), key_bindings_name);
			char_stream * const cs = load_stream(NULL, key_bindings, false, false);
			if (cs) {
				char * p = cs->stream;
				int line = 1;

				while(p - cs->stream < cs->len) {
					if (*p && !cmdcmp(KEY_KEYWORD, p)) {
						while(*p && !isasciispace(*p)) p++;

						int c;
						if (sscanf(p, "%x %*s", &c) == 1) {
							if (c >= 0 && c < NUM_KEYS) {
								if (c != 27 && c != 13) {
									while(isasciispace(*p)) p++;
									while(*p && !isasciispace(*p)) p++;
									while(isasciispace(*p)) p++;
									if (*p) {
										key_binding[c] = p;
										key_binding_source[c] = source;
									}
									else error_in_key_bindings(line, "no command specified.");
								}
								else error_in_key_bindings(line, "you cannot redefine ESCAPE and RETURN.");
							}
							else error_in_key_bindings(line, "key code out of range.");
						}
						else error_in_key_bindings(line, "can't read key code.");
					}
					else if (*p && !cmdcmp(SEQ_KEYWORD, p)) {
						char *buf;
						while(*p && !isasciispace(*p)) p++;	 /* skip past SEQ */
						while(isasciispace(*p)) p++;			  /* skip to quoted sequence, like  "\x1b[A" */
						buf = p;	/* Risky: we're replacing the double-quoted string with its parsed equivalent in situ. */
						if (parse_string((unsigned char **)&p, (unsigned char *)buf, strlen(p)) > 0) {  /* parse_string() expects double-quoted string. */
							while(*p && isasciispace(*p)) p++;  /* skip to key code */
							int c;
							if (*p && sscanf(p, "%x %*s", &c) == 1) {	 /* convert key code */
								if (c >= 0 && c < NUM_KEYS) {
									if (c != 27 && c != 13) {
										if (key_may_set(buf, -c - 1, source) == 0)
											error_in_key_bindings(line, "sequence table full." );
									}
									else error_in_key_bindings(line, "you cannot redefine ESCAPE and RETURN.");
								}
								else error_in_key_bindings(line, "key code out of range.");
							}
							else error_in_key_bindings(line, "can't read key code.");
						}
						else error_in_key_bindings(line, "can't read double quoted character sequence.");
					}
					line++;
					p += strlen(p) + 1;
				}
			}

			free(key_bindings);
		}
	}
}

char *cur_dir(void) {
	static char *cur_dir = "./";
	return cur_dir;
}

/* Key bindings override easily, so pull in any global bindings
   first, then override with the users bindings. */
void get_key_bindings(const char * key_bindings_name) {
	for (int i = 0; i < NUM_KEYS; i++) key_binding_source[i] = BUILT_IN;
	get_key_bind(key_bindings_name, exists_gprefs_dir, GLOBAL_PREFS);
	get_key_bind(key_bindings_name, exists_prefs_dir, USER_PREFS);
	get_key_bind(key_bindings_name, cur_dir, CUR_DIR);
}


