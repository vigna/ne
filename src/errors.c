/* Error message vector.

	Copyright (C) 1993-1998 Sebastiano Vigna 
	Copyright (C) 1999-2011 Todd M. Lewis and Sebastiano Vigna

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


#include "errors.h"

/* Whenever this vector is updated, the corresponding enum in errors.h
*must* be updated too. */

char *error_msg[ERROR_COUNT] = {
	"",
	"Syntax error.",
	"Not found.",
	"Can't save a document. Exit suspended.",
	"You are not positioned over {}, (), [] or <>.",
	"Can't find matching bracket.",
	"Bookmark not set.",
	"Invalid Bookmark designation (use 0 through 9, -1, +1, or '-').",
	"No unset Bookmarks to set.",
	"No set Bookmarks to goto.",
	"No set Bookmarks to unset.",
	"You cannot insert a character whose ASCII code is 0.",
	"No search string.",
	"No replace string.",
	"TAB size out of range.",
	"Mark a block first.",
	"Out of memory. DANGER!",
	"Nothing to undo.",
	"Nothing to redo",
	"Undo is not enabled",
	"No such command.",
	"Can execute only preference commands.",
	"Command needs a numeric argument.",
	"Command has no arguments.",
	"Command requires an argument.",
	"Wrong character after backslash.",
	"Can't open file.",
	"Can't open temporary files.",
	"Error while writing.",
	"Document name has no extension.",
	"Can't find or create $HOME/.ne directory.",
	"Clip does not exist.",
	"Mark is out of document.",
	"Can't open macro.",
	"This file is read-only.",
	"Can't open file (file is migrated).",
	"Can't open file (file is a directory).",
	"Stopped.",
	"I/O error.",
	"The argument string is empty.",
	"External command error.",
	"Escape time out of range.",
	"Prefs stack is full.",
	"Prefs stack is empty.",
	"The argument is not a number.",
	"This character is not supported in this configuration.",
	"This string is not supported in this configuration.",
	"This buffer is not UTF-8 encoded.",
	"This clip cannot be pasted in this buffer (incompatible encoding).",
	"This command line cannot be executed in this buffer (incompatible encoding).",
	"This string cannot be searched for in this buffer (incompatible encoding).",
	"This replacement string cannot be used in this buffer (incompatible encoding).",
	"UTF-8 character classes in regular expressions are not supported.",
	"Character classes cannot be complemented when matching against UTF-8 text.",
	"The specified regex replacement group is not available in UTF-8 mode",
	"Syntax highlighting is not enabled",
	"There is no syntax for that extension"
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
	"Cancelled."
};
