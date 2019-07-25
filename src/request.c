/* Requester handling.

   Copyright (C) 1993-1998 Sebastiano Vigna
   Copyright (C) 1999-2018 Todd M. Lewis and Sebastiano Vigna

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
#include "termchar.h"
#include <dirent.h>


/* request_strings() prompts the user to choose one between several (cur_entries) strings,
   contained in the entries array. The maximum string width is given as
   max_name_len. The strings are displayed as an array. More than one page will
   be available if there are many strings. If string n was selected with
   RETURN, n is returned; if string n was selected with TAB, -n-2 is returned.
   On escaping, ERROR is returned.

   We rely on a series of auxiliary functions and a few static variables. */

static req_list rl, *rl0; /* working and original * req_list */

static int x, y, page, max_names_per_line, max_names_per_col, names_per_page, fuzz_len;
static bool prune;

/* ne traditionally has displayed request entries by row, i.e.
      a   b   c
      d   e   f
      g   h
   rather than by column, i.e.
      a   d   g
      b   e   h
      c   f
   which, while easier to program, is somewhat harder to read.
   The request code was also full of tricky expressions like those in
   the macros below.

   This version attempts to switch to a by-row request display, and also
   to consolidate as many of the tricky expressions into one small set of
   macros.

   If request_order is true you should get by-column request list. Otherwise it will
   exibit the by-row behavior of the older versions of ne. */

#define BR_NAMES_PER_LINE(p) max_names_per_line
#define BR_NAMES_PER_COL(p) max_names_per_col
#define BR_PXY2N(p,x,y)  (((y) + (p) * max_names_per_col) * max_names_per_line + (x))
#define BR_N2P(n)        ((n) / names_per_page)
#define BR_N2X(n)        (((n) % names_per_page) % max_names_per_line)
#define BR_N2Y(n)        (((n) % names_per_page) / max_names_per_line)
#define BR_DX(p)         1
#define BR_DY            max_names_per_line

/*
    This Perl snippet is useful for tweaking the NAMES_PER_LINE and NAMES_PER_COL macros. The point of
    the complexity on the last page is to use a rectangle in the upper-left part of the window that's
    roughly proportional to the window itself. (Prior pages use the entire window of course.)
    Calculating $x first gives slight priority to taller columns rather than wider lines.

    Translating the Perl "$x =" and "$y =" to the C macros NAMES_PER_LINE and NAMES_PER_COL,
    respectively, is a matter of substituting the following:
      $N  ($M - $n)
      $n  (rl.cur_entries % names_per_page)    The number of actual entries on the last page.
      $X  max_names_per_line
      $Y  max_names_per_col
      $M  names_per_page
      $x  NAMES_PER_LINE(p)

#!/usr/bin/perl -w
use strict;
my ($X,$Y,$M,$N,$x,$y,$n);
use integer; # use integer math, like C macros do
($X,$Y) = (5,9);  # change to test different column/row configurations.
$M = $X * $Y;
for $n ( 1 .. $M ) {
    $N = $M - $n;
    $x = $X - ($X*($N-1)*($N-1)-$M)/($M*$M);
    $y = ($n+$x-1) / $x;
    print  "  n (rows,cols) (capacity => n)\n" if $n == 1;
    printf "%3d: (%2d,%2d)  (%3d >= %3d)? %s\n", $n, $x, $y, $x*$y, $n, ($x*$y >= $n ? "good" : '**BAD**');
}

*/
#define LASTPAGE(p)      ((rl.cur_entries / names_per_page) > p ? 0 : 1 )

#define BC_NAMES_PER_LINE(p) (LASTPAGE(p) ? (max_names_per_line - (max_names_per_line*((names_per_page - (rl.cur_entries % names_per_page))-1)*((names_per_page - (rl.cur_entries % names_per_page))-1)-names_per_page)/(names_per_page*names_per_page)) : max_names_per_line )
#define BC_NAMES_PER_COL(p)  (LASTPAGE(p) ? (((rl.cur_entries % names_per_page)+NAMES_PER_LINE(p)-1) / NAMES_PER_LINE(p)) : max_names_per_col)
#define BC_PXY2N(p,x,y)   ((p) * names_per_page + (x) * NAMES_PER_COL(p) + (y))
#define BC_N2P(n)         ((n) / names_per_page)
#define BC_N2X(n)        (((n) % names_per_page) / NAMES_PER_COL(N2P(n)))
#define BC_N2Y(n)        (((n) % names_per_page) % NAMES_PER_COL(N2P(n)))
#define BC_DX(p)         NAMES_PER_COL(p)
#define BC_DY            1


#define NAMES_PER_LINE(p) (req_order ? BC_NAMES_PER_LINE(p) : BR_NAMES_PER_LINE(p))
#define NAMES_PER_COL(p)  (req_order ? BC_NAMES_PER_COL(p)  : BR_NAMES_PER_COL(p) )
#define PXY2N(p,x,y)      (req_order ? BC_PXY2N(p,x,y)      : BR_PXY2N(p,x,y)     )
#define N2P(n)            (req_order ? BC_N2P(n)            : BR_N2P(n)           )
#define N2X(n)            (req_order ? BC_N2X(n)            : BR_N2X(n)           )
#define N2Y(n)            (req_order ? BC_N2Y(n)            : BR_N2Y(n)           )
#define DX(p)             (req_order ? BC_DX(p)             : BR_DX(p)            )
#define DY                (req_order ? BC_DY                : BR_DY               )

