/* Optimal cursor motion definitions.
   Based primarily on public domain code written by Chris Torek.
   Originally part of GNU Emacs.	Vastly edited and modified for use within ne.

	Copyright (C) 1985, 1989 Free Software Foundation, Inc.
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


/* This structure holds everything needed to do cursor motion. */

extern struct cm {

	/* Cursor position.  -1 in *both* variables means the cursor
		position is unknown, in order to force absolute cursor motion. */

	int	cm_curY,	/* current row */
			cm_curX;	/* current column */

	/* Capabilities from terminfo */

	char	*cm_up,		/* up (up) */
			*cm_down,	/* down (do) */
			*cm_left,	/* left (bs) */
			*cm_right,	/* right (nd) */
			*cm_home,	/* home (ho) */
			*cm_cr,		/* carriage return (cr) */
			*cm_ll,		/* last line (ll) */
			*cm_tab,		/* tab (ta) */
			*cm_backtab,		/* backtab (bt) */
			*cm_abs,	/* absolute (cm) */
			*cm_habs,	/* horizontal absolute (ch) */
			*cm_vabs,	/* vertical absolute (cv) */
			*cm_multiup,		/* multiple up (UP) */
			*cm_multidown,		/* multiple down (DO) */
			*cm_multileft,		/* multiple left (LE) */
			*cm_multiright;	/* multiple right (RI) */

	int	cm_cols,	/* Number of cols on screen (co) */
			cm_rows,	/* Number of rows on screen (li) */
			cm_tabwidth;	/* tab width (it) */

	unsigned int
			cm_autowrap:1,		/* autowrap flag (am) */
			cm_magicwrap:1,	/* vt100s: cursor stays in last col but
										will wrap if next char is printing (xn) */
			cm_usetabs:1,		/* if set, use tabs */
			cm_autolf:1,		/* \r performs a \r\n (rn) */
			cm_losewrap:1; 	/* if reach right margin, forget cursor location */


	/* Costs */

	int	cc_up,		/* cost for up */
			cc_down,	/* etc */
			cc_left,
			cc_right,
			cc_home,
			cc_cr,
			cc_ll,
			cc_tab,
			cc_backtab,
			cc_abs,		/* abs costs are actually min costs */
			cc_habs,
			cc_vabs;
} Wcm;


/* Shorthands */

#define curY		Wcm.cm_curY
#define curX		Wcm.cm_curX
#define Up		Wcm.cm_up
#define Down		Wcm.cm_down
#define Left		Wcm.cm_left
#define Right		Wcm.cm_right
#define Tab		Wcm.cm_tab
#define BackTab		Wcm.cm_backtab
#define TabWidth	Wcm.cm_tabwidth
#define CR		Wcm.cm_cr
#define Home		Wcm.cm_home
#define LastLine	Wcm.cm_ll
#define AbsPosition	Wcm.cm_abs
#define ColPosition	Wcm.cm_habs
#define RowPosition	Wcm.cm_vabs
#define MultiUp		Wcm.cm_multiup
#define MultiDown	Wcm.cm_multidown
#define MultiLeft	Wcm.cm_multileft
#define MultiRight	Wcm.cm_multiright
#define AutoWrap	Wcm.cm_autowrap
#define MagicWrap	Wcm.cm_magicwrap
#define UseTabs		Wcm.cm_usetabs
#define ScreenRows	Wcm.cm_rows
#define ScreenCols	Wcm.cm_cols

#define UpCost		Wcm.cc_up
#define DownCost	Wcm.cc_down
#define LeftCost	Wcm.cc_left
#define RightCost	Wcm.cc_right
#define HomeCost	Wcm.cc_home
#define CRCost		Wcm.cc_cr
#define LastLineCost	Wcm.cc_ll
#define TabCost		Wcm.cc_tab
#define BackTabCost	Wcm.cc_backtab
#define AbsPositionCost	Wcm.cc_abs
#define ColPositionCost	Wcm.cc_habs
#define RowPositionCost	Wcm.cc_vabs
#define MultiUpCost	Wcm.cc_multiup
#define MultiDownCost	Wcm.cc_multidown
#define MultiLeftCost	Wcm.cc_multileft
#define MultiRightCost	Wcm.cc_multiright

#define	cmat(row,col)	(curY = (row), curX = (col))
#define cmplus(n)					\
  {							\
    if ((curX += (n)) >= ScreenCols && !MagicWrap)	\
      {							\
	if (Wcm.cm_losewrap) losecursor ();		\
	else if (AutoWrap) curX = 0, curY++;		\
	else curX--;					\
      }							\
  }

#define losecursor()	(curX = -1, curY = -1)

int cmputc(int);
int Wcm_init (void);
void cmcostinit (void);
void cmgoto (int row, int col);

#include "debug.h"
