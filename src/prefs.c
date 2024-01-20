/* Preferences functions.

   Copyright (C) 1993-1998 Sebastiano Vigna
   Copyright (C) 1999-2023 Todd M. Lewis and Sebastiano Vigna

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
#include "support.h"
#include <fnmatch.h>

/* These are the names of ne's autoprefs directory. */

#define PREFS_DIR ".ne"

/* This string is appended to the filename extension. It tries to
be enough strange to avoid clashes with macros. */

#define PREF_FILE_SUFFIX   "#ap"

/* The name of the virtual extensions file (local and global). */

#define VIRTUAL_EXT_NAME    ".extensions"
#define VIRTUAL_EXT_NAME_G   "extensions"

/* The maximum number of characters scanned during a regex search for
virtual extensions. */

#define REGEX_SCAN_LIMIT (100000)

/* We suppose a configuration file won't be bigger than this. Having it
bigger just causes a reallocation. */

#define PREF_FILE_SIZE_GUESS (256)

/* If we're saving default prefs, we include global prefs
   that are not buffer specific. Likewise, if we're saving
   auto prefs, we don't want to include global prefs. */

static bool saving_defaults;

/* Returns a pointer to the extension of a filename, or NULL if there is no
   extension (or no filename!). */

const char *extension(const char * const filename) {

	if ( filename ) {
		for(int i = strlen(filename); i-- != 0;) {
			if (filename[i] == '/') return NULL;
			if (filename[i] == '.') return &filename[i + 1];
		}
	}

	return NULL;
}




/* Returns a pointer to the absolute name of ne's prefs directory with '/' appended.
   The name is cached internally, so it needs not to be free()ed. If the directory does not
   exist, it is created. NULL is returned on failure. */


char *exists_prefs_dir(void) {

	static char *prefs_dir;

	/* If we have been already called, we already computed the name. */

	if (prefs_dir) return prefs_dir;

	/* In the UN*X case, we first get the home directory. Then
	we allocate space for the directory name. */

	char * home_dir;
	if (!(home_dir = getenv("HOME"))) home_dir = ".";

	if (prefs_dir = malloc(strlen(home_dir) + strlen(PREFS_DIR) + 3)) {

		strcat(strcat(strcpy(prefs_dir, home_dir), "/"), PREFS_DIR);

		struct stat s;
		if (stat(prefs_dir, &s)) {
			if (mkdir(prefs_dir, 0700)) {
				free(prefs_dir);
				return prefs_dir = NULL;
			}
		}
		else if (!S_ISDIR(s.st_mode)) {
			free(prefs_dir);
			return prefs_dir = NULL;
		}

		return strcat(prefs_dir, "/");
	}
	else return NULL;
}


/* Returns a pointer to the absolute name of ne's global prefs directory with '/' appended.
   The name is cached internally, so it needs not to be free()ed. If the directory
   does not exist, it is not created. NULL is returned on failure. */

char *exists_gprefs_dir(void) {
	static char *gprefs_dir = NULL;

	/* If we have been already called, we already computed the name. We
		should free up the name and re-compute it (because the global dir may
		have changed). */

	if (gprefs_dir) {
		free(gprefs_dir);
		gprefs_dir = NULL;
	}

	const char *global_dir;
	if ((global_dir = get_global_dir()) && (gprefs_dir = malloc(strlen(global_dir) + 3 ))) {
		strcpy(gprefs_dir, global_dir);
		struct stat s;
		if (stat(gprefs_dir, &s)) {
			free(gprefs_dir);
			return gprefs_dir = NULL;
		}
		else if (!S_ISDIR(s.st_mode)) {
			free(gprefs_dir);
			return gprefs_dir = NULL;
		}
		return strcat(gprefs_dir, "/");
	}
	return NULL;
}


/* Saves the preferences of the given buffer onto the given file name. If b or
   name are NULL, ERROR is returned.  */

