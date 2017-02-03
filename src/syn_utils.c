/* Syntax highlighting from Joe's Own Editor: Various utilities.

	Copyright (C) 1992 Joseph H. Allen
	Copyright (C) 2001 Marek 'Marx' Grac
	Copyright (C) 2009-2017 Todd M. Lewis and Sebastiano Vigna

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
#include <signal.h>

/*
 * return minimum/maximum of two numbers
 */
unsigned int uns_min(unsigned int a, unsigned int b)
{
	return a < b ? a : b;
}

signed int int_min(signed int a, signed int b)
{
	return a < b ? a : b;
}

signed long int long_max(signed long int a, signed long int b)
{
	return a > b ? a : b;
}

signed long int long_min(signed long int a, signed long int b)
{
	return a < b ? a : b;
}

/* Normal malloc() */

void *joe_malloc(size_t size)
{
	void *p = malloc(size);
	
	if (!p) kill(getpid(), SIGTERM);
	
	return p;
}

void *joe_calloc(size_t nmemb,size_t size)
{
	void *p = calloc(nmemb, size);
	
	if (!p) kill(getpid(), SIGTERM);
		
	return p;
	
}

void *joe_realloc(void *ptr,size_t size)
{
	void *p = realloc(ptr, size);
	
	if (!p)
		/*ttsig(-1);*/
		kill(getpid(), SIGTERM);
	
	return p;
	
}

void joe_free(void *ptr)
{
	free(ptr);
}

size_t zlen(unsigned char *s)
{
	return strlen((char *)s);
}

int zcmp(unsigned char *a, unsigned char *b)
{
	return strcmp((char *)a, (char *)b);
}

int zncmp(unsigned char *a, unsigned char *b, size_t len)
{
	return strncmp((char *)a, (char *)b, len);
}

unsigned char *zdup(unsigned char *bf)
{
	int size = zlen(bf);
	unsigned char *p = (unsigned char *)joe_malloc(size+1);
	memcpy(p,bf,size+1);
	return p;
}

unsigned char *zcpy(unsigned char *a, unsigned char *b)
{
	strcpy((char *)a,(char *)b);
	return a;
}

unsigned char *zstr(unsigned char *a, unsigned char *b)
{
	return (unsigned char *)strstr((char *)a,(char *)b);
}

unsigned char *zncpy(unsigned char *a, unsigned char *b, size_t len)
{
	strncpy((char *)a,(char *)b,len);
	return a;
}

unsigned char *zcat(unsigned char *a, unsigned char *b)
{
	strcat((char *)a,(char *)b);
	return a;
}

unsigned char *zchr(unsigned char *s, int c)
{
	return (unsigned char *)strchr((char *)s,c);
}

unsigned char *zrchr(unsigned char *s, int c)
{
	return (unsigned char *)strrchr((char *)s,c);
}

#ifdef junk

void *replenish(void **list,int size)
{
	unsigned char *i = joe_malloc(size*16);
	int x;
	for (x=0; x!=15; ++x) {
		fr_single(list, i);
		i += size;
	}
	return i;
}

/* Destructors */

GC *gc_free_list = 0;

void gc_add(GC **gc, void **var, void (*rm)(void *val))
{
	GC *g;
	for (g = *gc; g; g=g->next)
		if (g->var == var)
			return;
	g = al_single(&gc_free_list, GC);
	g = gc_free_list;
	gc_free_list = g->next;
	g->next = *gc;
	*gc = g;
	g->var = var;
	g->rm = rm;
}

void gc_collect(GC **gc)
{
	GC *g = *gc;
	while (g) {
		GC *next = g->next;
		if (*g->var) {
			g->rm(*g->var);
			*g->var = 0;
		}
		fr_single(&gc_free_list,g);
		g = next;
	}
	*gc = 0;
}

#endif

/* Zstrings */

void rm_zs(ZS z)
{
	joe_free(z.s);
}

ZS raw_mk_zs(GC **gc,unsigned char *s,int len)
{
	ZS zs;
	zs.s = (unsigned char *)joe_malloc(len+1);
	if (len)
		memcpy(zs.s,s,len);
	zs.s[len] = 0;
	return zs;
}


/* Helpful little parsing utilities */

/* Skip whitespace and return first non-whitespace character */

int parse_ws(unsigned char **pp,int cmt)
{
	unsigned char *p = *pp;
	while (*p==' ' || *p=='\t')
		++p;
	if (*p=='\r' || *p=='\n' || *p==cmt)
		*p = 0;
	*pp = p;
	return *p;
}

/* Parse an identifier into a buffer.  Identifier is truncated to a maximum of len-1 chars. */

int parse_ident(unsigned char **pp, unsigned char *buf, int len)
{
	unsigned char *p = *pp;
	if (isalpha(*p) || *p == '_') {
		while(len > 1 && (isalnum(*p) || *p == '_'))
			*buf++= *p++, --len;
		*buf=0;
		while(isalnum(*p) || *p == '_')
			++p;
		*pp = p;
		return 0;
	} else
		return -1;
}

/* Parse to next whitespace */

