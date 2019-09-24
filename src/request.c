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

#define BY_COLUMN         (req_order)
#define BY_ROW            (!BY_COLUMN)

/* X and Y constitute a screen coordinate derived from (C,R) which varies from page to page.
     (Y, being a synonym for R, is implemented as a macro.)
   R and C are the row and column of the currently selected entry.
   page is the index into req_page_table.req_page.
   fuzz_len is the length of the matching prefix into our current entry. */
static int X, R, C, page, fuzz_len;
#define Y (R)

static bool prune;

/* Traditionally ne has displayed request entries BY_ROW (req_order==0):
      a   b   c   d
      e   f   g   h
      i   j
   rather than BY_COLUMN (req_order==1):
      a   e   i
      b   f   j
      c   g
      d   h
   which, while easier to program, is somewhat harder to read.
   The request code is full of tricky expressions that deal with
   multiple pages of entries displayed either BY_ROW or BY_COLUMN,
   and attempts to consolidate as many of those tricky expressions
   into one small set of macros and functions. */

#define NAMES_PER_ROW(p)     (req_page_table.req_page[(p)].cols)
#define NAMES_PER_COL(p)     (req_page_table.req_page[(p)].rows)
#define N2PCRX(n,p,c,r,x)    (n2pcrx((n),&(p),&(c),&(r),&(x)))
#define N2P(n)               (n2pcrx((n),NULL,NULL,NULL,NULL))
#define N2X(n,x)             (n2pcrx((n),NULL,NULL,NULL,&(x)))
#define PCR2N(p,c,r)         (pcr2n((p),(c),(r)))
#define DX(d)                (dxd((d)))
#define DY(d)                (dyd((d)))

static req_list rl, *rl0; /* working req_list and pointer to the original req_list */

#define MAX_REQ_COLS  16

typedef struct {
	int first;     /* index into rl->entries[] of first entry on this page */
	int entries;   /* how many entries on this page */
	int cols;      /* columns fitting onto this page */
	int rows;      /* rows fitting onto this page */
	int col_width[MAX_REQ_COLS]; /* array of ints with width of each colummn */
} req_page_t;


/* n0        index of the first entry for this page.
   entries   number of entries to fit onto this page.
   cols      number of columns to create.
   rows      number of rows to create.
   page      req_page_t to fill in with these parameters.
   Returns true if the *page will fit on screen and has the requested number of entries; false otherwise.
   (Assumes 1 column always fits.) */

static bool fit_page(int n0, int entries, int cols, int rows, req_page_t * page) {
	int i, combined_col_widths;
	assert(n0 + entries <= rl.cur_entries);
	assert(cols <= MAX_REQ_COLS && cols > 0);
	assert(entries <= cols * rows);
	assert(rows <= ne_lines - 1);
	memset(page->col_width, 0, sizeof(int) * MAX_REQ_COLS);
	page->first = n0;
	page->entries = 0;
	page->cols = 0;
	page->rows = 0;
	for (i=0, combined_col_widths=0; i<entries && (cols == 1 || combined_col_widths <= ne_columns); i++) {
		int row = req_order ? i % rows : i / cols;
		int col = req_order ? i / rows : i % cols;
		if (page->col_width[col] < rl.lens[n0 + i]) {
			combined_col_widths += rl.lens[n0 + i] - page->col_width[col];
			page->col_width[col] = rl.lens[n0 + i];
		}
		page->entries++;
		if (page->cols < col+1) page->cols = col+1;
		if (page->rows < row+1) page->rows = row+1;
	}
	return page->entries == entries && (cols == 1 || combined_col_widths <= ne_columns);
}


static struct {
	int pages;             /* allocated req_pag_t structures */
	req_page_t * req_page; /* array of allocated req_page_t */
} req_page_table = { 0, NULL} ;


/* Return the index of the req_page containing entry n.
   If n is beyond configured req_pages, return 0 (i.e. it's probably not set up yet.)
   As a side effect, set any of *P, *C, *R, or *x if P, C, R, or x is not NULL. */