int save_prefs(buffer * const b, const char * const name) {
	if (!b || !name) return ERROR;

	assert_buffer(b);

	char_stream *cs = alloc_char_stream(PREF_FILE_SIZE_GUESS);
	if (cs) {
		/* We create a macro by recording an action for each kind of flag. */

		if (!saving_defaults && b->syn) record_action(cs, SYNTAX_A, -1, (const char *)b->syn->name, verbose_macros);

		record_action(cs, TABSIZE_A,          b->opt.tab_size,       NULL, verbose_macros);
		/* Skip cur_clip */
		record_action(cs, RIGHTMARGIN_A,      b->opt.right_margin,   NULL, verbose_macros);
		record_action(cs, FREEFORM_A,         b->opt.free_form,      NULL, verbose_macros);
		record_action(cs, HEXCODE_A,          b->opt.hex_code,       NULL, verbose_macros);
		record_action(cs, WORDWRAP_A,         b->opt.word_wrap,      NULL, verbose_macros);
		record_action(cs, AUTOINDENT_A,       b->opt.auto_indent,    NULL, verbose_macros);
		record_action(cs, PRESERVECR_A,       b->opt.preserve_cr,    NULL, verbose_macros);
		record_action(cs, INSERT_A,           b->opt.insert,         NULL, verbose_macros);
		record_action(cs, DOUNDO_A,           b->opt.do_undo,        NULL, verbose_macros);
		record_action(cs, AUTOPREFS_A,        b->opt.auto_prefs,     NULL, verbose_macros);
		record_action(cs, NOFILEREQ_A,        b->opt.no_file_req,    NULL, verbose_macros);
		/* Skip read_only */
		/* Skip search_back */
		record_action(cs, CASESEARCH_A,       b->opt.case_search,    NULL, verbose_macros);
		record_action(cs, TABS_A,             b->opt.tabs,           NULL, verbose_macros);
		record_action(cs, DELTABS_A,          b->opt.del_tabs,       NULL, verbose_macros);
		record_action(cs, SHIFTTABS_A,        b->opt.shift_tabs,     NULL, verbose_macros);
		record_action(cs, AUTOMATCHBRACKET_A, b->opt.automatch,      NULL, verbose_macros);
		record_action(cs, BINARY_A,           b->opt.binary,         NULL, verbose_macros);
		record_action(cs, UTF8AUTO_A,         b->opt.utf8auto,       NULL, verbose_macros);
		record_action(cs, VISUALBELL_A,       b->opt.visual_bell,    NULL, verbose_macros);
		if (bracketed_paste)
			record_action(cs, BRACKETEDPASTE_A, -1, cur_bracketed_paste_value(b), verbose_macros);

		if (saving_defaults) {
			/* We only save the global flags that differ from their defaults. */
			/* Make sure these are in sync with the defaults near the top of ne.c. */
#ifndef ALTPAGING
			if (req_order)       record_action(cs, REQUESTORDER_A,   req_order,      NULL, verbose_macros);
#else
			if (!req_order)      record_action(cs, REQUESTORDER_A,   req_order,      NULL, verbose_macros);
#endif
			if (fast_gui)        record_action(cs, FASTGUI_A,        fast_gui,       NULL, verbose_macros);
			if (!status_bar)     record_action(cs, STATUSBAR_A,      status_bar,     NULL, verbose_macros);
			if (!verbose_macros) record_action(cs, VERBOSEMACROS_A,  verbose_macros, NULL, verbose_macros);
			saving_defaults = false;
		}

		const int error = save_stream(cs, name, b->is_CRLF, false);
		free_char_stream(cs);
		return error;
	}

	return OUT_OF_MEMORY;
}



/* Loads the given preferences file. The file is just executed, but with the
   exec_only_options flag set. If b or name are NULL, ERROR is returned. */

int load_prefs(buffer * const b, const char * const name) {
	if (!b || !name) return ERROR;

	assert_buffer(b);

	b->exec_only_options = 1;

	int error = OK;
	char_stream * const cs = load_stream(NULL, name, false, false);
	if (cs) {
		error = play_macro(cs, 1);
		free_char_stream(cs);
	}
	else error = CANT_OPEN_FILE;

	b->exec_only_options = 0;

	return error;
}

/* Loads the given syntax, taking care to preserve the old
   syntax if the new one cannot be loaded. */

int load_syntax_by_name(buffer * const b, const char * const name) {
	assert_buffer(b);
	assert(name != NULL);

	struct high_syntax *syn = load_syntax((unsigned char *)name);
	if (!syn) syn = load_syntax((unsigned char *)ext2syntax(name));
	if (syn) {
		b->syn = syn;
		reset_syntax_states(b);
		return OK;
	}
	return NO_SYNTAX_FOR_EXT;
}

/* This data structure holds information about virtual extensions. */

static struct {
	int max_line;
	char *ext;
	char *regex;
	bool case_sensitive;
} *virt_ext;

static int num_virt_ext;
static int64_t max_max_line;

