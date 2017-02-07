/* Undo/redo system management functions.

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


/* How many undo steps we (re)allocate whenever we need more. */

#define STD_UNDO_STEP_SIZE		(1024)

/* How many undo stream bytes we (re)allocate whenever we need more. */

#define STD_UNDO_STREAM_SIZE	(16*1024)

/* This is the main function for recording an undo step (though it should be
   called through add_undo_step). It adds to the given undo buffer an undo step
   with given line, position and length, possibly enlarging the undo step
   buffer. The redo stream is reset. Note that this function is transparent
   with respect to the various positive/negative conventions about len and
   pos. */


static int cat_undo_step(undo_buffer * const ub, const int64_t line, const int64_t pos, const int64_t len) {
	if (!ub) return false;

	assert_undo_buffer(ub);

	if (ub->cur_step >= ub->steps_size) {
		undo_step * const ud = realloc(ub->steps, (ub->steps_size + STD_UNDO_STEP_SIZE) * sizeof(undo_step));
		if (ud) {
			ub->steps_size += STD_UNDO_STEP_SIZE;
			ub->steps = ud;
		}
		else return OUT_OF_MEMORY;
	}

	ub->steps[ub->cur_step].line = line;
	ub->steps[ub->cur_step].pos = pos;
	ub->steps[ub->cur_step].len = len;

	if (ub->last_save_step > ub->cur_step) ub->last_save_step = -1;
	ub->last_step = ++ub->cur_step;
	ub->last_stream = ub->cur_stream;
	reset_stream(&ub->redo);
	return 0;
}



/* Activates the chaining feature of the undo system. Any operations recorded
   between start_undo_chain() and end_undo_chain() will be undone or redone as
   a single entity. These calls can be nested, since a nesting index keeps
   track of multiple calls. */


void start_undo_chain(buffer * const b) {

#ifdef NE_TEST
	D(fprintf(stderr, "# start_undo_chain: %d -> %d\n", b->link_undos, b->link_undos + 1);)
	D(fprintf(stderr, "#   undo.cur_step: %d; undo.last_step: %d\n", b->undo.cur_step, b->undo.last_step);)
#endif

	assert_buffer(b);
	assert(b->undo.cur_step == 0 || b->link_undos || b->undo.steps[b->undo.cur_step - 1].pos >= 0);

	b->link_undos++;
}


/* See the comments to the previous function. */

void end_undo_chain(buffer * const b) {

#ifdef NE_TEST
	D(fprintf(stderr, "# end_undo_chain: %d -> %d\n", b->link_undos, b->link_undos - 1);)
	D(fprintf(stderr, "#   undo.cur_step: %d; undo.last_step: %d\n", b->undo.cur_step, b->undo.last_step);)
#endif

	assert_undo_buffer(&b->undo);

	if (--b->link_undos) return;

	if (b->undo.cur_step && b->undo.steps[b->undo.cur_step - 1].pos < 0) b->undo.steps[b->undo.cur_step - 1].pos = -(1 + b->undo.steps[b->undo.cur_step - 1].pos);
}


/* This function is the external interface to the undo recording system. It
   takes care of recording a position of -pos-1 if the undo linking feature is
   in use. A positive len records an insertion, a negative len records a
   deletion. When an insertion is recorded, len characters have to be added to
   the undo stream with add_to_undo_stream(). */

int add_undo_step(buffer * const b, const int64_t line, const int64_t pos, const int64_t len) {
	return cat_undo_step(&b->undo, line, b->link_undos ? -pos - 1 : pos, len);
}

/* Fixes the last undo step adding the given delta to its length. This
   function is needed by delete_stream(), as it is not possible to know
   the exact length of a deletion until it is performed. */

void fix_last_undo_step(buffer * const b, const int64_t delta) {
	b->undo.steps[b->undo.cur_step - 1].len += delta;
}


/* Adds to the undo stream a block of len characters pointed to by p. */

int add_to_undo_stream(undo_buffer * const ub, const char * const p, const int64_t len) {

	assert(len > 0);
	assert(ub != NULL);
	assert(ub->cur_step && ub->steps[ub->cur_step - 1].len > 0);

	if (!ub) return -1;

	assert_undo_buffer(ub);

	if (!ub->cur_step || ub->steps[ub->cur_step - 1].len < 0) return -1;

	if (ub->cur_stream + len >= ub->streams_size) {
		char *new_stream;

		if (new_stream = realloc(ub->streams, (ub->cur_stream + len + STD_UNDO_STREAM_SIZE))) {
			ub->streams_size = ub->cur_stream + len + STD_UNDO_STREAM_SIZE;
			ub->streams = new_stream;
		}
		else return OUT_OF_MEMORY;
	}

	memcpy(&ub->streams[ub->cur_stream], p, len);
	ub->last_stream = (ub->cur_stream += len);

	return 0;
}


