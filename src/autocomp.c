/* AutoComplete

   Copyright (C) 1999-2010 Todd M. Lewis and Sebastiano Vigna

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


#include "ne.h"

#define SONS 256
#define TRIE_NODE_POOL_SIZE 128
#define FLAG_CHAR '*'

typedef struct trie_node {
	int flag; /* 0: not a terminator; 1: cur_buffer string; 2: other buffer string; */
	struct trie_node *sons[SONS];
	} trie;

typedef struct trie_node_pool {
	struct trie_node_pool *next;
	int                    used;
	trie trie_node[TRIE_NODE_POOL_SIZE];
} trie_pool;

static trie ac_root;
static trie_pool *ac_pool = NULL;

D(
static int alloced_trie_nodes;
static int freed_trie_pools;
)

static void init_trie(void) {
	int i;
	ac_root.flag = 0;
	D(alloced_trie_nodes = freed_trie_pools = 0;)
	for (i = 0; i < SONS; i++) {
		ac_root.sons[i] = NULL;
	}
}

static trie *new_trie(void) {
	trie *nt;
	trie_pool *tp;

	if (!ac_pool || ac_pool->used >= TRIE_NODE_POOL_SIZE) {
		if (tp = calloc(sizeof(trie_pool), 1)) {
				tp->next = ac_pool;
				ac_pool = tp;
		} else return NULL;
	}
	D(alloced_trie_nodes++;)
	return &ac_pool->trie_node[ac_pool->used++];
}

static void delete_trie(void) {
	trie_pool *tp;
	while (ac_pool) {
		tp = ac_pool->next;
		free(ac_pool);
	   D(freed_trie_pools++;)
		ac_pool = tp;
	}
}

static void add_to_trie(const unsigned char * const str, const int len, int *cum_len, int *cum_entries, int flag) {
	int l, r, m, t, minlen, c, i;
	trie *trie =  &ac_root;

	if (!str || !*str) return;
	assert(flag);
	for (i=0,c=str[i]; i<len; i++,c=str[i]) {
		if (!trie->sons[c]) {
			if (!(trie->sons[c] = new_trie())) return; /* Epic fail. Figure out how to abort this eventually. */
		}
		trie = trie->sons[c];
	}
	assert(trie != NULL);
	if (!trie->flag) {
		trie->flag = flag;
		(*cum_len) += len + 1 + (flag == 2 ? 1 : 0);
		(*cum_entries)++;
	}
}

static void search_buff(const buffer *b, const unsigned char *p, int *max_len, int *cum_len, int *cum_entries, int case_search, int flag) {
	line_desc *ld = (line_desc *)b->line_desc_list.head, *next;
	int p_len = strlen(p);
	int l, r;
	
	assert(p != NULL);
	while (next = (line_desc *)ld->ld_node.next) {
		l = r = 0;
		do {
			/* find left edge of word */
			while (l < ld->line_len - p_len && !ne_isword(get_char(&ld->line[l], b->encoding), b->encoding)) l += get_char_width(&ld->line[l], b->encoding);
			if    (l < ld->line_len - p_len ) {
				/* find right edge of word */
				r = l + get_char_width(&ld->line[l], b->encoding);
				while (r < ld->line_len && ne_isword(get_char(&ld->line[r], b->encoding), b->encoding)) r += get_char_width(&ld->line[r], b->encoding);
				if (r - l >= p_len && p_len == 0 || !(case_search ? strncmp : strncasecmp)(p, &ld->line[l], p_len)) {
					add_to_trie(&ld->line[l], r - l, cum_len, cum_entries, flag);
					if ( *max_len < r - l ) *max_len = r - l;
				}
				l = r;
			}
			assert(l <= ld->line_len);
		} while (l < ld->line_len - p_len);
		
		ld = next;
	}
}

unsigned char *autocomplete(unsigned char *p) {
	unsigned char *char_store, *c, *scratch;
	unsigned char **entries, **e;
	line_desc *ld;
	buffer *b = (buffer *)buffers.head;
	int max_len = 0, cum_entries = 0, cum_len = 0, x, i, *dd;
	trie **tries;
	
	init_trie();

	search_buff(cur_buffer, p, &max_len, &cum_len, &cum_entries, cur_buffer->opt.case_search, 1);
	while (b->b_node.next) {
		if (b != cur_buffer)
			search_buff(b, p, &max_len, &cum_len, &cum_entries, cur_buffer->opt.case_search, 2);
		b = (buffer *)b->b_node.next;
	}
	max_len++;



	/* traverse the trie and produce our strings; itterative rather than recursive
	                --------x                
	    scratch   " . . . . . . . . . "      
	                                         
	         dd (   _     _         _  )    |
	            (       _       _      )    |
	            (     _       _        )    y
	            (           _     _    )             */

	/* fprintf(stderr,"cum_entries: %d, cum_len: %d, max_len: %d\n", cum_entries, cum_len, max_len); */
	if (cum_entries &&
	    (scratch = calloc(sizeof(char),   max_len)) &&
	    (tries   = calloc(sizeof(trie *), max_len)) &&
	    (dd      = calloc(sizeof(int),    max_len)) &&
	    (e = entries = calloc(sizeof(char *), cum_entries)) &&
	    (c = char_store = malloc(cum_len))) {
		tries[0] = &ac_root;
		x = 0;
		#define y (dd[x])
		while (x >= 0) {
			/* fprintf(stderr,"LOOP-TOP: x: %3d, y: %3d (", x, y); 
			for (i=0; i<max_len; i++) {
				fprintf(stderr,"%3d ",dd[i]);
				fflush(NULL);
			}                                                      
			fprintf(stderr,")\n"); fflush(NULL);                   */
			if (!y) {
				if (tries[x]->flag) {
					assert(e<entries+cum_entries);
					*e++ = c;
					for (i=0; i<x; i++) *c++ = scratch[i];
					if (tries[x]->flag == 2) *c++ = FLAG_CHAR;
					*c++ = '\0';
					assert(c<=char_store+cum_len);
				}
			}
			if (tries[x]->sons[y]) {
				scratch[x] = y;
				tries[x+1] = tries[x]->sons[y];
				x++;
				y = 0;
			} else if (++y == SONS) {
				y = 0;
				x--;
				if (x >= 0) y++;
			}
		}
	}
	assert(e==entries+cum_entries);
	assert(c==char_store+cum_len);
	if (dd) free(dd);
	if (tries) free(tries);
	if (scratch) free(scratch);
	delete_trie();
	free(p);
	p = NULL;
	
	/* Although the trie traversal produces sorted entries, they aren't in dictionary order. */
	qsort(entries, cum_entries, sizeof(char *), strdictcmp);  
	
	if (entries && char_store) {
		if ((i = request_strings((const char * const *)entries, cum_entries, 0, max_len, FLAG_CHAR)) != ERROR) {
			unsigned char *cp = entries[i >= 0 ? i : -i - 2];
			if (p = malloc(strlen(cp) + 1)) {
				strncpy(p,cp,strlen(cp) + 1);
				if (p[strlen(p) - 1 ] == FLAG_CHAR) p[strlen(p) - 1] = '\0';
			}
		}
	}
	if (entries) free(entries);
	if (char_store) free(char_store);
	D(fprintf(stderr,"autocomp returning '%s', entries: %d, cum_len %d, trie nodes: %d, trie pools: %d.\n", p, cum_entries, cum_len, alloced_trie_nodes, freed_trie_pools);)
	return p;
}
