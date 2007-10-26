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
#include "dch.h"

#ifdef LIBTOP_DBG
#define LIBTOP_DCH_MAGIC 0x4327589e
#endif

static boolean_t
dch_p_grow(libtop_dch_t *a_dch);
static boolean_t
dch_p_shrink(libtop_dch_t *a_dch);
static void
dch_p_insert(libtop_ch_t *a_ch, libtop_chi_t * a_chi);

libtop_dch_t *
dch_new(libtop_dch_t *a_dch, unsigned a_base_table,
    unsigned a_base_grow, unsigned a_base_shrink,
    libtop_ch_hash_t *a_hash, libtop_ch_key_comp_t *a_key_comp)
{
	libtop_dch_t	*retval;

	assert(a_base_table > 0);
	assert(a_base_grow > 0);
	assert(a_base_grow > a_base_shrink);
	assert(a_hash != NULL);
	assert(a_key_comp != NULL);

	if (a_dch != NULL) {
		retval = a_dch;
		memset(retval, 0, sizeof(libtop_dch_t));
		retval->is_malloced = FALSE;
	} else {
		retval = (libtop_dch_t *)malloc(sizeof(libtop_dch_t));
		memset(retval, 0, sizeof(libtop_dch_t));
		retval->is_malloced = TRUE;
	}

	retval->base_table = a_base_table;
	retval->base_grow = a_base_grow;
	retval->base_shrink = a_base_shrink;
	retval->grow_factor = 1;
	retval->hash = a_hash;
	retval->key_comp = a_key_comp;

	retval->ch =
	    (libtop_ch_t *)malloc(LIBTOP_CH_TABLE2SIZEOF(retval->base_table));
	if (retval->ch == NULL) {
		if (retval->is_malloced) {
			free(retval);
		}
		retval = NULL;
		goto RETURN;
	}
	ch_new(retval->ch, retval->base_table, retval->hash, retval->key_comp);

#ifdef LIBTOP_DBG
	retval->magic = LIBTOP_DCH_MAGIC;
#endif

	RETURN:
	return retval;
}

void
dch_delete(libtop_dch_t *a_dch)
{
	assert(a_dch != NULL);
	assert(a_dch->magic == LIBTOP_DCH_MAGIC);

#ifdef LIBTOP_DCH_VERBOSE
	fprintf(stderr,
	    "%s(%p): num_collisions: %llu, num_inserts: %llu,"
	    " num_removes: %llu, num_searches: %llu, num_grows: %llu,"
	    " num_shrinks: %llu\n",
	    __FUNCTION__, a_dch, a_dch->ch->num_collisions,
	    a_dch->ch->num_inserts, a_dch->ch->num_removes,
	    a_dch->ch->num_searches, a_dch->num_grows, a_dch->num_shrinks);
#endif

	ch_delete(a_dch->ch);

	if (a_dch->is_malloced) {
		free(a_dch);
	}
#ifdef LIBTOP_DBG
	else {
		memset(a_dch, 0x5a, sizeof(libtop_dch_t));
	}
#endif
}

unsigned
dch_count(libtop_dch_t *a_dch)
{
	assert(a_dch != NULL);
	assert(a_dch->magic == LIBTOP_DCH_MAGIC);

	return ch_count(a_dch->ch);
}

boolean_t
dch_insert(libtop_dch_t *a_dch, const void *a_key, const void *a_data,
    libtop_chi_t *a_chi)
{
	boolean_t	retval;

	assert(a_dch != NULL);
	assert(a_dch->magic == LIBTOP_DCH_MAGIC);

	if (dch_p_grow(a_dch)) {
		retval = TRUE;
		goto RETURN;
	}
	ch_insert(a_dch->ch, a_key, a_data, a_chi);

	retval = FALSE;
	RETURN:
	return retval;
}

boolean_t
dch_remove(libtop_dch_t *a_dch, const void *a_search_key, void **r_key,
    void **r_data, libtop_chi_t **r_chi)
{
	boolean_t	retval;

	assert(a_dch != NULL);
	assert(a_dch->magic == LIBTOP_DCH_MAGIC);

	if (dch_p_shrink(a_dch)
	    || ch_remove(a_dch->ch, a_search_key, r_key, r_data, r_chi)) {
		retval = TRUE;
		goto RETURN;
	}

	retval = FALSE;
	RETURN:
	return retval;
}

void
dch_clear(libtop_dch_t *a_dch)
{
	assert(a_dch != NULL);
	assert(a_dch->magic == LIBTOP_DCH_MAGIC);

	ch_clear(a_dch->ch);
}

boolean_t
dch_search(libtop_dch_t *a_dch, const void *a_key, void **r_data)
{
	assert(a_dch != NULL);
	assert(a_dch->magic == LIBTOP_DCH_MAGIC);

	return ch_search(a_dch->ch, a_key, r_data);
}

boolean_t
dch_get_iterate(libtop_dch_t *a_dch, void **r_key, void **r_data)
{
	assert(a_dch != NULL);
	assert(a_dch->magic == LIBTOP_DCH_MAGIC);

	return ch_get_iterate(a_dch->ch, r_key, r_data);
}

