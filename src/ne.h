/* Main typedefs and defines.

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


#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include <stdint.h>
#include <inttypes.h>
#include <stdbool.h>

#define PARAMS(protos) protos
#include "syn_types.h"
#include "syn_hash.h"
#include "syn_regex.h"
#include "syn_utf8.h"
#include "syn_utils.h"

#include <limits.h>
#include <fcntl.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/stat.h>

#ifndef TERMCAP
#include <curses.h>
#include <term.h>
#else
#include "info2cap.h"
#endif

#include <termios.h>


#include "termchar.h"
#include "utf8.h"

#define CURDIR "."
#include <unistd.h>

#ifndef min
#define   min(a,b)    ((a)<(b)?(a):(b))
#endif
#ifndef max
#define   max(a,b)    ((a)>(b)?(a):(b))
#endif

/* Include the list of all possible actions that do_action() can execute.
   Note also that menu handling is governed by such a command (ESCAPE). */

#include "enums.h"

/* The input_class of a character is a first differentiation between
   alphabetic, command-activating, and ignorable character. The class[] vector
   (indexed by the extended character code) gives back the input class. */

typedef enum {
	ALPHA, COMMAND, RETURN, TAB, IGNORE, INVALID, INPUT_CLASS_COUNT
} input_class;

extern const char *input_class_names[];

/* This is the expected max length of the current directory name. */

#define CUR_DIR_MAX_SIZE    (16*1024)

/* The name of the default preferences file. */

#define DEF_PREFS_NAME     ".default"

/* The name of the syntax subdirectory (global and local). */

#define SYNTAX_DIR         "syntax"

/* This is the extension of syntax definition files. */

#define SYNTAX_EXT         ".jsf"

/* The name of the file containing the mappings from extensions to syntax names. */

#define EXT_2_SYN          "ext2syn"

/* Files with more than this number of characters will not have syntax highlighting enabled automatically. */

#define MAX_SYNTAX_SIZE		(10000000)

/* This is the name taken by unnamed documents. */

#define UNNAMED_NAME       "<unnamed>"

/* The number of key codes, including ISO-8859-1 plus 256 extra codes. */

#define NUM_KEYS           (256+256)

/* A key code that has class INVALID. */

#define INVALID_CHAR       INT_MIN

/* The mask for commands. */

#define COMMAND_MASK       0x80000000

#define CHAR_CLASS(c) ( ((c)<0) ? (((c) == INVALID_CHAR) ? INVALID : COMMAND) : ((c)>0xFF) ? ALPHA : char_class[(c)] )

/* This number is used throughout ne when an integer has to be expanded in
   characters. It is expected that sprintf("%d")ing an integer will produce a
   string shorter than the following number. PORTABILITY PROBLEM: improbable,
   but possible. */

#define MAX_INT_LEN        32

/* A buffer or clip at any given time may be marked as ASCII, UTF-8 or
   8-bit. This means, respectively, that it contains just bytes below 0x80, an
   UTF-8-encoded byte sequence or an arbitrary byte sequence.

   The attribution is lazy, but stable. This means that a buffer starts as ASCII,
   and becomes UTF-8 or 8-bit as soon as some insertion makes it necessary. This
   is useful to delay the encoding choice as much as possible. */

typedef enum {
	ENC_ASCII = 0, ENC_UTF8, ENC_8_BIT
} encoding_type;

/* Bookmark designations. User get 0 through 9 plus the AUTO_BOOKMARK '-'. */

enum {
	MAX_USER_BOOKMARK = 9,
	AUTO_BOOKMARK,
	WORDWRAP_BOOKMARK,
	NUM_BOOKMARKS
};

enum {
	COMPLETE_NONE = 0,
	COMPLETE_FILE,
	COMPLETE_CMD_FILE,  /* Unimplemented ??? */
	COMPLETE_SYNTAX
};

/* This provides a mechanism to easily create a list for request(). */

