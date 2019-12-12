/* Requester handling.

   Copyright (C) 1993-1998 Sebastiano Vigna
   Copyright (C) 1999-2019 Todd M. Lewis and Sebastiano Vigna

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

/* How to build and use a req_list:
     req_list_init()
       loop:  req_list_add()
              req_list_del()
       req_list_finalize()
       request_strings()
     req_list_free()

   req_list_init()     initializes a req_list structure.

   req_list_add()      is called once for every string you wish to include in
                       the request. If during the process of adding entries to
                       the req_list you decide to remove any previously added
                       entry, you should use req_list_del() to remove it.

   req_list_finalize() must be called after all the entries have been added to
                       the req_list but before request_strings(). It takes care
                       of some final housekeeping duties to prepare the
                       req_list for use by request_strings();

   request_strings()   prompts the user to choose one from among possibly many
                       strings in the req_list. If string n was selected with
                       RETURN, n is returned; if string n was selected with
                       TAB, -n-2 is returned. On escaping, ERROR is returned.

   req_list_free()     must be called on any req_list that has been initialized
                       with req_list_init().

   We rely on a set of auxiliary functions and a few static variables:
     req_order
     X, R, C, page, fuzz_len
     rl, rl0
*/

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

#define BY_COLUMN         (req_order)
#define BY_ROW            (!BY_COLUMN)

/* X and Y constitute a screen coordinate derived from (C,R) which varies from page to page.
     (Y, being a synonym for R, is implemented as a macro.)
   R and C are the row and column of the currently selected entry.
   page is the index into req_page_table.req_page.
   fuzz_len is the length of the matching prefix into our current entry. */
static int X, R, C, page, fuzz_len;
#define Y (R)

#define NAMES_PER_ROW(p)     (req_page_table.req_page[(p)].cols)
#define NAMES_PER_COL(p)     (req_page_table.req_page[(p)].rows)
#define N2PCRX(n,p,c,r,x)    (n2pcrx((n),&(p),&(c),&(r),&(x)))
#define N2P(n)               (n2pcrx((n),NULL,NULL,NULL,NULL))
#define N2X(n,x)             (n2pcrx((n),NULL,NULL,NULL,&(x)))
#define PCR2N(p,c,r)         (pcr2n((p),(c),(r)))
#define DX(d)                (dxd((d)))
#define DY(d)                (dyd((d)))


