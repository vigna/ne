/* Clip handling functions.

	Copyright (C) 1993-1998 Sebastiano Vigna 
	Copyright (C) 1999-2015 Todd M. Lewis and Sebastiano Vigna

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


/* A clip is a numbered node in the global clip list. The contents of the clip
	are handled through the stream functions contained in streams.c.

	At creation time, a clip is marked with an encoding. Clips, of course, may
	be pasted only in buffers with a compatible encoding. 

	Note that pasting a clip in an ASCII buffer may change its encoding.
*/


/* Allocates a clip descriptor. */

clip_desc *alloc_clip_desc(int n, int size) {

	clip_desc *cd;

	assert(n >= 0);
	assert(size >= 0);

	if (cd = calloc(1, sizeof(clip_desc))) {
		cd->n = n;
		if (cd->cs = alloc_char_stream(size)) return cd;

		free(cd);
	}
	return NULL;
}



/* Reallocates a clip descriptor of the given size. If cd is NULL, this is
	equivalent to calling alloc_clip_desc. */

clip_desc *realloc_clip_desc(clip_desc *cd, int n, int size) {

	char_stream *cs;

	assert(n >= 0);
	assert(size >= 0);

	if (!cd) return alloc_clip_desc(n, size);

	assert_clip_desc(cd);

	if (cd->n != n) return NULL;

	if (cs = realloc_char_stream(cd->cs, size)) {
		cd->cs = cs;
		return cd;
	}
	
	return NULL;
}


/* Frees a clip descriptor. */

void free_clip_desc(clip_desc *cd) {

	if (!cd) return;

	assert_clip_desc(cd);

	free_char_stream(cd->cs);
	free(cd);
}


/* Scans the global clip list, searching for a specific numbered clip. Returns
	NULL on failure. */

clip_desc *get_nth_clip(int n) {
	clip_desc *cd = (clip_desc *)clips.head;

	while (cd->cd_node.next) {

		assert_clip_desc(cd);

		if (cd->n == n) return cd;
		cd = (clip_desc *)cd->cd_node.next;
	}

	return NULL;
}



/* Copies the characters between the cursor and the block marker of the given
	buffer to the nth clip. If the cut flag is true, the characters are also
	removed from the text. The code scans the text two times: the first time in
	order to determine the exact length of the text, the second time in order to
	actually copy it. */