typedef struct {
	int (*cmpfnc)(const char *, const char *);
	unsigned int
		allow_dupes:1,     /* Searches are more efficient if we have no duplicate entries. */
		allow_reorder:1,   /* Allow NextDoc/PrevDoc keys to re-order entries. */
		ignore_tab:1,      /* Permits Tab to exit requester. */
		reordered:1,       /* Indicates whether reordering was done during request. */
		prune:1,           /* Whether to start off pruning by partial input. */
		find_quits:1,      /* Map FIND_A to QUIT_A (for long input ^F requester). */
		help_quits:1,      /* Map HELP_A to QUIT_A (for help requester). */
		selectdoc_quits:1; /* Map SELECTDOC_A to QUIT_A (for F4 requester). */
	char suffix;
	int cur_entries;      /* count of entries */
	int alloc_entries;    /* allocated slots in **entries */
	char **entries;       /* The array of alloc_entries string pointers pointing into *chars */
	int *lengths;         /* When set, the column width needed to display corresponding entry (strlen() + suffix + blank) */
	int *reorder;         /* maps from original order to new order when allow_reorder is true. */

	int cur_chars;        /* count of used characters */
	int alloc_chars;      /* allocated characters in *chars */
	char *chars;          /* contiguous block of allocated characters */
} req_list;

/* These are the list and node structures used throughout ne. See the exec.c
   source for some elaborations on the subject. */

typedef struct struct_node {
	struct struct_node *next;
	struct struct_node *prev;
} node;


typedef struct {
	node *head;
	node *tail;
	node *tail_pred;
} list;



/* All the structures that follow have an assertion macro which checks for
   partial internal consistency of the structure. These macros are extensively
   used in the code. */


/* This structure represent a sequence, or stream, of NULL-terminated
   strings. It is used for many purposes, such as storage of macros, clips,
   etc. General functions allow to allocate, free, load, save and append to
   such streams. Size represent the size of the chunk of memory pointed to by
   stream, while len gives you the number of bytes currently used. You can
   append in the last size-len bytes, or realloc() stream, updating size
   correspondingly. */

typedef struct {
	int64_t size, len;
	char *stream;
	encoding_type encoding;
} char_stream;


#ifndef NDEBUG
#define assert_char_stream(cs) {if ((cs)) {\
	assert((cs)->len<=(cs)->size);\
	assert((cs)->len >= 0);\
	assert(((cs)->size == 0) == ((cs)->stream == NULL));\
}}
#else
#define assert_char_stream(cs) ;
#endif


/* This structure defines a line descriptor; it is a node containing a pointer
   to the line text, and an integer containing the line length in bytes. The
   line pointer by ld_text is NOT NULL-terminated. line_len is zero iff line
   is NULL, in which case we are representing an empty line. ld_node->next
   is NULL iff this node is free for use. */


typedef struct {
	node ld_node;
	char *line;
	int64_t line_len;
	HIGHLIGHT_STATE highlight_state; /* Initial highlight state for this line */
} line_desc;

/* The purpose of this structure is to provide the byte count for allocating
   line descriptors when no syntax highlighting is required.  */

typedef struct {
	node ld_node;
	char *line;
	int64_t line_len;
} no_syntax_line_desc;

#ifndef NDEBUG
#define assert_line_desc(ld, encoding) {if ((ld)) { \
	assert((ld)->line_len >= 0);\
	assert(((ld)->line == NULL) == ((ld)->line_len == 0));\
	assert(((ld)->line_len == 0) || ((ld)->line[0] != 0 && (ld)->line[(ld)->line_len - 1] != 0));\
	if (encoding == ENC_UTF8) { int i = 0; while(i < (ld)->line_len) { assert(utf8len((ld)->line[i]) > 0); i = next_pos((ld)->line, i, encoding);} } \
}}
#else
#define assert_line_desc(ld, encoding) ;
#endif