boolean_t
dch_remove_iterate(libtop_dch_t *a_dch, void **r_key, void **r_data,
    libtop_chi_t **r_chi)
{
	boolean_t	retval;

	assert(a_dch != NULL);
	assert(a_dch->magic == LIBTOP_DCH_MAGIC);

	dch_p_shrink(a_dch);
	if (ch_remove_iterate(a_dch->ch, r_key, r_data, r_chi)) {
		retval = TRUE;
		goto RETURN;
	}
	retval = FALSE;
	RETURN:
	return retval;
}

/*
 * Given the ch API, there is no way to both safely and efficiently transfer the
 * contents of one ch to another.  Therefore, this function mucks with ch
 * internals.
 */
static boolean_t
dch_p_grow(libtop_dch_t *a_dch)
{
	libtop_ch_t	*t_ch;
	libtop_chi_t	*chi;
	unsigned	count, i;

	count = ch_count(a_dch->ch);

	if ((count + 1) > (a_dch->grow_factor * a_dch->base_grow)) {
		/* Too big.  Create a new ch twice as large and populate it. */
		t_ch = (libtop_ch_t *)malloc(
		    LIBTOP_CH_TABLE2SIZEOF(a_dch->base_table
		    * a_dch->grow_factor * 2));
		if (t_ch == NULL) return FALSE;
		ch_new(t_ch, a_dch->base_table * a_dch->grow_factor * 2,
		    a_dch->hash, a_dch->key_comp);
		for (i = 0; i < count; i++) {
			chi = ql_first(&a_dch->ch->chi_ql);
			ql_remove(&a_dch->ch->chi_ql, chi, ch_link);
			ql_elm_new(chi, slot_link);
			dch_p_insert(t_ch, chi);
		}

		a_dch->grow_factor *= 2;
#ifdef LIBTOP_DCH_COUNT
		a_dch->num_grows++;
		t_ch->num_collisions += a_dch->ch->num_collisions;
		t_ch->num_inserts += a_dch->ch->num_inserts;
		t_ch->num_removes += a_dch->ch->num_removes;
		t_ch->num_searches += a_dch->ch->num_searches;
#endif
		/*
		 * Set to NULL to keep ch_delete() from deleting all the items.
		 */
		ql_first(&a_dch->ch->chi_ql) = NULL;
		ch_delete(a_dch->ch);
		a_dch->ch = t_ch;
	}

	return FALSE;
}

/*
 * Given the ch API, there is no way to both safely and efficiently transfer the
 * contents of one ch to another.  Therefore, this function mucks with ch
 * internals.
 */
static boolean_t
dch_p_shrink(libtop_dch_t *a_dch)
{
	libtop_ch_t	*t_ch;
	libtop_chi_t	*chi;
	unsigned	count, i;

	count = ch_count(a_dch->ch);

	if ((count - 1 < a_dch->base_shrink * a_dch->grow_factor)
	    && (a_dch->grow_factor > 1)) {
		unsigned	new_factor;

		/*
		 * Too big.  Create a new ch with the smallest grow factor that
		 * does not cause the ch to be overflowed.
		 */
		for (new_factor = 1;
		     new_factor * a_dch->base_grow <= count - 1;
		     new_factor *= 2) {
			assert(new_factor < a_dch->grow_factor);
		}
		assert(new_factor > 0);
		assert(new_factor < a_dch->grow_factor);

		t_ch = (libtop_ch_t *)malloc(
		    LIBTOP_CH_TABLE2SIZEOF(a_dch->base_table * new_factor));
		if (t_ch == NULL) return FALSE;

		ch_new(t_ch, a_dch->base_table * new_factor, a_dch->hash,
		    a_dch->key_comp);
		for (i = 0; i < count; i++) {
			chi = ql_first(&a_dch->ch->chi_ql);
			ql_remove(&a_dch->ch->chi_ql, chi, ch_link);
			ql_elm_new(chi, slot_link);
			dch_p_insert(t_ch, chi);
		}

		a_dch->grow_factor = new_factor;
#ifdef LIBTOP_DCH_COUNT
		a_dch->num_shrinks++;
		t_ch->num_collisions += a_dch->ch->num_collisions;
		t_ch->num_inserts += a_dch->ch->num_inserts;
		t_ch->num_removes += a_dch->ch->num_removes;
		t_ch->num_searches += a_dch->ch->num_searches;
#endif
		/*
		 * Set to NULL to keep ch_delete() from deleting all the items.
		 */
		ql_first(&a_dch->ch->chi_ql) = NULL;
		ch_delete(a_dch->ch);
		a_dch->ch = t_ch;
	}

	return FALSE;
}

/*
 * Given the ch API, there is no way to both safely and efficiently transfer the
 * contents of one ch to another.  Therefore, this function mucks with ch
 * internals.
 */
static void
dch_p_insert(libtop_ch_t *a_ch, libtop_chi_t *a_chi)
{
	unsigned	slot;

	/* Initialize a_chi. */
	slot = a_ch->hash(a_chi->key) % a_ch->table_size;
	a_chi->slot = slot;

	/* Hook into ch-wide list. */
	ql_tail_insert(&a_ch->chi_ql, a_chi, ch_link);

	/* Hook into the slot list. */
#ifdef LIBTOP_DCH_COUNT
	if (ql_first(&a_ch->table[slot]) != NULL) {
		a_ch->num_collisions++;
	}
#endif
	ql_head_insert(&a_ch->table[slot], a_chi, slot_link);

	a_ch->count++;
#ifdef LIBTOP_DCH_COUNT
	a_ch->num_inserts++;
#endif
}