int parse_tows(unsigned char **pp, unsigned char *buf)
{
	unsigned char *p = *pp;
	while (*p && *p!=' ' && *p!='\t' && *p!='\n' && *p!='\r' && *p!='#')
		*buf++ = *p++;

	*pp = p;
	*buf = 0;
	return 0;
}

/* Parse over a specific keyword */

int parse_kw(unsigned char **pp, unsigned char *kw)
{
	unsigned char *p = *pp;
	while(*kw && *kw==*p)
		++kw, ++p;
	if(!*kw && !isalnum(*p)) {
		*pp = p;
		return 0;
	} else
		return -1;
}

/* Parse a field (same as parse_kw, but string must be terminated with whitespace) */

int parse_field(unsigned char **pp, unsigned char *kw)
{
	unsigned char *p = *pp;
	while(*kw && *kw==*p)
		++kw, ++p;
	if(!*kw && (!*p || *p==' ' || *p=='\t' || *p=='#' || *p=='\n' || *p=='\r')) {
		*pp = p;
		return 0;
	} else
		return -1;
}

/* Parse a specific character */

int parse_char(unsigned char **pp, unsigned char c)
{
	unsigned char *p = *pp;
	if (*p == c) {
		*pp = p+1;
		return 0;
	} else
		return -1;
}

/* Parse an integer.  Returns 0 for success. */

int parse_int(unsigned char **pp, int *buf)
{
	unsigned char *p = *pp;
	if ((*p>='0' && *p<='9') || *p=='-') {
		*buf = atoi((char *)p);
		if(*p=='-')
			++p;
		while(*p>='0' && *p<='9')
			++p;
		*pp = p;
		return 0;
	} else
		return -1;
}

/* Parse a long */

int parse_long(unsigned char **pp, long *buf)
{
	unsigned char *p = *pp;
	if ((*p>='0' && *p<='9') || *p=='-') {
		*buf = atol((char *)p);
		if(*p=='-')
			++p;
		while(*p>='0' && *p<='9')
			++p;
		*pp = p;
		return 0;
	} else
		return -1;
}

/* Parse a string of the form "xxxxx" into a fixed-length buffer.  The
 * address of the buffer is 'buf'.  The length of this buffer is 'len'.  A
 * terminating NUL is added to the parsed string.  If the string is larger
 * than the buffer, the string is truncated.
 *
 * C string escape sequences are handled.
 *
 * 'p' holds an address of the input string pointer.  The pointer
 * is updated to point right after the parsed string if the function
 * succeeds.
 *
 * Returns the length of the string (not including the added NUL), or
 * -1 if there is no string or if the input ended before the terminating ".
 */

int parse_string(unsigned char **pp, unsigned char *buf, int len)
{
	unsigned char *start = buf;
	unsigned char *p= *pp;
	if(*p=='\"') {
		++p;
		while(len > 1 && *p && *p!='\"') {
			int x = 50;
			int c = escape(0, &p, &x);
			*buf++ = c;
			--len;
		}
		*buf = 0;
		while(*p && *p!='\"')
			if(*p=='\\' && p[1])
				p += 2;
			else
				p++;
		if(*p == '\"') {
			*pp = p + 1;
			return buf - start;
		}
	}
	return -1;
}

/* Emit a string with escape sequences */

#ifdef junk

/* Used originally for printing macros */

void emit_string(FILE *f,unsigned char *s,int len)
{
	unsigned char buf[8];
	unsigned char *p, *q;
	fputc('\"',f);
	while(len) {
		p = unescape(buf,*s++);
		for(q=buf;q!=p;++q)
			fputc(*q,f);
		--len;
	}
	fputc('\"',f);
}
#endif

/* Emit a string */

void emit_string(FILE *f,unsigned char *s,int len)
{
	fputc('"',f);
	while(len) {
		if (*s=='"' || *s=='\\')
			fputc('\\',f), fputc(*s,f);
		else if(*s=='\n')
			fputc('\\',f), fputc('n',f);
		else if(*s=='\r')
			fputc('\\',f), fputc('r',f);
		else if(*s==0)
			fputc('\\',f), fputc('0',f), fputc('0',f), fputc('0',f);
		else
			fputc(*s,f);
		++s;
		--len;
	}
	fputc('"',f);
}

/* Parse a character range: a-z */

int parse_range(unsigned char **pp, int *first, int *second)
{
	unsigned char *p= *pp;
	int a, b;
	if(!*p)
		return -1;
	if(*p=='\\' && p[1]) {
		++p;
		if(*p=='n')
			a = '\n';
		else if(*p=='t')
  			a = '\t';
		else
			a = *p;
		++p;
	} else
		a = *p++;
	if(*p=='-' && p[1]) {
		++p;
		if(*p=='\\' && p[1]) {
			++p;
			if(*p=='n')
				b = '\n';
			else if(*p=='t')
				b = '\t';
			else
				b = *p;
			++p;
		} else
			b = *p++;
	} else
		b = a;
	*first = a;
	*second = b;
	*pp = p;
	return 0;
}
