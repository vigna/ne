/* Menu handling function. Includes also key and menu configuration parsing.

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
#include "termchar.h"

/* The default number of menus. */

#define DEF_MENU_NUM 8

/* The number of extras spaces around each menu item, with and without standout. */

#define MENU_EXTRA 2
#define MENU_NOSTANDOUT_EXTRA 4

/* The maximum length of the status bar, excluding the file name. */

#define MAX_BAR_BUFFER_SIZE 128

/* The maximum length of the flag string. */

#define MAX_FLAG_STRING_SIZE 32

/* The maximum length of a message. */

#define MAX_MESSAGE_LENGTH 1024

/* The name of the menu configuration file. */

#define MENU_CONF_NAME ".menus"

/* The name of the key bindings file. */

#define KEY_BINDINGS_NAME ".keys"


/* The keywords used in the configuration files. */

#define MENU_KEYWORD "MENU"
#define ITEM_KEYWORD "ITEM"
#define KEY_KEYWORD "KEY"
#define SEQ_KEYWORD "SEQ"


/* This structure defines a menu item. command_line points to
the command line to be executed when the menu item is selected. */

typedef struct {
	const char *text;
	const char *command_line;
} menu_item;



/* This structure defines a menu. It contains number of items, the
horizontal position of the menu, its width, the current item, the menu
name and a pointer to the item array. Note that xpos has to be greater
than zero. */

typedef struct {
	int item_num;
	int xpos, width;
	int cur_item;
	const char *text;
	const menu_item *items;
} menu;

#ifndef ALTPAGING
	#define PICK(A,B,C,D) {A,B},
#else
	#define PICK(A,B,C,D) {C,D},
#endif

/* The following structures describe ne's standard menus. */

static menu_item file_item[] =
	{
		{ "Open...     ^O", OPEN_ABBREV },
		{ "Open New... [N", OPENNEW_ABBREV },
		{ "Save        ^S", SAVE_ABBREV },
		{ "Save As...    ", SAVEAS_ABBREV },
		{ "Save All      ", SAVEALL_ABBREV },
		{ "Quit Now    [Q", QUIT_ABBREV },
		{ "Save&Exit   [X", EXIT_ABBREV },
		{ "About         ", ABOUT_ABBREV }
	};

static const menu_item documents_item[] =
	{
		{ "New       ^D", NEWDOC_ABBREV },
		{ "Clear       ", CLEAR_ABBREV },
		{ "Close     ^Q", CLOSEDOC_ABBREV },
		{ "Next   f2/[D", NEXTDOC_ABBREV },
		{ "Prev      f3", PREVDOC_ABBREV },
		{ "Select... f4", SELECTDOC_ABBREV }
	};

static const menu_item edit_item[] =
	{
		{ "Mark Block  ^B", MARK_ABBREV },
		{ "Cut         ^X", CUT_ABBREV },
		{ "Copy        ^C", COPY_ABBREV },
		{ "Paste       ^V", PASTE_ABBREV },
		{ "Mark Vert   ^@", MARKVERT_ABBREV },
		{ "Paste Vert  ^W", PASTEVERT_ABBREV },
		{ "Through     [T", THROUGH_ABBREV },
		{ "Erase         ", ERASE_ABBREV },
		{ "Delete EOL  [Y", DELETEEOL_ABBREV },
		{ "Delete Line ^Y", DELETELINE_ABBREV },
		{ "Undel Line  ^U", UNDELLINE_ABBREV },
		{ "Del Prev Word ", DELETEPREVWORD_ABBREV },
		{ "Del Next Word ", DELETENEXTWORD_ABBREV },
		{ "Open Clip   [O", OPENCLIP_ABBREV },
		{ "Save Clip   [S", SAVECLIP_ABBREV }
	};


static const menu_item search_item[] =
	{
		{ "Find...        ^F", FIND_ABBREV },
		{ "Find RegExp... ^_", FINDREGEXP_ABBREV },
		{ "Replace...     ^R", REPLACE_ABBREV },
		{ "Replace Once...  ", REPLACEONCE_ABBREV },
		{ "Replace All...   ", REPLACEALL_ABBREV },
		{ "Repeat Last    ^G", REPEATLAST_ABBREV },
		{ "Goto Line...   ^J", GOTOLINE_ABBREV },
		{ "Goto Col...    [J", GOTOCOLUMN_ABBREV },
		{ "Goto Mark        ", GOTOMARK_ABBREV },
		{ "Match Bracket  ^]", MATCHBRACKET_ABBREV },
		{ "Set Bookmark   [K", SETBOOKMARK_ABBREV },
		{ "Goto Bookmark  [G", GOTOBOOKMARK_ABBREV }
	};