/* This structure defines a pool of line descriptors. pool points to an
   array of size line descriptors, which are kept in free_list. The
   allocated_items field keeps track of how many items are allocated. */

typedef struct {
	node ldp_node;
	list free_list;
	int64_t size;
	int64_t allocated_items;
	void *pool; // The type of line descriptor can vary.
	bool mapped;
} line_desc_pool;

#ifndef NDEBUG
#define assert_line_desc_pool(ldp) {if ((ldp)) {\
	assert((ldp)->allocated_items <= (ldp)->size);\
	assert((ldp)->allocated_items == (ldp)->size || (ldp)->free_list.head->next);\
	assert((ldp)->pool != NULL);\
	assert((ldp)->size != 0);\
}}
#else
#define assert_line_desc_pool(ldp) ;
#endif



/* This structure defines a pool of characters. size represents the size of
   the pool pointed by pool, while first_used and last_used represent
   the min and max characters which are used. A character is not used if it
   is zero. It is perfectly possible (and likely) that between first_used
   and last_used there are many free chars, which are named "lost" chars. See
   the source buffer.c for some elaboration on the subject. */

typedef struct {
	node cp_node;
	int64_t size;
	int64_t first_used, last_used;
	char *pool;
	bool mapped;
} char_pool;

#ifndef NDEBUG
#define assert_char_pool(cp) {if ((cp)) {\
	assert((cp)->first_used<=(cp)->first_used);\
	assert((cp)->pool[(cp)->first_used] != 0);\
	assert((cp)->first_used >= 0);\
	assert((cp)->first_used == 0 || (cp)->pool[(cp)->first_used - 1] == 0);\
	assert((cp)->pool[(cp)->last_used] != 0);\
	assert((cp)->last_used >= 0);\
	assert((cp)->last_used == (cp)->size - 1 || (cp)->pool[(cp)->last_used + 1] == 0);\
	assert((cp)->pool != NULL);\
	assert((cp)->size != 0);\
}}
#else
#define assert_char_pool(cp) ;
#endif


/* This structure defines a macro. A macro is just a stream plus a node, a
   file name and a hash code relative to the filename (it is used to make the
   search for a given macro quicker). */

typedef struct struct_macro_desc {
	struct struct_macro_desc *next;
	char *name;
	char_stream *cs;
} macro_desc;

#ifndef NDEBUG
#define assert_macro_desc(md) if (md) assert_char_stream((md)->cs);
#else
#define assert_macro_desc(md) ;
#endif


/* This structure defines a clip. Clip are numbered from 0 onwards, and
   contain a stream of characters. The stream may be optionally marked as UTF-8. */

typedef struct {
	node cd_node;
	int n;
	char_stream *cs;
} clip_desc;

#ifndef NDEBUG
#define assert_clip_desc(cd) {if ((cd)) {\
	assert((cd)->n >= 0);\
	assert_char_stream((cd)->cs);\
}}
#else
#define assert_clip_desc(cd) ;
#endif

/* An undo step is given by a position, a transformation which can be
   INSERT_CHAR or DELETE_CHAR and the length of the stream to which the
   transformation applies. For compactness reasons, the transformation is
   really stored in the sign of the length. Plus means insert, minus means
   delete. Note also that pos can be negative, in which case the real position
   is -(pos+1), and the undo step is linked to the following one (in the sense
   that they should be performed indivisibly). */

typedef struct {
	int64_t line;
	int64_t pos;
	int64_t len;
} undo_step;




/* This structure defines an undo buffer. steps points to an array of
   steps_size undo_steps, used up to cur_step. last_step represent the undo step
   which is the next to be redone in case some undo had place. Note that the
   characters stored in streams, which are used when executing an insertion undo
   step, are not directly pointed to by the undo step. The correct position is
   calculated incrementally, and kept in cur_stream and last_stream.  Redo
   contains the stream of characters necessary to perform the redo
   steps. last_save_step is the step (if any) corresponding to the last successful
   buffer save operation. */

