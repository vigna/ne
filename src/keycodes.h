/* Extended codes for special keys.

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


/* These are curses-like key codes. They are defined for all keys available
in the terminfo database. The whole 64 function keys are supported. This allows
to map, if necessary, any additional key not included in terminfo's capabilities
onto an unused function key capability. */


/* Cursor movement keys */

#define	NE_KEY_UP					0x101

#define	NE_KEY_DOWN					0x102

#define	NE_KEY_LEFT					0x103

#define	NE_KEY_RIGHT				0x104

#define	NE_KEY_HOME					0x105

#define	NE_KEY_END					0x106

#define	NE_KEY_NPAGE				0x107

#define	NE_KEY_PPAGE				0x108

#define	NE_KEY_SCROLL_FORWARD	0x109

#define	NE_KEY_SCROLL_REVERSE	0x10A


/* Editing keys */

#define	NE_KEY_CLEAR_TO_EOL		0x110

#define	NE_KEY_CLEAR_TO_EOS		0x111

#define	NE_KEY_BACKSPACE			0x112

#define	NE_KEY_DELETE_LINE		0x113

#define	NE_KEY_INSERT_LINE		0x114

#define	NE_KEY_DELETE_CHAR		0x115

#define	NE_KEY_INSERT_CHAR		0x116

#define	NE_KEY_EXIT_INSERT_CHAR	0x117

#define	NE_KEY_CLEAR				0x118


/* Keypad keys */

#define	NE_KEY_A1		0x120

#define	NE_KEY_A3		0x121

#define	NE_KEY_B2		0x122

#define	NE_KEY_C1		0x123

#define	NE_KEY_C3		0x124


/* Fake (simulated) command key. */

#define	NE_KEY_COMMAND		0x125

/* The ignorable key. */

#define NE_KEY_IGNORE      0x126

/* Tab keys (never used in the standard configuration) */

#define	NE_KEY_CLEAR_ALL_TABS	0x128

#define	NE_KEY_CLEAR_TAB			0x129

#define	NE_KEY_SET_TAB				0x12A


/* Function keys */

#define	NE_KEY_F0		0x140

#define	NE_KEY_F(n) 	(NE_KEY_F0+(n))


/* Prefix-simulated META */

#define	NE_KEY_META0	0x180

#define	NE_KEY_META(n) 	(NE_KEY_META0+(n))