static const menu_item macros_item[] =
	{
		{ "Start/Stop Rec ^T", RECORD_ABBREV },
		{ "Play Once   f9/[M", PLAYONCE_ABBREV },
		{ "Play Many...     ", PLAY_ABBREV },
		{ "Play Macro...    ", MACRO_ABBREV },
		{ "Open Macro...    ", OPENMACRO_ABBREV },
		{ "Save Macro...    ", SAVEMACRO_ABBREV },
	};


static const menu_item extras_item[] =
	{
		{ "Exec...      ^K", EXEC_ABBREV },
		{ "Suspend      ^Z", SUSPEND_ABBREV },
		{ "Help...     f10", HELP_ABBREV },
		{ "Refresh      ^L", REFRESH_ABBREV },
		{ "Undo      f5/[U", UNDO_ABBREV },
		{ "Redo      f6/[R", REDO_ABBREV },
		{ "Center         ", CENTER_ABBREV },
		{ "Shift Right    ", SHIFT_ABBREV },
		{ "Shift Left     ", SHIFTLEFT_ABBREV },
		{ "Paragraph    [P", PARAGRAPH_ABBREV },
		{ "Adjust View  ^^", ADJUSTVIEW_ABBREV },
		{ "Middle View  [C", MIDDLEVIEW_ABBREV },
		{ "ToUpper      [V", TOUPPER_ABBREV },
		{ "ToLower      [L", TOLOWER_ABBREV },
		{ "Capitalize     ", CAPITALIZE_ABBREV },
		{ "AutoComplete [I", AUTOCOMPLETE_ABBREV },
		{ "UTF-8          ", UTF8_ABBREV }
	};



static const menu_item navigation_item[] =
	{
		{ "Move Left       ", MOVELEFT_ABBREV },
		{ "Move Right      ", MOVERIGHT_ABBREV },
		{ "Line Up         ", LINEUP_ABBREV },
		{ "Line Down       ", LINEDOWN_ABBREV },
PICK(   "Prev Page     ^P", PREVPAGE_ABBREV    , "Prev Page       ", PREVPAGE_ABBREV)
PICK(   "Next Page     ^N", NEXTPAGE_ABBREV    , "Next Page       ", NEXTPAGE_ABBREV)
PICK(   "Page Up         ", PAGEUP_ABBREV      , "Page Up       ^P", PAGEUP_ABBREV)
PICK(   "Page Down       ", PAGEDOWN_ABBREV    , "Page Down     ^N", PAGEDOWN_ABBREV)
		{ "Start Of File [A", MOVESOF_ABBREV },
		{ "End Of File   [E", MOVEEOF_ABBREV },
		{ "Start Of Line ^A", MOVESOL_ABBREV },
		{ "End Of Line   ^E", MOVEEOL_ABBREV },
		{ "Top Of Screen   ", MOVETOS_ABBREV },
		{ "Bottom Of Screen", MOVEBOS_ABBREV },
PICK(   "Incr Up     Home", MOVEINCUP_ABBREV   , "Incr Up         ", MOVEINCUP_ABBREV)
PICK(   "Incr Down    End", MOVEINCDOWN_ABBREV , "Incr Down       ", MOVEINCDOWN_ABBREV)
		{ "Prev Word  f7/[B", PREVWORD_ABBREV },
		{ "Next Word  f8/[F", NEXTWORD_ABBREV }
	};



static const menu_item prefs_item[] =
	{
		{ "Tab Size...     ", TABSIZE_ABBREV },
		{ "Tabs as Spaces  ", TABS_ABBREV },
		{ "Insert/Over  Ins", INSERT_ABBREV },
		{ "Free Form       ", FREEFORM_ABBREV },
		{ "Status Bar      ", STATUSBAR_ABBREV },
		{ "Hex Code        ", HEXCODE_ABBREV },
		{ "Fast GUI        ", FASTGUI_ABBREV },
		{ "Word Wrap     [W", WORDWRAP_ABBREV },
		{ "Right Margin    ", RIGHTMARGIN_ABBREV },
		{ "Auto Indent     ", AUTOINDENT_ABBREV },
		{ "Request Order   ", REQUESTORDER_ABBREV },
		{ "Preserve CR     ", PRESERVECR_ABBREV },
		{ "Save CR/LF    [Z", CRLF_ABBREV },
		{ "Load Prefs...   ", LOADPREFS_ABBREV },
		{ "Save Prefs...   ", SAVEPREFS_ABBREV },
		{ "Load Auto Prefs ", LOADAUTOPREFS_ABBREV },
		{ "Save Auto Prefs ", SAVEAUTOPREFS_ABBREV },
		{ "Save Def Prefs  ", SAVEDEFPREFS_ABBREV },
	};