static char **extra_ext;
static int64_t num_extra_exts;

static void load_virt_ext(char *vname) {
	/* Our find_regexp() is geared to work on buffers rather than streams, so we'll create a
	   stand-alone buffer. This also buys us proper handling of encodings. */
	buffer * vb = alloc_buffer(NULL);
	if (vb == NULL) return;
	clear_buffer(vb);
	vb->opt.auto_prefs = 0;
	vb->opt.do_undo = 0;
	vb->opt.case_search = 0;
	if (load_file_in_buffer(vb, vname) != OK) return;

	bool skip_first = false;
	vb->find_string = "^\\s*(\\w+)\\s+([0-9]+i?)\\s+(.+[^ \\t])\\s*$|^\\.([^ \\t/]+)\\s*$";
	vb->find_string_changed = 1;

	if ((virt_ext = realloc(virt_ext, (num_virt_ext + vb->num_lines) * sizeof *virt_ext))
			&& (extra_ext = realloc(extra_ext, (num_extra_exts + vb->num_lines) * sizeof *extra_ext))) {
		while (find_regexp(vb, NULL, skip_first, false) == OK) {
			skip_first = true;
			if (nth_regex_substring_nonempty(vb->cur_line_desc, 1)) {
				char * const ext          = nth_regex_substring(vb->cur_line_desc, 1);
				char * const max_line_str = nth_regex_substring(vb->cur_line_desc, 2);
				char * const regex        = nth_regex_substring(vb->cur_line_desc, 3);
				if (!ext || !max_line_str || !regex) break;

				errno = 0;
				char *endptr;
				int64_t max_line = strtoll(max_line_str, &endptr, 0);
				if (max_line < 1 || errno) max_line = INT64_MAX;

				int i;
				for(i = 0; i < num_virt_ext; i++)
					if (strcmp(virt_ext[i].ext, ext) == 0) {
						free(virt_ext[i].ext);
						free(virt_ext[i].regex);
						break;
					}

				virt_ext[i].ext = ext;
				virt_ext[i].max_line = max_line;
				virt_ext[i].regex = regex;
				virt_ext[i].case_sensitive = *endptr != 'i';
				free(max_line_str);
				if (i == num_virt_ext) num_virt_ext++;
			}
			else {
				char * const ext = nth_regex_substring(vb->cur_line_desc, 4);
				if (! ext) break;
				int i;
				for(i = 0; i < num_extra_exts; i++)
					if (strcmp(extra_ext[i], ext) == 0) break;
				if (i == num_extra_exts) extra_ext[num_extra_exts++] = ext;
				else free(ext);
			}
		}
	}

	vb->find_string = NULL; /* Or free_buffer() would free() it. */
	free_buffer(vb);
}

/* Loads and stores internally the virtual extensions. First we source the
   global extensions file, and then the local one. Local specifications
   override global ones. */

void load_virtual_extensions() {
	assert(virt_ext == NULL);
	char *prefs_dir;

	/* Try global directory first. */
	if (prefs_dir = exists_gprefs_dir()) {
		char virt_name[strlen(VIRTUAL_EXT_NAME_G) + strlen(prefs_dir) + 1];
		strcat(strcpy(virt_name, prefs_dir), VIRTUAL_EXT_NAME_G);
		load_virt_ext(virt_name);
	}

	/* Then try the user's ~/.ne/.extensions, possibly overriding global settings. */
	if (prefs_dir = exists_prefs_dir()) {
		char virt_name[strlen(VIRTUAL_EXT_NAME) + strlen(prefs_dir) + 1];
		strcat(strcpy(virt_name, prefs_dir), VIRTUAL_EXT_NAME);
		load_virt_ext(virt_name);
	}

	for(int i = 0; i < num_virt_ext; i++)
		max_max_line = max(max_max_line, virt_ext[i].max_line);
}


/* virtual_extension() returns an extension determined by a buffers contents and
   the user's VIRTUAL_EXT_NAME file or possibly the global VIRTUAL_EXT_NAME file.
	The returned string need not be free()'d. */