static int n2pcrx(int n, int *P, int *C, int *R, int *x) {
	req_page_t * pp = &req_page_table.req_page[0];
	for (int pg = 0; pg < req_page_table.pages && pp->entries; pg++, pp++) {
		if (pp->first + pp->entries > n) {
			if (P) *P = pg;
			int dn = n - pp->first;
			if (R) *R = req_order ? dn % pp->rows : dn / pp->cols;
			int col   = req_order ? dn / pp->rows : dn % pp->cols;
			if (C) *C = col;
			if (x) {
				int cum=0;
				for (int i=0; i < col; i++)
					cum += pp->col_width[i];
				*x = cum;
			}
			return pg;
		}
	}
	if (P) *P = 0;
	if (C) *C = 0;
	if (R) *R = 0;
	if (x) *x = 0;
	return 0;
}


/* pcr2n() can return an n > rl.cur_entries. This is intentional. */
static int pcr2n(int p, int c, int r) {
	req_page_t *pp = &req_page_table.req_page[p];
	if (BY_ROW) return pp->first + pp->cols * r + c;
	return pp->first + pp->rows * c + r;
}


static void req_page_table_free() {
	free(req_page_table.req_page);
	req_page_table.req_page = NULL;
	req_page_table.pages = 0;
}


/* sums n integers starting at *i */
static int sum_ints(int n, int *i) {
	int s = 0;
	while(n--) s += *i++;
	return s;
}


static int req_page_table_expand() {
	int new_pages = req_page_table.pages + MAX_REQ_COLS; /* Nothing to do with cols, but should be plenty. */
	req_page_t *p = realloc(req_page_table.req_page, sizeof(req_page_t) * new_pages);
	if (!p) return 0;
	memset(p + req_page_table.pages, 0, MAX_REQ_COLS * sizeof(req_page_t));
	req_page_table.pages = new_pages;
	req_page_table.req_page = p;
	return new_pages;
}


/* Rebuild the req_page array starting with the page containing n0. If n0==0, the
   array is rebuilt from scratch (as per requester start-up, or when the screen is
   resized). The case for n0>0 is when elements are re-ordered via F2/F3 (for
   requesters where reordering is allowed). You must never call with n0>0 unless
   existing pages contain n0. If progressive filtering is in play, then they all
   must be rebuilt (n0==0).
   Returns 0 on error, in which case the whole requester is abandoned. */

static int reset_req_pages(int n0) {
	if (req_page_table.pages == 0 && req_page_table_expand() == 0) return 0;
	int cur_page = N2P(n0);
	int first = cur_page == 0 ? 0 : req_page_table.req_page[cur_page-1].first + req_page_table.req_page[cur_page-1].entries;
	memset(&req_page_table.req_page[cur_page], 0, req_page_table.pages - cur_page * sizeof(req_page_t));
	int max_cols = min(ne_columns / 3, MAX_REQ_COLS);
	int max_rows = ne_lines - 1;
	int c;
	while (first < rl.cur_entries) {
		if (cur_page >= req_page_table.pages && req_page_table_expand() == 0) return 0;
		for (c = max_cols; c > 0; c--)
			if (fit_page(first, min(c*max_rows, rl.cur_entries - first), c, max_rows, &req_page_table.req_page[cur_page])) break;
		if (c==0) return 0; /* This should never happen. */
		first += req_page_table.req_page[cur_page].entries;
		cur_page++;
	}
	/* Tweak the final page in a BY_COLUMN layout so that the rectangle containing strings is roughly
	   proportional to the terminal window. This is purely an aesthetic thing. I'd like to do it for the
	   BY_ROW layout, too, but "some people" have "opinions". Meh. */
	if (BY_COLUMN) {
		req_page_t * pp = &req_page_table.req_page[cur_page-1];
		int needed_entries = pp->entries;
		int first = pp->first;

		for (int rows = 1; rows <= max_rows; rows++) {
			c = needed_entries / rows + (needed_entries % rows ? 1 : 0);
			if (c >= MAX_REQ_COLS) continue;
			if (fit_page(first, needed_entries, c, rows, pp)) {
				if (rows == max_rows || sum_ints(pp->cols, pp->col_width) * 1000 / pp->rows < ne_columns * 1000 / (ne_lines - 1)) {
					return cur_page;
				}
			}
		}
	}
	return cur_page;
}


/* Move 1 column left (negative) or right (positive).
   Assumes 'page', 'R', and 'C' are correct.
   Do not alter these values, as they are needed by normalize()
   to determine whether screen updates are needed.
   Returns the new selected entry's index. */

