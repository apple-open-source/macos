/* Copyright (c) 2002-2011 Pigeonhole authors, see the included COPYING file 
 */

/* Match-type ':contains' 
 */

#include "lib.h"

#include "sieve-match-types.h"
#include "sieve-comparators.h"
#include "sieve-match.h"

#include <string.h>
#include <stdio.h>

/*
 * Forward declarations
 */ 

static int mcht_contains_match_key
	(struct sieve_match_context *mctx, const char *val, size_t val_size, 
		const char *key, size_t key_size);

/*
 * Match-type object
 */

const struct sieve_match_type_def contains_match_type = {
	SIEVE_OBJECT("contains", &match_type_operand,	SIEVE_MATCH_TYPE_CONTAINS),
	NULL,
	sieve_match_substring_validate_context,
	NULL, NULL, NULL,
	mcht_contains_match_key,
	NULL
};

/*
 * Match-type implementation
 */

/* FIXME: Naive substring match implementation. Should switch to more 
 * efficient algorithm if large values need to be searched (e.g. message body).
 */
static int mcht_contains_match_key
(struct sieve_match_context *mctx, const char *val, size_t val_size, 
	const char *key, size_t key_size)
{
	const struct sieve_comparator *cmp = mctx->comparator;
	const char *vend = (const char *) val + val_size;
	const char *kend = (const char *) key + key_size;
	const char *vp = val;
	const char *kp = key;

	if ( val_size == 0 ) 
		return ( key_size == 0 );

	if ( cmp->def == NULL || cmp->def->char_match == NULL ) 
		return FALSE;

	while ( (vp < vend) && (kp < kend) ) {
		if ( !cmp->def->char_match(cmp, &vp, vend, &kp, kend) )
			vp++;
	}
    
	return (kp == kend);
}


