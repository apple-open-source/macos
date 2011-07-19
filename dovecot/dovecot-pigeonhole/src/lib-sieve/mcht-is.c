/* Copyright (c) 2002-2011 Pigeonhole authors, see the included COPYING file 
 */
 
/* Match-type ':is': 
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

static int mcht_is_match_key
	(struct sieve_match_context *mctx, const char *val, size_t val_size, 
		const char *key, size_t key_size);

/* 
 * Match-type object 
 */

const struct sieve_match_type_def is_match_type = {
	SIEVE_OBJECT("is", &match_type_operand, SIEVE_MATCH_TYPE_IS),
	NULL, NULL, NULL, NULL, NULL,
	mcht_is_match_key,
	NULL
};

/*
 * Match-type implementation
 */

static int mcht_is_match_key
(struct sieve_match_context *mctx ATTR_UNUSED, 
	const char *val, size_t val_size, 
	const char *key, size_t key_size)
{
	if ( val_size == 0 ) 
		return ( key_size == 0 );

	if ( mctx->comparator->def != NULL && mctx->comparator->def->compare != NULL )
		return (mctx->comparator->def->compare(mctx->comparator, 
			val, val_size, key, key_size) == 0);

	return FALSE;
}