static menu def_menus[DEF_MENU_NUM] = {
	{
		sizeof(file_item) / sizeof(menu_item),
		1,
		14,
		0,
		"File",
		file_item
	},
	{
		sizeof(documents_item) / sizeof(menu_item),
		6,
		12,
		0,
		"Documents",
		documents_item
	},
	{
		sizeof(edit_item) / sizeof(menu_item),
		16,
		14,
		0,
		"Edit",
		edit_item
	},
	{
		sizeof(search_item) / sizeof(menu_item),
		21,
		17,
		0,
		"Search",
		search_item
	},
	{
		sizeof(macros_item) / sizeof(menu_item),
		28,
		17,
		0,
		"Macros",
		macros_item
	},
	{
		sizeof(extras_item) / sizeof(menu_item),
		35,
		15,
		0,
		"Extras",
		extras_item
	},
	{
		sizeof(navigation_item) / sizeof(menu_item),
		42,
		16,
		0,
		"Navigation",
		navigation_item
	},
	{
		sizeof(prefs_item) / sizeof(menu_item),
		53,
		16,
		0,
		"Prefs",
		prefs_item
	}
};



/* current_menu remembers the last menu activated. menu_num is the number of
menus. */

static int current_menu, menu_num = DEF_MENU_NUM;


/* menus points to an array of menu_num menu structures. */

static menu *menus = def_menus;


#ifdef NE_TEST
int dump_config(void) {
	int menu, item, key;
	FILE *f;

	if (!(f = fopen("ne_test_dump_config","w")) ) return ERROR;

	for (menu = 0; menu < menu_num; menu++) {
		fprintf(f,"%s \"%s\"\n", MENU_KEYWORD, menus[menu].text );
		for (item = 0; item < menus[menu].item_num; item++) {
			fprintf(f,"%s \"%s\" \"%s\"\n", ITEM_KEYWORD,
															menus[menu].items[item].text,
															menus[menu].items[item].command_line);
		}
		fprintf(f,"\n");
	}

	for (key = 0; key < NUM_KEYS; key++) {
		if (key_binding[key] && key_binding[key][0])
			fprintf(f,"%s\t%4x\t%s\n",KEY_KEYWORD, key, key_binding[key] );
	}
	fclose(f);
	return OK;
}
#endif

static void draw_cur_item(const int n) {
	move_cursor(menus[n].cur_item + 1, menus[n].xpos - (fast_gui || !standout_ok));
	if (!fast_gui && standout_ok) output_chars(menus[n].items[menus[n].cur_item].text, NULL, menus[n].width - (cursor_on_off_ok ? 0 : 1), true);
}


static void undraw_cur_item(const int n) {
	if (!fast_gui && standout_ok)  {
		set_attr(0);
		standout_on();
		move_cursor(menus[n].cur_item + 1, menus[n].xpos);
		output_chars(menus[n].items[menus[n].cur_item].text, NULL, menus[n].width - (cursor_on_off_ok ? 0 : 1), true);
		standout_off();
	}
}


/* Draws a given menu. It also draws the current menu item. */

static void draw_menu(const int n) {
	assert(menus[n].xpos > 0);

	if (menus[n].cur_item + 1 + (standout_ok == 0) >= ne_lines - 1) menus[n].cur_item = 0;

	move_cursor(0, menus[n].xpos);
	set_attr(0);
	output_string(menus[n].text, true);

	int i;
	for(i = 0; i < menus[n].item_num; i++) {
		if (i + 1 + (standout_ok == 0) >= ne_lines - 1) break;

		move_cursor(i + 1, menus[n].xpos - 1);

		if (!standout_ok) output_string("|", false);

		standout_on();
		output_string(" ", false);
		output_string(menus[n].items[i].text, true);
		output_string(" ", false);
		standout_off();

		if (!standout_ok) output_string("|", false);
	}

	if (!standout_ok) {
		move_cursor(i + 1, menus[n].xpos - 1);
		for(i = 0; i < menus[n].width + (standout_ok ? MENU_EXTRA : MENU_NOSTANDOUT_EXTRA); i++) output_string("-", false);
	}

	draw_cur_item(n);
}


/* Undraws a menu. This is obtained by refreshing part of the
screen via output_line_desc(). */

static void undraw_menu(const int n) {
	set_attr(0);
	standout_on();
	move_cursor(0, menus[n].xpos);
	output_string(menus[n].text, true);
	standout_off();

	line_desc *ld = cur_buffer->top_line_desc;
	for(int i = 1; i <= menus[n].item_num + (standout_ok == 0); i++) {
		if (i >= ne_lines - 1) break;
		if (ld->ld_node.next->next) {
			ld = (line_desc *)ld->ld_node.next;
			if (cur_buffer->syn) parse(cur_buffer->syn, ld, ld->highlight_state, cur_buffer->encoding == ENC_UTF8);
			output_line_desc(i, menus[n].xpos - 1, ld, cur_buffer->win_x + menus[n].xpos - 1, menus[n].width + (standout_ok ? MENU_EXTRA : MENU_NOSTANDOUT_EXTRA), cur_buffer->opt.tab_size, false, cur_buffer->encoding == ENC_UTF8, cur_buffer->syn ? attr_buf : NULL, NULL, 0);
		}
		else {
			move_cursor(i, menus[n].xpos - 1);
			clear_to_eol();
		}
	}
}