int copy_to_clip(buffer *b, int n, int cut) {

	int i, chaining, bsp, pass, start_pos, end_pos, len, clip_len, y = b->cur_line;
	unsigned char *p = NULL;
	clip_desc *cd, *new_cd;
	line_desc *ld = b->cur_line_desc;

	if (!b->marking) return MARK_BLOCK_FIRST;

	if (b->block_start_line >= b->num_lines) return MARK_OUT_OF_BUFFER;

	/* If the mark and the cursor are on the same line and on the same position
	(or both beyond the line length), we can't copy anything. */

	cd = get_nth_clip(n);

	if (y == b->block_start_line &&
		(b->cur_pos == b->block_start_pos ||
		 b->cur_pos >= ld->line_len && b->block_start_pos >= ld->line_len)) {

		if (!(new_cd = realloc_clip_desc(cd, n, 0))) return OUT_OF_MEMORY;
		if (!cd) add_head(&clips, &new_cd->cd_node);
		return OK;
	}


	/* We have two different loops for direct or inverse copying. Making this
		conditional code would be cumbersome, awkward, and definitely inefficient. */

	if (y > b->block_start_line || y == b->block_start_line && b->cur_pos > b->block_start_pos) {
		/* mark before/above cursor */
		for(chaining = pass = clip_len = 0; pass < 2; pass++) {

			ld = b->cur_line_desc;

			for(i = y; i >= b->block_start_line; i--) {
				start_pos = 0;

				if (i == b->block_start_line) {
					if (!pass && cut && ld->line_len < b->block_start_pos) {
						if (!chaining) {
							chaining = 1;
							start_undo_chain(b);
						}
						bsp = b->block_start_pos; /* because the mark will move when we insert_spaces()! */
						insert_spaces(b, ld, i, ld->line_len, b->block_start_pos - ld->line_len);
						b->block_start_pos = bsp;
					}
					start_pos = min(ld->line_len,b->block_start_pos);
				}

				if (i == y) end_pos = min(ld->line_len,b->cur_pos);
				else end_pos = ld->line_len;
				len = end_pos - start_pos;

				if (pass) {
					assert(!(len != 0 && ld->line == NULL));

					if (i != y) *--p = 0;
					p -= len;
					if (ld->line) memcpy(p, ld->line + start_pos, len);
				}
				else clip_len += len + (i != y);

				ld = (line_desc *)ld->ld_node.prev;
			}

			if (pass) {
				cd->cs->len = clip_len;
				set_stream_encoding(cd->cs, b->encoding);
				assert_clip_desc(cd);

				if (cut) {
					goto_line(b, b->block_start_line);
					goto_column(b, calc_width(b->cur_line_desc, b->block_start_pos, b->opt.tab_size, b->encoding));
					delete_stream(b, b->cur_line_desc, b->cur_line, b->cur_pos, clip_len);
					update_syntax_and_lines(b, b->cur_line_desc, NULL);
				}

				if (chaining) end_undo_chain(b);
				return OK;
			}

			if (!(new_cd = realloc_clip_desc(cd, n, clip_len))) {
				if (chaining) end_undo_chain(b);
				return OUT_OF_MEMORY;
			}
			if (!cd) add_head(&clips, &new_cd->cd_node);
			cd = new_cd;
			p = cd->cs->stream + clip_len;
		}
	}
	else {
		/* mark after cursor */
		for(chaining = pass = clip_len = 0; pass < 2; pass++) {

			ld = b->cur_line_desc;

			for(i = y; i <= b->block_start_line; i++) {

				start_pos = 0;

				if (i == y) {
					if (!pass && cut && b->cur_pos > ld->line_len) {
						if (!chaining) {
							chaining = 1;
							start_undo_chain(b);
						}
						insert_spaces(b, ld, i, ld->line_len, b->cur_pos - ld->line_len);
					}
					start_pos = b->cur_pos > ld->line_len ? ld->line_len : b->cur_pos;
				}

				if (i == b->block_start_line) end_pos = min(b->block_start_pos, ld->line_len);
				else end_pos = ld->line_len;
				len = end_pos - start_pos;

				if (pass) {
					assert(!(len != 0 && ld->line == NULL));

					if (ld->line) memcpy(p, ld->line + start_pos, len);
					p += len;
					if (i != b->block_start_line) *(p++) = 0;
				}
				else clip_len += len + (i != y);

				ld = (line_desc *)ld->ld_node.next;
			}

			if (pass) {
				cd->cs->len = clip_len;
				set_stream_encoding(cd->cs, b->encoding);
				assert_clip_desc(cd);
				if (cut) delete_stream(b, b->cur_line_desc, b->cur_line, b->cur_pos, clip_len);
				update_syntax_and_lines(b, b->cur_line_desc, NULL);
				if (chaining) end_undo_chain(b);
				return OK;
			}

			if (!(new_cd = realloc_clip_desc(cd, n, clip_len))) {
				if (chaining) end_undo_chain(b);
				return OUT_OF_MEMORY;
			}
			if (!cd) add_head(&clips, &new_cd->cd_node);
			cd = new_cd;
			p = cd->cs->stream;
		}
	}
	if (chaining) end_undo_chain(b);
	return OK;
}



/* Simply erases a block, without putting it in a clip. Calls update_syntax_and_lines(). */