int common_prefix_len(req_list *rlp) {
	char * const p0 = rlp->entries[0];
	int len = strlen(p0);
	for (int i = 0; len && i < rlp->cur_entries; i++) {
		char * const p1 = rlp->entries[i];
		for ( ; len && strncasecmp(p0, p1, len); len--)
			;
	}
	return len;
}

/* This is the printing function used by the requester. It prints the
strings from the entries array existing in a certain page (a page contains
(lines-1)*max_names_per_line items) with rl.max_entry_len maximum width. */

static void print_strings() {
	const int dx = rl.max_entry_len + 1 + (rl.suffix ? 1 : 0);

	set_attr(0);
	for(int row = 0; row < max_names_per_col; row++) {
		move_cursor(row, 0);
		clear_to_eol();
		if (row < NAMES_PER_COL(page)) {
			for(int col = 0; col < NAMES_PER_LINE(page); col++) {
				if (PXY2N(page, col, row) < rl.cur_entries) {
					move_cursor(row, col * dx);
					const char * const p = rl.entries[PXY2N(page, col, row)];
					if (rl.suffix) set_attr(p[strlen(p) - 1] == rl.suffix ? BOLD : 0);
					output_string(p, io_utf8);
				}
			}
		}
	}
}

/* Reset x, y, and page in terms of n.
   Return true if we called print_strings(), false otherwise. */
static bool normalize(int n) {
	const int p = page;

	if (n < 0 ) n = 0;
	if (n >= rl.cur_entries ) n = rl.cur_entries - 1;
	x = N2X(n);
	y = N2Y(n);
	page = N2P(n);
	if ( p != page ) {
		print_strings();
		return true;
	}
	return false;
}

static void request_move_to_sol(void) {
	x = 0;
}


static void request_move_to_eol(void) {
	while (x < NAMES_PER_LINE(page) - 1 && PXY2N(page, x+1, y) < rl.cur_entries) {
		x++;
	}
}

static void request_move_to_sof(void) {
	normalize(0);
}

static void request_move_to_eof() {
	normalize(rl.cur_entries - 1);
}

static void request_toggle_seof(void) {
	if (x + y+page == 0) request_move_to_eof();
	else request_move_to_sof();
}

static void request_prev_page(void) {
	if (page == 0 ) normalize(PXY2N(page, 0, 0));
	else normalize(PXY2N(page-1, x, y));
}


static void request_next_page(void) {
	normalize(PXY2N(page+1, x, y));
}


static void request_move_up(void) {
	normalize(PXY2N(page, x, y) - DY);
}

static void request_move_inc_up(void) {
	if (x == 0) {
		if (y == 0) request_move_to_sof();
		else request_prev_page();
	}
	else request_move_to_sol();
}

static void request_move_down(void) {
	normalize(PXY2N(page, x, y) + DY);
}

void request_move_inc_down(void) {
	if (x == NAMES_PER_LINE(page) - 1) {
		if (y == NAMES_PER_COL(page) - 1) request_move_to_eof();
		else request_next_page();
	}
	else request_move_to_eol();
}


static void request_move_left(void) {
	if ( x == 0 && y + page > 0 ) {
		request_move_up();
		request_move_to_eol();
	}
	else {
		normalize(PXY2N(page, x, y) - DX(page));
	}
}

static void request_move_next(void) {
	normalize(PXY2N(page, x, y)+1);
}

static void request_move_previous(void) {
	normalize(PXY2N(page, x, y)-1);
}

static void request_move_right(void) {
	if (y < NAMES_PER_COL(page) - 1 && PXY2N(page, 0, y+1) < rl.cur_entries &&
	    (x == NAMES_PER_LINE(page) - 1 || PXY2N(page, x+1, y) > rl.cur_entries -1)  ) {
		request_move_to_sol();
		request_move_down();
	}
	else if (y == NAMES_PER_COL(page) - 1 && x == NAMES_PER_LINE(page) - 1 && PXY2N(page+1, 0, 0) < rl.cur_entries) {
		normalize(PXY2N(page+1, 0, 0));
	}
	else if (PXY2N(page, x, y) + DX(page) < rl.cur_entries ) {
		normalize(PXY2N(page, x, y) + DX(page));
	}
}

/* Reorder (i.e. swap) the current entry n with entry n+dir. dir should be
   either 1 or -1. */
static bool request_reorder(const int dir) {
	if (! rl0->allow_reorder || rl.cur_entries < 2) return false;

	const int n0 = PXY2N(page, x, y);
	const int n1 = (n0 + dir + rl.cur_entries ) % rl.cur_entries; /* Allows wrap around. */
	char * const p0 = rl.entries[n0];
	char * const p1 = rl.entries[n1];
	int i0, i1, i;
	for (i=0, i0=-1, i1=-1; i<rl0->cur_entries && (i0<0 || i1<0); i++) {
		if (i0 < 0 && p0 == rl0->entries[i] ) {
			i0 = i;
		}
		if (i1 < 0 && p1 == rl0->entries[i] ) {
			i1 = i;
		}
	}

	i = rl0->orig_order[i0];
	rl0->orig_order[i0] = rl0->orig_order[i1];
	rl0->orig_order[i1] = i;

	rl.entries[n0] = p1;
	rl.entries[n1] = p0;

	rl0->entries[i0] = p1;
	rl0->entries[i1] = p0;

	page = -1; /* causes normalize() to call print_strings() */
	normalize(n1);
	return true;
}

