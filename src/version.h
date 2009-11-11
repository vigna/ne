/* String definitions for version and 'About...' messages.

	Copyright (C) 1993-1998 Sebastiano Vigna 
	Copyright (C) 1999-2009 Todd M. Lewis and Sebastiano Vigna

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
	Software Foundation, Inc., 59 Temple Place - Suite 331, Boston, MA
	02111-1307, USA.  */


#define DATE "(20.02.2009)"
#define VERSION "2.0.3"
#define PROGRAM_NAME "ne, the nice editor"
#define ABOUT_MSG PROGRAM_NAME " " VERSION ". " DATE

#ifdef _AMIGA
#define VERSION_STRING "\0$VER:" PROGRAM_NAME " " VERSION ". " DATE
#else
#define VERSION_STRING "@(#)"ABOUT_MSG
#endif