int erase_block(buffer *b) {

	int i, bsp, start_pos, end_pos, len, erase_len = 0, chaining = 0, y = b->cur_line;
	line_desc *ld = b->cur_line_desc;

	if (!b->marking) return MARK_BLOCK_FIRST;
	if (b->block_start_line >= b->num_lines) return MARK_OUT_OF_BUFFER;

	if (y == b->block_start_line &&
		(b->cur_pos == b->block_start_pos ||
		 b->cur_pos >= ld->line_len && b->block_start_pos >= ld->line_len))
		return OK;

	if (y > b->block_start_line || y == b->block_start_line && b->cur_pos > b->block_start_pos) {
		for(i = y; i >= b->block_start_line; i--) {
			start_pos = 0;
			if (i == b->block_start_line) {
				if (ld->line_len < b->block_start_pos) {
					if (!chaining) {
						chaining = 1;
						start_undo_chain(b);
					}
					bsp = b->block_start_pos; /* because the mark will move when we insert_spaces()! */
					insert_spaces(b, ld, i, ld->line_len, b->block_start_pos - ld->line_len);
					b->block_start_pos = bsp;
				}
				start_pos = min(ld->line_len,b->block_start_pos);
			}
			if (i == y) end_pos = min(ld->line_len,b->cur_pos);
			else end_pos = ld->line_len;
			len = end_pos - start_pos;

			erase_len += len + 1;
			ld = (line_desc *)ld->ld_node.prev;
		}
		goto_line(b, b->block_start_line);
		goto_column(b, calc_width(b->cur_line_desc, b->block_start_pos, b->opt.tab_size, b->encoding));
	}
	else {
		for(i = y; i <= b->block_start_line; i++) {
			start_pos = 0;
			if (i == y) {
				if (b->cur_pos > ld->line_len) {
					if (!chaining) {
						chaining = 1;
						start_undo_chain(b);
					}
					insert_spaces(b, ld, i, ld->line_len, b->cur_pos - ld->line_len);
				}
				start_pos = b->cur_pos > ld->line_len ? ld->line_len : b->cur_pos;
			}
			if (i == b->block_start_line) end_pos = min(b->block_start_pos, ld->line_len);
			else end_pos = ld->line_len;
			len = end_pos - start_pos;

			erase_len += len + 1;
			ld = (line_desc *)ld->ld_node.next;
		}
	}

	delete_stream(b, b->cur_line_desc, b->cur_line, b->cur_pos, erase_len - 1);
	if (chaining) end_undo_chain(b);
	update_syntax_and_lines(b, b->cur_line_desc, NULL);
	return OK;
}




/* Pastes a clip into a buffer. Since clips are streams, the operation is
	definitely straightforward. */

int paste_to_buffer(buffer *b, int n) {

	clip_desc *cd;

	if (!(cd = get_nth_clip(n))) return CLIP_DOESNT_EXIST;

	if (!cd->cs->len) return OK;
	if (cd->cs->encoding == ENC_ASCII || b->encoding == ENC_ASCII || cd->cs->encoding == b->encoding) {
		line_desc * const ld = b->cur_line_desc, * const end_ld = (line_desc *)b->cur_line_desc->ld_node.next;
		if (b->encoding == ENC_ASCII) b->encoding = cd->cs->encoding;

		start_undo_chain(b);
		if (b->cur_pos > ld->line_len) 
			insert_spaces(b, ld, b->cur_line, ld->line_len, b->win_x + b->cur_x - calc_width(ld, ld->line_len, b->opt.tab_size, b->encoding));

		insert_stream(b, ld, b->cur_line, b->cur_pos, cd->cs->stream, cd->cs->len);
		end_undo_chain(b);

		assert(ld == b->cur_line_desc);
		update_syntax_and_lines(b, ld, end_ld);
		return OK;
	}

	return INCOMPATIBLE_CLIP_ENCODING;
}



/* Works like copy_to_clip(), but the region to copy is the rectangle defined
	by the cursor and the marker. Same comments apply. Note that in case of a
	cut we use start_undo_chain() in order to make the various deletions a
	single undo operation. */

