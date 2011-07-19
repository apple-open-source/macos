/* Copyright (c) 2002-2011 Pigeonhole authors, see the included COPYING file
 */

/* Extension relational
 * --------------------
 *
 * Author: Stephan Bosch
 * Specification: RFC 3431
 * Implementation: full
 * Status: testing
 *
 */

#include "lib.h"
#include "str.h"

#include "sieve-common.h"

#include "sieve-ast.h"
#include "sieve-code.h"
#include "sieve-extensions.h"
#include "sieve-commands.h"
#include "sieve-comparators.h"
#include "sieve-match-types.h"
#include "sieve-validator.h"
#include "sieve-generator.h"
#include "sieve-interpreter.h"

#include "ext-relational-common.h"

/*
 * Extension
 */

static bool ext_relational_validator_load
(const struct sieve_extension *ext, struct sieve_validator *valdtr);

const struct sieve_extension_def relational_extension = { 
	"relational", 
	NULL, NULL,
	ext_relational_validator_load,
	NULL, NULL, NULL, NULL, NULL,
	SIEVE_EXT_DEFINE_NO_OPERATIONS, 
	SIEVE_EXT_DEFINE_OPERAND(rel_match_type_operand)
};

static bool ext_relational_validator_load
(const struct sieve_extension *ext, struct sieve_validator *valdtr)
{
	sieve_match_type_register(valdtr, ext, &value_match_type); 
	sieve_match_type_register(valdtr, ext, &count_match_type); 

	return TRUE;
}


