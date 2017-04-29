/* Error message vector.

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


#include "errors.h"

/* Whenever this vector is updated, the corresponding enum in errors.h
*must* be updated too. */

char *error_msg[ERROR_COUNT] = {
	/*  0 */ "",
	/*  1 */ "Syntax error.",
	/*  2 */ "Not found. (RepeatLast to wrap)",
	/*  3 */ "Can't save a document. Exit suspended.",
	/*  4 */ "Can't save all modified documents.",
	/*  5 */ "You are not positioned over {}, (), [] or <>.",
	/*  6 */ "Can't find matching bracket.",
	/*  7 */ "Bookmark not set.",
	/*  8 */ "Invalid Bookmark designation (use 0 through 9, -1, +1, or '-').",
	/*  9 */ "No unset Bookmarks to set.",
	/* 10 */ "No set Bookmarks to goto.",
	/* 11 */ "No set Bookmarks to unset.",
	/* 12 */ "Invalid level (use 0, '+', '-', or none).",
	/* 13 */ "You cannot insert a character whose ASCII code is 0.",
	/* 14 */ "No search string.",
	/* 15 */ "No replace string.",
	/* 16 */ "TAB size out of range.",
	/* 17 */ "Invalid match mode.",
	/* 18 */ "Mark a block first.",
	/* 19 */ "Out of memory. DANGER!",
	/* 20 */ "Nothing to undo.",
	/* 21 */ "Nothing to redo",
	/* 22 */ "Undo is not enabled",
	/* 23 */ "No such command.",
	/* 24 */ "Can execute only preference commands.",
	/* 25 */ "Command needs a numeric argument.",
	/* 26 */ "Command has no arguments.",
	/* 27 */ "Command requires an argument.",
	/* 28 */ "Wrong character after backslash.",
	/* 29 */ "Can't open file.",
	/* 30 */ "Can't open temporary files.",
	/* 31 */ "Error while writing.",
	/* 32 */ "Document name has no extension.",
	/* 33 */ "Can't find or create $HOME/.ne directory.",
	/* 34 */ "Clip does not exist.",
	/* 35 */ "Mark is out of document.",
	/* 36 */ "Can't open macro.",
	/* 37 */ "Maximum macro depth exceeded.",
	/* 38 */ "This document is read-only.",
	/* 39 */ "Can't open file (file is migrated).",
	/* 40 */ "Can't open file (file is a directory).",
	/* 41 */ "Can't open file (file is too large).",
	/* 42 */ "Stopped.",
	/* 43 */ "I/O error.",
	/* 44 */ "The argument string is empty.",
	/* 45 */ "External command error.",
	/* 46 */ "Escape time out of range.",
	/* 47 */ "Prefs stack is full.",
	/* 48 */ "Prefs stack is empty.",
	/* 49 */ "The argument is not a number.",
	/* 50 */ "This character is not supported in this configuration.",
	/* 51 */ "This string is not supported in this configuration.",
	/* 52 */ "This buffer is not UTF-8 encoded.",
	/* 53 */ "This clip cannot be pasted in this buffer (incompatible encoding).",
	/* 54 */ "This command line cannot be executed in this buffer (incompatible encoding).",
	/* 55 */ "This string cannot be searched for in this buffer (incompatible encoding).",
	/* 56 */ "This replacement string cannot be used in this buffer (incompatible encoding).",
	/* 57 */ "UTF-8 character classes in regular expressions are not supported.",
	/* 58 */ "Character classes cannot be complemented when matching against UTF-8 text.",
	/* 59 */ "The specified regex replacement group is not available in UTF-8 mode",
	/* 60 */ "Syntax highlighting is not enabled",
	/* 61 */ "There is no syntax for that extension",
	/* 62 */ "Invalid Shift specified (use [<|>][#][s|t]; default is \">1t\").",
	/* 63 */ "Insufficient white space for requested left shift.",
	/* 64 */ "Document not saved.",
	/* 65*/	"File is too large--syntax highlighting disabled (use SYNTAX to reactivate).",
	/* 66*/	"Cannot save: disk full.",
	/* 67*/	"Out of memory (insufficient disk space?). DANGER!"
};

char *info_msg[INFO_COUNT] = {
	"Saving...",
	"Saved.",
	"All modified documents saved.",
	"SELECT cursor/Enter FILTER chars/BS MODE Ins/Del EDIT Tab FILENAME F1/Esc-Esc/Esc",
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
	"SELECT cursor/Enter FILTER chars/BS MODE Ins/Del REORDER F2/F3 QUIT F1/Esc-Esc/Esc",
	"File has been modified since buffer was loaded or saved; are you sure?",
	"A file with that name already exists; are you sure?",
	"HELP cursor/Enter QUIT F1/Esc-Esc/Esc",
	"BACK Enter QUIT F1/Esc-Esc/Esc"
};
