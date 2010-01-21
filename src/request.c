/* Requester handling.

	Copyright (C) 1993-1998 Sebastiano Vigna 
	Copyright (C) 1999-2010 Todd M. Lewis and Sebastiano Vigna

	This file is part of ne, the nice editor.

	This program is free software; you can redistribute it and/or modify it
	under the terms of the GNU General Public License as published by the
	Free Software Foundation; either version 2, or (at your option) any
	later version.
	
	This program is distributed in the hope that it will be useful, but
	WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
	General Public License for more details.
	
	You should have received a copy of the GNU General Public License along
	with this program; see the file COPYING.  If not, write to the Free
	Software Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
	02111-1307, USA.  */


#include "ne.h"
#include "termchar.h"
#include <dirent.h>


/* This is the expected max length of the current directory name. */

#define CUR_DIR_MAX_SIZE		(16*1024)

/* These are the default allocation sizes for the entry array and for the
name array when reading a directory. The allocation sizes start with these
values, and they are doubled each time more space is needed. This ensures a
reasonable number of retries. */

#define DEF_ENTRIES_ALLOC_SIZE	256
#define DEF_NAMES_ALLOC_SIZE		(4*1024)



/* Prompts the user to choose one between several (num_entries) strings,
   contained in the entries array. The maximum string width is given as
   max_name_len. The strings are displayed as an array. More than one page will
   be available if there are many strings. If string n was selected with
   RETURN, n is returned; if string n was selected with TAB, -n-2 is returned.
   On escaping, ERROR is returned.

We rely on a series of auxiliary functions. */


static int x, y, page, max_names_per_line, max_names_per_col, names_per_page, num_entries, max_name_len, mark_char;

static const char * const *entries;

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
   
   If true, the DISPLAYBYROW define should exibit the behavior of the older
   versions of ne. If false, the second set of macros are used, and these implement
   the by-column request display. For now, this may be considered experimental. */

#define DISPLAYBYROW 1

#if DISPLAYBYROW

#define NAMES_PER_LINE(p) max_names_per_line
#define NAMES_PER_COL(p) max_names_per_col
#define PXY2N(p,x,y)  (((y) + (p) * max_names_per_col) * max_names_per_line + (x))
#define N2P(n)        ((n) / names_per_page)
#define N2X(n)        (((n) % names_per_page) % max_names_per_line)
#define N2Y(n)        (((n) % names_per_page) / max_names_per_line)
#define DX(p)         1
#define DY            max_names_per_line

#else
/* #define NAMES_PER_COL(p) (LASTPAGE(p) ? (((num_entries % names_per_page) / max_names_per_line) + 1: max_names_per_col) */

#define LASTPAGE(p)      ((num_entries / names_per_page) > p ? 0 : 1 )
/*
    This Perl snippet is useful for tweaking the NAMES_PER_LINE and NAMES_PER_COL macros. The point of
    the complexity on the last page is to use a rectangle in the upper-left part of the window that's
    roughly proportional to the window itself. (Prior pages use the entire window of course.)
    Calculating $x first gives slight priority to taller columns rather than wider lines.
    
    Translating the Perl "$x =" and "$y =" to the C macros NAMES_PER_LINE and NAMES_PER_COL,
    respectively, is a matter of substituting the following:
      $N  ($M - $n)
      $n  (num_entries % names_per_page)    The number of actual entries on the last page.
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
#define NAMES_PER_LINE(p) (LASTPAGE(p) ? (max_names_per_line - (max_names_per_line*((names_per_page - (num_entries % names_per_page))-1)*((names_per_page - (num_entries % names_per_page))-1)-names_per_page)/(names_per_page*names_per_page)) : max_names_per_line )
#define NAMES_PER_COL(p)  (LASTPAGE(p) ? (((num_entries % names_per_page)+NAMES_PER_LINE(p)-1) / NAMES_PER_LINE(p)) : max_names_per_col)
#define PXY2N(p,x,y)   ((p) * names_per_page + (x) * NAMES_PER_COL(p) + (y))
#define N2P(n)         ((n) / names_per_page)
#define N2X(n)        (((n) % names_per_page) / NAMES_PER_COL(N2P(n))) 
#define N2Y(n)        (((n) % names_per_page) % NAMES_PER_COL(N2P(n)))
#define DX(p)         NAMES_PER_COL(p)
#define DY            1

#endif

/* This is the printing function used by the requester. It prints the
strings from the entries array existing in a certain page (a page contains
(lines-1)*max_names_per_line items) with max_name_len maximum width. */