static void draw_next_item(void) {
	undraw_cur_item(current_menu);
	menus[current_menu].cur_item = (menus[current_menu].cur_item + 1) % menus[current_menu].item_num;
	if (menus[current_menu].cur_item + 1 + (standout_ok == 0) >= ne_lines - 1) menus[current_menu].cur_item = 0;
	draw_cur_item(current_menu);
}

static void draw_prev_item(void) {
	undraw_cur_item(current_menu);
	if (--(menus[current_menu].cur_item) < 0) menus[current_menu].cur_item = menus[current_menu].item_num - 1;
	if (menus[current_menu].cur_item + 1 + (standout_ok == 0) >= ne_lines - 1) menus[current_menu].cur_item = ne_lines - 3 - (standout_ok == 0) ;
	draw_cur_item(current_menu);
}

static void draw_item(const int item) {
	undraw_cur_item(current_menu);
	menus[current_menu].cur_item = item;
	draw_cur_item(current_menu);
}

static void draw_next_menu(void) {
	undraw_menu(current_menu);
	current_menu = (current_menu + 1) % menu_num;
	if (menus[current_menu].xpos >= ne_columns) current_menu = 0;
	draw_menu(current_menu);
}

static void draw_prev_menu(void) {
	undraw_menu(current_menu);
	if (--current_menu < 0) current_menu = menu_num - 1;
	while(menus[current_menu].xpos >= ne_columns) current_menu--;
	draw_menu(current_menu);
}

int search_menu_title(int n, const int c) {
	for(int i = 0; i < menu_num - 1; i++) {
		if (menus[++n % menu_num].xpos >= ne_columns) continue;
		if (menus[n % menu_num].text[0] == c) return n % menu_num;
	}

	return -1;
}

int search_menu_item(int n, int c) {
	c = toupper(c);

	for(int i = 0, j = menus[n].cur_item; i < menus[n].item_num - 1; i++) {
		if (++j % menus[n].item_num + 1 + (standout_ok == 0) >= ne_lines - 1) continue;
		if (menus[n].items[j % menus[n].item_num].text[0] == c) return j % menus[n].item_num;
	}

	return -1;
}


static void item_search(const int c) {
	int new_item;

	if (c >= 'a' && c <= 'z') {
		new_item = search_menu_item(current_menu, c);
		if (new_item >= 0) draw_item(new_item);
	}
	else if (c >= 'A' && c <= 'Z') {
		new_item = search_menu_title(current_menu, c);
		if (new_item >= 0) {
			undraw_menu(current_menu);
			current_menu = new_item;
			draw_menu(current_menu);
		}
	}
}


static void draw_first_menu(void) {
	move_cursor(0,0);

	set_attr(0);
	standout_on();
	if (!fast_gui && standout_ok) cursor_off();

	for(int i = 0, n = 0; i < ne_columns; ) {
		output_string(" ", false);
		i++;
		if (n < menu_num) {
			output_string(menus[n].text, true);
			i += strlen(menus[n].text);
			n++;
		}
	}

	if (standout_ok) standout_off();

	if (menus[current_menu].xpos >= ne_columns) current_menu = 0;
	draw_menu(current_menu);
}

static void undraw_last_menu(void) {
	undraw_menu(current_menu);
	update_line(cur_buffer, 0, false, false);
	cursor_on();
}


static void do_menu_action(void) {
	undraw_last_menu();
	print_error(execute_command_line(cur_buffer, menus[current_menu].items[menus[current_menu].cur_item].command_line));
}

/* showing_msg tells draw_status_bar() that a message is currently shown, and
   should be cancelled only on the next refresh. Bar gone says that the status
   bar doesn't exists any longer, so we have to rebuild it entirely. */

static bool showing_msg;
static bool bar_gone = true;



/* Resets the status bar. It does not perform the refresh, just sets bar_gone
   to true. */

void reset_status_bar(void) {
	bar_gone = true;
}


/* This support function returns a copy of the status string which is
never longer than MAX_FLAG_STRING_SIZE characters. The string is kept in
a static buffer which is overwritten at each call. Note that the string
includes a leading space. This way, if both the line numbers and
the flags are updated the cursor does not need to be moved after
printing the numbers (an operation which usually needs the output
of several characters). */