static char *virtual_extension(buffer * const b) {
	if (virt_ext == NULL) return NULL;
	/* If the buffer filename has an extension, check that it's in extra_ext. */
	const char * const filename_ext = extension(b->filename);
	if (filename_ext != NULL) {
		int i;
		for(i = 0; i < num_extra_exts; i++)
			if (fnmatch(extra_ext[i], filename_ext, 0) == 0) break;
		if (i == num_extra_exts) return NULL;
	}

	/* Reduce the maximum number of lines to scan so that no more
	than REGEX_SCAN_LIMIT characters are regex'd. */
	int64_t line_limit = 0, pos_limit = -1, len = 0;
	for(line_desc *ld = (line_desc *)b->line_desc_list.head; ld->ld_node.next && line_limit < max_max_line;
		ld = (line_desc *)ld->ld_node.next, line_limit++)
		if ((len += ld->line_len + 1) > REGEX_SCAN_LIMIT) {
			line_limit++;
			pos_limit = REGEX_SCAN_LIMIT - (len - ld->line_len - 1);
			break;
		}

	int64_t earliest_found_line = INT64_MAX;
	char *ext = NULL;

	const int64_t b_cur_line    = b->cur_line;
	const int64_t b_cur_pos     = b->cur_pos;
	const int     b_search_back = b->opt.search_back;
	const int     b_case_search = b->opt.case_search;
	const int     b_last_was_regexp = b->last_was_regexp;
	char * const find_string    = b->find_string;

	b->opt.search_back = true;

	for(int i = 0; earliest_found_line > 0 && i < num_virt_ext && !stop; i++) {
		int64_t min_line = -1; /* max_line is 1-based, but internal line numbers (min_line) are 0-based. */
		/* Search backwards in b from max_line for the first occurrence of regex. */
		b->opt.case_search = virt_ext[i].case_sensitive;
		const int64_t max_line = min(virt_ext[i].max_line, line_limit);
		goto_line(b, max_line - 1);
		goto_pos(b, max_line == line_limit && pos_limit != -1 ? pos_limit : b->cur_line_desc->line_len);
		b->find_string = virt_ext[i].regex;
		b->find_string_changed = 1;
		while (find_regexp(b, NULL, true, false) == OK) {
			min_line = b->cur_line;
			D(fprintf(stderr, "[%d] --- found match for '%s' on line <%d>\n", __LINE__, ext, min_line);)
			if (min_line == 0) break;
		}
		if (min_line > -1) {
			if (min_line < earliest_found_line) {
				earliest_found_line = min_line;
				ext = virt_ext[i].ext;
			}
		}
	}

	goto_line_pos(b, b_cur_line, b_cur_pos);
	b->opt.search_back = b_search_back;
	b->opt.case_search = b_case_search;
	b->last_was_regexp = b_last_was_regexp;
	b->find_string = find_string;
	b->find_string_changed = 1;

	return ext;
}



/* Performs an automatic preferences operation, which can be loading or saving,
   depending on the function pointed to by prefs_func. The extension given by
   ext is used in order to locate the appropriate file. If ext is NULL, the
   extension of the buffer filename is used instead. If b is NULL, ERROR is
   returned.  */

static int do_auto_prefs(buffer *b, const char * ext, int (prefs_func)(buffer *, const char *)) {
	if (!b) return ERROR;
	assert_buffer(b);
	if (!ext && !(ext = virtual_extension(b)) && !(ext = extension(b->filename))) return HAS_NO_EXTENSION;

	/* Try global autoprefs -- We always load these before ~/.ne autoprefs.
	   That way the user can override whatever he wants, but anything he
	   doesn't override still gets passed through. */

	int error = OK;
	char *auto_name, *prefs_dir;
	if (*prefs_func == load_prefs && (prefs_dir = exists_gprefs_dir())) {
		if (auto_name = malloc(strlen(ext) + strlen(prefs_dir) + strlen(PREF_FILE_SUFFIX) + 2)) {
			strcat(strcat(strcpy(auto_name, prefs_dir), ext), PREF_FILE_SUFFIX);
			error = prefs_func(b, auto_name);
			free(auto_name);
			/* We don't "return error;" here because we still haven't loaded
				the user's autoprefs. */
		}
	}

	/* Try ~/.ne autoprefs */
	if (prefs_dir = exists_prefs_dir()) {
		if (auto_name = malloc(strlen(ext) + strlen(prefs_dir) + strlen(PREF_FILE_SUFFIX) + 2)) {
			strcat(strcat(strcpy(auto_name, prefs_dir), ext), PREF_FILE_SUFFIX);
			error = prefs_func(b, auto_name);
			free(auto_name);
		}
		else error = OUT_OF_MEMORY;
	}
	else error = CANT_FIND_PREFS_DIR;

	if (do_syntax && !b->syn) load_syntax_by_name(b, ext);

	return error;
}


