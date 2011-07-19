/* Copyright (c) 2002-2011 Pigeonhole authors, see the included COPYING file
 */

/* Extension comparator-i;ascii-numeric
 * ------------------------------------
 *
 * Author: Stephan Bosch
 * Specification: RFC 2244
 * Implementation: full
 * Status: testing
 *
 */

#include "sieve-common.h"

#include "sieve-code.h"
#include "sieve-extensions.h"
#include "sieve-comparators.h"
#include "sieve-validator.h"
#include "sieve-generator.h"
#include "sieve-interpreter.h"

#include <ctype.h>

/* 
 * Forward declarations 
 */

static const struct sieve_operand_def my_comparator_operand;

const struct sieve_comparator_def i_ascii_numeric_comparator;

/* 
 * Extension
 */

static bool ext_cmp_i_ascii_numeric_validator_load
	(const struct sieve_extension *ext, struct sieve_validator *validator);

const struct sieve_extension_def comparator_i_ascii_numeric_extension = { 
	"comparator-i;ascii-numeric", 
	NULL, NULL,
	ext_cmp_i_ascii_numeric_validator_load,
	NULL, NULL, NULL, NULL, NULL,
	SIEVE_EXT_DEFINE_NO_OPERATIONS, 
	SIEVE_EXT_DEFINE_OPERAND(my_comparator_operand)
};

static bool ext_cmp_i_ascii_numeric_validator_load
(const struct sieve_extension *ext, struct sieve_validator *validator)
{
	sieve_comparator_register(validator, ext, &i_ascii_numeric_comparator);
	return TRUE;
}

/*
 * Operand
 */

static const struct sieve_extension_objects ext_comparators =
	SIEVE_EXT_DEFINE_COMPARATOR(i_ascii_numeric_comparator);
	
static const struct sieve_operand_def my_comparator_operand = { 
	"comparator-i;ascii-numeric", 
	&comparator_i_ascii_numeric_extension,
	0, 
	&sieve_comparator_operand_class,
	&ext_comparators
};

/*
 * Comparator
 */

/* Forward declarations */
 
static int cmp_i_ascii_numeric_compare
	(const struct sieve_comparator *cmp, 
		const char *val1, size_t val1_size, const char *val2, size_t val2_size);

/* Comparator object */

const struct sieve_comparator_def i_ascii_numeric_comparator = { 
	SIEVE_OBJECT("i;ascii-numeric", &my_comparator_operand, 0),
	SIEVE_COMPARATOR_FLAG_ORDERING | SIEVE_COMPARATOR_FLAG_EQUALITY,
	cmp_i_ascii_numeric_compare,
	NULL,
	NULL
};

/* Comparator implementation */

static int cmp_i_ascii_numeric_compare
	(const struct sieve_comparator *cmp ATTR_UNUSED, 
		const char *val, size_t val_size, const char *key, size_t key_size)
{	
	const char *vend = val + val_size;
	const char *kend = key + key_size;
	const char *vp = val;
	const char *kp = key;
	int digits, i;

	/* RFC 4790: All input is valid; strings that do not start with a digit 
	 * represent positive infinity.
	 */
	if ( !i_isdigit(*vp) ) {
		if ( i_isdigit(*kp) ) {
			/* Value is greater */
			return 1;
		}
	} else {
		if ( !i_isdigit(*kp) ) {
			/* Value is less */
			return -1;
		}
	}
	
	/* Ignore leading zeros */

	while ( *vp == '0' && vp < vend )  
		vp++;

	while ( *kp == '0' && kp < kend )  
		kp++;

	/* Check whether both numbers are equally long in terms of digits */

	digits = 0;
	while ( vp < vend && kp < kend && i_isdigit(*vp) && i_isdigit(*kp) ) {
		vp++;
		kp++;
		digits++;	
	}

	if ( vp == vend || !i_isdigit(*vp) ) {
		if ( kp != kend && i_isdigit(*kp) ) {
			/* Value is less */
			return -1;
		}
	} else {
		/* Value is greater */	
		return 1;
	}

	/* Equally long: compare digits */

	vp -= digits;
	kp -= digits;
	i = 0;
	while ( i < digits ) {
		if ( *vp > *kp )
			return 1;
		else if ( *vp < *kp )
			return -1;

		kp++;
		vp++;
		i++;
	}
		
	return 0;
}