char *gen_flag_string(const buffer * const b) {

	static char string[MAX_FLAG_STRING_SIZE];
	const int ch = b->cur_pos < b->cur_line_desc->line_len ? (b->encoding == ENC_UTF8 ? utf8char(&b->cur_line_desc->line[b->cur_pos]) : (unsigned char)b->cur_line_desc->line[b->cur_pos]) : -1;
	int i = 0;

	string[i++] = ' ';

	string[i++] = b->opt.insert         ? 'i' : '-';
	string[i++] = b->opt.auto_indent    ? 'a' : '-';
	string[i++] = b->opt.search_back    ? 'b' : '-';
	string[i++] = b->opt.case_search    ? 'c' : '-';
	string[i++] = b->opt.word_wrap      ? 'w' : '-';
	string[i++] = b->opt.free_form      ? 'f' : '-';
	string[i++] = b->opt.auto_prefs     ? 'p' : '-';
	string[i++] = verbose_macros        ? 'v' : '-';
	string[i++] = b->opt.do_undo        ? (b->atomic_undo ? 'U' : 'u') : '-';
	string[i++] = b->opt.read_only      ? 'r' : '-';
	string[i++] = b->opt.tabs           ? (b->opt.shift_tabs ? 'T' : 't' ) : '-';
	string[i++] = b->opt.del_tabs       ? 'd' : '-';
	string[i++] = b->opt.binary         ? 'B' : ((line_desc *)b->line_desc_list.tail_pred)->line_len ? '!' : '-';
	string[i++] = b->marking            ? (b->mark_is_vertical ? 'V' :'M') : '-';
	string[i++] = b->recording          ? 'R' : '-';
	string[i++] = b->opt.preserve_cr    ? 'P' : '-';
	string[i++] = b->is_CRLF            ? 'C' : '-';
	string[i++] = io_utf8               ? '@' : '-';
	string[i++] = b->encoding != ENC_8_BIT? (b->encoding == ENC_UTF8 ? 'U' : 'A') : '8';
	string[i++] = b->is_modified        ? '*' : '-';

	if (b->opt.hex_code && !fast_gui) {
		string[i++] = ' ';
		if (ch > 0xFFFF) {
			string[i++] = "0123456789abcdef"[(ch >> 28) & 0x0f];
			string[i++] = "0123456789abcdef"[(ch >> 24) & 0x0f];
			string[i++] = "0123456789abcdef"[(ch >> 20) & 0x0f];
			string[i++] = "0123456789abcdef"[(ch >> 16) & 0x0f];
		}
		else for(int j = 0; j < 4; j++) string[i++] = ' ';
		if (ch > 0xFF) {
			string[i++] = "0123456789abcdef"[(ch >> 12) & 0x0f];
			string[i++] = "0123456789abcdef"[(ch >> 8) & 0x0f];
		}
		else for(int j = 0; j < 2; j++) string[i++] = ' ';
		if (ch > -1) {
			string[i++] = "0123456789abcdef"[(ch >> 4) & 0x0f];
			string[i++] = "0123456789abcdef"[ch & 0x0f];
		}
		else for(int j = 0; j < 2; j++) string[i++] = ' ';
	}

	string[i] = 0;

	assert(i < MAX_FLAG_STRING_SIZE);

	return string;
}



/* Draws the status bar. If showing_msg is true, it is set to false, bar_gone
   is set to true and the update is deferred to the next call. If the bar is
   not completely gone, we try to just update the line and column numbers, and
   the flags. The function keeps track internally of their last values, so that
   unnecessary printing is avoided. */