/* rebuild rl.entries[] from rl0.entries[].
   If prune is true, include only those entries from rl0 which match the currently
   highlighted rl entry up through fuzz_len characters.
   If prune is false, include all entries from rl0.
   In any case, return the (possibly new) index of the highlighted entry in rl. */

static int reset_rl_entries() {
	const char * const p0 = rl.entries[PXY2N(page, x, y)];
	int i, n;
	for (int j = n = i = 0; j < rl0->cur_entries; j++) {
		char * const p1 = rl0->entries[j];
		if ( ! prune || ! strncasecmp(p0, p1, fuzz_len) ) {
			if (p1 == p0) n = i;
			rl.entries[i++] = p1;
		}
	}
	rl.cur_entries = i;
	return n;
}

/* Count how many entries match our highlighted entry up through len characters. */

static int count_fuzz_matches(const int len) {
	const int n0 = PXY2N(page, x, y);
	const char * const p0 = rl.entries[n0];
	if (len <= 0) return rl.cur_entries;
	if (len > strlen(p0)) return 1;
	int c;
	for (int i = c = 0; i < rl.cur_entries; i++) {
		if ( ! strncasecmp(p0, rl.entries[i], len)) c++;
	}
	return c;
}

/* Shift fuzz_len by +1 or -1 until the matching count changes or we run out of string. */

static void shift_fuzz(const int d) {
	const char * const p0 = rl.entries[PXY2N(page, x, y)];
	assert(d==1||d==-1);
	if (d==-1) {
		while(fuzz_len > 0 && count_fuzz_matches(fuzz_len+1) == count_fuzz_matches(fuzz_len)) fuzz_len--;
	} else {
		while (fuzz_len < strlen(p0) && count_fuzz_matches(fuzz_len) == count_fuzz_matches(fuzz_len+1)) fuzz_len++;
	}
}

/* Back up one or more chars from fuzz_len, possibly pulling in
   matching entries from the original req_list *rl0 in such a way as
   to preserve original order. This can behave quite differently
   depending on the value of 'prune'. If true, we disregard the
   current subset of entries and rely only on matching fuzz_len
   characters. If false, we keep the current subset while still
   pulling in matches from rl0.  */

static void fuzz_back() {
	const int orig_entries = rl.cur_entries;
	const int n0 = PXY2N(page, x, y);
	const char * const p0 = rl.entries[PXY2N(page, x, y)];
	int n1 = n0;
	if (fuzz_len == 0) return;
	if (prune) {
		while (rl.cur_entries == orig_entries && fuzz_len > 0) {
			fuzz_len = max(0, fuzz_len-1);
			n1 = reset_rl_entries();
		}
	} else {
		fuzz_len--;

		int shiftsize = rl.alloc_entries - rl.cur_entries;

		if (shiftsize) {
			/* shift our current entries to the end of their buffer */
			memmove(rl.entries + shiftsize, rl.entries, rl.cur_entries * sizeof(char *));

			int rldest, rl0src, rlsrc, n0shifted = n0 + shiftsize;

			for (rldest = rl0src = 0, rlsrc = shiftsize; rl0src < rl0->cur_entries; rl0src++) {

				if (rl0->entries[rl0src] == rl.entries[rlsrc]) {
					/* This was in our old list, so we keep it */
					if (n0shifted == rlsrc) n1 = rldest;  /* it was our marked entry */
					rl.entries[rldest++] = rl.entries[rlsrc++];

				} else if ( ! strncasecmp(p0, rl0->entries[rl0src], fuzz_len) ) {
					/* Wasn't in our old list due to prior purning, but should be */
					rl.entries[rldest++] = rl0->entries[rl0src];
				}
			}
			rl.cur_entries = rldest;
		}
		shift_fuzz(-1);
	}
	page = -1; /* causes normalize() to call print_strings() */
	normalize(n1);
}

/* given a localised_up_case character c, find the next entry that
   matches our current fuzz_len chars plus this new character. The
   behavior is quite different depending on 'prune'. If true, we keep
   only entries that match our current fuzz_len prefix plus this
   additional character while preserving the relative order of
   rl.entries[]. If false, we keep all the current entries, and only
   (possibly) change the selected one. */

static void fuzz_forward(const int c) {
	const int n0 = PXY2N(page, x, y);
	const char * const p0 = rl.entries[n0];

	assert(fuzz_len >= 0);

	if (prune) {
		int i = 0, n1 = 0;
		for (int j = 0; j < rl.cur_entries; j++) {
			char * const p1 = rl.entries[j];
			const int cmp = strncasecmp(p0, p1, fuzz_len);
			if (! cmp && strlen(p1) > fuzz_len && localised_up_case[(unsigned char)p1[fuzz_len]] == c) {
				if (p1 == p0)
					n1 = i;
				rl.entries[i++] = p1;
			}
		}
		if (i) {
			rl.cur_entries = i;
			fuzz_len = common_prefix_len(&rl);
			page = -1; /* causes normalize() to call print_strings() */
			normalize(n1);
		}
	} else {
		/* find the next matching string, possibly wrapping around */
		for (int n=n0, i=rl.cur_entries; i; i--, n=(n+1)%rl.cur_entries) {
			char * const p1 = rl.entries[n];
			const int cmp = strncasecmp(p0, p1, fuzz_len);
			if (!cmp && strlen(p1) > fuzz_len && localised_up_case[(unsigned char)p1[fuzz_len]] == c) {
				fuzz_len++;
				page = -1;
				normalize(n);
				break;
			}
		}
		shift_fuzz(1);
	}
}

