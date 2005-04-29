/*
 * Copyright (c) 1999 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this
 * file.
 * 
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */
/*
 * Copyright (c) 1990, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */


#include <stddef.h>
#include <stdlib.h>

/*
 * Perform a binary search.
 *
 * The code below is a bit sneaky.  After a comparison fails, we
 * divide the work in half by moving either left or right. If lim
 * is odd, moving left simply involves halving lim: e.g., when lim
 * is 5 we look at item 2, so we change lim to 2 so that we will
 * look at items 0 & 1.  If lim is even, the same applies.  If lim
 * is odd, moving right again involes halving lim, this time moving
 * the base up one item past p: e.g., when lim is 5 we change base
 * to item 3 and make lim 2 so that we will look at items 3 and 4.
 * If lim is even, however, we have to shrink it by one before
 * halving: e.g., when lim is 4, we still looked at item 2, so we
 * have to make lim 3, then halve, obtaining 1, so that we will only
 * look at item 3.
 */
#ifdef notdef
void *
bsearch(key, base0, nmemb, size, compar)
	register const void *key;
	const void *base0;
	size_t nmemb;
	register size_t size;
	register int (*compar) __P((const void *, const void *));
{
	register const char *base = base0;
	register size_t lim;
	register int cmp;
	register const void *p;

	for (lim = nmemb; lim != 0; lim >>= 1) {
		p = base + (lim >> 1) * size;
		cmp = (*compar)(key, p);
		if (cmp == 0)
			return ((void *)p);
		if (cmp > 0) {	/* key > p: move right */
			base = (char *)p + size;
			lim--;
		}		/* else move left */
	}
	return (NULL);
}
#endif /* notdef */

#include <mach-o/loader.h>
#include <mach-o/nlist.h>
#include <string.h>

static
inline
struct nlist *
inline_bsearch_nlist(
register const void *key,
/* const void *base0 */
const struct nlist *base0,
size_t nmemb)

/* register size_t size; */
/* size -> sizeof(struct nlist) */
/* register int (*compar) __P((const void *, const void *)); */
/* compar -> nlist_bsearch -> strcmp using nlist_bsearch_strings as a base to
			      the string table */
{
	register const struct nlist *base = base0;
	register size_t lim;
	register int cmp;
	register const struct nlist *p;

	for (lim = nmemb; lim != 0; lim >>= 1) {
/* p = base + (lim >> 1) * sizeof(struct nlist); */
		p = base + (lim >> 1);

		cmp = strcmp(key, nlist_bsearch_strings + p->n_un.n_strx);
		if (cmp == 0)
			return ((void *)p);
		if (cmp > 0) {	/* key > p: move right */
/* base = (char *)p + sizeof(struct nlist); */
			base = p + 1;
			lim--;
		}		/* else move left */
	}
	return(NULL);
}


static
inline
struct dylib_table_of_contents *
inline_bsearch_toc(
register const void *key,
/* const void *base0 */
const struct dylib_table_of_contents *base0,
size_t nmemb)

/* register size_t size; */
/* size -> sizeof(struct dylib_table_of_contents) */
/* register int (*compar) __P((const void *, const void *)); */
/* compar -> nlist_bsearch -> strcmp using
    toc_bsearch_symbols as a base for the symbols
    and nlist_bsearch_strings as a base to the string table */
{
	register const struct dylib_table_of_contents *base = base0;
	register size_t lim;
	register int cmp;
	register const struct dylib_table_of_contents *p;

	for (lim = nmemb; lim != 0; lim >>= 1) {
/* p = base + (lim >> 1) * sizeof(struct dylib_table_of_contents); */
		p = base + (lim >> 1);

		cmp = strcmp(key, toc_bsearch_strings +
			     toc_bsearch_symbols[p->symbol_index].n_un.n_strx);
		if (cmp == 0)
			return ((void *)p);
		if (cmp > 0) {	/* key > p: move right */
/* base = (char *)p + sizeof(struct dylib_table_of_contents); */
			base = p + 1;
			lim--;
		}		/* else move left */
	}
	return(NULL);
}

/*
 * inline_bsearch_toc_with_index() searches for symbol_name in the specified
 * tocs (table of contents) with ntocs number of entries.  If starting_index is
 * less then the number of entries then that is used as the start of the search.
 * the symbol table for the table of contents is pointed to by
 * toc_bsearch_symbols and the string table is pointed to by
 * toc_bsearch_strings.  If a table of contents entry for the symbol is found
 * a pointer to it is returned.  Else NULL is returned.
 */
static
inline
struct dylib_table_of_contents *
inline_bsearch_toc_with_index(
char *symbol_name,
struct dylib_table_of_contents *tocs,
unsigned long ntocs,
unsigned long starting_index)
{
    long low, high, mid;
    int cmp;

	low = 0;
	high = ntocs - 1;
	if(starting_index >= ntocs)
	    mid = (low + high) / 2;
	else
	    mid = starting_index;
	while(low <= high){
	    cmp = strcmp(symbol_name, toc_bsearch_strings +
		     toc_bsearch_symbols[tocs[mid].symbol_index].n_un.n_strx);
	    if(cmp == 0)
		return(tocs + mid);
	    if(cmp > 0){ /* symbol_name > tocs[mid] move right */
		low = mid + 1;
	    }
	    else{ /* else move left */
		high = mid - 1;
	    }
	    mid = (low + high) / 2;
	}
	return(NULL);
}