/* The master req_list that gets built prior to calling request_strings() has its own
   copy of each of the strings. request() builds a working copy of the master
   req_list. This working req_list doesn't duplicate the master's strings, but
   it does have an array of pointers to some of those strings. The working
   req_list's array may exclude some entries if progressive search (rl.prune) is
   turned on (via the Insert key), and some entries may be out of order
   relative to those in the master req_list because of F2/F3 reordering during
   SelectDoc, in which case the master req_list keeps a mapping between the
   original order and the displayed order.

   The req_list_del() function removes an item from the req_list. It is the
   counterpart to req_list_add() which we use for building the master req_list
   one item at a time. req_list_del() can be called either while building the
   req_list, or after the req_list has been finalized. It's used when we close
   a document with the SelectDoc req_list showing to remove that closed
   document's entry from the master req_list. Coordinating changes between the
   working req_list and the master req_list when deleting an entry from the
   master req_list requires some of the string data of the master req_list to be
   shifted. Because the working list doesn't have its own copy of the strings,
   there's no character data that needs shifting for the working req_list.
   However, the working req_list does have its own list of pointers into that
   data, and any of them pointing into the shifted character memory needs to be
   adjusted appropriately. */

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
		if (page->col_width[col] < rl.lengths[n0 + i]) {
			combined_col_widths += rl.lengths[n0 + i] - page->col_width[col];
			page->col_width[col] = rl.lengths[n0 + i];
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
	memset(&req_page_table.req_page[cur_page], 0, (req_page_table.pages - cur_page) * sizeof(req_page_t));
	int max_cols = min(ne_columns / 3, MAX_REQ_COLS);
	int max_rows = ne_lines - 1;
	int cols;
	while (first < rl.cur_entries) {
		if (cur_page >= req_page_table.pages && req_page_table_expand() == 0) return 0;
		for (cols = max_cols; cols > 0; cols--)
			if (fit_page(first, min(cols*max_rows, rl.cur_entries - first), cols, max_rows, &req_page_table.req_page[cur_page])) break;
		if (cols==0) return 0; /* This should never happen. */
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
			cols = needed_entries / rows + (needed_entries % rows ? 1 : 0);
			if (cols >= MAX_REQ_COLS) continue;
			if (fit_page(first, needed_entries, cols, rows, pp)) {
				if (rows == max_rows || sum_ints(pp->cols, pp->col_width) * 1000 / pp->rows < ne_columns * 1000 / (ne_lines - 1)) {
					int good_rows = rows;
					while (good_rows > 1 && cols * (good_rows - 1) >= needed_entries && fit_page(first, needed_entries, cols, good_rows - 1, pp))
						good_rows--;
					fit_page(first, needed_entries, cols, good_rows, pp);
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


static int common_prefix_len(req_list *cpl_rl) {
	char * const p0 = cpl_rl->entries[0];
	int len = strlen(p0);
	for (int i = 0; len && i < cpl_rl->cur_entries; i++) {
		char * const p1 = cpl_rl->entries[i];
		for ( ; len && strncasecmp(p0, p1, len); len--)
			;
	}
	return len;
}


/* Given that the req_list entries in rl may be reordered WRT *rl0,
   return the original index corresponding to the new index "n". */
static int reverse_reorder(const int n) {
	if (!rl0->allow_reorder) return n;
	for (int i = 0; i < rl0->cur_entries; i++)
		if (rl0->reorder[i] == n) return i;
	return n; /* Should never happen! */
}

#ifdef DEBUGPRINTF
static void dump_reorder() {
	if (rl0->allow_reorder) {
		fprintf(stderr,"===================================\n");
		for (int i = 0; i < rl0->cur_entries; i++) {
			fprintf(stderr,"%d:%d %s | %s\n", i, rl0->reorder[i],
			   rl0->entries[i],
			   i < rl.cur_entries ? rl.entries[i] : "");
		}
		fprintf(stderr,"-----------------------------------\n");
		fflush(NULL);
	}
}

static void dump_rl_chars(req_list * const drc_rl) {
	char *c = drc_rl->chars;
	int entry;
	fprintf(stderr,"===================================\n");
	for (entry = 0; entry < drc_rl->cur_entries; entry++) {
		char *p = drc_rl->entries[entry];
		if (c != p) {
			fprintf(stderr,"dump_rl_chars: c(%s) != p(%s); aborting\n", c, p);
			return;
		}
		fprintf(stderr, "dump_rl_chars: ");
		while (*c) fprintf(stderr," %c", *c++);
		fprintf(stderr, " \\0"); c++;
		while (*c) fprintf(stderr," %c", *c++);
		fprintf(stderr, " \\0\n"); c++;
	}
	fprintf(stderr,"-----------------------------------\n");
	fflush(NULL);
}
#endif


/* This is the printing function used by the requester. It prints the
   strings from the entries array existing in "page". */

static void print_strings() {
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
	D(dump_reorder();)
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


static void request_move_inc_down(void) {
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
   dir should be either 1 or -1.
   Rearranges pointers in rl.cur_entries, and
   updates the reorder array in *rl0.
   Note: under no circumstances do the original rl0->entries[] change order. */

static bool request_reorder(const int dir) {
	if (! rl0->allow_reorder || rl.cur_entries < 2) return false;

	/* p0 and p1 point to the strings we want to swap */
	/* n0 and n1 are the indicies in   rl.entries[] for p0 and p1. (Also for rl.lengths[].) */
	/* i0 and i1 are the indicies in rl0->entries[] for p0 and p1 */
	const int n0 = PCR2N(page, C, R);
	const int n1 = (n0 + dir + rl.cur_entries ) % rl.cur_entries; /* Allows wrap around. */

	char * const p0 = rl.entries[n0];
	char * const p1 = rl.entries[n1];

	int i0, i1, i;
	for (i = 0, i0 = i1 = -1; i < rl0->cur_entries && (i0 < 0 || i1 < 0); i++) {
		if (i0 < 0 && p0 == rl0->entries[i] ) {
			i0 = i;
		}
		if (i1 < 0 && p1 == rl0->entries[i] ) {
			i1 = i;
		}
	}

   i = rl0->reorder[i0];
	rl0->reorder[i0] = rl0->reorder[i1];
	rl0->reorder[i1] = i;

	rl.entries[n0] = p1;
	rl.entries[n1] = p0;

	i = rl.lengths[n0];
	rl.lengths[n0] = rl.lengths[n1];
	rl.lengths[n1] = i;

	rl0->reordered = rl.reordered = true;
	reset_req_pages(min(n0,n1));
	page = -1; /* causes normalize() to call print_strings() */
	normalize(n1);
	return true;
}


/* rebuild rl.entries[] from rl0->entries[].
   If rl.prune is true, include only those entries from rl0 which match the currently
   highlighted rl entry up through fuzz_len characters.
   If rl.prune is false, include all entries from rl0.
   In any case, return the (possibly new) index of the highlighted entry in rl. */

static int rebuild_rl_entries() {
	const char * const p0 = rl.entries[PCR2N(page, C, R)];
	int i, j, n;
	for (i = j = n = 0; i < rl0->cur_entries; i++) {
		int orig = reverse_reorder(i);
		char * const p1 = rl0->entries[orig];
		D(fprintf(stderr, "rre: i:%d, j:%d, n:%d p0:%x p1:%x fuzz_len:%d rl.prune:%d\n",
		       i, j, n, p0, p1, fuzz_len, rl.prune);)
		if ( ! rl.prune || ! strncasecmp(p0, p1, fuzz_len) ) {
			if (p1 == p0) {
				n = j;
				D(fprintf(stderr, "rre: set n <- j (%d)\n", j);)
			}
			rl.entries[j] = p1;
			rl.lengths[j++] = rl0->lengths[orig];
			D(fprintf(stderr, "rre: set rl.entries[%d] <- %x ('%s')\n", j-1, p1, p1);)
			D(fprintf(stderr, "rre: set rl.lengths[%d] <- rl0->lengths[%d] (%d)\n", j-1, orig, rl0->lengths[orig]);)
		}
	}
	rl.cur_entries = j;
	reset_req_pages(0);
	return n;
}


/* Count how many entries match our highlighted entry up through len characters. */

static int count_fuzz_matches(const int len) {
	const int n0 = PCR2N(page, C, R);
	const char * const p0 = rl.entries[n0];
	if (len <= 0) return rl0->cur_entries;
	if (len > strlen(p0)) return 1;
	int i, c;
	for (i = c = 0; i < rl0->cur_entries; i++) {
		if (p0 == rl0->entries[i] || ! strncasecmp(p0, rl0->entries[i], len)) c++;
	}
	return c;
}


/* Shift fuzz_len by +1 or -1 until the matching count changes or we run out of string.
   It's more subtle than that actually. Going left (-1), we want to stop on the first
   fuzz_len that has a different count_fuzz_matches() than the initial position. But
   going right (+1), we want to stop on the last fuzz_len that has the same
   count_fuzz_matches() as the initial position. In either case, the "gap" to the right
   of fuzz_len constitutes a transition. */

static void shift_fuzz(const int d) {
	const char * const p0 = rl.entries[PCR2N(page, C, R)];
	assert(d == 1 || d == -1);
	int initial_fuzz_matches = count_fuzz_matches(fuzz_len);
	if (d == -1) {
		while(fuzz_len > 0 && count_fuzz_matches(fuzz_len) == initial_fuzz_matches) fuzz_len--;
	} else {
		while (fuzz_len < strlen(p0) + 1 && count_fuzz_matches(fuzz_len + 1) == initial_fuzz_matches) fuzz_len++;
	}
}


/* Back up one or more chars from fuzz_len, possibly pulling in
   matching entries from the original req_list *rl0 in such a way as
   to preserve original order. This can behave quite differently
   depending on the value of 'rl.prune'. If true, we disregard the
   current subset of entries and rely only on matching fuzz_len
   characters. If false, we keep the current subset while still
   pulling in matches from rl0. */

static void fuzz_back() {
	const int n0 = PCR2N(page, C, R);
	N2PCRX(n0, page, C, R, X);
	if (fuzz_len == 0) return;
	shift_fuzz(-1);

	int n1 = rebuild_rl_entries();
	reset_req_pages(0);
	page = -1; /* causes normalize() to call print_strings() */
	normalize(n1);
}


/* given a localised_up_case character c, find the next entry that
   matches our current fuzz_len chars plus this new character. The
   behavior is quite different depending on 'rl.prune'. If true, we keep
   only entries that match our current fuzz_len prefix plus this
   additional character while preserving the relative order of
   rl.entries[]. If false, we keep all the current entries, and only
   (possibly) change the selected one. */

static void fuzz_forward(const int c) {
	const int n0 = PCR2N(page, C, R);
	const char * const p0 = rl.entries[n0];

	assert(fuzz_len >= 0);

	if (rl.prune) {
		int i = 0, n1 = 0;
		for (int j = 0; j < rl.cur_entries; j++) {
			char * const p1 = rl.entries[j];
			int len = rl.lengths[j];
			const int cmp = strncasecmp(p0, p1, fuzz_len);
			if (! cmp && strlen(p1) > fuzz_len && localised_up_case[(unsigned char)p1[fuzz_len]] == c) {
				if (p1 == p0)
					n1 = i;
				rl.entries[i] = p1;
				rl.lengths[i++] = len;
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
		for (int n = n0, i = rl.cur_entries; i; i--, n = (n + 1) % rl.cur_entries) {
			char * const p1 = rl.entries[n];
			const int cmp = strncasecmp(p0, p1, fuzz_len);
			if (!cmp && strlen(p1) > fuzz_len && localised_up_case[(unsigned char)p1[fuzz_len]] == c) {
				N2PCRX(n, page, C, R, X);
				fuzz_len++;
				shift_fuzz(1);
				page = -1;
				normalize(n);
				break;
			}
		}
	}
}


/* request_strings_init() is only ever called by request_strings().

   request_strings_init() takes the original master list of strings, described
   by *rlp0, and makes a working copy described by the static rl. This working
   copy has an allocated buffer large enough to hold a copy of all the master's
   original char pointers, but at any time may have fewer entries due to fuzzy
   matching (prune) and entry deletions. The counterpart to
   request_strings_init() is request_strings_cleanup() which cleans up the
   allocations. It too is only ever called by request_strings(). */

static int request_strings_init(req_list *rlp0) {
	rl.cur_entries = rlp0->cur_entries;
	rl.suffix = rlp0->suffix;
	if (!(rl.entries = calloc(rlp0->cur_entries, sizeof(char *)))) return 0;
	if (!(rl.lengths = calloc(rlp0->cur_entries, sizeof(int)))) {
		free(rl.entries);
		return 0;
	}
	rl.alloc_entries = rlp0->cur_entries;
	memcpy(rl.entries, rlp0->entries, rl.cur_entries * sizeof(char *));
	memcpy(rl.lengths, rlp0->lengths, rl.cur_entries * sizeof(int));
	rl0 = rlp0;
	rl.allow_dupes     = rl0->allow_dupes;
	rl.allow_reorder   = rl0->allow_reorder;
	rl.ignore_tab      = rl0->ignore_tab;
	rl.reordered       = rl0->reordered;
	rl.find_quits      = rl0->find_quits;
	rl.help_quits      = rl0->help_quits;
	rl.selectdoc_quits = rl0->selectdoc_quits;
	rl.prune           = rl0->prune;
	/* rl doesn't have its own allocated characters; critically, its entries point
	   to allocations that belong to *rlp0, a.k.a the static *rl0. */
	rl.cur_chars = rl.alloc_chars = 0;
	rl.chars = NULL;
	fuzz_len = common_prefix_len(&rl);
	return rl.cur_entries;
}


static int request_strings_cleanup() {
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
	if (rl.lengths) free(rl.lengths);
	rl.lengths = NULL;
	req_page_table_free();
	return n;
}


/* indicates the function which request_strings() should call upon CLOSEDOC_A */

static int (*rs_closedoc)(int n) = NULL;

/* indicates the correct function to call to restore the status bar after
   suspend/resume, particularly during requesters. Note: not static, as
   this is used by interrupt handlers elsewhere in the source code. */

void (*resume_status_bar)(const char *message);
static bool resume_bar = false;


/* Present a list of strings for the user to select one from. If _rl->suffix is
   not '\0', bold names ending with it. The integer returned is one of the
   following:

     n >= 0  User selected string n with the enter key.
     -1      Error or abort; no selection made.
     n < -1  User selected string -n - 2 with the TAB key.

   (Yes, it's kind of evil, but it's nothing compared to what request() does!)
   If reordering occurred, the indicated number is the original index of the
   entry (modulo deletions). The index into the reordered entries would be
   rlp0->reorder[n]. */

int request_strings(req_list *rlp0, int n) {

	assert(rlp0->cur_entries > 0);

	int ne_lines0 = 0, ne_columns0 = 0;
	X = R = C = page = fuzz_len = 0;
	if ( ! request_strings_init(rlp0) ) return ERROR;

	if ( ! reset_req_pages(0) ) {
		request_strings_cleanup();
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
					n = request_strings_cleanup();
					if (n >= rlp0->cur_entries) return ERROR;
					else return -n - 2;
				}
				break;

			case RETURN:
				n = request_strings_cleanup();
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
						request_reorder(1);
						break;

					case PREVDOC_A:
						request_reorder(-1);
						break;

					case INSERT_A:
					case DELETECHAR_A:
						rl0->prune = rl.prune = !rl.prune;
						int n1 = rebuild_rl_entries();
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
						request_strings_cleanup();
						return -1;
					}
				}
				break;

			default:
				break;
		}
	}
}


static void load_syntax_names(req_list *lsn_rl, DIR *d, int flag) {

	const int extlen = strlen(SYNTAX_EXT);
	stop = false;

	for( struct dirent *de; !stop && (de = readdir(d)); ) {
		if (is_directory(de->d_name)) continue;
		const int len = strlen(de->d_name);
		if (len > extlen && !strcmp(de->d_name+len - extlen, SYNTAX_EXT)) {
			char ch = de->d_name[len-extlen];
			de->d_name[len-extlen] = '\0';
			if (!req_list_add(lsn_rl, de->d_name, flag)) break;
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
	req_list rs_rl;
	DIR *d;

	if (req_list_init(&rs_rl, filenamecmp, false, false, '*') != OK) return NULL;
	if ((p = exists_prefs_dir()) && strlen(p) + 2 + strlen(SYNTAX_DIR) < sizeof syn_dir_name) {
		strcat(strcpy(syn_dir_name, p), SYNTAX_DIR);
		if (d = opendir(syn_dir_name)) {
			load_syntax_names(&rs_rl, d, true);
			closedir(d);
		}
	}
	if ((p = exists_gprefs_dir()) && strlen(p) + 2 + strlen(SYNTAX_DIR) < sizeof syn_dir_name) {
		strcat(strcpy(syn_dir_name, p), SYNTAX_DIR);
		if (d = opendir(syn_dir_name)) {
			load_syntax_names(&rs_rl, d, false);
			closedir(d);
		}
	}
	req_list_finalize(&rs_rl);
	p = NULL;
	int result;
	if (rs_rl.cur_entries && (result = request_strings(&rs_rl, 0)) != ERROR) {
		char * const q = rs_rl.entries[result >= 0 ? result : -result - 2];
		if (p = malloc(strlen(q)+3)) {
			strcpy(p, q);
			if (p[strlen(p)-1] == rs_rl.suffix) p[strlen(p)-1] = '\0';
			if (result < 0) {
				memmove(p + 1, p, strlen(p) + 1);
				p[0] = '\0';
			}
		}
	}
	req_list_free(&rs_rl);
	return p;
}


/* This is the file requester. It reads the directory in which the filename
   lives, builds an array of strings and calls request_strings(). If a
   directory name is returned, it enters the directory and the process just
   described is repeated for that directory. Returns NULL on error or escaping,
   a pointer to the selected filename if RETURN is pressed, or a pointer to a
   NUL char '\0' followed by the selected filename (or directory) if TAB is
   pressed (so by checking whether the first character of the returned string
   is NUL you can check which key the user pressed). */

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

	req_list rf_rl;
	bool next_dir;
	char *result = NULL;
	do {
		next_dir = false;
		if (req_list_init(&rf_rl, filenamecmp, true, false, '/') != OK) break;

		DIR * const d = opendir(CURDIR);
		if (d) {

			stop = false;
			for(struct dirent * de; !stop && (de = readdir(d)); ) {
				const bool is_dir = is_directory(de->d_name);
				if (use_prefix && !is_prefix(file_part(filename), de->d_name)) continue;
				if (!req_list_add(&rf_rl, de->d_name, is_dir)) break;
			}

			req_list_finalize(&rf_rl);

			if (rf_rl.cur_entries) {
				const int t = request_strings(&rf_rl, 0);
				if (t != ERROR) {
					char * const p = rf_rl.entries[t >= 0 ? t : -t - 2];
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
		req_list_free(&rf_rl);
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
   "n" is the index into rl.entries of the "char *p" corresponding to an entry
   in rl0->entries, the index "o" of which corresponds to the index of the
   buffer we want to close IFF it is unmodified. */

static int handle_closedoc(int n) {

	const char *p = rl.entries[n];

	int o;
	for (o = 0; o < rl0->cur_entries && rl0->entries[o] != p; o++) /* empty loop */ ;

	if (o == rl0->cur_entries) return n; /* This should never happen. */

	buffer *bp = get_nth_buffer(o);

	/* We don't close modified buffers here, nor the last buffer. */
	if (!bp || bp->is_modified || rl0->cur_entries == 1) return n;

	/* We've determined we are going to close document *bp. */

	/* Ensure we'll still have an entry in view after closing *bp. */
	if (rl.cur_entries == 1 && rl0->cur_entries > 1) {
		fuzz_back(); /* "*p" is still valid, but "n" isn't.*/
	}

	for (int i = 0, j = 0; i < rl.cur_entries; i++) {
		char * const entry = rl.entries[i];
		int len = rl.lengths[i];
		if (p == entry) n = i;
		else {
			rl.entries[j] = entry;
			rl.lengths[j++] = len;
		}
	}
	/* n is valid again as the index in rl.entries[] that we just dropped.
	   If it was the last entry, n == rl.cur_entries. */
	rl.cur_entries--;
	fuzz_len = common_prefix_len(&rl);

	/* This is the only time we change rl0->entries[] or the character buffer rl0->chars.
	   shift_len will be the number of bytes by which the end of rl0->chars was shifted
	   to remove entry o. */
	int shift_len = req_list_del(rl0, o);

	for (int i = 0; i < rl.cur_entries; i++)
		if (rl.entries[i] >= p)
			rl.entries[i] -= shift_len;

	/* rebuild_rl_entries() is not necessary, as dropping an entry will never
	   necessitate pulling in new entries into rl.entries[] from rl0->entries[]. */
	reset_req_pages(0);
	n = min(n, rl.cur_entries - 1);
	normalize(n);

	buffer *nextb = (buffer *)bp->b_node.next;

	rem(&bp->b_node);
	free_buffer(bp);

	if (! nextb->b_node.next) nextb = (buffer *)buffers.head;

#if 0
	/* As we don't close the last remaining document through this requester,
	   this code is irrelevant. But leave it here in the quite likely event that
	   decision is revisited. */
	if (nextb == (buffer *)&buffers.tail) {
		close_history();
		unset_interactive_mode();
		exit(0);
	}
#endif

	if (bp == cur_buffer) cur_buffer = nextb;

	return n;
}


/* Presents to the user a list of the documents currently available.  It
   returns the number of the document selected, or -1 on escape or error.
   It may reorder the document list, and may close any unmodified document
   up to but not including the last remaining document. */

int request_document(void) {
	static int rd_prune = false;
	int i = -1;
	req_list rd_rl;
	buffer *b = (buffer *)buffers.head;

	if (b->b_node.next && req_list_init(&rd_rl, NULL, true, true, '*')==OK) {
		i = 0;
		int cur_entry = 0;
		while(b->b_node.next) {
			if (b == cur_buffer) cur_entry = i;
			req_list_add(&rd_rl, b->filename ? b->filename : UNNAMED_NAME, b->is_modified);
			b = (buffer *)b->b_node.next;
			i++;
		}
		rd_rl.ignore_tab = true;
		rd_rl.selectdoc_quits = true;
		rd_rl.prune = rd_prune;
		req_list_finalize(&rd_rl);
		print_message(info_msg[SELECT_DOC]);
		rs_closedoc = &handle_closedoc;

		i = request_strings(&rd_rl, cur_entry);
		/* i is the index into the local rd_rl.entries[] array, which may be shorter
		   than it started due to closing documents, and the remaining document
		   names may have been reordered. */

		rs_closedoc = NULL;
		rd_prune = rd_rl.prune;
		if (i >= 0 && rd_rl.reordered) {
			/* The array of pointers at rd_rl.entries[] is no longer needed to
			   reference strings, but as it's large enough to hold all the buffer
			   pointers, and we need a place to temporary store these buffer
			   pointers while we reorder them, we repurpose rd_rl.entries as a
			   buffer pointer array. */
			b = (buffer *)buffers.head;
			for (int j = 0; b->b_node.next; j++ ) {
				D(fprintf(stderr,"rqd: j:%d '%s'\n", j, b->filename ? b->filename : UNNAMED_NAME);)
				rd_rl.entries[rd_rl.reorder[j]] = (char *)b;
				b = (buffer *)b->b_node.next;
				rem(b->b_node.prev);
			}
			/* We're rem()'d all our buffers from the buffer list. rd_rl.entries[]
			   now contains pointers to each buffer in their intended final order. */
			for (int j = 0; j < rd_rl.cur_entries; j++) {
				b = (buffer *)rd_rl.entries[j];
				D(fprintf(stderr,"rqd: add_tail %d ('%s')\n", j, b->filename ? b->filename : UNNAMED_NAME);)
				add_tail(&buffers, (node *)rd_rl.entries[j]);
			}
			D(fprintf(stderr,"i:%d -> %d\n", i, rd_rl.reorder[i]);)
			i = rd_rl.reorder[i];
		}
		reset_window();
		draw_status_bar();

		req_list_free(&rd_rl);
	}

	return i;
}


/* The req_list_* functions below provide a mechanism suitable for building
   request lists for directory entries, syntax recognizers, autocompletions, etc. */

/* These are the default allocation sizes for the entry array and for the
   name array when reading a directory. The allocation sizes start with these
   values, and they are doubled each time more space is needed. This ensures a
   reasonable number of retries. */

#define DEF_ENTRIES_ALLOC_SIZE     256
#define DEF_CHARS_ALLOC_SIZE   (4*1024)

/* Delete the nth string from the given request list. This will work regardless
   of whether the req_list has been finalized. Returns the length of the shift.

   Note: should not be called on the working static req_list lr, as it doesn't
   contain it's own character buffer. */

int req_list_del(req_list * const rld, int nth) {

	if (nth < 0 || nth >= rld->cur_entries ) return ERROR;
	char * const str = rld->entries[nth];
	const int len0 = strlen(str);
	int len = len0;

	len += str[len + 1] ? 3 : 2;  /* 'a b c \0  * \0' or
	                                 'a b c \0 \0'    or
	                                 'a b * \0 \0'    depending on whether req_list_finalize() has been called. */
	memmove(str, str + len, sizeof(char)*(rld->alloc_chars - (str + len - rld->chars)));
	rld->cur_chars -= len;

	for(int i = 0; i < rld->cur_entries; i++)
		if (rld->entries[i] >= str )
			rld->entries[i] -= len;

	if (rld->reorder) {
		int val = rld->reorder[nth];
		for (int i = 0, j = 0; i < rld->cur_entries; i++, j++) {
			if (i == nth) j--;
			else rld->reorder[j] = (rld->reorder[i] < val) ? rld->reorder[i] : rld->reorder[i] - 1;
		}
	}

	memmove(&rld->entries[nth], &rld->entries[nth+1], sizeof(char *) * (rld->cur_entries - nth));
	memmove(&rld->lengths[nth], &rld->lengths[nth+1], sizeof(int)    * (rld->cur_entries - nth));
	rld->cur_entries--;

	return len;
}

void req_list_free(req_list * const rlf) {
	if (rlf->entries) free(rlf->entries);
	rlf->entries = NULL;
	if (rlf->chars) free(rlf->chars);
	rlf->chars = NULL;
	if (rlf->reorder) free(rlf->reorder);
	rlf->reorder = NULL;
	if (rlf->lengths) free(rlf->lengths);
	rlf->lengths = NULL;
	rlf->allow_reorder = false;
	rlf->cur_entries = rlf->alloc_entries = 0;
	rlf->cur_chars = rlf->alloc_chars = 0;
}


/* Initialize a request list.

   A comparison function cmpfnc may be provided; if it is provided, that
   function will be used to keep the entries sorted. If NULL is provided
   instead, entries are kept in the order they are added.

   The boolean allow_dupes determines whether duplicate entries are allowed. If
   not, and if cmpfnc is NULL, then each addition requires a linear search over
   the current entries.

   The boolean allow_reorder, when set, enables the user to move the highlighted
   entry forward or backward in the list of entries by use of the NextDoc and
   PrevDoc commands (invoked normally by the F2 and F3 keys). In this case, you
   will need to consult the rl->reorder map upon return to determine the
   indicated new order for entries.

   If a suffix character is provided, it can optionally be added to individual
   entries as they are added to the req_list. Entries thus marked will be
   highlighted when displayed. Choose carefully, as entries which naturally end
   with said character are indistinguishable from marked entries. */

int req_list_init( req_list * const rli, int cmpfnc(const char *, const char *), const bool allow_dupes, const bool allow_reorder, const char suffix) {
	rli->cmpfnc = cmpfnc;
	rli->allow_dupes = allow_dupes;
	rli->allow_reorder = allow_reorder;
	rli->ignore_tab = false;
	rli->prune = false;
	rli->find_quits = false;
	rli->help_quits = false;
	rli->selectdoc_quits = false;
	rli->suffix = suffix;
	rli->cur_entries = rli->alloc_entries = 0;
	rli->cur_chars = rli->alloc_chars = 0;
	if (rli->entries = malloc(sizeof(char *) * DEF_ENTRIES_ALLOC_SIZE)) {
		if (rli->chars = malloc(sizeof(char) * DEF_CHARS_ALLOC_SIZE)) {
			if (rli->lengths = malloc(sizeof(int) * DEF_ENTRIES_ALLOC_SIZE)) {
				/* lengths will track alloc_entries, so we don't have to track it separately. */
				rli->alloc_entries = DEF_ENTRIES_ALLOC_SIZE;
				rli->alloc_chars = DEF_CHARS_ALLOC_SIZE;
				return OK;
			}
			free(rli->chars);
		}
		free(rli->entries);
	}
	rli->chars = NULL;
	rli->entries = NULL;
	rli->lengths = NULL;
	return OUT_OF_MEMORY;
}


/* req_list strings are stored with a trailing '\0', followed by an optional
   suffix character, and an additional trailing '\0'. This allows comparing
   strings w/o having to consider the optional suffixes while adding entries to
   the req_list. Finalizing the req_list effectively shifts the suffixes left,
   exchanging them for the preceeding '\0'. After this operation, all the
   strings will be just normal C strings, some of which happen to end with the
   suffix character, and all of which are followed by two null bytes.

   req_list_finalize() also initializes the reorder array IFF allow_reorder
   is true. If the array cannot be allocated, allow_reorder is simply reset to
   false rather than returning an error. But if your system is this tight on
   RAM, you've got bigger problems on the way. */

void req_list_finalize(req_list * const rlf) {
	/* until now, entries and suffixes have been stored as: 'a' 'b' 'c' '\0' suffix '\0'
	   Morph that into                                      'a' 'b' 'c' suffix '\0' '\0' */
	for (int i = 0; i < rlf->cur_entries; i++) {
		const int len = strlen(rlf->entries[i]);
		*(rlf->entries[i]+len) = *(rlf->entries[i]+len+1);
		*(rlf->entries[i]+len+1) = '\0';
	}
	rlf->reorder = NULL;
	if (rlf->allow_reorder ) {
		if ( rlf->reorder = malloc(sizeof(int) * rlf->cur_entries)) {
			for (int i = 0; i < rlf->cur_entries; i++)
				rlf->reorder[i] = i;
		}
		else rlf->allow_reorder = false;
	}
}


/* Add a string plus an optional suffix to a request list. We really add two
   null-terminated strings: the actual entry, and a possibly empty suffix.
   These pairs should be merged later by req_list_finalize(). If duplicates are
   not allowed (see req_list_init()) and the str already exists in the table
   (according to the comparison function or by strcmp if there is no comparison
   function), then the conflicting entry is returned. Otherwise, the new entry
   is returned. On error, NULL is returned.*/

char *req_list_add(req_list * const rla, char * const str, const int suffix) {
	const int len = strlen(str);
	const int lentot = len + ((rla->suffix && suffix) ? 3 : 2); /* 'a b c \0 Suffix \0' or 'a b c \0 \0' */

	int ins;
	if (rla->cmpfnc) { /* implies the entries are sorted */
		int l = 0, m = 0;
		int r = rla->cur_entries - 1;
		while(l <= r) {
			m = (r + l)/2;
			const int cmp = (*rla->cmpfnc)(str, rla->entries[m]);
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
			if (!rla->allow_dupes) return rla->entries[-m - 1];
			ins = -m;
		}
		else if (r < m) { ins = m; /* insert at i */ }
		else if (l > m) { ins = m + 1; /* insert at i + 1 */ }
		else { /* impossible! */ ins = rla->cur_entries; }
	}
	else {/* not ordered */
		ins = rla->cur_entries; /* append to end */
		if (!rla->allow_dupes) {
			for(int i = 0; i < rla->cur_entries; i++)
				if(!strcmp(rla->entries[i], str)) return rla->entries[i];
		}
	}

	/* make enough space to store the new string */
	if (rla->cur_chars + lentot > rla->alloc_chars) {
		char * p0 = rla->chars;
		char * p1;
		p1 = realloc(rla->chars, sizeof(char) * (rla->alloc_chars * 2 + lentot));
		if (!p1) return NULL;
		rla->alloc_chars = rla->alloc_chars * 2 + lentot;
		rla->chars = p1;
		/* all the strings just moved from *p0 to *p1, so adjust accordingly */
		for (int i = 0; i < rla->cur_entries; i++)
			rla->entries[i] += ( p1 - p0 );
	}

	/* make enough slots to hold the string pointer and lengths */
	if (rla->cur_entries >= rla->alloc_entries) {
		char **newentries;
		int *newlens, orig_alloc_entries = rla->alloc_entries;
		if (newentries = realloc(rla->entries, sizeof(char *) * (rla->alloc_entries * 2 + 1))) {
			rla->alloc_entries = rla->alloc_entries * 2 + 1;
			rla->entries = newentries;
			if (newlens = realloc(rla->lengths, sizeof(int) * (orig_alloc_entries * 2 + 1))) {
				rla->lengths = newlens;
			} else if (newentries = realloc(rla->entries, sizeof(char *) * orig_alloc_entries)) {
				rla->entries = newentries;
				rla->alloc_entries = orig_alloc_entries;
				return NULL;
			} else {
				rla->alloc_entries = orig_alloc_entries; /* not true, but it's the min(entries,lengths) */
				return NULL;
			}
		} else return NULL;
	}

	char * const newstr = &rla->chars[rla->cur_chars];
	char * p = strcpy(newstr, str)+len+1;
	if (rla->suffix && suffix) *p++ = rla->suffix;
	*p = '\0';
	rla->cur_chars += lentot;
	if (ins < rla->cur_entries) {
		memmove(&rla->entries[ins+1], &rla->entries[ins], sizeof(char *) * (rla->cur_entries - ins));
		memmove(&rla->lengths[ins+1], &rla->lengths[ins], sizeof(int)    * (rla->cur_entries - ins));
	}
	rla->entries[ins] = newstr;
	rla->lengths[ins] = lentot;
	rla->cur_entries++;
	return newstr;
}