/* The original master list of strings is described by *rlp0. We make a working copy
   described by rl, which has an allocated buffer large enough to hold all the
   original char pointers, but may at any time have fewer entries due to fuzzy
   matching. request_strings_init() sets up this copy, and request_strings_cleanup()
   cleans up the allocations. A small handful of static variables keep up with
   common prefix size, default entry index, etc. */

static int request_strings_init(req_list *rlp0) {
	rl.cur_entries = rlp0->cur_entries;
	rl.max_entry_len = rlp0->max_entry_len;
	rl.suffix = rlp0->suffix;
	if (!(rl.entries = calloc(rlp0->cur_entries, sizeof(char *)))) return 0;
	rl.alloc_entries = rlp0->cur_entries;
	memcpy(rl.entries, rlp0->entries, rl.cur_entries * sizeof(char *));
	rl0 = rlp0;
	rl.allow_dupes = rl0->allow_dupes;
	rl.allow_reorder = rl0->allow_reorder;
	rl.ignore_tab = rl0->ignore_tab;
	rl.reordered = rl0->reordered;
	rl.fuzz_len = rl0->fuzz_len;
	prune = rl.prune = rl0->prune;
	rl.cur_chars = rl.alloc_chars = 0;
	rl.chars = NULL;
	fuzz_len = rl.fuzz_len ? rl.fuzz_len : common_prefix_len(&rl);
	return rl.cur_entries;
}

static int request_strings_cleanup(bool reordered) {
	int n = PXY2N(page, x, y);
	const char * const p0 = rl.entries[n];
	for (int i = 0; i<rl0->cur_entries; i++) {
		if (rl0->entries[i] == p0) {
			n = i;
			break;
		}
	}
	if (rl.entries) free(rl.entries);
	rl.entries = NULL;
	rl0->reordered = reordered;
	return n;
}

/* indicates the function request_strings() should call upon CLOSEDOC_A */
static int (*rs_closedoc)(int n) = NULL;

/* indicates the correct function to call to restore the status bar after
   suspend/resume, particularly during requesters. */
void (*resume_status_bar)(const char *message);
static bool resume_bar = false;

/* Given a list of strings, let the user pick one. If _rl->suffix is not '\0', we bold names ending with it.
   The integer returned is one of the following:
   n >= 0  User selected string n with the enter key.
   -1      Error or abort; no selection made.
   -n - 2  User selected string n with the TAB key.
   (Yes, it's kind of evil, but it's nothing compared to what request() does!) */

int request_strings(req_list *rlp0, int n) {

	assert(rlp0->cur_entries > 0);

	int ne_lines0 = 0, ne_columns0 = 0;
	bool reordered = false;
	max_names_per_line = max_names_per_col = x = y = page = fuzz_len = 0;
	if ( ! request_strings_init(rlp0) ) return ERROR;

	const int dx = rl.max_entry_len + 1 + (rl.suffix ? 1 : 0);

	while(true) {
		if (ne_lines0 != ne_lines || ne_columns0 != ne_columns || resume_bar) {
			if (ne_lines0 && ne_columns0 ) n = PXY2N(page, x, y);
			if (!(max_names_per_line = ne_columns / dx)) max_names_per_line = 1;
			max_names_per_col = ne_lines - 1;
			names_per_page = max_names_per_line * max_names_per_col;
			ne_lines0 = ne_lines;
			ne_columns0 = ne_columns;
			page = N2P(n);
			x = N2X(n);
			y = N2Y(n);
			print_strings();
			if (resume_bar) {
				resume_status_bar(NULL);
				resume_bar = false;
			}
		}

		n = PXY2N(page, x, y);

		assert(fuzz_len >= 0);

		fuzz_len = min(fuzz_len, strlen(rl.entries[n]));

		move_cursor(y, x * dx + fuzz_len);

		int c;
		input_class ic;
		do c = get_key_code(); while((ic = CHAR_CLASS(c)) == IGNORE);

		if (window_changed_size) {
			window_changed_size = false;
			resume_bar = true;
			continue; /* Window resizing. */
		}

		switch(ic) {
			case INVALID:
				/* ignore and move on */
				break;

			case ALPHA:
				if (n >= rl.cur_entries) n = rl.cur_entries - 1;

				c = localised_up_case[(unsigned char)c];

				fuzz_forward( c );

				break;

			case TAB:
				if (! rlp0->ignore_tab) {
					n = request_strings_cleanup(reordered);
					if (n >= rlp0->cur_entries) return ERROR;
					else return -n - 2;
				}
				break;

			case RETURN:
				n = request_strings_cleanup(reordered);
				if (n >= rlp0->cur_entries) return ERROR;
				else return n;

			case COMMAND:
				if (c < 0) c = -c - 1;
				const int a = parse_command_line(key_binding[c], NULL, NULL, false);
				if (a >= 0) {
					switch(a) {
					case SUSPEND_A:
						stop_ne();
						resume_bar = true;
						break;

					case BACKSPACE_A:
						fuzz_back();
						break;

					case MOVERIGHT_A:
						request_move_right();
						break;

					case MOVELEFT_A:
						request_move_left();
						break;

					case MOVESOL_A:
						request_move_to_sol();
						break;

					case MOVEEOL_A:
						request_move_to_eol();
						break;

					case TOGGLESEOL_A:
						if (x != 0) x = 0;
						else request_move_to_eol();
						break;

					case LINEUP_A:
						request_move_up();
						break;

					case LINEDOWN_A:
						request_move_down();
						break;

					case MOVEINCUP_A:
						request_move_inc_up();
						break;

					case MOVEINCDOWN_A:
						request_move_inc_down();
						break;

					case PAGEUP_A:
					case PREVPAGE_A:
						request_prev_page();
						break;

					case PAGEDOWN_A:
					case NEXTPAGE_A:
						request_next_page();
						break;

					case MOVESOF_A:
						request_move_to_sof();
						break;

					case MOVEEOF_A:
						request_move_to_eof();
						break;

					case TOGGLESEOF_A:
						request_toggle_seof();
						break;

					case NEXTWORD_A:
						request_move_next();
						break;

					case PREVWORD_A:
						request_move_previous();
						break;

					case NEXTDOC_A:
						reordered |= request_reorder(1);
						break;

					case PREVDOC_A:
						reordered |= request_reorder(-1);
						break;

					case INSERT_A:
					case DELETECHAR_A:
						prune = !prune;
						int n1 = reset_rl_entries();
						page = -1;
						normalize(n1);
						break;

					case CLOSEDOC_A:
						if (rs_closedoc) {
							int n0 = rs_closedoc(PXY2N(page, x, y));
							page = -1;
							normalize(n0);
						}
						break;

					case SELECTDOC_A:           /* Allow F4 to toggle us in */
						if (!rs_closedoc) break; /* out of requestor.        */
					case ESCAPE_A:
					case QUIT_A:
						request_strings_cleanup(reordered);
						return -1;
					}
				}
				break;

			default:
				break;
		}
	}
}