/* These functions just instantiate do_auto_prefs to either
load_prefs or save_prefs. */


int load_auto_prefs(buffer * const b, const char *name) {
	return do_auto_prefs(b, name, load_prefs);
}


int save_auto_prefs(buffer * const b, const char *name) {
	/* In practice, the only time we call save_auto_prefs with a name is
	   when we save the default prefs. If that changes, so too must this
	   method of setting this static flag used by save_prefs. */
	saving_defaults = name ? true : false;
	return do_auto_prefs(b, name, save_prefs);
}

/* This bit has to do with pushing and popping preferences
   on the prefs stack. */

#define MAX_PREF_STACK_SIZE  32

typedef struct {
	int pcount;
	int psize;
	options_t pref[MAX_PREF_STACK_SIZE];
} pref_stack_t;

static pref_stack_t pstack = { 0, MAX_PREF_STACK_SIZE };

int push_prefs(buffer * const b) {
	char msg[120];
	if (pstack.pcount >= MAX_PREF_STACK_SIZE) {
		sprintf(msg, "PushPrefs failed, stack is full. %d prefs now on stack.", pstack.pcount);
		print_message(msg);
		return PREFS_STACK_FULL;
	}

	pstack.pref[pstack.pcount++] = b->opt;
	sprintf(msg, "User Prefs Pushed, %d Prefs now on stack.", pstack.pcount);
	print_message(msg);
	return OK;
}

int pop_prefs(buffer * const b) {
	char msg[120];
	if (pstack.pcount <= 0) {
		sprintf(msg, "PopPrefs failed, stack is empty.");
		print_message(msg);
		return PREFS_STACK_EMPTY;
	}
	else {
		b->opt = pstack.pref[--pstack.pcount];
		sprintf(msg, "User Prefs Popped, %d Prefs remain on stack.", pstack.pcount);
		print_message(msg);
		return OK;
	}
}

/* Bracketed paste support has its own private options cache. */

static options_t bpaste_opt_cache;
static struct {
	int64_t pos;
	int64_t line;
	int cur_y;
} bpaste_marks[2];

#define BUFSIZE 2048
static char cmdbuf[BUFSIZE+1];

void bracketed_paste_begin(buffer *b) {
	if (!bracketed_paste || b->bpaste_support < 1 || b->bpasting) return;
	bpaste_opt_cache = b->opt;
	b->bpasting = 1;
	if (b->bpaste_support == 1) {
		bpaste_marks[0].pos = b->cur_pos;
		bpaste_marks[0].line = b->cur_line;
		bpaste_marks[0].cur_y = b->cur_y;
		b->opt.auto_indent = 0;
		start_undo_chain(b);
	} else if (b->bpaste_support == 2) {
		cmdbuf[0] = '\0';
		strncat(strncpy(cmdbuf, "Macro ", BUFSIZE), b->bpaste_macro_before, BUFSIZE);
		execute_command_line(b, cmdbuf);
	} else b->bpasting = 0;
}

void bracketed_paste_end(buffer *b) {
	b->bpasting = 0;
	if (!bracketed_paste || b->bpaste_support < 1) return;
	b->opt = bpaste_opt_cache;
	if (b->bpaste_support == 1) {
		if (b->opt.auto_indent && bpaste_marks[0].line < b->cur_line && bpaste_marks[0].pos > 0) {
			int64_t block_start_line_tmp = b->block_start_line, block_start_pos_tmp = b->block_start_pos;
			int marking_tmp = b->marking, mark_is_vertical_tmp = b->mark_is_vertical;
			b->block_start_line = bpaste_marks[0].line + 1;
			b->block_start_pos = 0;
			b->marking = 1;
			b->mark_is_vertical = 0;
			snprintf(cmdbuf, BUFSIZE, "> %ld s", bpaste_marks[0].pos);
			shift(b, cmdbuf, &cmdbuf[0], BUFSIZE);
			b->block_start_line = block_start_line_tmp;
			b->block_start_pos = block_start_pos_tmp;
			b->marking - marking_tmp;
			b->mark_is_vertical = mark_is_vertical_tmp;
		}
		end_undo_chain(b);
	} else if (b->bpaste_support == 2) {
		strncat(strncpy(cmdbuf, "Macro ", BUFSIZE), b->bpaste_macro_after, BUFSIZE);
		execute_command_line(b, cmdbuf);
	}
}
