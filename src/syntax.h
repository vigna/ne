/* Syntax highlighting from Joe's Own Editor: Syntax highlighting DFA interpreter.

	Copyright (C) 2004 Joseph H. Allen
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


#ifndef _Isyntax
#define _Isyntax 1

/* Color definition */

struct high_color {
	struct high_color *next;
	unsigned char *name;		/* Symbolic name of color */
	uint32_t color;			/* Color value */
};

/* State */

struct high_state {
	int32_t no;				/* State number */
	uint32_t color;			/* Color for this state */
	unsigned char *name;		/* Highlight state name */
	struct high_cmd *cmd[256];	/* Character table */
	struct high_cmd *delim;		/* Matching delimiter */
};

/* Parameter list */

struct high_param {
	struct high_param *next;
	unsigned char *name;
};

/* Command (transition) */

struct high_cmd {
	unsigned noeat : 1;		/* Set to give this character to next state */
	unsigned start_buffering : 1;	/* Set if we should start buffering */
	unsigned stop_buffering : 1;	/* Set if we should stop buffering */
	unsigned save_c : 1;		/* Save character */
	unsigned save_s : 1;		/* Save string */
	unsigned ignore : 1;		/* Set to ignore case */
	unsigned start_mark : 1;	/* Set to begin marked area including this char */
	unsigned stop_mark : 1;		/* Set to end marked area excluding this char */
	unsigned recolor_mark : 1;	/* Set to recolor marked area with new state */
	unsigned rtn : 1;		/* Set to return */
	unsigned reset : 1;		/* Set to reset the call stack */
	int recolor;			/* No. chars to recolor if <0. */
	struct high_state *new_state;	/* The new state */
	HASH *keywords;			/* Hash table of keywords */
	struct high_cmd *delim;		/* Matching delimiter */
	struct high_syntax *call;	/* Syntax subroutine to call */
};

/* Call stack frame */

struct high_frame {
	struct high_frame *parent;		/* Caller's frame */
	struct high_frame *child;		/* First callee's frame */
	struct high_frame *sibling;		/* Caller's next callee's frame */
	struct high_syntax *syntax;		/* Current syntax subroutine */
	struct high_state *return_state;	/* Return state in the caller's subroutine */
};

/* Loaded form of syntax file or subroutine */

struct high_syntax {
	struct high_syntax *next;	/* Linked list of loaded syntaxes */
	unsigned char *name;		/* Name of this syntax */
	unsigned char *subr;		/* Name of the subroutine (or NULL for whole file) */
	struct high_param *params;	/* Parameters defined */
	struct high_state **states;	/* The states of this syntax.  states[0] is idle state */
	HASH *ht_states;		/* Hash table of states */
	int nstates;			/* No. states */
	int szstates;			/* Malloc size of states array */
	struct high_color *color;	/* Linked list of color definitions */
	struct high_cmd default_cmd;	/* Default transition for new states */
	struct high_frame *stack_base;  /* Root of run-time call tree */
};

/* Find a syntax.  Load it if necessary. */

struct high_syntax *load_syntax PARAMS((unsigned char *name));

/* Parse a lines.  Returns new state. */

extern uint32_t *attr_buf;
extern int64_t attr_len;
HIGHLIGHT_STATE parse PARAMS((struct high_syntax *syntax, line_desc *ld, HIGHLIGHT_STATE h_state, bool utf8));

#define clear_state(s) (((s)->saved_s[0] = 0), ((s)->state = 0), ((s)->stack = 0))
#define invalidate_state(s) ((s)->state = -1)
#define move_state(to,from) (*(to)= *(from))
#define eq_state(x,y) ((x)->state == (y)->state && (x)->stack == (y)->stack && !zcmp((x)->saved_s, (y)->saved_s))

extern struct high_color *global_colors;
void parse_color_def PARAMS((struct high_color **color_list,unsigned char *p,unsigned char *name,int line));

#endif