/* The completion function. Returns NULL if no file matches start_prefix, or
   the longest prefix common to all files extending start_prefix. */

char *complete_filename(const char *start_prefix) {

	/* This might be NULL if the current directory has been unlinked, or it is not readable.
	   in that case, we end up moving to the completion directory. */
	char * const cur_dir_name = ne_getcwd(CUR_DIR_MAX_SIZE);

	char * const dir_name = str_dup(start_prefix);
	if (dir_name) {
		char * const p = (char *)file_part(dir_name);
		*p = 0;
		if (p != dir_name && chdir(tilde_expand(dir_name)) == -1) {
			free(dir_name);
			return NULL;
		}
	}

	start_prefix = file_part(start_prefix);
	bool is_dir, unique = true;
	char *cur_prefix = NULL;
	DIR * const d = opendir(CURDIR);

	if (d) {
		for(struct dirent * de; !stop && (de = readdir(d)); ) {
			if (is_prefix(start_prefix, de->d_name))
				if (cur_prefix) {
					cur_prefix[max_prefix(cur_prefix, de->d_name)] = 0;
					unique = false;
				}
				else {
					cur_prefix = str_dup(de->d_name);
					is_dir = is_directory(de->d_name);
				}
		}

		closedir(d);
	}

	char * result = NULL;

	if (cur_prefix) {
		result = malloc(strlen(dir_name) + strlen(cur_prefix) + 2);
		strcat(strcat(strcpy(result, dir_name), cur_prefix), unique && is_dir ? "/" : "");
	}

	if (cur_dir_name != NULL) {
		chdir(cur_dir_name);
		free(cur_dir_name);
	}
	free(dir_name);
	free(cur_prefix);

	return result;
}

static void load_syntax_names(req_list *rl, DIR *d, int flag) {

	const int extlen = strlen(SYNTAX_EXT);
	stop = false;

	for( struct dirent *de; !stop && (de = readdir(d)); ) {
		if (is_directory(de->d_name)) continue;
		const int len = strlen(de->d_name);
		if (len > extlen && !strcmp(de->d_name+len - extlen, SYNTAX_EXT)) {
			char ch = de->d_name[len-extlen];
			de->d_name[len-extlen] = '\0';
			if (!req_list_add(rl, de->d_name, flag)) break;
			de->d_name[len-extlen] = ch;
		}
	}
}

/* This is the syntax requester. It reads the user's syntax directory and the
   global syntax directory, builds an array of strings and calls request_strings().
   Returns NULL on error or escaping, or a pointer to the selected syntax name sans
   extension if RETURN or TAB key was pressed. As per request_files(), if the
   selection was made with the TAB key, the first character of the returned string
   is a NUL, so callers (currently only request()) must take care to handle this
   case. */

