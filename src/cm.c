/* Optimal cursor motion functions.
   Based primarily on public domain code written by Chris Torek.
   Originally part of GNU Emacs.	Vastly edited and modified for use within ne.

	Copyright (C) 1985, 1995 Free Software Foundation, Inc.
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


#include <stdio.h>
#include <string.h>

#ifndef TERMCAP
#include <curses.h>
#include <term.h>
#else
#include "info2cap.h"
#endif

#include "cm.h"

#define	BIG	9999

int	cost;					/* sums up costs */


/* This function is used in place of putchar() in tputs() so can we can
computed the padded length of a capability string. Note that they should
be putchar()-like, so we have to care about the returned value. */

int evalcost (int c) {
	cost++;
	return c;
}



/* This function is used in tputs(). */

int cmputc (int c) {
	return putchar(c & 0x7f);
}


/* Terminals with magicwrap (xn) don't all behave identically.  The VT100
   leaves the cursor in the last column but will wrap before printing the next
   character.  I hear that the Concept terminal does the wrap immediately but
   ignores the next newline it sees.  And some terminals just have buggy
   firmware, and think that the cursor is still in limbo if we use direct
   cursor addressing from the phantom column.  The only guaranteed safe thing
   to do is to emit a CRLF immediately after we reach the last column; this
   takes us to a known state. */

void cmcheckmagic () {
	if (curX == ScreenCols) {
		assert(MagicWrap && curY < ScreenRows - 1);
		putchar ('\r');
		putchar ('\n');
		curX = 0;
		curY++;
	}
}


/* (Re)Initialises the cost factors, given the output speed of the terminal in
   the variable ospeed.  (Note: this holds B300, B9600, etc -- ie stuff out of
   <sgtty.h>.) */

void cmcostinit () {
	char *p;
	
#define	COST(x,e)	(x ? (cost = 0, tputs (x, 1, e), cost) : BIG)
#define CMCOST(x,e)	((x == 0) ? BIG : (p = tgoto(x, 0, 0), COST(p ,e)))
	
	Wcm.cc_up =		COST (Wcm.cm_up, evalcost);
	Wcm.cc_down =	COST (Wcm.cm_down, evalcost);
	Wcm.cc_left =	COST (Wcm.cm_left, evalcost);
	Wcm.cc_right =	COST (Wcm.cm_right, evalcost);
	Wcm.cc_home =	COST (Wcm.cm_home, evalcost);
	Wcm.cc_cr =		COST (Wcm.cm_cr, evalcost);
	Wcm.cc_ll =		COST (Wcm.cm_ll, evalcost);
	Wcm.cc_tab =	Wcm.cm_tabwidth ? COST (Wcm.cm_tab, evalcost) : BIG;
	
	/* These last three are actually minimum costs.  When (if) they are
		candidates for the least-cost motion, the real cost is computed.  (Note
		that "0" is the assumed to generate the minimum cost.  While this is not
		necessarily true, I have yet to see a terminal for which is not; all the
		terminals that have variable-cost cursor motion seem to take straight
		numeric values.  --ACT) */
	
	Wcm.cc_abs =  CMCOST (Wcm.cm_abs, evalcost);
	Wcm.cc_habs = CMCOST (Wcm.cm_habs, evalcost);
	Wcm.cc_vabs = CMCOST (Wcm.cm_vabs, evalcost);
	
#undef CMCOST
#undef COST
}


/* Calculates the cost to move from (srcy, srcx) to (dsty, dstx) using up and
 down, and left and right, motions, and tabs.  If doit is set actually perform
 the motion. */

static int calccost (int srcy, int srcx, int dsty, int dstx, int doit) {
	register int	 deltay, deltax, c, totalcost;
	int ntabs, n2tabs, tabx, tab2x, tabcost;
	register char  *p;
	
	/* If have just wrapped on a terminal with xn, don't believe the cursor
		 position: give up here and force use of absolute positioning.  */

	if (curX == Wcm.cm_cols) goto fail;

	totalcost = 0;
	if ((deltay = dsty - srcy) == 0) goto x;

	if (deltay < 0) p = Wcm.cm_up, c = Wcm.cc_up, deltay = -deltay;
	else p = Wcm.cm_down, c = Wcm.cc_down;

	if (c == BIG) {		/* caint get thar from here */
		if (doit) printf ("OOPS");
		return c;
	}

	totalcost = c * deltay;
	if (doit) while (deltay-- != 0) tputs (p, 1, cmputc);
x: 
	if ((deltax = dstx - srcx) == 0)	goto done;
	if (deltax < 0) {
		p = Wcm.cm_left, c = Wcm.cc_left, deltax = -deltax;
		goto dodelta;		/* skip all the tab junk */
	}
	/* Tabs (the toughie) */
	if (Wcm.cc_tab >= BIG || !Wcm.cm_usetabs) goto olddelta;		/* forget it! */

	 /* ntabs is # tabs towards but not past dstx; n2tabs is one more (ie past
		 dstx), but this is only valid if that is not past the right edge of the
		 screen.  We can check that at the same time as we figure out where we
		 would be if we use the tabs (which we will put into tabx (for ntabs) and
		 tab2x (for n2tabs)). */

	ntabs = (deltax + srcx % Wcm.cm_tabwidth) / Wcm.cm_tabwidth;
	n2tabs = ntabs + 1;
	tabx = (srcx / Wcm.cm_tabwidth + ntabs) * Wcm.cm_tabwidth;
	tab2x = tabx + Wcm.cm_tabwidth;
	
	if (tab2x >= Wcm.cm_cols) n2tabs = 0; /* too far (past edge) */
		
	
	/* Now set tabcost to the cost for using ntabs, and c to the cost for using
		n2tabs, then pick the minimum. */

	/* cost for ntabs - cost for right motion */
	tabcost = ntabs ? ntabs * Wcm.cc_tab + (dstx - tabx) * Wcm.cc_right : BIG;

	/* cost for n2tabs -  cost for left motion */
	c = n2tabs  ?	 n2tabs * Wcm.cc_tab + (tab2x - dstx) * Wcm.cc_left : BIG;
	
	if (c < tabcost)		/* then cheaper to overshoot & back up */
		ntabs = n2tabs, tabcost = c, tabx = tab2x;

	if (tabcost >= BIG)		/* caint use tabs */
		goto newdelta;

	 /* See if tabcost is less than just moving right */

	if (tabcost < (deltax * Wcm.cc_right)) {
		totalcost += tabcost;	/* use the tabs */
		if (doit) while (ntabs-- != 0) tputs (Wcm.cm_tab, 1, cmputc);
		srcx = tabx;
	}

	 /* Now might as well just recompute the delta. */

 newdelta: 
	if ((deltax = dstx - srcx) == 0)	goto done;
 olddelta: 
	if (deltax > 0) p = Wcm.cm_right, c = Wcm.cc_right;
	else p = Wcm.cm_left, c = Wcm.cc_left, deltax = -deltax;
	
 dodelta: 
	if (c == BIG) {		/* caint get thar from here */
	fail:
		if (doit)
			printf ("OOPS");
		return BIG;
	}
	totalcost += c * deltax;
	if (doit) while (deltax-- != 0) tputs (p, 1, cmputc);
done: 
	return totalcost;
}