static int dxd(int dir) {
	req_page_t *pp = &req_page_table.req_page[page];
	int last_n = pp->first + pp->entries - 1;
	int n0;

	if (dir == -1) {
		if (R == 0 && C == 0) {
			if (page > 0) return pp->first - 1;
		} else {
			if (C > 0) return pcr2n(page, C - 1, R);
			else if ((n0 = pcr2n(page, pp->cols - 1, R - 1)) <= last_n) return n0;
			else return pcr2n(page, pp->cols - 2, R - 1);
		}
	} else { /* dir == 1 */
		if (pcr2n(page, C, R) == last_n) {
			if (last_n < rl.cur_entries)
				return last_n + 1;
		} else {
			if (C < pp->cols - 1 && (n0 = pcr2n(page, C + 1, R)) < pp->first + pp->entries) return n0;
			else if (R < pp->rows - 1) return pcr2n(page, 0, R + 1);
		}
	}
	return pcr2n(page, C, R); /* Can't move; hold position. */
}


/* Like dxd(), but -1 is "up", +1 is "down". */

static int dyd(int dir) {
	req_page_t *pp = &req_page_table.req_page[page];
	int last_n = pp->first + pp->entries - 1;
	if (dir == -1) { /* up */
		if (R == 0 && C == 0) {
			if (page > 0) return pp->first - 1;
		} else {
			if (R > 0) return pcr2n(page, C, R - 1);
			else return pcr2n(page, C - 1, pp->rows - 1);
		}
	} else { /* dir == 1, i.e. "down" */
		if (pcr2n(page, C, R) == last_n) return last_n + 1;
		else if (R == pp->rows - 1) return pcr2n(page, C + 1, 0);
		else return pcr2n(page, C, R + 1);
	}
	return pcr2n(page, C, R); /* Can't move; hold position */
}


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
   strings from the entries array existing in "page". */

static void print_strings() {
	const int dx = rl.max_entry_len + 1 + (rl.suffix ? 1 : 0);

	set_attr(0);
	req_page_t *pp = &req_page_table.req_page[page];
	for(int row = 0; row < ne_lines - 1; row++) {
		move_cursor(row, 0);
		clear_to_eol();
		if (row < pp->rows) {
			for(int col = 0, x=0; col < pp->cols; col++) {
				if (PCR2N(page, col, row) < rl.cur_entries) {
					move_cursor(row, x);
					const char * const p = rl.entries[PCR2N(page, col, row)];
					if (rl.suffix) set_attr(p[strlen(p) - 1] == rl.suffix ? BOLD : 0);
					output_string(p, io_utf8);
					x += pp->col_width[col];
				}
			}
		}
	}
}


/* Reset X, (Y; not really; same as R), R, C, and page in terms of n.
   Returns true if we called print_strings(), false otherwise. */

static bool normalize(int n) {
	const int p = page;

	if (n < 0 ) n = 0;
	if (n >= rl.cur_entries ) n = rl.cur_entries - 1;
	N2PCRX(n, page, C, R, X);
	if ( p != page ) {
		print_strings();
		return true;
	}
	return false;
}


static void request_move_to_sol(void) {
	while (C > 0) normalize(DX(-1));
}


static void request_move_to_eol(void) {
	while (C < NAMES_PER_ROW(page) - 1 && PCR2N(page, C + 1, R) < rl.cur_entries) normalize(DX(1));
}


static void request_move_to_sof(void) {
	normalize(0);
}


static void request_move_to_eof(void) {
	normalize(rl.cur_entries - 1);
}


static void request_toggle_seof(void) {
	if (C + R + page == 0) request_move_to_eof();
	else request_move_to_sof();
}


static void request_prev_page(void) {
	if (page == 0 ) normalize(0);
	else {
		req_page_t *pp = &req_page_table.req_page[page];
		int n_per_mille = ((PCR2N(page, C, R) - pp->first) * 65536) / pp->entries;
		pp--;
		int new_n = max((n_per_mille * (pp->entries+1)) / 65536 + pp->first, pp->first);
		normalize(new_n);
	}
}


static void request_next_page(void) {
	req_page_t *pp = &req_page_table.req_page[page];
	if (pp->first + pp->entries < rl.cur_entries) {
		req_page_t *pp = &req_page_table.req_page[page];
		int n_per_mille = ((PCR2N(page, C, R) - pp->first) * 65536) / pp->entries;
		pp++;
		int new_n = min((n_per_mille * (pp->entries+1)) / 65536 + pp->first, pp->first + pp->entries - 1);
		normalize(new_n);
	} else normalize(rl.cur_entries - 1);
}