void draw_status_bar(void) {
	static char bar_buffer[MAX_BAR_BUFFER_SIZE];
	static char flag_string[MAX_FLAG_STRING_SIZE];
	static int64_t x = -1, y = -1;
	static int percent = -1;

	if (showing_msg) {
		showing_msg = false;
		bar_gone = true;
		return;
	}

	resume_status_bar = (void (*)(const char *message))&draw_status_bar;
	set_attr(0);
	int len;

	if (!bar_gone && status_bar) {
		const int new_percent = (int)floor(((cur_buffer->cur_line + 1) * 100.0) / cur_buffer->num_lines);
		/* This is the space occupied up to "L:", included. */
		const int offset = fast_gui || !standout_ok ? 5: 3;
		const bool update_x = x != cur_buffer->win_x + cur_buffer->cur_x;
		const bool update_y = y != cur_buffer->cur_line;
		const bool update_percent = percent != new_percent;
		char *p;
		const bool update_flags = strcmp(flag_string, p = gen_flag_string(cur_buffer));
		const bool update_filename = strlen(flag_string) != strlen(p);
		const bool update = update_x || update_y || update_percent || update_flags;

		if (!update) return;

		if (!fast_gui && standout_ok) standout_on();

		x = cur_buffer->win_x + cur_buffer->cur_x;
		y = cur_buffer->cur_line;
		percent = new_percent;

		if (update_y) {
			move_cursor(ne_lines - 1, offset);
			len = sprintf(bar_buffer, "%11" PRId64, y + 1);
			output_chars(bar_buffer, NULL, len, true);
		}

		if (update_x) {
			move_cursor(ne_lines - 1, offset + 14);
			len = sprintf(bar_buffer, "%11" PRId64, x + 1);
			output_chars(bar_buffer, NULL, len, true);
		}

		if (update_percent) {
			move_cursor(ne_lines - 1, offset + 26);
			len = sprintf(bar_buffer, "%3d", percent);
			output_chars(bar_buffer, NULL, len, true);
		}

		if (update_flags) {
			strcpy(flag_string, p);
			move_cursor(ne_lines - 1, offset + 31);
			output_string(flag_string, true);
		}

		if (!fast_gui && standout_ok) standout_off();

		if (!update_filename) return;
	}


	if (status_bar) {
		percent = (int)floor(((cur_buffer->cur_line + 1) * 100.0) / cur_buffer->num_lines);
		move_cursor(ne_lines - 1, 0);
		if (!fast_gui && standout_ok) standout_on();

		strcpy(flag_string, gen_flag_string(cur_buffer));

		x = cur_buffer->win_x + cur_buffer->cur_x;
		y = cur_buffer->cur_line;

		len = sprintf(bar_buffer, fast_gui || !standout_ok ? ">> L:%11" PRId64 " C:%11" PRId64 " %3d%% %s " : " L:%11" PRId64 " C:%11" PRId64 " %3d%% %s ", y + 1, x + 1, percent, flag_string);

		move_cursor(ne_lines - 1, 0);
		output_chars(bar_buffer, NULL, len, true);

		if (len < ne_columns - 1) {
			if (cur_buffer->filename) {
	 			/* This is a bit complicated because we have to compute the width of the filename first, and then
				discard initial characters until the remaning part will fit. */

				const int encoding = detect_encoding(cur_buffer->filename, strlen(cur_buffer->filename));
				int pos = 0, width = get_string_width(cur_buffer->filename, strlen(cur_buffer->filename), encoding);

				while(width > ne_columns - 1 - len) {
					width -= get_char_width(&cur_buffer->filename[pos], encoding);
					pos = next_pos(cur_buffer->filename, pos, encoding);
				}

				output_string(cur_buffer->filename + pos, encoding == ENC_UTF8);
			}
			else output_string(UNNAMED_NAME, false);
		}

		if (!fast_gui && standout_ok) {
			output_spaces(ne_columns, NULL);
			standout_off();
		}
		else clear_to_eol();
	}
	else if (bar_gone) {
		move_cursor(ne_lines - 1, 0);
		clear_to_eol();
	}

	bar_gone = false;
}



/* Prints a message over the status bar. It also sets showing_msg and
   bar_gone. If message is NULL and showing_msg is true, we reprint
   the last message. That necessitates caching the message when it
   isn't NULL. */

void print_message(const char * const message) {
	static char msg_cache[MAX_MESSAGE_LENGTH];

	resume_status_bar = (void (*)(const char *message))&print_message;
	if (message) {
		strncpy(msg_cache, message, MAX_MESSAGE_LENGTH);
		msg_cache[MAX_MESSAGE_LENGTH - 1] = '\0';
	}

	if (message || showing_msg) {
		move_cursor(ne_lines - 1, 0);

		set_attr(0);

		if (fast_gui || !standout_ok || !status_bar) {
			clear_to_eol();
			output_string(msg_cache, true);
		}
		else {
			standout_on();
			output_string(msg_cache, true);
			output_spaces(ne_columns - strlen(msg_cache), NULL);
			standout_off();
		}

		fflush(stdout);

		showing_msg = true;
	}
}



/* Prints an error on the status bar. error_num is a global error code. The
   function returns the error code passed, and does not do anything if the
   error code is OK or ERROR. */

int print_error(const int error_num) {

	assert(error_num < ERROR_COUNT);

	if (error_num > 0) {
		print_message(error_msg[error_num]);
		alert();
	}
	return error_num;
}



/* Prints an information on the status bar. info_num is a global information
   code. Note that no beep is generated. */


void print_info(const int info_num) {

	assert(info_num < INFO_COUNT);

	print_message(info_msg[info_num]);
}



/* Rings a bell or flashes the screen, depending on the user preference. */

void alert(void) {
	if (cur_buffer->opt.visual_bell) do_flash();
	else ring_bell();
}



/* Handles the menu system: it displays the menus, parses the keyboard input,
   and eventually executes the correct command line. Note that we support ':'
   for going to the command line, alphabetic search (upper case for menus,
   lower case for items) and the cursor movement keys (by line, character,
   page). Note also the all other actions are executed, so that you can use
   shortcuts while using menus. */

