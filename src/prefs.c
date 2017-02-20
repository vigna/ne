/* Preferences functions.

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


/* These are the names of ne's autoprefs directory. */

#define PREFS_DIR ".ne"

/* This string is appended to the filename extension. It tries to
be enough strange to avoid clashes with macros. */

#define PREF_FILE_SUFFIX   "#ap"

/* The name of the virtual extensions file (local and global). */

#define VIRTUAL_EXT_NAME    ".extensions"
#define VIRTUAL_EXT_NAME_G   "extensions"

/* We suppose a configuration file won't be bigger than this. Having it
bigger just causes a reallocation. */

#define PREF_FILE_SIZE_GUESS 256

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

		if (saving_defaults) {
			/* We only save the global flags that differ from their defaults. */
			/* Make sure these are in sync with the defaults near the top of ne.c. */
#ifndef ALTPAGING
			if (req_order)       record_action(cs, REQUESTORDER_A,  req_order,      NULL, verbose_macros);
#else
			if (!req_order)      record_action(cs, REQUESTORDER_A,  req_order,      NULL, verbose_macros);
#endif
			if (fast_gui)        record_action(cs, FASTGUI_A,       fast_gui,       NULL, verbose_macros);
			if (!status_bar)     record_action(cs, STATUSBAR_A,     status_bar,     NULL, verbose_macros);
			if (!verbose_macros) record_action(cs, VERBOSEMACROS_A, verbose_macros, NULL, verbose_macros);
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
		error = play_macro(b, cs);
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


/* "virtual extension" is all about preferences and syntax for files which
   have no extension. We use a "virtual extensions" file VIRTUAL_EXT_NAME
   (first local, then maybe global) to determine what auto_prefs and
   syntax to load based on a buffer's contents. */

static char *determine_virtual_extension( buffer * const b, char *vname, buffer **vbp) {
	char *virt_ext = NULL;
	buffer * vb = *vbp;
	/* Our find_regexp() is geared to work on buffers rather than streams, so we'll create a
	   stand-alone buffer. This also buys us proper handling of encodings. */
	if (vb == NULL && (*vbp = vb = alloc_buffer(NULL))) {
		clear_buffer(vb);
		vb->opt.do_undo = 0;
		vb->opt.auto_prefs = 0;
	}
	if (vb) {
		if (! buffer_file_modified(vb, vname) || load_file_in_buffer(vb, vname) == OK ) {
			goto_line(vb,0);
			goto_column(vb,0);
			int found = 0;
			int stop = false;
			/* Examine each line in vb. Most functions in navigation.c expect to display
			   the buffer we're navigating through, but we don't want to show this buffer! */
			int64_t earliest_found_line = INT64_MAX;
			int skip_first = false;
			vb->find_string = str_dup( "^\\s*(\\w+)\\s+([0-9]+)\\s+(.+)" );
			vb->find_string_changed = 1;
			while ( earliest_found_line > 0 && find_regexp(vb,NULL,skip_first) == OK && !stop) {
				skip_first = true;
				char *ext         = nth_regex_substring(vb->cur_line_desc, 1);
				char *maxline_str = nth_regex_substring(vb->cur_line_desc, 2);
				char *regex       = nth_regex_substring(vb->cur_line_desc, 3);
				D(fprintf(stderr,"[%d] Checking for <%s> <%s> <%s>\n",__LINE__, ext, maxline_str, regex);)
				if (ext && maxline_str && regex ) {
					errno = 0;
					char *endptr;
					int64_t maxline = strtoll(maxline_str, &endptr, 0);
					if (maxline < 1 || errno) maxline = UINT64_MAX;
					maxline--; /* users are 1-based, but internal line numbers are 0-based. */
					/* Search in b for the first occurance of regex. */
					int64_t b_cur_line    = b->cur_line;
					int64_t b_cur_pos     = b->cur_pos;
					int     b_search_back = b->opt.search_back;
					b->opt.search_back = false;
					goto_line(b, 0);
					goto_pos(b, 0);
					free(b->find_string);
					b->find_string_changed = 1;
					b->find_string = regex;
					regex = NULL;
					if ( find_regexp(b, NULL, false) == OK ) {
						D(fprintf(stderr,"[%d] --- found on line <%d>\n",__LINE__, b->cur_line);)
						if (b->cur_line <= maxline && b->cur_line < earliest_found_line) {
							found++;
							earliest_found_line = b->cur_line;
							if (virt_ext) free(virt_ext);
							virt_ext = ext;
							ext = NULL;
						}
					}
					goto_line(b, b_cur_line);
					goto_pos(b, b_cur_pos);
					b->opt.search_back = b_search_back;
				}
				if (maxline_str) free(maxline_str);
				if (ext) free(ext);
				if (regex) free(regex);
			}
		}
		/* free_buffer(vb); Not any more, because we're caching in *vbp. */
	}
	return virt_ext;
}

/* virtual_extension() returns an extension determined by a buffers contents and
   the user's VIRTUAL_EXT_NAME file or possibly the global VIRTUAL_EXT_NAME file.
   It's a pointer to a string that the caller must eventually free(). */

static char *virtual_extension(buffer * const b) {

	static buffer *g_extensions_buf, *u_extensions_buf;
	int error = OK;
	char *virt_ext = NULL;
	char *virt_name, *prefs_dir;

	/* Try the user's ~/.ne/.extenstion first. We only check the global config if the
	   users local one doesn't determine an extension. */
	if (prefs_dir = exists_prefs_dir()) {
		if (virt_name = malloc(strlen(VIRTUAL_EXT_NAME) + strlen(prefs_dir) + 2)) {
			strcat(strcpy(virt_name, prefs_dir), VIRTUAL_EXT_NAME);
			virt_ext = determine_virtual_extension(b, virt_name, &u_extensions_buf);
			free(virt_name);
		}
	}
	if (virt_ext == NULL && (prefs_dir = exists_gprefs_dir())) {
		if (virt_name = malloc(strlen(VIRTUAL_EXT_NAME_G) + strlen(prefs_dir) + 2)) {
			strcat(strcpy(virt_name, prefs_dir), VIRTUAL_EXT_NAME_G);
			virt_ext = determine_virtual_extension(b, virt_name, &g_extensions_buf);
			free(virt_name);
		}
	}
	return virt_ext;
}


/* Performs an automatic preferences operation, which can be loading or saving,
   depending on the function pointed to by prefs_func. The extension given by
   ext is used in order to locate the appropriate file. If ext is NULL, the
   extension of the buffer filename is used instead. If b is NULL, ERROR is
   returned.  */

static int do_auto_prefs(buffer *b, const char * ext, int (prefs_func)(buffer *, const char *)) {
	/* Track virtual_extension vext separate from ext b/c vext must be free()'d. */
	char *vext = NULL;
	if (!b) return ERROR;

	assert_buffer(b);

	if (!ext && !(ext = vext = virtual_extension(b)) && !(ext = extension(b->filename))) return HAS_NO_EXTENSION;

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

	if (vext) free(vext);

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
		sprintf(msg,"PushPrefs failed, stack is full. %d prefs now on stack.", pstack.pcount);
		print_message(msg);
		return PREFS_STACK_FULL;
	}

	pstack.pref[pstack.pcount++] = b->opt;
	sprintf(msg,"User Prefs Pushed, %d Prefs now on stack.", pstack.pcount);
	print_message(msg);
	return OK;
}

int pop_prefs(buffer * const b) {
	char msg[120];
	if (pstack.pcount <= 0) {
		sprintf(msg,"PopPrefs failed, stack is empty.");
		print_message(msg);
		return PREFS_STACK_EMPTY;
	}
	else {
		b->opt = pstack.pref[--pstack.pcount];
		sprintf(msg,"User Prefs Popped, %d Prefs remain on stack.", pstack.pcount);
		print_message(msg);
		return OK;
	}
}