int copy_vert_to_clip(buffer *b, int n, int cut) {

	int i, pass, start_pos, len, clip_len, y = b->cur_line, start_x, end_x;
	char *p = NULL;
	clip_desc *cd, *new_cd;
	line_desc *ld = b->cur_line_desc;

	if (!b->marking) return MARK_BLOCK_FIRST;
	if (b->block_start_line >= b->num_lines) return MARK_OUT_OF_BUFFER;

	cd = get_nth_clip(n);

	if (b->cur_pos == b->block_start_pos ||
		 y == b->block_start_line && b->cur_pos >= ld->line_len && 
		 b->block_start_pos >= ld->line_len) {

		if (!(new_cd = realloc_clip_desc(cd, n, 0))) return OUT_OF_MEMORY;
		set_stream_encoding(new_cd->cs, ENC_ASCII);
		if (!cd) add_head(&clips, &new_cd->cd_node);
		return OK;
	}

	start_x = calc_width(nth_line_desc(b, b->block_start_line), b->block_start_pos, b->opt.tab_size, b->encoding);
	end_x = b->win_x + b->cur_x;

	if (end_x < start_x) {
		i = start_x;
		start_x = end_x;
		end_x = i;
	}

	if (cut) start_undo_chain(b);

	if (y > b->block_start_line) {

		for(pass = clip_len = 0; pass < 2; pass++) {

			ld = b->cur_line_desc;

			for(i = y; i >= b->block_start_line; i--) {

				start_pos = calc_pos(ld, start_x, b->opt.tab_size, b->encoding);
				len = calc_pos(ld, end_x, b->opt.tab_size, b->encoding) - start_pos;

				if (pass) {
					*--p = 0;
					p -= len;
					if (len) memcpy(p, ld->line + start_pos, len);
					if (cut) delete_stream(b, ld, i, start_pos, len);
				}

				else clip_len += len + 1;
				ld = (line_desc *)ld->ld_node.prev;
			}

			if (pass) {
				cd->cs->len = clip_len;
				set_stream_encoding(cd->cs, b->encoding);
				assert_clip_desc(cd);
				if (cut) {
					update_syntax_and_lines(b, (line_desc *)ld->ld_node.next, b->cur_line_desc);
					goto_line(b, min(b->block_start_line, b->cur_line));
					goto_column(b, min(calc_width(b->cur_line_desc, b->block_start_pos, b->opt.tab_size, b->encoding), b->win_x + b->cur_x));
					end_undo_chain(b);
				}
				return OK;
			}

			if (!(new_cd = realloc_clip_desc(cd, n, clip_len))) return OUT_OF_MEMORY;
			if (!cd) add_head(&clips, &new_cd->cd_node);
			cd = new_cd;
			p = cd->cs->stream + clip_len;
		}
	}
	else {

		for(pass = clip_len = 0; pass < 2; pass++) {

			ld = b->cur_line_desc;

			for(i = y; i <= b->block_start_line; i++) {

				start_pos = calc_pos(ld, start_x, b->opt.tab_size, b->encoding);
				len = calc_pos(ld, end_x, b->opt.tab_size, b->encoding) - start_pos;

				if (pass) {
					if (len) memcpy(p, ld->line + start_pos, len);
					p += len;
					*(p++) = 0;
					if (cut) delete_stream(b, ld, i, start_pos, len);
				}

				else clip_len += len + 1;
				ld = (line_desc *)ld->ld_node.next;
			}

			if (pass) {
				cd->cs->len = clip_len;
				set_stream_encoding(cd->cs, b->encoding);
				assert_clip_desc(cd);
				if (cut) {
					update_syntax_and_lines(b, b->cur_line_desc, (line_desc *)ld->ld_node.prev);
					goto_line(b, min(b->block_start_line, b->cur_line));
					goto_column(b, min(calc_width(b->cur_line_desc, b->block_start_pos, b->opt.tab_size, b->encoding), b->win_x + b->cur_x));
					end_undo_chain(b);
				}
				return OK;
			}

			if (!(new_cd = realloc_clip_desc(cd, n, clip_len))) return OUT_OF_MEMORY;
			if (!cd) add_head(&clips, &new_cd->cd_node);
			cd = new_cd;
			p = cd->cs->stream;
		}
	}

	if (cut) end_undo_chain(b);
	return OK;
}

/* Simply erases a vertical block, without putting it in a clip. Calls update_syntax_and_lines(). */