char *request_syntax() {
	char syn_dir_name[512];
	char *p;
	req_list rl;
	DIR *d;

	if (req_list_init(&rl, filenamecmp, false, false, '*') != OK) return NULL;
	if ((p = exists_prefs_dir()) && strlen(p) + 2 + strlen(SYNTAX_DIR) < sizeof syn_dir_name) {
		strcat(strcpy(syn_dir_name, p), SYNTAX_DIR);
		if (d = opendir(syn_dir_name)) {
			load_syntax_names(&rl, d, true);
			closedir(d);
		}
	}
	if ((p = exists_gprefs_dir()) && strlen(p) + 2 + strlen(SYNTAX_DIR) < sizeof syn_dir_name) {
		strcat(strcpy(syn_dir_name, p), SYNTAX_DIR);
		if (d = opendir(syn_dir_name)) {
			load_syntax_names(&rl, d, false);
			closedir(d);
		}
	}
	req_list_finalize(&rl);
	p = NULL;
	int result;
	if (rl.cur_entries && (result = request_strings(&rl, 0)) != ERROR) {
		char * const q = rl.entries[result >= 0 ? result : -result - 2];
		if (p = malloc(strlen(q)+3)) {
			strcpy(p, q);
			if (p[strlen(p)-1] == rl.suffix) p[strlen(p)-1] = '\0';
			if (result < 0) {
				memmove(p + 1, p, strlen(p) + 1);
				p[0] = '\0';
			}
		}
	}
	req_list_free(&rl);
	return p;
}


/* This is the file requester. It reads the directory in which the filename
   lives, builds an array of strings and calls request_strings(). If a directory
   name is returned, it enters the directory. Returns NULL on error or escaping, a
   pointer to the selected filename if RETURN is pressed, or a pointer to the
   selected filename (or directory) preceeded by a NUL if TAB is pressed (so by
   checking whether the first character of the returned string is NUL you can
   check which key the user pressed). */


char *request_files(const char * const filename, bool use_prefix) {

	char * const cur_dir_name = ne_getcwd(CUR_DIR_MAX_SIZE);
	if (!cur_dir_name) return NULL;

	bool absolute = false;
	char * const dir_name = str_dup(filename);
	if (dir_name) {
		int result = 0;
		if (dir_name[0] == '/') absolute = true;
		char * const p = (char *)file_part(dir_name);
		if (p != dir_name) {
			*p = 0;
			result = chdir(tilde_expand(dir_name));
		}
		free(dir_name);
		if (result == -1) return NULL;
	}

	req_list rl;
	bool next_dir;
	char *result = NULL;
	do {
		next_dir = false;
		if (req_list_init(&rl, filenamecmp, true, false, '/') != OK) break;

		DIR * const d = opendir(CURDIR);
		if (d) {

			stop = false;
			for(struct dirent * de; !stop && (de = readdir(d)); ) {
				const bool is_dir = is_directory(de->d_name);
				if (use_prefix && !is_prefix(file_part(filename), de->d_name)) continue;
				if (!req_list_add(&rl, de->d_name, is_dir)) break;
			}

			req_list_finalize(&rl);

			if (rl.cur_entries) {
				const int t = request_strings(&rl, 0);
				if (t != ERROR) {
					char * const p = rl.entries[t >= 0 ? t : -t - 2];
					if (p[strlen(p) - 1] == '/' && t >= 0) {
						p[strlen(p) - 1] = 0;
						if (chdir(p)) alert();
						else use_prefix = false;
						next_dir = true;
					}
					else {
						result = ne_getcwd(CUR_DIR_MAX_SIZE + strlen(p) + 2);
						if (strcmp(result, "/")) strcat(result, "/");
						strcat(result, p);
						if (!absolute) {
							char *rp = relative_file_path(result, cur_dir_name);
							free(result);
							result = rp;
						}
						if (t < 0) {
							memmove(result + 1, result, strlen(result) + 1);
							result[0] = 0;
						}
					}
				}
			}
			closedir(d);
		}
		else alert();
		req_list_free(&rl);
	} while(next_dir);

	chdir(cur_dir_name);
	free(cur_dir_name);

	return result;
}


/* Requests a file name. If no_file_req is false, the file requester is firstly
   presented. If no_file_req is true, or the file requester is escaped, a long
   input is performed with the given prompt and default_name. */

char *request_file(const buffer *b, const char *prompt, const char *default_name) {

	char *p = NULL;

	if (!b->opt.no_file_req) {
		print_message(info_msg[PRESSF1]);
		p = request_files(default_name, false);
		reset_window();
		draw_status_bar();
		if (p && *p) return p;
	}

	if (p = request_string(b, prompt, p ? p + 1 : default_name, false, COMPLETE_FILE, io_utf8)) return p;

	return NULL;
}

/* This is the callback function for the SelectDoc requester's CloseDoc action. */
static int handle_closedoc(int n) {
	/* "n" is the index into rl.entries of the "char *p" corresponding to an
	   entry in rl0->entries, the index "o" of which -- possibly via
	   rl0->orig_order -- corresponds to the index of the buffer we want
	   to close if it is unmodified. */

	const char * const p = rl.entries[n];

	int o;
	for (o = 0; o < rl0->cur_entries && rl0->entries[o] != p; o++) /* empty loop */ ;

	if (o == rl0->cur_entries) return n; /* This should never happen. */

	if (rl0->orig_order) o = rl0->orig_order[o];

	buffer *bp = get_nth_buffer(o);

	if (!bp || bp->is_modified) return n; /* We don't close modified buffers here. */

	/* We've determined we are going to close document *bp. */

	if (rl.cur_entries == 1 && rl0->cur_entries > 1) {
		/* Ensure we'll still have an entry in view after closing *bp. */
		fuzz_back(); /* "n" is no longer valid. */
	}

	/* Normally when we close the indicated buffer, we advance the indicator to the next
	   buffer. But "wrapping around" when deleting the last entry is very jarring in a
	   requester, so in that case we back up one before closing the buffer. */

	if (PXY2N(page, x, y) == rl.cur_entries - 1 && rl.cur_entries > 1)
		normalize(rl.cur_entries - 2);

	req_list_del(rl0, o);
	n = reset_rl_entries();

	buffer *nextb = (buffer *)bp->b_node.next;

	rem(&bp->b_node);
	free_buffer(bp);

	if (! nextb->b_node.next) nextb = (buffer *)buffers.head;
	if (nextb == (buffer *)&buffers.tail) {
		close_history();
		unset_interactive_mode();
		exit(0);
	}

	if (bp == cur_buffer) cur_buffer = nextb;

	return n;
}