#define	USEREL	0
#define	USEHOME	1
#define	USELL	2
#define	USECR	3


void cmgoto (int row, int col) {
	int	  homecost, crcost, llcost, relcost, directcost;
	int	  use = USEREL;
	char	*p, *dcm;
	
	/* First the degenerate case */
	if (row == curY && col == curX) return; /* already there */
		
	if (curY >= 0 && curX >= 0) {
		/* We may have quick ways to go to the upper-left, bottom-left,
		 start-of-line, or start-of-next-line.  Or it might be best to start
		 where we are.  Examine the options, and pick the cheapest.  */
		
		relcost = calccost (curY, curX, row, col, 0);
		use = USEREL;
		if ((homecost = Wcm.cc_home) < BIG) homecost += calccost (0, 0, row, col, 0);
		if (homecost < relcost) relcost = homecost, use = USEHOME;
		if ((llcost = Wcm.cc_ll) < BIG) llcost += calccost (Wcm.cm_rows - 1, 0, row, col, 0);
		if (llcost < relcost) relcost = llcost, use = USELL;
		if ((crcost = Wcm.cc_cr) < BIG) {
			if (Wcm.cm_autolf)
				if (curY + 1 >= Wcm.cm_rows) crcost = BIG;
				else crcost += calccost (curY + 1, 0, row, col, 0);
			else crcost += calccost (curY, 0, row, col, 0);
		}
		if (crcost < relcost) relcost = crcost, use = USECR;
		directcost = Wcm.cc_abs, dcm = Wcm.cm_abs;
		if (row == curY && Wcm.cc_habs < BIG) directcost = Wcm.cc_habs, dcm = Wcm.cm_habs;
		else if (col == curX && Wcm.cc_vabs < BIG)
			directcost = Wcm.cc_vabs, dcm = Wcm.cm_vabs;
	}
	else {
		directcost = 0, relcost = 100000;
		dcm = Wcm.cm_abs;
	}
	
	/* In the following comparison, the = in <= is because when the costs are
	 the same, it looks nicer (I think) to move directly there. */
	if (directcost <= relcost) {
		/* compute REAL direct cost */
		cost = 0;
		p = dcm == Wcm.cm_habs ? tgoto (dcm, row, col) : tgoto (dcm, col, row);
		tputs (p, 1, evalcost);
		if (cost <= relcost) {	/* really is cheaper */
			tputs (p, 1, cmputc);
			curY = row, curX = col;
			return;
		}
	}
	
	switch (use) {
	case USEHOME: 
		tputs (Wcm.cm_home, 1, cmputc);
		curY = 0, curX = 0;
		break;
		
	case USELL: 
		tputs (Wcm.cm_ll, 1, cmputc);
		curY = Wcm.cm_rows - 1, curX = 0;
		break;
		
	case USECR: 
		tputs (Wcm.cm_cr, 1, cmputc);
		if (Wcm.cm_autolf) curY++;
		curX = 0;
		break;
	}
	
	(void) calccost (curY, curX, row, col, 1);
	curY = row, curX = col;
}

/* Clears out all terminal info.  Used before copying into it the info on the
   actual terminal. */

void Wcm_clear () {
	memset(&Wcm, 0, sizeof Wcm);
}

/* Initialises stuff. Returns 0 if can do CM, -1 if cannot, -2 if size not
   specified. */

int Wcm_init () {
	if (Wcm.cm_abs) return 0;
	/* Require up and left, and, if no absolute, down and right */
	if (!Wcm.cm_up || !Wcm.cm_left) return - 1;
	if (!Wcm.cm_abs && (!Wcm.cm_down || !Wcm.cm_right)) return - 1;
	/* Check that we know the size of the screen.... */
	if (Wcm.cm_rows <= 0 || Wcm.cm_cols <= 0) return - 2;
	return 0;
}
