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

#include <stdlib.h>
#include <string.h>
#include <mach/boolean.h>

#define LIBTOP_DBG
#ifndef LIBTOP_DBG
/* Disable assertions. */
#ifndef NDEBUG
#define NDEBUG
#endif
#endif
#include <assert.h>

#include "qr.h"
#include "ql.h"
#include "ch.h"

#ifdef LIBTOP_DBG
#define LIBTOP_CH_MAGIC 0x574936af
#define LIBTOP_CHI_MAGIC 0xabdcee0e
#endif

void
ch_new(libtop_ch_t *a_ch, unsigned a_table_size,
    libtop_ch_hash_t *a_hash, libtop_ch_key_comp_t *a_key_comp)
{
	assert(a_table_size > 0);
	assert(a_hash != NULL);
	assert(a_key_comp != NULL);
	assert(a_ch != NULL);

	memset(a_ch, 0, LIBTOP_CH_TABLE2SIZEOF(a_table_size));

	a_ch->table_size = a_table_size;
	a_ch->hash = a_hash;
	a_ch->key_comp = a_key_comp;

#ifdef LIBTOP_DBG
	a_ch->magic = LIBTOP_CH_MAGIC;
#endif
}

void
ch_delete(libtop_ch_t *a_ch)
{
	libtop_chi_t	*chi;

	assert(a_ch != NULL);
	assert(a_ch->magic == LIBTOP_CH_MAGIC);

#ifdef LIBTOP_CH_VERBOSE
	fprintf(stderr,
	    "%s(%p): num_collisions: %llu, num_inserts: %llu,"
	    " num_removes: %llu, num_searches: %llu\n",
	    __FUNCTION__, a_ch, a_ch->num_collisions, a_ch->num_inserts,
	    a_ch->num_removes, a_ch->num_searches);
#endif

	while (ql_first(&a_ch->chi_ql) != NULL) {
		chi = ql_first(&a_ch->chi_ql);
		assert(chi != NULL);
		assert(chi->magic == LIBTOP_CHI_MAGIC);
		ql_head_remove(&a_ch->chi_ql, libtop_chi_t, ch_link);
#ifdef LIBTOP_DBG
		memset(chi, 0x5a, sizeof(libtop_chi_t));
#endif
	}

#ifdef LIBTOP_DBG
	memset(a_ch, 0x5a, LIBTOP_CH_TABLE2SIZEOF(a_ch->table_size));
#endif
}

unsigned
ch_count(libtop_ch_t *a_ch)
{
	assert(a_ch != NULL);
	assert(a_ch->magic == LIBTOP_CH_MAGIC);

	return a_ch->count;
}

void
ch_insert(libtop_ch_t *a_ch, const void *a_key, const void *a_data,
    libtop_chi_t *a_chi)
{
	unsigned	slot;
	libtop_chi_t	*chi;

	assert(a_ch != NULL);
	assert(a_ch->magic == LIBTOP_CH_MAGIC);
	assert(a_chi != NULL);

	/* Initialize chi. */
	chi = a_chi;
	chi->key = a_key;
	chi->data = a_data;
	ql_elm_new(chi, ch_link);
	ql_elm_new(chi, slot_link);
	slot = a_ch->hash(a_key) % a_ch->table_size;
	chi->slot = slot;
#ifdef LIBTOP_DBG
	chi->magic = LIBTOP_CHI_MAGIC;
#endif

	/* Hook into ch-wide list. */
	ql_tail_insert(&a_ch->chi_ql, chi, ch_link);

	/* Hook into the slot list. */
#ifdef LIBTOP_CH_COUNT
	if (ql_first(&a_ch->table[slot]) != NULL) {
		a_ch->num_collisions++;
	}
#endif
	ql_head_insert(&a_ch->table[slot], chi, slot_link);

	a_ch->count++;
#ifdef LIBTOP_CH_COUNT
	a_ch->num_inserts++;
#endif
}

boolean_t
ch_remove(libtop_ch_t *a_ch, const void *a_search_key, void **r_key,
    void **r_data, libtop_chi_t **r_chi)
{
	boolean_t	retval;
	unsigned	slot;
	libtop_chi_t	*chi;

	assert(a_ch != NULL);
	assert(a_ch->magic == LIBTOP_CH_MAGIC);

	slot = a_ch->hash(a_search_key) % a_ch->table_size;

	for (chi = ql_first(&a_ch->table[slot]);
	     chi != NULL;
	     chi = ql_next(&a_ch->table[slot], chi, slot_link)) {
		assert(chi != NULL);
		assert(chi->magic == LIBTOP_CHI_MAGIC);

		/* Is this the chi we want? */
		if (a_ch->key_comp(a_search_key, chi->key)) {
			/* Detach from ch-wide list. */
			ql_remove(&a_ch->chi_ql, chi, ch_link);
			
			/* Detach from the slot list. */
			ql_remove(&a_ch->table[slot], chi, slot_link);

			if (r_key != NULL) {
				*r_key = (void *)chi->key;
			}
			if (r_data != NULL) {
				*r_data = (void *)chi->data;
			}
			if (r_chi != NULL) {
#ifdef LIBTOP_DBG
				chi->magic = 0;
#endif
				*r_chi = chi;
			}

			a_ch->count--;
#ifdef LIBTOP_CH_COUNT
			a_ch->num_removes++;
#endif
			retval = FALSE;
			goto RETURN;
		}
	}

	retval = TRUE;
	RETURN:
	return retval;
}