static void print_strings() {

	int row,col;
	const char *p;

	set_attr(0);
	for(row = 0; row < max_names_per_col; row++) {
		move_cursor(row, 0);
		clear_to_eol();
      if (row < NAMES_PER_COL(page)) {
			for(col = 0; col < NAMES_PER_LINE(page); col++) {
				if (PXY2N(page,col,row) < num_entries) {
					move_cursor(row, col * max_name_len);
					p = entries[PXY2N(page,col,row)];
					if (mark_char) set_attr(p[strlen(p) - 1] == mark_char ? BOLD : 0);
					output_string(p, io_utf8);
				}
			}
		}
	}
}

static void normalize(int n) {
	int p = page;

	if (n < 0 ) n = 0;
	if (n >= num_entries ) n = num_entries - 1;
	x = N2X(n);
	y = N2Y(n);
	page = N2P(n);
	if ( p != page ) print_strings();
}
	
static void request_move_to_sol(void) {
	x = 0;
}


static void request_move_to_eol(void) {
	while (x < NAMES_PER_LINE(page) - 1 && PXY2N(page,x+1,y) < num_entries) {
		x++;
	}
}

static void request_move_to_sof(void) {
	normalize(0);
}

static void request_move_to_eof() {
   normalize(num_entries - 1);
}


static void request_toggle_seof(void) {
	if (x + y+page == 0) request_move_to_eof();
	else request_move_to_sof();
}

static void request_prev_page(void) {
	if (page == 0 ) normalize(PXY2N(page,0,0));
	else normalize(PXY2N(page-1,x,y));
}


static void request_next_page(void) {
	normalize(PXY2N(page+1,x,y));
}


static void request_move_up(void) {
	normalize(PXY2N(page,x,y) - DY);
}

static void request_move_inc_up(void) {
	if (x == 0) {
		if (y == 0) request_move_to_sof();
		else request_prev_page();
	}
	else request_move_to_sol();
}

static void request_move_down(void) {
	normalize(PXY2N(page,x,y) + DY);
}

void request_move_inc_down(void) {
	if (x == NAMES_PER_LINE(page) - 1) {
		if (y == NAMES_PER_COL(page) - 1) request_move_to_eof();
		else request_next_page();
	}
	else request_move_to_eol();
}


static void request_move_left(void) {

	int p = page;
	int n = PXY2N(page,x,y);
	
	if ( x == 0 && y + page > 0 ) {
		request_move_up();
		request_move_to_eol();
	}
	else {
		normalize(PXY2N(page,x,y) - DX(page));
	}
}


static void request_move_right(void) {

	int p = page;
	int n = PXY2N(page,x,y);
	
	if (y < NAMES_PER_COL(page) - 1 && PXY2N(page,0,y+1) < num_entries && 
	    (x == NAMES_PER_LINE(page) - 1 || PXY2N(page,x+1,y) > num_entries -1)  ) {
		request_move_to_sol();
		request_move_down();
	}
	else if (y == NAMES_PER_COL(page) - 1 && x == NAMES_PER_LINE(page) - 1 && PXY2N(page+1,0,0) < num_entries) {
		normalize(PXY2N(page+1,0,0));
	}
	else if (PXY2N(page,x,y) + DX(page) < num_entries ) {
		normalize(PXY2N(page,x,y) + DX(page));
	}
}
			