static void request_move_up(void) {
	normalize(DY(-1));
}


static void request_move_inc_up(void) {
	if (C == 0) {
		if (R == 0) request_move_to_sof();
		else request_prev_page();
	}
	else request_move_to_sol();
}


static void request_move_down(void) {
	normalize(DY(1));
}


void request_move_inc_down(void) {
	if (C == NAMES_PER_ROW(page) - 1) {
		if (R == NAMES_PER_COL(page) - 1) request_move_to_eof();
		else request_next_page();
	}
	else request_move_to_eol();
}


static void request_move_left(void) {
	normalize(DX(-1));
}


static void request_move_next(void) {
	normalize(PCR2N(page, C, R) + 1);
}


static void request_move_previous(void) {
	normalize(PCR2N(page, C, R) - 1);
}


static void request_move_right(void) {
	normalize(DX(1));
}


/* Reorder (i.e. swap) the current entry n with entry n+dir.
   dir should be either 1 or -1. */

static bool request_reorder(const int dir) {
	if (! rl0->allow_reorder || rl.cur_entries < 2) return false;

	const int n0 = PCR2N(page, C, R);
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

	i = rl.lens[n0];
	rl.lens[n0] = rl.lens[n1];
	rl.lens[n1] = i;

	rl0->entries[i0] = p1;
	rl0->entries[i1] = p0;

	reset_req_pages(min(n0,n1));
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
	const char * const p0 = rl.entries[PCR2N(page, C, R)];
	int i, n;
	for (int j = n = i = 0; j < rl0->cur_entries; j++) {
		char * const p1 = rl0->entries[j];
		if ( ! prune || ! strncasecmp(p0, p1, fuzz_len) ) {
			if (p1 == p0) n = i;
			rl.entries[i] = p1;
			rl.lens[i++] = rl0->lens[j];
		}
	}
	rl.cur_entries = i;
	reset_req_pages(0);
	return n;
}


/* Count how many entries match our highlighted entry up through len characters. */