void handle_menus(void) {
	draw_first_menu();

	while(true) {
		int c;
		input_class ic;
		do c = get_key_code(); while((ic = CHAR_CLASS(c)) == IGNORE);

		if (window_changed_size) {
			window_changed_size = false;
			draw_first_menu();
			continue;
		}

		switch(ic) {
		case INVALID:
			alert();
			break;

		case TAB:
			draw_next_menu();
			break;

		case ALPHA:
			if (c == ':') {
				undraw_last_menu();
				do_action(cur_buffer, EXEC_A, -1, NULL);
				return;
			}
			item_search(c);
			break;

		case RETURN:
			do_menu_action();
			return;

		case COMMAND:
			if (c < 0) c = -c - 1;
			int64_t n;
			char *p;
			const int a = parse_command_line(key_binding[c], &n, &p, false);
			if (a >= 0) {
				switch(a) {
				case MOVELEFT_A:
					draw_prev_menu();
					break;

				case MOVERIGHT_A:
					draw_next_menu();
					break;

				case LINEUP_A:
					draw_prev_item();
					break;

				case LINEDOWN_A:
					draw_next_item();
					break;

				case PREVPAGE_A:
				case PAGEUP_A:
					draw_item(0);
					break;

				case NEXTPAGE_A:
				case PAGEDOWN_A:
					draw_item(menus[current_menu].item_num - 1);
					break;

				case MOVESOL_A:
					if (current_menu != 0) {
						undraw_menu(current_menu);
						current_menu = 0;
						draw_menu(current_menu);
					}
					break;
					
				case MOVEEOL_A:
					if (current_menu != menu_num) {
						undraw_menu(current_menu);
						current_menu = menu_num - 1;
						draw_menu(current_menu);
					}
					break;
					
				case ESCAPE_A:
					undraw_last_menu();
					return;

				default:
					undraw_last_menu();
					print_error(do_action(cur_buffer, a, n, p));
					return;
				}
			}
			break;

		default:
			break;
		}

	}
}

static void error_in_menu_configuration(const int line, const char * const s) {
	fprintf(stderr, "Error in menu configuration file at line %d: %s\n", line, s);
	exit(0);
}


static void get_menu_conf(const char * menu_conf_name, char * (exists_prefs_func)()) {
	if (!menu_conf_name) menu_conf_name = MENU_CONF_NAME;

	menu *new_menus = NULL;
	menu_item *new_items = NULL;

	char * const prefs_dir = exists_prefs_func();
	if (prefs_dir) {
		char * const menu_conf = malloc(strlen(prefs_dir) + strlen(menu_conf_name) + 1);
		if (menu_conf) {
			strcat(strcpy(menu_conf, prefs_dir), menu_conf_name);
			char_stream *cs;
			if ((cs = load_stream(NULL, menu_conf_name, false, false)) || (cs = load_stream(NULL, menu_conf, false, false))) {

				for(int pass = 0; pass < 2; pass++) {
					char *p = cs->stream;
					int line = 1;
					int cur_menu = -1;
					int cur_item = 0, num_items_in_menu = 0;

					while(p - cs->stream < cs->len) {
						if (*p) {

							if (!cmdcmp(MENU_KEYWORD, p)) {

								if (cur_menu < 0 || num_items_in_menu) {
									cur_menu++;
									num_items_in_menu = 0;

									if (pass) {
										while(*p && *p++ != '"');
										if (*p) {
											new_menus[cur_menu].text = p;
											while(*p && *++p != '"');
											if (*p) {
												*p++ = 0;
												if (cur_menu == 0) new_menus[0].xpos = 1;
												else new_menus[cur_menu].xpos = new_menus[cur_menu - 1].xpos + strlen(new_menus[cur_menu - 1].text) + 1;
												new_menus[cur_menu].items = &new_items[cur_item];
											}
											else error_in_menu_configuration(line, "menu name has to end with quotes.");
										}
										else error_in_menu_configuration(line, "menu name has to start with quotes.");
									}
								}
								else if (cur_menu >= 0) error_in_menu_configuration(line - 1, "no items specified for this menu.");

							}
							else if (!cmdcmp(ITEM_KEYWORD, p)) {
								if (cur_menu < 0) error_in_menu_configuration(line, "no menu specified for this item.");

								if (pass) {
									while(*p && *p++ != '"');
									if (*p) {
										new_items[cur_item].text = p;
										while(*p && *++p != '"');
										if (*p) {
											*p++ = 0;
											if (num_items_in_menu == 0 || strlen(new_items[cur_item].text) == new_menus[cur_menu].width) {
												if (num_items_in_menu == 0) {
													if ((new_menus[cur_menu].width = strlen(new_items[cur_item].text)) == 0)
														error_in_menu_configuration(line, "menu item name width has to be greater than zero.");
												}
												while (isasciispace(*p)) p++;
												if (*p) {
													new_items[cur_item].command_line = p;
													new_menus[cur_menu].item_num = num_items_in_menu + 1;
												}
												else error_in_menu_configuration(line, "no command specified.");
											}
											else error_in_menu_configuration(line, "menu item name width has to be constant throughout the menu.");
										}
										else error_in_menu_configuration(line, "menu item name has to end with quotes.");
									}
									else error_in_menu_configuration(line, "menu item name has to start with quotes.");
								}
								num_items_in_menu++;
								cur_item++;
							}

						}
						line++;
						p += strlen(p) + 1;
					}

					if (pass == 0) {
						if (!num_items_in_menu) error_in_menu_configuration(line - 1, "no items specified for this menu.");
						if (cur_menu == -1 || cur_item == 0) error_in_menu_configuration(line, "no menus or items specified.");

						if (!(new_menus = calloc(cur_menu + 1, sizeof(menu))) || !(new_items = calloc(cur_item, sizeof(menu_item))))
							error_in_menu_configuration(line, "not enough memory.");
					}
					else {
						menu_num = cur_menu + 1;
						menus = new_menus;
					}
				}
			}

			free(menu_conf);
		}
	}
}

