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

/* Maintain counters used to get an idea of performance. */
/* #define LIBTOP_CH_COUNT */
#ifdef LIBTOP_CH_COUNT
/* Print counter values to stderr in ch_delete(). */
/* #define LIBTOP_CH_VERBOSE */
#endif

/* Pseudo-opaque type. */
typedef struct libtop_ch_s libtop_ch_t;
typedef struct libtop_chi_s libtop_chi_t;

/*
 * Internal container used by ch, one per item.  chi's are internally linked to
 * multiple ql's in order to implement various LIFO/FIFO orderings.
 */
struct libtop_chi_s {
#ifdef LIBTOP_DBG
	unsigned	magic;
#endif
	/* Key. */
	const void	*key;

	/* Data. */
	const void	*data;

	/* Link into the ch-wide list of chi's. */
	ql_elm(libtop_chi_t) ch_link;

	/* Link into the slot's list of chi's. */
	ql_elm(libtop_chi_t) slot_link;

	/* Slot number. */
	unsigned	slot;
};

struct libtop_ch_s {
#ifdef LIBTOP_DBG
	unsigned	magic;
#endif

#ifdef LIBTOP_CH_COUNT
	/* Counters used to get an idea of performance. */
	unsigned	num_collisions;
	unsigned	num_inserts;
	unsigned	num_removes;
	unsigned	num_searches;
#endif

	/* Head of the list of chi's. */
	ql_head(libtop_chi_t) chi_ql;

	/* Total number of items. */
	unsigned	count;

	/* Number of table slots. */
	unsigned	table_size;

	/* Hashing and key comparison function pointers. */
	unsigned	(*hash)(const void *);
	boolean_t	(*key_comp)(const void *, const void *);

	/*
	 * Must be last field, since it is used for array indexing of chi's
	 * beyond the end of the structure.
	 */
	ql_head(libtop_chi_t) table[1];
};

/* Typedefs to allow easy function pointer passing. */
typedef unsigned libtop_ch_hash_t (const void *);
typedef boolean_t libtop_ch_key_comp_t (const void *, const void *);

/*
 * Calculates ch size, given the number of hash table slots.  Use this to
 * calculate space allocation when passing pre-allocated space to ch_new().
 */
#define LIBTOP_CH_TABLE2SIZEOF(t)					\
	(sizeof(libtop_ch_t) + (((t) - 1) * sizeof(libtop_chi_t *)))

void
ch_new(libtop_ch_t *a_ch, unsigned a_table_size,
    libtop_ch_hash_t *a_hash, libtop_ch_key_comp_t *a_key_comp);

void
ch_delete(libtop_ch_t *a_ch);

unsigned
ch_count(libtop_ch_t *a_ch);

void
ch_insert(libtop_ch_t *a_ch, const void *a_key, const void *a_data,
    libtop_chi_t *a_chi);

boolean_t
ch_remove(libtop_ch_t *a_ch, const void *a_search_key, void **r_key,
    void **r_data, libtop_chi_t **r_chi);

void
ch_clear(libtop_ch_t *a_ch);

boolean_t
ch_search(libtop_ch_t *a_ch, const void *a_key, void **r_data);

boolean_t
ch_get_iterate(libtop_ch_t *a_ch, void **r_key, void **r_data);

boolean_t
ch_remove_iterate(libtop_ch_t *a_ch, void **r_key, void **r_data,
    libtop_chi_t **r_chi);

unsigned
ch_string_hash(const void *a_key);

unsigned
ch_direct_hash(const void *a_key);

boolean_t
ch_string_key_comp(const void *a_k1, const void *a_k2);

boolean_t
ch_direct_key_comp(const void *a_k1, const void *a_k2);