void
ch_clear(libtop_ch_t *a_ch)
{
	assert(a_ch != NULL);
	assert(a_ch->magic == LIBTOP_CH_MAGIC);

	ch_new(a_ch, a_ch->table_size, a_ch->hash, a_ch->key_comp);
}

boolean_t
ch_search(libtop_ch_t *a_ch, const void *a_key, void **r_data)
{
	boolean_t	retval;
	unsigned	slot;
	libtop_chi_t	*chi;

	assert(a_ch != NULL);
	assert(a_ch->magic == LIBTOP_CH_MAGIC);

	slot = a_ch->hash(a_key) % a_ch->table_size;

	for (chi = ql_first(&a_ch->table[slot]);
	     chi != NULL;
	     chi = ql_next(&a_ch->table[slot], chi, slot_link)) {
		assert(chi != NULL);
		assert(chi->magic == LIBTOP_CHI_MAGIC);

		/* Is this the chi we want? */
		if (a_ch->key_comp(a_key, chi->key) == TRUE) {
			if (r_data != NULL) {
				*r_data = (void *)chi->data;
			}
			retval = FALSE;
			goto RETURN;
		}
	}

	retval = TRUE;
	RETURN:
#ifdef LIBTOP_CH_COUNT
	a_ch->num_searches++;
#endif
	return retval;
}

boolean_t
ch_get_iterate(libtop_ch_t *a_ch, void **r_key, void **r_data)
{
	boolean_t	retval;
	libtop_chi_t	*chi;

	assert(a_ch != NULL);
	assert(a_ch->magic == LIBTOP_CH_MAGIC);

	chi = ql_first(&a_ch->chi_ql);
	if (chi == NULL) {
		retval = TRUE;
		goto RETURN;
	}
	assert(chi != NULL);
	assert(chi->magic == LIBTOP_CHI_MAGIC);
	if (r_key != NULL) {
		*r_key = (void *)chi->key;
	}
	if (r_data != NULL) {
		*r_data = (void *)chi->data;
	}

	/* Rotate the list. */
	ql_first(&a_ch->chi_ql) = qr_next(ql_first(&a_ch->chi_ql), ch_link);

	retval = FALSE;
	RETURN:
	return retval;
}

boolean_t
ch_remove_iterate(libtop_ch_t *a_ch, void **r_key, void **r_data,
    libtop_chi_t **r_chi)
{
	boolean_t	retval;
	libtop_chi_t	*chi;

	assert(a_ch != NULL);
	assert(a_ch->magic == LIBTOP_CH_MAGIC);

	chi = ql_first(&a_ch->chi_ql);
	if (chi == NULL) {
		retval = TRUE;
		goto RETURN;
	}
	assert(chi != NULL);
	assert(chi->magic == LIBTOP_CHI_MAGIC);

	/* Detach from the ch-wide list. */
	ql_remove(&a_ch->chi_ql, chi, ch_link);

	/* Detach from the slot list. */
	ql_remove(&a_ch->table[chi->slot], chi, slot_link);

	if (r_key != NULL) {
		*r_key = (void *)chi->key;
	}
	if (r_data != NULL) {
		*r_data = (void *)chi->data;
	}
	if (r_chi != NULL) {
#ifdef LIBTOP_DBG
		chi->magic = 0;
#endif
		*r_chi = chi;
	}

	a_ch->count--;
#ifdef LIBTOP_CH_COUNT
	a_ch->num_removes++;
#endif

	retval = FALSE;
	RETURN:
	return retval;
}

unsigned
ch_string_hash(const void *a_key)
{
	unsigned	retval, c;
	char		*str;

	assert(a_key != NULL);

	for (str = (char *)a_key, retval = 5381; (c = *str) != 0; str++) {
		retval = ((retval << 5) + retval) + c;
	}

	return retval;
}

unsigned
ch_direct_hash(const void *a_key)
{
	unsigned	t = (unsigned)a_key;

	/* Shift right until we've shifted one 1 bit off. */
#if (SIZEOF_INT_P == 8)
	t >>= 32 * !(t & 0xffffffff);
#endif
	t >>= 16 * !(t & 0xffff);
	t >>= 8 * !(t & 0xff);
	t >>= 4 * !(t & 0xf);
	t >>= 2 * !(t & 0x3);
	t >>= 1 * !(t & 0x1);
	t >>= 1;

	return t;
}

boolean_t
ch_string_key_comp(const void *a_k1, const void *a_k2)
{
	assert(a_k1 != NULL);
	assert(a_k2 != NULL);

	return strcmp((char *)a_k1, (char *)a_k2) ? FALSE : TRUE;
}

boolean_t
ch_direct_key_comp(const void *a_k1, const void *a_k2)
{
	return (a_k1 == a_k2) ? TRUE : FALSE;
}