typedef struct {
	undo_step *steps;
	char *streams;
	char_stream redo;
	int64_t steps_size;
	int64_t streams_size;
	int64_t cur_step;
	int64_t cur_stream;
	int64_t last_step;
	int64_t last_stream;
	int64_t last_save_step;
} undo_buffer;

#ifndef NDEBUG
#define assert_undo_buffer(ub) {if ((ub)) {\
	assert((ub)->cur_step<=(ub)->last_step);\
	assert((ub)->cur_stream<=(ub)->last_stream);\
	assert((ub)->cur_step<=(ub)->steps_size);\
	assert((ub)->cur_stream<=(ub)->streams_size);\
	assert((ub)->last_step<=(ub)->steps_size);\
	assert((ub)->last_stream<=(ub)->streams_size);\
	assert_char_stream(&(ub)->redo);\
}}
#else
#define assert_undo_buffer(ub) ;
#endif

/* This structure defines all the per document options which can be
   used with PushPrefs and PopPrefs. */

typedef struct {
	int cur_clip;
	int tab_size;
	int right_margin;
	unsigned int
		free_form:1,       /* Editing is free form (cursor can be anywhere) */
		hex_code:1,        /* Show hexadecimal code under the cursor */
		word_wrap:1,       /* Word wrap is on */
		auto_indent:1,     /* Replicate indentation when creating a new line */
		preserve_cr:1,     /* Preserve Carriage Returns, don't treat as line terminators. */
		insert:1,          /* Insert mode */
		do_undo:1,         /* Record each action and allow undoing it */
		auto_prefs:1,      /* Use autoprefs */
		no_file_req:1,     /* Do not display the file requester */
		read_only:1,       /* Read-only mode */
		search_back:1,     /* Last search was backwards */
		case_search:1,     /* Look at case matching in searches */
		tabs:1,            /* TAB inserts TABs(1) vs. spaces(0) */
		del_tabs:1,        /* DEL/BS deletes tab's worth of space. */
		shift_tabs:1,      /* Shift may insert tabs, but only if tabs is also true */
		automatch:4,       /* Automatically match visible brackets */
		binary:1,          /* Load and save in binary mode */
		utf8auto:1,        /* Try to detect automatically UTF-8 */
		visual_bell:1;     /* Prefer visible bell to audible */
} options_t;

#ifndef NDEBUG
#define assert_options(o) {if ((o)) {\
	assert((o)->tab_size > 0);\
	assert_undo_buffer(&(b)->undo);\
}}
#else
#define assert_options(o) ;
#endif

/* This structure defines a buffer node; a buffer is composed by two lists,
   the list of line descriptor pools and the list of character pools, plus some
   data as the current window and cursor position. The line descriptors are
   kept in a list, too. The other fields are more or less self-documented.
   wanted_x represents the position the cursor would like to be on, but cannot
   because a line is too short or because it falls inside a TAB expansion. When
   a flag is declared as multilevel, this means that the flag is
   incremented/decremented rather than set/unset, so that activations and
   deactivations can be nested. */

