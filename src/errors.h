/* Error index enum, and extern declaration of the error vector.

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
	/*  4 */ CANT_SAVE_ALL,
	/*  5 */ NOT_ON_A_BRACKET,
	/*  6 */ CANT_FIND_BRACKET,
	/*  7 */ BOOKMARK_NOT_SET,
	/*  8 */ INVALID_BOOKMARK_DESIGNATION, 
	/*  9 */ NO_UNSET_BOOKMARKS_TO_SET,
	/* 10 */ NO_SET_BOOKMARKS_TO_GOTO,
	/* 11 */ NO_SET_BOOKMARKS_TO_UNSET,
	/* 12 */ INVALID_LEVEL,
	/* 13 */ CANT_INSERT_0,
	/* 14 */ NO_SEARCH_STRING,
	/* 15 */ NO_REPLACE_STRING,
	/* 16 */ TAB_SIZE_OUT_OF_RANGE,
	/* 17 */ INVALID_MATCH_MODE,
	/* 18 */ MARK_BLOCK_FIRST,
	/* 19 */ OUT_OF_MEMORY,
	/* 20 */ NOTHING_TO_UNDO,
	/* 21 */ NOTHING_TO_REDO,
	/* 22 */ UNDO_NOT_ENABLED,
	/* 23 */ NO_SUCH_COMMAND,
	/* 24 */ CAN_EXECUTE_ONLY_OPTIONS,
	/* 25 */ HAS_NUMERIC_ARGUMENT,
	/* 26 */ HAS_NO_ARGUMENT,
	/* 27 */ REQUIRES_ARGUMENT,
	/* 28 */ WRONG_CHAR_AFTER_BACKSLASH,
	/* 29 */ CANT_OPEN_FILE,
	/* 30 */ CANT_OPEN_TEMPORARY_FILE,
	/* 31 */ ERROR_WHILE_WRITING,
	/* 32 */ HAS_NO_EXTENSION,
	/* 33 */ CANT_FIND_PREFS_DIR,
	/* 34 */ CLIP_DOESNT_EXIST,
	/* 35 */ MARK_OUT_OF_BUFFER,
	/* 36 */ CANT_OPEN_MACRO,
	/* 37 */ MAX_MACRO_DEPTH_EXCEEDED,
	/* 38 */ DOCUMENT_IS_READ_ONLY,
	/* 39 */ FILE_IS_MIGRATED,
	/* 40 */ FILE_IS_DIRECTORY,
	/* 41 */ FILE_IS_TOO_LARGE,
	/* 42 */ STOPPED,
	/* 43 */ IO_ERROR,
	/* 44 */ STRING_IS_EMPTY,
	/* 45 */ EXTERNAL_COMMAND_ERROR,
	/* 46 */ ESCAPE_TIME_OUT_OF_RANGE,
	/* 47 */ PREFS_STACK_FULL,
	/* 48 */ PREFS_STACK_EMPTY,
	/* 49 */ NOT_A_NUMBER,
	/* 50 */ INVALID_CHARACTER,
	/* 51 */ INVALID_STRING,
	/* 52 */ BUFFER_IS_NOT_UTF8,
	/* 53 */ INCOMPATIBLE_CLIP_ENCODING,
	/* 54 */ INCOMPATIBLE_COMMAND_ENCODING,
	/* 55 */ INCOMPATIBLE_SEARCH_STRING_ENCODING,
	/* 56 */ INCOMPATIBLE_REPLACE_STRING_ENCODING,
	/* 57 */ UTF8_REGEXP_CHARACTER_CLASS_NOT_SUPPORTED,
	/* 58 */ UTF8_REGEXP_COMP_CHARACTER_CLASS_NOT_SUPPORTED,
	/* 59 */ GROUP_NOT_AVAILABLE,
	/* 60 */ SYNTAX_NOT_ENABLED,
	/* 61 */ NO_SYNTAX_FOR_EXT,
	/* 62 */ INVALID_SHIFT_SPECIFIED,
	/* 63 */ INSUFFICIENT_WHITESPACE,
	/* 64 */ DOCUMENT_NOT_SAVED,
	/* 65 */ FILE_TOO_LARGE_SYNTAX_HIGHLIGHTING_DISABLED,
	/* 66 */ CANNOT_SAVE_DISK_FULL,
	/* 67 */ OUT_OF_MEMORY_DISK_FULL,

	ERROR_COUNT
};

enum info {
	SAVING,
	SAVED,
	MODIFIED_SAVED,
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
	SELECT_DOC,
	FILE_HAS_BEEN_MODIFIED,
	FILE_ALREADY_EXISTS,
	HELP_KEYS,
	HELP_COMMAND_KEYS,

	INFO_COUNT
};

extern char *error_msg[ERROR_COUNT];
extern char *info_msg[INFO_COUNT];
