/* Copyright (c) 2002-2011 Pigeonhole authors, see the included COPYING file 
 */

/* Comparator 'i;octet': 
 *
 */

#include "lib.h"

#include "sieve-common.h"
#include "sieve-comparators.h"

#include <string.h>
#include <stdio.h>

/*
 * Forward declarations
 */

static int cmp_i_octet_compare
	(const struct sieve_comparator *cmp,
		const char *val1, size_t val1_size, const char *val2, size_t val2_size);
static bool cmp_i_octet_char_match
	(const struct sieve_comparator *cmp, const char **val1, const char *val1_end, 
		const char **val2, const char *val2_end);

/*
 * Comparator object
 */

const struct sieve_comparator_def i_octet_comparator = {
	SIEVE_OBJECT("i;octet",	&comparator_operand, SIEVE_COMPARATOR_I_OCTET),
	SIEVE_COMPARATOR_FLAG_ORDERING | SIEVE_COMPARATOR_FLAG_EQUALITY |
		SIEVE_COMPARATOR_FLAG_SUBSTRING_MATCH | SIEVE_COMPARATOR_FLAG_PREFIX_MATCH,
	cmp_i_octet_compare,
	cmp_i_octet_char_match,
	sieve_comparator_octet_skip	
};

/*
 * Comparator implementation
 */
 
static int cmp_i_octet_compare(
	const struct sieve_comparator *cmp ATTR_UNUSED,
	const char *val1, size_t val1_size, const char *val2, size_t val2_size)
{
	int result;

	if ( val1_size == val2_size ) {
		return memcmp((void *) val1, (void *) val2, val1_size);
	} 
	
	if ( val1_size > val2_size ) {
		result = memcmp((void *) val1, (void *) val2, val2_size);
		
		if ( result == 0 ) return 1;
		
		return result;
	} 

	result = memcmp((void *) val1, (void *) val2, val1_size);
		
	if ( result == 0 ) return -1;
		
	return result;
}

static bool cmp_i_octet_char_match
	(const struct sieve_comparator *cmp ATTR_UNUSED, 
		const char **val, const char *val_end, 
		const char **key, const char *key_end)
{
	const char *val_begin = *val;
	const char *key_begin = *key;
	
	while ( **val == **key && *val < val_end && *key < key_end ) {
		(*val)++;
		(*key)++;
	}
	
	if ( *key < key_end ) {
		/* Reset */
		*val = val_begin;
		*key = key_begin;	
	
		return FALSE;
	}
	
	return TRUE;
}
 
 