/* If mark_char is not '\0', we bold names ending with it. */
int request_strings(const char * const * const _entries, const int _num_entries, int n, const int _max_name_len, int _mark_char) {

	action a;
	input_class ic;
	int c, i, ne_lines0, ne_columns0;

	assert(_num_entries > 0);

	ne_lines0 = ne_columns0 = max_names_per_line = max_names_per_col = x = y = page = 0;
	entries = _entries;
	num_entries = _num_entries;
	max_name_len = _max_name_len + 1;
	mark_char = _mark_char;

	while(TRUE) {
		if (ne_lines0 != ne_lines || ne_columns0 != ne_columns) {
			if (ne_lines0 && ne_columns0 ) n = PXY2N(page,x,y);
			if (!(max_names_per_line = ne_columns / (max_name_len))) max_names_per_line = 1;
			max_names_per_col = ne_lines - 1;
			names_per_page = max_names_per_line * max_names_per_col;
			ne_lines0 = ne_lines;
			ne_columns0 = ne_columns;
			page = N2P(n);
			x = N2X(n);
			y = N2Y(n);
			print_strings();
			print_message(NULL);
		}

		move_cursor(y, x * max_name_len);

		do c = get_key_code(); while((ic = CHAR_CLASS(c)) == IGNORE || ic == INVALID);

		n = PXY2N(page,x,y);

		switch(ic) {
			case ALPHA:
				if (n >= num_entries) n = num_entries - 1;

				c = localised_up_case[(unsigned char)c];

				for(i = 1; i < num_entries; i++)
					if (localised_up_case[(unsigned char)entries[(n + i) % num_entries][0]] == c) {
						normalize((n + i) % num_entries);
						break;
					}
				break;

			case TAB:
				if (n >= num_entries) return ERROR;
				else return -n - 2;

			case RETURN:
				if (n >= num_entries) return ERROR;
				else return n;

			case COMMAND:
				if (c < 0) c = -c - 1;
				if ((a = parse_command_line(key_binding[c], NULL, NULL, FALSE))>=0) {
					switch(a) {
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

					case ESCAPE_A:
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

char *complete(const char *start_prefix) {

	int is_dir, unique = TRUE;
	char *p, *dir_name, *cur_dir_name, *cur_prefix = NULL, *result = NULL;
	DIR *d;
	struct dirent *de;
	
	/* This might be NULL if the current directory has been unlinked, or it is not readable.
		in that case, we end up moving to the completion directory. */
	cur_dir_name = ne_getcwd(CUR_DIR_MAX_SIZE);

	if (dir_name = str_dup(start_prefix)) {
		*(p = (char *)file_part(dir_name)) = 0;
		if (p != dir_name && chdir(tilde_expand(dir_name)) == -1) {
			free(dir_name);
			return NULL;
		}
	}

	start_prefix = file_part(start_prefix);

	if (d = opendir(CURDIR)) {
		
		while(!stop && (de = readdir(d))) {

			if (is_prefix(start_prefix, de->d_name)) 
				if (cur_prefix) {
					cur_prefix[max_prefix(cur_prefix, de->d_name)] = 0;
					unique = FALSE;
				}
				else {
					cur_prefix = str_dup(de->d_name);
					is_dir = is_directory(de->d_name);
				}
		}
		
		closedir(d);
	}
	
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


/* This is the file requester. It reads the directory in which the filename
lives, builds an array of strings and calls request_strings(). If a directory
name is returned, it enters the directory. Returns NULL on error or escaping, a
pointer to the selected filename if RETURN is pressed, or a pointer to the
selected filename (or directory) preceeded by a NUL if TAB is pressed (so by
checking whether the first character of the returned string is NUL you can
check which key the user pressed). */


char *request_files(const char * const filename, int use_prefix) {

	int i, num_entries, name_len, max_name_len, total_len, next_dir, is_dir,
		entries_alloc_size = DEF_ENTRIES_ALLOC_SIZE,
		names_alloc_size = DEF_NAMES_ALLOC_SIZE;

	char *dir_name, **entries = NULL, *names = NULL, *cur_dir_name, *result = NULL, *p;

	DIR *d;
	struct dirent *de;

	if (!(cur_dir_name = ne_getcwd(CUR_DIR_MAX_SIZE))) return NULL;

	if (dir_name = str_dup(filename)) {
		i = 0;
		if ((p = (char *)file_part(dir_name)) != dir_name) {
			*p = 0;
			i = chdir(tilde_expand(dir_name));
		}
		free(dir_name);
		if (i == -1) return NULL;
	}

	if (entries = malloc(sizeof(char *) * entries_alloc_size)) {
		if (names = malloc(sizeof(char) * names_alloc_size)) {
			do {
				next_dir = FALSE;

				if (d = opendir(CURDIR)) {

					num_entries = max_name_len = total_len = 0;

#ifdef _AMIGA
					total_len = 2;
					num_entries++;
					strcpy(names, "/");
					entries[0] = names;
#endif

					stop = FALSE;

					while(!stop && (de = readdir(d))) {
						is_dir = is_directory(de->d_name);
						if (use_prefix && !is_prefix(file_part(filename), de->d_name)) continue;
						name_len = strlen(de->d_name) + is_dir + 1;

						if (name_len > max_name_len) max_name_len = name_len;

						if (total_len + name_len > names_alloc_size) {
							char *t;
							t = realloc(names, sizeof(char) * (names_alloc_size = names_alloc_size * 2 + name_len));
							if (!t) break;
							names = t;
							/* Now adjust the entries to point to the newly reallocated strings */
							entries[0] = names;
							for (i = 1; i < num_entries; i++)
							  entries[i] = entries[i - 1] + strlen(entries[i - 1]) + 1;
						}

						if (num_entries >= entries_alloc_size) {
							char **t;
							t = realloc(entries, sizeof(char *) * (entries_alloc_size *= 2));
							if (!t) break;
							entries = t;
						}

						strcpy(entries[num_entries] = names + total_len, de->d_name);
						if (is_dir) strcpy(names + total_len + name_len - 2, "/");
						total_len += name_len;
						num_entries++;
					}

					if (num_entries) {
						qsort(entries, num_entries, sizeof(char *), filenamecmpp);

						if ((i = request_strings((const char * const *)entries, num_entries, 0, max_name_len, '/')) != ERROR) {
							p = entries[i >= 0 ? i : -i - 2];
							if (p[strlen(p) - 1] == '/' && i >= 0) {
#ifndef _AMIGA
								p[strlen(p) - 1] = 0;
#endif
								if (chdir(p)) alert();
								else use_prefix = FALSE;
								next_dir = TRUE;
							}
							else {
								result = ne_getcwd(CUR_DIR_MAX_SIZE + strlen(p) + 2);
								if (strcmp(result, "/")) strcat(result, "/");
								strcat(result, p);
								if (i < 0) {
									memmove(result + 1, result, strlen(result) + 1);
									result[0] = 0;
								}
							}
						}
					}

					closedir(d);
				}
				else alert();

			} while(next_dir);

			free(names);
		}
		free(entries);
	}

	chdir(cur_dir_name);
	free(cur_dir_name);

	return result;
}



/* Requests a file name. If no_file_req is FALSE, the file requester is firstly
   presented. If no_file_req is TRUE, or the file requester is escaped, a long
   input is performed with the given prompt and default_name. */

char *request_file(const buffer *b, const char *prompt, const char *default_name) {

	char *p = NULL;

	if (!b->opt.no_file_req) {
		print_message(info_msg[PRESSF1]);
		p = request_files(default_name, FALSE);
		reset_window();
		draw_status_bar();
		if (p && *p) return p;
	}

	if (p = request_string(prompt, p ? p + 1 : default_name, FALSE, TRUE, io_utf8)) return p;

	return NULL;
}


/* Presents to the user a list of the documents currently available.  It
   returns the number of the document selected, or -1 on escape or error. */

int request_document(void) {

	int i = -1, num_entries, max_name_len, total_len;
	char **entries, *names, *p, unnamed_name[] = UNNAMED_NAME;
	buffer *b = (buffer *)buffers.head;

	num_entries = max_name_len = total_len = 0;

	while(b->b_node.next) {
		p = b->filename ? b->filename : unnamed_name;

		if (strlen(p)>max_name_len) max_name_len = strlen(p);

		total_len += strlen(p) + 1;
		num_entries++;

		b = (buffer *)b->b_node.next;
	}

	max_name_len += 8;

	if (num_entries) {

		if (entries = malloc(sizeof(char *) * num_entries)) {
			if (names = malloc(sizeof(char) * total_len)) {

				p = names;

				b = (buffer *)buffers.head;

				for(i = 0; i < num_entries; i++) {
					entries[i] = p;
					strcpy(p, b->filename ? b->filename : unnamed_name);
					p += strlen(p) + 1;

					b = (buffer *)b->b_node.next;
				}

				i = request_strings((const char * const *)entries, num_entries, 0, max_name_len, '\0');

				reset_window();

				free(names);
			}
			free(entries);
		}
	}

	return i;
}