/* Presents to the user a list of the documents currently available.  It
   returns the number of the document selected, or -1 on escape or error. */

int request_document(void) {

	int i = -1;
	req_list rl;
	buffer *b = (buffer *)buffers.head;

	if (b->b_node.next && req_list_init(&rl, NULL, true, true, '*')==OK) {
		i = 0;
		int cur_entry = 0;
		while(b->b_node.next) {
			if (b == cur_buffer) cur_entry = i;
			req_list_add(&rl, b->filename ? b->filename : UNNAMED_NAME, b->is_modified);
			b = (buffer *)b->b_node.next;
			i++;
		}
		rl.ignore_tab = true;
		req_list_finalize(&rl);
		print_message(info_msg[SELECT_DOC]);
		rs_closedoc = &handle_closedoc;
		i = request_strings(&rl, cur_entry);
		rs_closedoc = NULL;
		reset_window();
		draw_status_bar();
		if (i >= 0 && rl.reordered) {
			/* We're going to cheat big time here. We have an array of pointers
			   at rl.entries that's big enough to hold all the buffer pointers,
			   and that's exactly what we're going to use it for now. */
			b = (buffer *)buffers.head;
			for (int j = 0; b->b_node.next; j++ ) {
				rl.entries[j] = (char *)b;
				b = (buffer *)b->b_node.next;
				rem(b->b_node.prev);
			}
			/* Ack! We're removed all our buffers! */
			for (int j = 0; j < rl.cur_entries; j++) {
				add_tail(&buffers, (node *)rl.entries[rl.orig_order[j]]);
			}
		}

		req_list_free(&rl);
	}

	return i;
}

/* The req_list functions below provide a simple mechanism suitable for building
   request lists for directory entries, syntax recognizers, etc. */

/* These are the default allocation sizes for the entry array and for the
   name array when reading a directory. The allocation sizes start with these
   values, and they are doubled each time more space is needed. This ensures a
   reasonable number of retries. */

#define DEF_ENTRIES_ALLOC_SIZE     256
#define DEF_CHARS_ALLOC_SIZE  (4*1024)


/* Delete the nth string from the given request list. This will work regardless of whether
   the req_list has been finalized. */

int req_list_del(req_list * const rl, int nth) {

	if (nth < 0 || nth >= rl->cur_entries ) return ERROR;
	char * const str = rl->entries[nth];
	const int len0 = strlen(str);
	int len = len0;

	len += str[len + 1] ? 3 : 2;  /* 'a b c \0 Suffix \0' or 'a b c \0 \0' or 'a b c Suffix \0 \0' */
	memmove(str, str + len, sizeof(char)*(rl->cur_chars - ((str + len) - rl->chars)));

	for(int i = 0; i < rl->cur_entries; i++)
		if (rl->entries[i] > str )
			rl->entries[i] -= len;

	if (rl->orig_order) {
		int nth0 = rl->orig_order[nth];
		int skip = 0;
		for (int i = 0; i < rl->cur_entries; i++) {
			if (i >= nth) skip = 1;
			int i0 = rl->orig_order[i+skip];
			rl->orig_order[i] = (i0 >= nth0) ? i0 - 1 : i0;
		}
	}

	rl->cur_chars -= len;
	memmove(&rl->entries[nth], &rl->entries[nth+1], sizeof(char *)*(rl->cur_entries - nth));
	rl->cur_entries--;
	/* did we just delete longest string? */

	if (len0 == rl->max_entry_len) {
		for (int i = rl->max_entry_len = 0; i < rl->cur_entries; i++) {
			if ((len = strlen(rl->entries[i])) > rl->max_entry_len)
				rl->max_entry_len = len;
		}
	}
	return rl->cur_entries;
}

void req_list_free(req_list * const rl) {
	if (rl->entries) free(rl->entries);
	rl->entries = NULL;
	if (rl->chars) free(rl->chars);
	rl->chars = NULL;
	if (rl->allow_reorder && rl->orig_order) free(rl->orig_order);
	rl->orig_order = NULL;
	rl->allow_reorder = false;
	rl->cur_entries = rl->alloc_entries = rl->max_entry_len = 0;
	rl->cur_chars = rl->alloc_chars = 0;
}

/* Initialize a request list. A comparison function may be provided; if it is provided,
   that function will be used to keep the entries sorted. If NULL is provided instead,
   entries are kept in the order they are added. The boolean allow_dupes determines
   whether duplicate entries are allowed. If not, and if cmpfnc is NULL, then each
   addition requires a linear search over the current entries. If a suffix character is
   provided, it can optionally be added to individual entries as they are added, in which
   case req_list_finalize() should be called before the entries are used in a
   request_strings() call. */