/* Menu configs are all or nothing, so if the user has one,
   skip any global one. */
void get_menu_configuration(const char * menu_conf_name) {
	get_menu_conf(menu_conf_name, exists_prefs_dir);
	if (menus == def_menus) get_menu_conf(menu_conf_name, exists_gprefs_dir);
}


static void error_in_key_bindings(const int line, const char * const s) {
	fprintf(stderr, "Error in key bindings file at line %d: %s\n", line, s);
	exit(0);
}

static void get_key_bind(const char * key_bindings_name, char * (exists_prefs_func)()) {
	if (!key_bindings_name) key_bindings_name = KEY_BINDINGS_NAME;

	char * const prefs_dir = exists_prefs_func();
	if (prefs_dir) {
		char * const key_bindings = malloc(strlen(prefs_dir) + strlen(key_bindings_name) + 1);
		if (key_bindings) {
			strcat(strcpy(key_bindings, prefs_dir), key_bindings_name);
			char_stream * const cs = load_stream(NULL, key_bindings, false, false);
			if (cs) {
				char * p = cs->stream;
				int line = 1;

				while(p - cs->stream < cs->len) {
					if (*p && !cmdcmp(KEY_KEYWORD, p)) {
						while(*p && !isasciispace(*p)) p++;

						int c;
						if (sscanf(p, "%x %*s", &c) == 1) {
							if (c >= 0 && c < NUM_KEYS) {
								if (c != 27 && c != 13) {
									while(isasciispace(*p)) p++;
									while(*p && !isasciispace(*p)) p++;
									while(isasciispace(*p)) p++;
									if (*p) key_binding[c] = p;
									else error_in_key_bindings(line, "no command specified.");
								}
								else error_in_key_bindings(line, "you cannot redefine ESCAPE and RETURN.");
							}
							else error_in_key_bindings(line, "key code out of range.");
						}
						else error_in_key_bindings(line, "can't read key code.");
					}
					else if (*p && !cmdcmp(SEQ_KEYWORD, p)) {
						char *buf;
						while(*p && !isasciispace(*p)) p++;	 /* skip past SEQ */
						while(isasciispace(*p)) p++;			  /* skip to quoted sequence, like  "\x1b[A" */
						buf = p;	/* Risky: we're replacing the double-quoted string with its parsed equivalent in situ. */
						if (parse_string((unsigned char **)&p, (unsigned char *)buf, strlen(p)) > 0) {  /* parse_string() expects double-quoted string. */
							while(*p && isasciispace(*p)) p++;  /* skip to key code */
							int c;
							if (*p && sscanf(p, "%x %*s", &c) == 1) {	 /* convert key code */
								if (c >= 0 && c < NUM_KEYS) {
									if (c != 27 && c != 13) {
										if (key_may_set(buf, -c - 1) == 0)
											error_in_key_bindings(line, "sequence table full." );
									}
									else error_in_key_bindings(line, "you cannot redefine ESCAPE and RETURN.");
								}
								else error_in_key_bindings(line, "key code out of range.");
							}
							else error_in_key_bindings(line, "can't read key code.");
						}
						else error_in_key_bindings(line, "can't read double quoted character sequence.");
					}
					line++;
					p += strlen(p) + 1;
				}
			}

			free(key_bindings);
		}
	}
}

char *cur_dir(void) {
	static char *cur_dir = "./";
	return cur_dir;
}

/* Key bindings override easily, so pull in any global bindings
   first, then override with the users bindings. */
void get_key_bindings(const char * key_bindings_name) {
	get_key_bind(key_bindings_name, exists_gprefs_dir);
	get_key_bind(key_bindings_name, exists_prefs_dir);
	get_key_bind(key_bindings_name, cur_dir);
}


