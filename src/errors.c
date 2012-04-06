/* Error message vector.

	Copyright (C) 1993-1998 Sebastiano Vigna 
	Copyright (C) 1999-2012 Todd M. Lewis and Sebastiano Vigna

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


#include "errors.h"

/* Whenever this vector is updated, the corresponding enum in errors.h
*must* be updated too. */

char *error_msg[ERROR_COUNT] = {
	/*  0 */ "",
	/*  1 */ "Syntax error.",
	/*  2 */ "Not found.",
	/*  3 */ "Can't save a document. Exit suspended.",
	/*  4 */ "You are not positioned over {}, (), [] or <>.",
	/*  5 */ "Can't find matching bracket.",
	/*  6 */ "Bookmark not set.",
	/*  7 */ "Invalid Bookmark designation (use 0 through 9, -1, +1, or '-').",
	/*  8 */ "No unset Bookmarks to set.",
	/*  9 */ "No set Bookmarks to goto.",
	/* 10 */ "No set Bookmarks to unset.",
	/* 11 */ "Invalid level (use 0, '+', '-', or none).",
	/* 12 */ "You cannot insert a character whose ASCII code is 0.",
	/* 13 */ "No search string.",
	/* 14 */ "No replace string.",
	/* 15 */ "TAB size out of range.",
	/* 16 */ "Invalid match mode.",
	/* 17 */ "Mark a block first.",
	/* 18 */ "Out of memory. DANGER!",
	/* 19 */ "Nothing to undo.",
	/* 20 */ "Nothing to redo",
	/* 21 */ "Undo is not enabled",
	/* 22 */ "No such command.",
	/* 23 */ "Can execute only preference commands.",
	/* 24 */ "Command needs a numeric argument.",
	/* 25 */ "Command has no arguments.",
	/* 26 */ "Command requires an argument.",
	/* 27 */ "Wrong character after backslash.",
	/* 28 */ "Can't open file.",
	/* 29 */ "Can't open temporary files.",
	/* 30 */ "Error while writing.",
	/* 31 */ "Document name has no extension.",
	/* 32 */ "Can't find or create $HOME/.ne directory.",
	/* 33 */ "Clip does not exist.",
	/* 34 */ "Mark is out of document.",
	/* 35 */ "Can't open macro.",
	/* 36 */ "Maximum macro depth exceeded.",
	/* 37 */ "This file is read-only.",
	/* 38 */ "Can't open file (file is migrated).",
	/* 39 */ "Can't open file (file is a directory).",
	/* 40 */ "Can't open file (file is too large).",
	/* 41 */ "Stopped.",
	/* 42 */ "I/O error.",
	/* 43 */ "The argument string is empty.",
	/* 44 */ "External command error.",
	/* 45 */ "Escape time out of range.",
	/* 46 */ "Prefs stack is full.",
	/* 47 */ "Prefs stack is empty.",
	/* 48 */ "The argument is not a number.",
	/* 49 */ "This character is not supported in this configuration.",
	/* 50 */ "This string is not supported in this configuration.",
	/* 51 */ "This buffer is not UTF-8 encoded.",
	/* 52 */ "This clip cannot be pasted in this buffer (incompatible encoding).",
	/* 53 */ "This command line cannot be executed in this buffer (incompatible encoding).",
	/* 54 */ "This string cannot be searched for in this buffer (incompatible encoding).",
	/* 55 */ "This replacement string cannot be used in this buffer (incompatible encoding).",
	/* 56 */ "UTF-8 character classes in regular expressions are not supported.",
	/* 57 */ "Character classes cannot be complemented when matching against UTF-8 text.",
	/* 58 */ "The specified regex replacement group is not available in UTF-8 mode",
	/* 59 */ "Syntax highlighting is not enabled",
	/* 60 */ "There is no syntax for that extension",
	/* 61 */ "Invalid Shift specified (use [<|>][#][s|t]; default is \">1t\").",
	/* 62 */ "Insufficient white space for requested left shift."

};

char *info_msg[INFO_COUNT] = {
	"Saving...",
	"Saved.",
	"Select file or press F1, Escape or Escape-Escape to enter a file name.",
	"Start of block marked. Move to the end of block and request actions.",
	"Start of vertical block marked. Move to the end of block and request actions.",
	"Starting macro recording...",
	"Macro recording completed.",
	"Some documents have not been saved; are you sure?",
	"Press a key to see ne's corresponding key code:",
	"This document is not saved; are you sure?",
	"There is another document with the same name; are you sure?",
	"No matching words found.",
	"Completed.",
	"Partially completed.",
	"Cancelled.",
};
