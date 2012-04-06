/* Error index enum, and extern declaration of the error vector.

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


/* Here we define the table of error messages. These defines are used
whenever a function wants to return a specific error to the caller, and
wants also to specify that an error message has to be printed. The generic
error code which just says "something went wrong" is -1. 0 represents
success. Any positive value is one of these enum items, and can be used to
index the error message table. Whenever this enum is updated, the
corresponding vector in errors.c *must* be updated too. */

#ifdef OK
#undef OK
#endif

#ifdef ERROR
#undef ERROR
#endif

#ifdef ABORT
#undef ABORT
#endif

#define ABORT	(-2)
#define ERROR	(-1)
#define OK		(0)

enum error {
	/*  1 */ SYNTAX_ERROR = 1,
	/*  2 */ NOT_FOUND,
	/*  3 */ CANT_SAVE_EXIT_SUSPENDED,
	/*  4 */ NOT_ON_A_BRACKET,
	/*  5 */ CANT_FIND_BRACKET,
	/*  6 */ BOOKMARK_NOT_SET,
	/*  7 */ INVALID_BOOKMARK_DESIGNATION, 
	/*  8 */ NO_UNSET_BOOKMARKS_TO_SET,
	/*  9 */ NO_SET_BOOKMARKS_TO_GOTO,
	/* 10 */ NO_SET_BOOKMARKS_TO_UNSET,
	/* 11 */ INVALID_LEVEL,
	/* 12 */ CANT_INSERT_0,
	/* 13 */ NO_SEARCH_STRING,
	/* 14 */ NO_REPLACE_STRING,
	/* 15 */ TAB_SIZE_OUT_OF_RANGE,
	/* 16 */ INVALID_MATCH_MODE,
	/* 17 */ MARK_BLOCK_FIRST,
	/* 18 */ OUT_OF_MEMORY,
	/* 19 */ NOTHING_TO_UNDO,
	/* 20 */ NOTHING_TO_REDO,
	/* 21 */ UNDO_NOT_ENABLED,
	/* 22 */ NO_SUCH_COMMAND,
	/* 23 */ CAN_EXECUTE_ONLY_OPTIONS,
	/* 24 */ HAS_NUMERIC_ARGUMENT,
	/* 25 */ HAS_NO_ARGUMENT,
	/* 26 */ REQUIRES_ARGUMENT,
	/* 27 */ WRONG_CHAR_AFTER_BACKSLASH,
	/* 28 */ CANT_OPEN_FILE,
	/* 29 */ CANT_OPEN_TEMPORARY_FILE,
	/* 30 */ ERROR_WHILE_WRITING,
	/* 31 */ HAS_NO_EXTENSION,
	/* 32 */ CANT_FIND_PREFS_DIR,
	/* 33 */ CLIP_DOESNT_EXIST,
	/* 34 */ MARK_OUT_OF_BUFFER,
	/* 35 */ CANT_OPEN_MACRO,
	/* 36 */ MAX_MACRO_DEPTH_EXCEEDED,
	/* 37 */ FILE_IS_READ_ONLY,
	/* 38 */ FILE_IS_MIGRATED,
	/* 39 */ FILE_IS_DIRECTORY,
	/* 40 */ FILE_IS_TOO_LARGE,
	/* 41 */ STOPPED,
	/* 42 */ IO_ERROR,
	/* 43 */ STRING_IS_EMPTY,
	/* 44 */ EXTERNAL_COMMAND_ERROR,
	/* 45 */ ESCAPE_TIME_OUT_OF_RANGE,
	/* 46 */ PREFS_STACK_FULL,
	/* 47 */ PREFS_STACK_EMPTY,
	/* 48 */ NOT_A_NUMBER,
	/* 49 */ INVALID_CHARACTER,
	/* 50 */ INVALID_STRING,
	/* 51 */ BUFFER_IS_NOT_UTF8,
	/* 52 */ INCOMPATIBLE_CLIP_ENCODING,
	/* 53 */ INCOMPATIBLE_COMMAND_ENCODING,
	/* 54 */ INCOMPATIBLE_SEARCH_STRING_ENCODING,
	/* 55 */ INCOMPATIBLE_REPLACE_STRING_ENCODING,
	/* 56 */ UTF8_REGEXP_CHARACTER_CLASS_NOT_SUPPORTED,
	/* 57 */ UTF8_REGEXP_COMP_CHARACTER_CLASS_NOT_SUPPORTED,
	/* 58 */ GROUP_NOT_AVAILABLE,
	/* 59 */ SYNTAX_NOT_ENABLED,
	/* 60 */ NO_SYNTAX_FOR_EXT,
	/* 61 */ INVALID_SHIFT_SPECIFIED,
	/* 62 */ INSUFFICIENT_WHITESPACE,

	ERROR_COUNT
};

enum info {
	SAVING,
	SAVED,
	PRESSF1,
	BLOCK_START_MARKED,
	VERTICAL_BLOCK_START_MARKED,
	STARTING_MACRO_RECORDING,
	MACRO_RECORDING_COMPLETED,
	SOME_DOCUMENTS_ARE_NOT_SAVED,
	PRESS_A_KEY,
	THIS_DOCUMENT_NOT_SAVED,
	SAME_NAME,
	AUTOCOMPLETE_NO_MATCH,
	AUTOCOMPLETE_COMPLETED,
	AUTOCOMPLETE_PARTIAL,
	AUTOCOMPLETE_CANCELLED,
	INFO_COUNT
};

extern char *error_msg[ERROR_COUNT];
extern char *info_msg[INFO_COUNT];