typedef struct {
	node b_node;
	list line_desc_pool_list;
	list line_desc_list;
	list char_pool_list;
	line_desc *cur_line_desc;
	line_desc *top_line_desc;
	char_stream *cur_macro;
	char_stream *last_deleted;
	char *filename;
	char *find_string;
	char *replace_string;
	char *command_line;
	unsigned long mtime;      /* mod time of on-disk file when it was last loaded/saved, or 0 */
	int64_t win_x, win_y;     /* line and pos of upper left-most visible character. */
	int cur_x, cur_y;         /* position of cursor within the window */
	int64_t wanted_x;         /* desired x position modulo short lines, tabs, etc. Valid only if x_wanted is true. */
	int64_t wanted_y;         /* desired y position modulo top or bottom of buffer. Valid if y_wanted is true. */
	int64_t wanted_cur_y;     /* desired cur_y, valid if y_wanted is true */
	int64_t cur_line;         /* the current line */
	int64_t cur_pos;          /* position of cursor within the document buffer (counts bytes); -1 if not in sync with win_x/cur_x */
	int64_t cur_char;         /* position of cursor within the attribute buffer (counts characters); invalid if cur_pos == -1 */
	int64_t num_lines;
	int64_t block_start_line, block_start_pos;
	struct {
		int shown;
		int x;                 /* Visual (on-screen) coordinates of the highlighted bracket, if shown is true. */
		int y;
	} automatch;
	int64_t allocated_chars;
	int64_t free_chars;
	encoding_type encoding;
	undo_buffer undo;
	struct {
		int64_t pos;
		int64_t line;
		int cur_y;
	} bookmark[NUM_BOOKMARKS];
	int bookmark_mask;          /* bit N is set if bookmark[N] is set */
	int cur_bookmark;           /* For Goto(Next|Prev)Bookmark. */

	struct high_syntax *syn;    /* Syntax loaded for this buffer. */
	uint32_t *attr_buf;              /* If attr_len >= 0, a pointer to the list of *current* attributes of the *current* line. */
	int64_t attr_size;              /* attr_buf size. */
	int64_t attr_len;               /* attr_buf valid number of characters, or -1 to denote that attr_buf is not valid. */
	HIGHLIGHT_STATE next_state; /* If attr_len >= 0, the state after the *current* line. */

	int link_undos;             /* Link the undo steps. Multilevel. */

	unsigned int
		is_modified:1,           /* Buffer has been modified since last save */
		marking:1,               /* We are marking a block */
		x_wanted:1,              /* We're not where we would like to be */
		y_wanted:1,              /* We've been paging up/down */
		exec_only_options:1,     /* Only option changes can be executed */
		last_was_replace:1,      /* The last search operation was a replace */
		last_was_regexp:1,       /* The last search operation was done with regexps */
		undoing:1,               /* We are currently undoing an action */
		redoing:1,               /* We are currently redoing an action */
		mark_is_vertical:1,      /* The current marking is vertical */
		atomic_undo:1,           /* subsequent commands undo as a block */
		is_CRLF:1;               /* Buffer should be saved with CR/LF terminators */

	unsigned int find_string_changed; /* 0 = unset; 1 = force; else prior search's serial number */

	options_t opt;              /* These get pushed/popped on the prefs stack */
} buffer;


#ifndef NDEBUG
#define assert_buffer(b) {if ((b)) {\
	assert((b)->line_desc_list.head->next == NULL || (b)->cur_line_desc != NULL);\
	assert((b)->line_desc_list.head->next == NULL || (b)->top_line_desc != NULL);\
	assert_line_desc((b)->cur_line_desc, (b)->encoding);\
	assert_line_desc((b)->top_line_desc, (b)->encoding);\
	assert_char_stream((b)->last_deleted);\
	assert_char_stream((b)->cur_macro);\
	assert((b)->win_y + (b)->cur_y<(b)->num_lines);\
	assert((b)->num_lines > 0);\
	assert((b)->opt.tab_size > 0);\
	assert((b)->free_chars <= (b)->allocated_chars);\
	assert((b)->cur_line == (b)->win_y + (b)->cur_y);\
	assert((b)->cur_pos == -1 || calc_width((b)->cur_line_desc, (b)->cur_pos, (b)->opt.tab_size, (b)->encoding) == (b)->win_x + (b)->cur_x);\
	assert((b)->cur_pos == -1 || calc_virt_pos((b)->cur_line_desc, (b)->win_x + (b)->cur_x, (b)->opt.tab_size, (b)->encoding) == (b)->cur_pos);\
	assert((b)->cur_pos == -1 || (b)->cur_pos > (b)->cur_line_desc->line_len || calc_char_len((b)->cur_line_desc, (b)->cur_pos, (b)->encoding) == (b)->cur_char);\
	assert((b)->cur_pos < 0 || (b)->cur_pos >= (b)->cur_line_desc->line_len || (b)->encoding != ENC_UTF8 || utf8len((b)->cur_line_desc->line[(b)->cur_pos] > 0));\
	assert_undo_buffer(&(b)->undo);\
}}