int req_list_init( req_list * const rl, int cmpfnc(const char *, const char *), const bool allow_dupes, const bool allow_reorder, const char suffix) {
	rl->cmpfnc = cmpfnc;
	rl->allow_dupes = allow_dupes;
	rl->allow_reorder = allow_reorder;
	rl->ignore_tab = false;
	rl->prune = false;
	rl->fuzz_len = 0;
	rl->suffix = suffix;
	rl->cur_entries = rl->alloc_entries = rl->max_entry_len = 0;
	rl->cur_chars = rl->alloc_chars = 0;
	if (rl->entries = malloc(sizeof(char *) * DEF_ENTRIES_ALLOC_SIZE)) {
		if (rl->chars = malloc(sizeof(char) * DEF_CHARS_ALLOC_SIZE)) {
			rl->alloc_entries = DEF_ENTRIES_ALLOC_SIZE;
			rl->alloc_chars = DEF_CHARS_ALLOC_SIZE;
			return OK;
		}
		free(rl->entries);
		rl->entries = NULL;
	}
	return OUT_OF_MEMORY;
}

/* req_list strings are stored with a trailing '\0', followed by an optional suffix
   character, and an additional trailing '\0'. This allows comparing strings w/o
   having to consider the optional suffexes. Finalizing the req_list effectively
   shifts the suffixes left, exchanging them for the preceeding '\0'. After this
   operation, all the strings will be just strings, some of which happen to end
   with the suffix character, and all of which are followed by two null bytes.

   req_list_finalize() also initializes the orig_order array if allow_reorder
   is true. If the array cannot be allocated, allow_reorder is simply reset to
   false rather than returning an error. */

void req_list_finalize(req_list * const rl) {
	for (int i = 0; i < rl->cur_entries; i++) {
		const int len = strlen(rl->entries[i]);
		*(rl->entries[i]+len) = *(rl->entries[i]+len+1);
		*(rl->entries[i]+len+1) = '\0';
	}
	rl->orig_order = NULL;
	if (rl->allow_reorder ) {
		if ( rl->orig_order = malloc(sizeof(int) * rl->cur_entries)) {
			for (int i = 0; i < rl->cur_entries; i++)
				rl->orig_order[i] = i;
		}
		else rl->allow_reorder = false;
	}
}

/* Add a string plus an optional suffix to a request list.
   We really add two null-terminated strings: the actual entry, and a
   possibly empty suffix. These pairs should be merged later by
   req_list_finalize(). If duplicates are not allowed (see req_list_init()) and
   the str already exists in the table (according to the comparison function or
   by strcmp if there is not comparison function), then the conflicting entry
   is returned. */
char *req_list_add(req_list * const rl, char * const str, const int suffix) {
	const int len = strlen(str);
	const int lentot = len + ((rl->suffix && suffix) ? 3 : 2); /* 'a b c \0 Suffix \0' or 'a b c \0 \0' */

	int ins;
	if (rl->cmpfnc) { /* implies the entries are sorted */
		int l = 0, m = 0;
		int r = rl->cur_entries - 1;
		while(l <= r) {
			m = (r + l)/2;
			const int cmp = (*rl->cmpfnc)(str, rl->entries[m]);
			if (cmp < 0 )
				r = m - 1;
			else if (cmp > 0)
				l = m + 1;
			else {
				m = - m - 1;
				break;
			}
		}
		if (m < 0) { /* found a match at -m - 1 */
			if (!rl->allow_dupes) return rl->entries[-m - 1];
			ins = -m;
		}
		else if (r < m) { ins = m; /* insert at i */ }
		else if (l > m) { ins = m + 1; /* insert at i + 1 */ }
		else { /* impossible! */ ins = rl->cur_entries; }
	}
	else {/* not ordered */
		ins = rl->cur_entries; /* append to end */
		if (!rl->allow_dupes) {
			for(int i = 0; i < rl->cur_entries; i++)
				if(!strcmp(rl->entries[i], str)) return rl->entries[i];
		}
	}

	if (len > rl->max_entry_len) rl->max_entry_len = len;

	/* make enough space to store the new string */
	if (rl->cur_chars + lentot > rl->alloc_chars) {
		char * p0 = rl->chars;
		char * p1;
		p1 = realloc(rl->chars, sizeof(char) * (rl->alloc_chars * 2 + lentot));
		if (!p1) return NULL;
		rl->alloc_chars = rl->alloc_chars * 2 + lentot;
		rl->chars = p1;
		/* all the strings just moved from *p0 to *p1, so adjust accordingly */
		for (int i = 0; i < rl->cur_entries; i++)
			rl->entries[i] += ( p1 - p0 );
	}
	/* make enough slots to hold the string pointer */
	if (rl->cur_entries >= rl->alloc_entries) {
		char **t;
		t = realloc(rl->entries, sizeof(char *) * (rl->alloc_entries * 2 + 1));
		if (!t) return NULL;
		rl->alloc_entries = rl->alloc_entries * 2 + 1;
		rl->entries = t;
	}

	char * const newstr = &rl->chars[rl->cur_chars];
	char * p = strcpy(newstr, str)+len+1;
	if (rl->suffix && suffix) *p++ = rl->suffix;
	*p = '\0';
	rl->cur_chars += lentot;
	if (ins < rl->cur_entries)
		memmove(&rl->entries[ins+1], &rl->entries[ins], sizeof(char *)*(rl->cur_entries - ins));
	rl->entries[ins] = newstr;
	rl->cur_entries++;
	return newstr;
}

