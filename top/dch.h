/******************************************************************************
 *
 * Copyright (C) 2002 Jason Evans <jasone@canonware.com>.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice(s), this list of conditions and the following disclaimer
 *    unmodified other than the allowable addition of one or more
 *    copyright notices.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice(s), this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDER(S) ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT HOLDER(S) BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
 * OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 ******************************************************************************/

#ifdef LIBTOP_CH_COUNT
/* Maintain counters used to get an idea of performance. */
/* #define LIBTOP_DCH_COUNT */
#ifdef LIBTOP_DCH_COUNT
/* Print counter values to stderr in dch_delete(). */
/* #define LIBTOP_DCH_VERBOSE */
#endif
#endif

/* Pseudo-opaque type. */
typedef struct libtop_dch_s libtop_dch_t;

struct libtop_dch_s {
#ifdef LIBTOP_DBG
	unsigned	magic;
#endif

#ifdef LIBTOP_DCH_COUNT
	/* Counters used to get an idea of performance. */
	unsigned	num_grows;
	unsigned	num_shrinks;
#endif

	boolean_t	is_malloced;

	/*
	 * Intial table size and high/low water marks.  These values are used in
	 * proportion if the table grows.
	 */
	unsigned	base_table;
	unsigned	base_grow;
	unsigned	base_shrink;

	/* (grow_factor * base_table) is the current table size. */
	unsigned	grow_factor;

	/* Cached for later ch creation during rehashes. */
	unsigned	(*hash)(const void *);
	boolean_t	(*key_comp)(const void *, const void *);

	/* Where all of the real work is done. */
	libtop_ch_t	*ch;
};

libtop_dch_t *
dch_new(libtop_dch_t *a_dch, unsigned a_base_table,
    unsigned a_base_grow, unsigned a_base_shrink,
    libtop_ch_hash_t *a_hash, libtop_ch_key_comp_t *a_key_comp);

void
dch_delete(libtop_dch_t *a_dch);

unsigned
dch_count(libtop_dch_t *a_dch);

boolean_t
dch_insert(libtop_dch_t *a_dch, const void *a_key, const void *a_data,
    libtop_chi_t *a_chi);

boolean_t
dch_remove(libtop_dch_t *a_dch, const void *a_search_key, void **r_key,
    void **r_data, libtop_chi_t **r_chi);

void
dch_clear(libtop_dch_t *a_dch);

boolean_t
dch_search(libtop_dch_t *a_dch, const void *a_key, void **r_data);

boolean_t
dch_get_iterate(libtop_dch_t *a_dch, void **r_key, void **r_data);

boolean_t
dch_remove_iterate(libtop_dch_t *a_dch, void **r_key, void **r_data,
    libtop_chi_t **r_chi);