int erase_vert_block(buffer *b) {

	int i, start_pos, len, y = b->cur_line, start_x, end_x;
	line_desc *ld = b->cur_line_desc;
	line_desc *bsld;

	if (!b->marking) return MARK_BLOCK_FIRST;
	if (b->block_start_line >= b->num_lines) return MARK_OUT_OF_BUFFER;

	if (b->cur_pos == b->block_start_pos ||
		y == b->block_start_line && b->cur_pos >= ld->line_len && 
		b->block_start_pos >= ld->line_len) 
		return OK;

	start_x = calc_width(nth_line_desc(b, b->block_start_line), b->block_start_pos, b->opt.tab_size, b->encoding);
	end_x = b->win_x + b->cur_x;

	if (end_x < start_x) {
		i = start_x;
		start_x = end_x;
		end_x = i;
	}

	start_undo_chain(b);

	if (y > b->block_start_line) {
		for(i = y; i >= b->block_start_line; i--) {
			start_pos = calc_pos(ld, start_x, b->opt.tab_size, b->encoding);
			len = calc_pos(ld, end_x, b->opt.tab_size, b->encoding) - start_pos;
			delete_stream(b, ld, i, start_pos, len);
			ld = (line_desc *)ld->ld_node.prev;
		}
		update_syntax_and_lines(b, (line_desc *)ld->ld_node.next, b->cur_line_desc);
	}
	else {
		for(i = y; i <= b->block_start_line; i++) {
			start_pos = calc_pos(ld, start_x, b->opt.tab_size, b->encoding);
			len = calc_pos(ld, end_x, b->opt.tab_size, b->encoding)-start_pos;
			delete_stream(b, ld, i, start_pos, len);
			ld = (line_desc *)ld->ld_node.next;
		}
		update_syntax_and_lines(b, b->cur_line_desc, (line_desc *)ld->ld_node.prev);
	}

	end_undo_chain(b);

	goto_line(b, min(b->block_start_line, b->cur_line));
	goto_column(b, min(calc_width(b->cur_line_desc, b->block_start_pos, b->opt.tab_size, b->encoding), b->win_x + b->cur_x));

	return OK;
}



/* Performs a vertical paste. It has to be done via an insert_stream() for each
	string of the clip. Again, the undo linking feature makes all these
	operations a single undo step. */

int paste_vert_to_buffer(buffer *b, int n) {

	int len, stream_len, i, x, line;
	unsigned char *p;
	clip_desc *cd;
	line_desc * ld = b->cur_line_desc;
	unsigned char *stream;

	if (!(cd = get_nth_clip(n))) return CLIP_DOESNT_EXIST;

	if (!cd->cs->len) return OK;

	if (cd->cs->encoding != ENC_ASCII && b->encoding != ENC_ASCII && cd->cs->encoding != b->encoding) return INCOMPATIBLE_CLIP_ENCODING;
	if (b->encoding == ENC_ASCII) b->encoding = cd->cs->encoding;

	p = stream = cd->cs->stream;
	stream_len = cd->cs->len;
	line = b->cur_line;
	x = b->cur_x + b->win_x;

	start_undo_chain(b);

	while(p - stream < stream_len) {
		if (!ld->ld_node.next) {
			insert_one_line(b, (line_desc *)ld->ld_node.prev, line - 1, ((line_desc *)ld->ld_node.prev)->line_len);
			ld = (line_desc *)ld->ld_node.prev;
		}

		if (len = strnlen_ne(p, stream_len - (p - stream))) {
			for(n = i=0; i < ld->line_len && n < x; i = next_pos(ld->line, i, b->encoding)) {
				if (ld->line[i] == '\t') n += b->opt.tab_size - n % b->opt.tab_size;
				else n += get_char_width(&ld->line[i], b->encoding);
			}
			
			if (i == ld->line_len && n < x) {
				/* We miss x - n characters after the end of the line. */
				insert_spaces(b, ld, line, ld->line_len, x - n);
				insert_stream(b, ld, line, ld->line_len, p, len);
			}
			else insert_stream(b, ld, line, i, p, len);
		}

		p += len + 1;
		ld = (line_desc *)ld->ld_node.next;
		line++;
	}

	end_undo_chain(b);
	update_syntax_and_lines(b, b->cur_line_desc, ld);
	return OK;
}


/* Loads a clip. It is just a load_stream, plus an insertion in the clip
	list. If preserve_cr is TRUE, CRs are preserved. */

int load_clip(int n, const char *name, const int preserve_cr, const int binary) {
	int error;

	clip_desc *cd = get_nth_clip(n);

	if (!cd) {
		if (!(cd = alloc_clip_desc(n, 0))) return OUT_OF_MEMORY;
		add_head(&clips, &cd->cd_node);
	}

	error = load_stream(cd->cs, name, preserve_cr, binary) ? OK : CANT_OPEN_FILE;

	if (error == OK) set_stream_encoding(cd->cs, ENC_ASCII);

	return error;
}


/* Saves a clip to a file. If CRLF is true, the clip is saved with CR/LF pairs
	as line terminators. */

int save_clip(int n, const char *name, const int CRLF, const int binary) {

	clip_desc *cd = get_nth_clip(n);

	if (!cd) return CLIP_DOESNT_EXIST;
	return save_stream(cd->cs, name, CRLF, binary);
}