/* Resets the undo buffer. All the previous undo steps are lost. */

void reset_undo_buffer(undo_buffer * const ub) {
	ub->cur_step =
	ub->last_step =
	ub->cur_stream =
	ub->last_stream =
	ub->steps_size =
	ub->streams_size = 0;
	ub->last_save_step = 0;
	free(ub->streams);
	free(ub->steps);
	ub->streams = NULL;
	ub->steps = NULL;
	reset_stream(&ub->redo);
}


/* Undoes the current undo step, which is the last one, if no undo has still be
   done, or an intermediate one, if some undo has already been done. */


int undo(buffer * const b) {

	if (!b) return -1;

	assert_buffer(b);

	if (b->undo.cur_step == 0) return NOTHING_TO_UNDO;

	/* WARNING: insert_stream() and delete_stream() do different things while
		undoing or redoing. */

	b->undoing = 1;

#ifdef NE_TEST
	D(fprintf(stderr, "# undo():  undo.cur_step: %d; undo.last_step: %d\n", b->undo.cur_step, b->undo.last_step);)
#endif
	do {

		b->undo.cur_step--;

		if (b->undo.steps[b->undo.cur_step].len) {
			goto_line(b, b->undo.steps[b->undo.cur_step].line);
			goto_pos(b, b->undo.steps[b->undo.cur_step].pos >= 0 ? b->undo.steps[b->undo.cur_step].pos : -(1 + b->undo.steps[b->undo.cur_step].pos));

			if (b->undo.steps[b->undo.cur_step].len < 0) {
				delete_stream(b, b->cur_line_desc, b->cur_line, b->cur_pos, -b->undo.steps[b->undo.cur_step].len);
				update_syntax_and_lines(b, b->cur_line_desc, NULL);
			}
			else {
				line_desc *end_ld = (line_desc *)b->cur_line_desc->ld_node.next;
				insert_stream(b, b->cur_line_desc, b->cur_line, b->cur_pos, b->undo.streams + (b->undo.cur_stream -= b->undo.steps[b->undo.cur_step].len), b->undo.steps[b->undo.cur_step].len);
				update_syntax_and_lines(b, b->cur_line_desc, end_ld);
			}

		}

#ifdef NE_TEST
	D(fprintf(stderr, "# undo():  undo.cur_step: %d; undo.last_step: %d\n", b->undo.cur_step, b->undo.last_step);)
#endif
	} while(b->undo.cur_step && b->undo.steps[b->undo.cur_step - 1].pos < 0);

	b->undoing = 0;

	return 0;
}


/* Redoes the last step undone. */


int redo(buffer * const b) {

	if (!b) return -1;

	assert_buffer(b);

	if (b->undo.cur_step == b->undo.last_step) return NOTHING_TO_REDO;

	/* Important! insert_stream() and delete_stream() do different things
	while undoing or redoing. */

	b->redoing = 1;

#ifdef NE_TEST
	D(fprintf(stderr, "# redo():  undo.cur_step: %d; undo.last_step: %d\n", b->undo.cur_step, b->undo.last_step);)
#endif
	do {
		if (b->undo.steps[b->undo.cur_step].len) {
			goto_line(b, b->undo.steps[b->undo.cur_step].line);
			goto_pos(b, b->undo.steps[b->undo.cur_step].pos >= 0 ? b->undo.steps[b->undo.cur_step].pos : -(1 + b->undo.steps[b->undo.cur_step].pos));

			if (b->undo.steps[b->undo.cur_step].len < 0) { 
				line_desc *end_ld = (line_desc *)b->cur_line_desc->ld_node.next;
				insert_stream(b, b->cur_line_desc, b->cur_line, b->cur_pos, b->undo.redo.stream + (b->undo.redo.len += b->undo.steps[b->undo.cur_step].len), -b->undo.steps[b->undo.cur_step].len);
				update_syntax_and_lines(b, b->cur_line_desc, end_ld);
			}	
			else {
				delete_stream(b, b->cur_line_desc, b->cur_line, b->cur_pos, b->undo.steps[b->undo.cur_step].len);
				b->undo.cur_stream += b->undo.steps[b->undo.cur_step].len;
				update_syntax_and_lines(b, b->cur_line_desc, NULL);
			}
		}

		b->undo.cur_step++;

#ifdef NE_TEST
	D(fprintf(stderr, "# redo():  undo.cur_step: %d; undo.last_step: %d\n", b->undo.cur_step, b->undo.last_step);)
#endif
	} while(b->undo.cur_step < b->undo.last_step && b->undo.steps[b->undo.cur_step - 1].pos < 0);

	b->redoing = 0;

	return 0;
}