static int count_fuzz_matches(const int len) {
	const int n0 = PCR2N(page, C, R);
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
	const char * const p0 = rl.entries[PCR2N(page, C, R)];
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
	const int n0 = PCR2N(page, C, R);
	const char * const p0 = rl.entries[n0];
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
			memmove(rl.lens    + shiftsize, rl.lens,    rl.cur_entries * sizeof(int));

			int rldest, rl0src, rlsrc, n0shifted = n0 + shiftsize;

			for (rldest = rl0src = 0, rlsrc = shiftsize; rl0src < rl0->cur_entries; rl0src++) {

				if (rl0->entries[rl0src] == rl.entries[rlsrc]) {
					/* This was in our old list, so we keep it */
					if (n0shifted == rlsrc) n1 = rldest;  /* it was our marked entry */
					rl.entries[rldest] = rl.entries[rlsrc];
					rl.lens[rldest++] = rl.lens[rlsrc++];

				} else if ( ! strncasecmp(p0, rl0->entries[rl0src], fuzz_len) ) {
					/* Wasn't in our old list due to prior purning, but should be */
					rl.entries[rldest] = rl0->entries[rl0src];
					rl.lens[rldest++] = rl0->lens[rl0src];
				}
			}
			rl.cur_entries = rldest;
		}
		shift_fuzz(-1);
	}
	reset_req_pages(0);
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
	const int n0 = PCR2N(page, C, R);
	const char * const p0 = rl.entries[n0];

	assert(fuzz_len >= 0);

	if (prune) {
		int i = 0, n1 = 0;
		for (int j = 0; j < rl.cur_entries; j++) {
			char * const p1 = rl.entries[j];
			int len = rl.lens[j];
			const int cmp = strncasecmp(p0, p1, fuzz_len);
			if (! cmp && strlen(p1) > fuzz_len && localised_up_case[(unsigned char)p1[fuzz_len]] == c) {
				if (p1 == p0)
					n1 = i;
				rl.entries[i] = p1;
				rl.lens[i++] = len;
			}
		}
		if (i) {
			rl.cur_entries = i;
			fuzz_len = common_prefix_len(&rl);
			reset_req_pages(0);
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
	if (!(rl.lens = calloc(rlp0->cur_entries, sizeof(int)))) {
		free(rl.entries);
		return 0;
	}
	rl.alloc_entries = rlp0->cur_entries;
	memcpy(rl.entries, rlp0->entries, rl.cur_entries * sizeof(char *));
	memcpy(rl.lens,    rlp0->lens,    rl.cur_entries * sizeof(int));
	rl0 = rlp0;
	rl.allow_dupes     = rl0->allow_dupes;
	rl.allow_reorder   = rl0->allow_reorder;
	rl.ignore_tab      = rl0->ignore_tab;
	rl.reordered       = rl0->reordered;
	rl.find_quits      = rl0->find_quits;
	rl.help_quits      = rl0->help_quits;
	rl.selectdoc_quits = rl0->selectdoc_quits;
	prune = rl.prune   = rl0->prune;
	/* rl doesn't have its own allocated characters; critically, its entries point
	   to allocations that belong to rlp0. */
	rl.cur_chars = rl.alloc_chars = 0;
	rl.chars = NULL;
	fuzz_len = common_prefix_len(&rl);
	return rl.cur_entries;
}


static int request_strings_cleanup(bool reordered) {
	int n = PCR2N(page, C, R);
	const char * const p0 = rl.entries[n];
	for (int i = 0; i<rl0->cur_entries; i++) {
		if (rl0->entries[i] == p0) {
			n = i;
			break;
		}
	}
	if (rl.entries) free(rl.entries);
	rl.entries = NULL;
	if (rl.lens) free(rl.lens);
	rl.lens = NULL;
	rl0->reordered = reordered;
	req_page_table_free();
	return n;
}


/* indicates the function which request_strings() should call upon CLOSEDOC_A */

static int (*rs_closedoc)(int n) = NULL;

/* indicates the correct function to call to restore the status bar after
   suspend/resume, particularly during requesters. */

void (*resume_status_bar)(const char *message);
static bool resume_bar = false;


/* Given a list of strings, let the user pick one. If _rl->suffix is not '\0', bold names ending with it.
   The integer returned is one of the following:
   n >= 0  User selected string n with the enter key.
   -1      Error or abort; no selection made.
   -n - 2  User selected string n with the TAB key.
   (Yes, it's kind of evil, but it's nothing compared to what request() does!) */

int request_strings(req_list *rlp0, int n) {

	assert(rlp0->cur_entries > 0);

	int ne_lines0 = 0, ne_columns0 = 0;
	bool reordered = false;
	X = R = C = page = fuzz_len = 0;
	if ( ! request_strings_init(rlp0) ) return ERROR;

   if ( ! reset_req_pages(0) ) {
		request_strings_cleanup(reordered);
		return ERROR;
	}

	while(true) {
		if (ne_lines0 != ne_lines || ne_columns0 != ne_columns || resume_bar) {
			if (ne_lines0 && ne_columns0 ) {
				n = PCR2N(page, C, R);
				reset_req_pages(0);
			}
			ne_lines0 = ne_lines;
			ne_columns0 = ne_columns;
			N2PCRX(n, page, C, R, X);
			print_strings();
			if (resume_bar) {
				resume_status_bar(NULL);
				resume_bar = false;
			}
		}

		n = PCR2N(page, C, R);
		N2X(n,X);

		assert(fuzz_len >= 0);

		fuzz_len = min(fuzz_len, strlen(rl.entries[n]));

		move_cursor(R, X + fuzz_len);

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
						if (C != 0) C = 0;
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
							int n0 = rs_closedoc(PCR2N(page, C, R));
							page = -1;
							normalize(n0);
						}
						break;

					/* Keystrokes that open requesters toggle them closed also. */
					case FIND_A:
						if (a == FIND_A && !rl.find_quits) break;
					case HELP_A:
						if (a == HELP_A && !rl.help_quits) break;
					case SELECTDOC_A:
						if (a == SELECTDOC_A && !rl.selectdoc_quits) break;
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



/* The filename completion function. Returns NULL if no file matches start_prefix,
   or the longest prefix common to all files extending start_prefix. */

char *complete_filename(const char *start_prefix) {

	/* This might be NULL if the current directory has been unlinked, or it is not readable.
	   In that case, we end up moving to the completion directory. */
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


/* This is the callback function for the SelectDoc requester's CloseDoc action.
   "n" is the index into rl.entries of the "char *p" corresponding to an
   entry in rl0->entries, the index "o" of which -- possibly via
   rl0->orig_order -- corresponds to the index of the buffer we want
   to close if it is unmodified. */

static int handle_closedoc(int n) {

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

	if (PCR2N(page, C, R) == rl.cur_entries - 1 && rl.cur_entries > 1)
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
		rl.selectdoc_quits = true;
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


/* The req_list_* functions below provide a simple mechanism suitable for building
   request lists for directory entries, syntax recognizers, etc. */

/* These are the default allocation sizes for the entry array and for the
   name array when reading a directory. The allocation sizes start with these
   values, and they are doubled each time more space is needed. This ensures a
   reasonable number of retries. */

#define DEF_ENTRIES_ALLOC_SIZE     256
#define DEF_CHARS_ALLOC_SIZE  (4*1024)


/* Delete the nth string from the given request list.
   This will work regardless of whether the req_list has been finalized. */

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
	memmove(&rl->entries[nth], &rl->entries[nth+1], sizeof(char *) * (rl->cur_entries - nth));
	memmove(&rl->lens[nth],    &rl->lens[nth+1],    sizeof(int) *    (rl->cur_entries - nth));
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
	if (rl->lens) free(rl->lens);
	rl->lens = NULL;
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
	rl->find_quits = false;
	rl->help_quits = false;
	rl->selectdoc_quits = false;
	rl->suffix = suffix;
	rl->cur_entries = rl->alloc_entries = rl->max_entry_len = 0;
	rl->cur_chars = rl->alloc_chars = 0;
	if (rl->entries = malloc(sizeof(char *) * DEF_ENTRIES_ALLOC_SIZE)) {
		if (rl->chars = malloc(sizeof(char) * DEF_CHARS_ALLOC_SIZE)) {
			if (rl->lens = malloc(sizeof(int) * DEF_ENTRIES_ALLOC_SIZE)) {
				/* lens will track alloc_entries, so we don't have to track it separately. */
				rl->alloc_entries = DEF_ENTRIES_ALLOC_SIZE;
				rl->alloc_chars = DEF_CHARS_ALLOC_SIZE;
				return OK;
			}
			free(rl->chars);
		}
		free(rl->entries);
	}
	rl->chars = NULL;
	rl->entries = NULL;
	rl->lens = NULL;
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
	/* until now, entries and suffixes have been stored as: 'a' 'b' 'c' '\0' suffix '\0'
	   Morph that into                                      'a' 'b' 'c' suffix '\0' '\0' */
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
   by strcmp if there is no comparison function), then the conflicting entry
   is returned. Otherwise, the new entry is returned. On error, NULL is returned.*/

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

	/* make enough slots to hold the string pointer and lengths */
	if (rl->cur_entries >= rl->alloc_entries) {
		char **newentries;
		int *newlens, orig_alloc_entries = rl->alloc_entries;
		if (newentries = realloc(rl->entries, sizeof(char *) * (rl->alloc_entries * 2 + 1))) {
			rl->alloc_entries = rl->alloc_entries * 2 + 1;
			rl->entries = newentries;
			if (newlens = realloc(rl->lens, sizeof(int) * (orig_alloc_entries * 2 + 1))) {
				rl->lens = newlens;
			} else if (newentries = realloc(rl->entries, sizeof(char *) * orig_alloc_entries)) {
				rl->entries = newentries;
				rl->alloc_entries = orig_alloc_entries;
				return NULL;
			} else {
				rl->alloc_entries = orig_alloc_entries; /* not true, but it's the min(entries,lens) */
				return NULL;
			}
		} else return NULL;
	}

	char * const newstr = &rl->chars[rl->cur_chars];
	char * p = strcpy(newstr, str)+len+1;
	if (rl->suffix && suffix) *p++ = rl->suffix;
	*p = '\0';
	rl->cur_chars += lentot;
	if (ins < rl->cur_entries) {
		memmove(&rl->entries[ins+1], &rl->entries[ins], sizeof(char *) * (rl->cur_entries - ins));
		memmove(&rl->lens[ins+1],    &rl->lens[ins],    sizeof(int)    * (rl->cur_entries - ins));
	}
	rl->entries[ins] = newstr;
	rl->lens[ins] = lentot;
	rl->cur_entries++;
	return newstr;
}