#define assert_buffer_content(b) {if ((b)) {\
	line_desc *ld;\
	ld = (line_desc *)(b)->line_desc_list.head;\
	while(ld->ld_node.next) {\
		assert_line_desc(ld, (b)->encoding);\
		if ((b)->syn) assert(ld->highlight_state.state != -1);\
		ld = (line_desc *)ld->ld_node.next;\
	}\
	if ((b)->syn) assert((b)->attr_len < 0 || (b)->attr_len == calc_char_len((b)->cur_line_desc, (b)->cur_line_desc->line_len, (b)->encoding));\
}}
#else
#define assert_buffer(b) ;
#define assert_buffer_content(b);
#endif

#include "syntax.h"

extern const char *key_binding[];

extern buffer *cur_buffer;

/* These are the global lists. */

extern list buffers, clips;

/* This integer keeps the global turbo parameter. */

extern int turbo;


/* If true, the current line has changed and care must be taken
   to update the initial state of the following lines. */

extern bool need_attr_update;


/* If true, we want the hardwired ANSI terminal, not a real one. */

extern bool ansi;


/* If true, we want requests by column, otherwise by row. */

extern bool req_order;


/* The status bar is displayed */

extern bool status_bar;


/* If true, we want abbreviated screen updates. */

extern bool fast_gui;


/* Recorded macros use long command names */

extern bool verbose_macros;


/* If true, we want syntax highlighting. */

extern bool do_syntax;


/* This flag can be set anywhere to false, and will become true if the user
   hits the interrupt key (usually CTRL-'\'). It is handled through SIGQUIT and
   SIGINT. */

extern bool stop;


/* This is set by the signal handler whenever a SIGWINCH happens. */

extern bool window_changed_size;


/* This vector associates to an extended key code (as returned by
get_key_code()) its input class. */

extern const input_class char_class[];

/* This vector associates key codes to strings indicating the
key combinations required to produce those key codes. */

extern const char *key_stroke[];

/* A boolean recording whether the last replace was for an empty string
  (of course, this can happen only with regular expressions). */

extern bool last_replace_empty_match;


/* This number defines the macro hash table size. This table can have
   conflicts. */

#define MACRO_HASH_TABLE_SIZE (101)


/* When recording a macro, this points to the recording stream.
   When not recording a macro, this is NULL. */

extern char_stream *recording_macro;


/* Whether we're currently executing a macro. */

extern bool executing_macro;


/* There are several functions that set the status bar:
   print_message(), print_prompt() and input_refresh(), and
   draw_status_bar(). If we're suspended while a requester is up, this
   is how request() knows what to do to properly restore the status bar
   upon resume. */
extern void (*resume_status_bar)(const char *message);


/* Upcasing vectors for the regex library. */
extern unsigned char localised_up_case[];
extern const unsigned char ascii_up_case[];

#include "keycodes.h"
#include "names.h"
#include "errors.h"
#include "protos.h"
#include "utf8.h"
#include "debug.h"

/* In the unfortunate case we are compiling in some known system
   with a completely different handling of binary and ASCII files,
   we force the use of binary files. */

#ifdef O_BINARY
#define READ_FLAGS  O_RDONLY | O_BINARY
#define WRITE_FLAGS O_CREAT | O_TRUNC | O_WRONLY | O_BINARY
#else
#define READ_FLAGS  O_RDONLY
#define WRITE_FLAGS O_CREAT | O_TRUNC | O_WRONLY
#endif
