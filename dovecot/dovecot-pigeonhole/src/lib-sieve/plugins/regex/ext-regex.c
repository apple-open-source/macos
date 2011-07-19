/* Copyright (c) 2002-2011 Pigeonhole authors, see the included COPYING file
 */

/* Extension regex
 * ---------------
 *
 * Authors: Stephan Bosch
 * Specification: draft-murchison-sieve-regex-08 (not latest)
 * Implementation: full
 * Status: testing
 *
 */

/* FIXME: Regular expressions are compiled during compilation and
 * again during interpretation. This is suboptimal and should be
 * changed. This requires dumping the compiled regex to the binary.
 * Most likely, this will only be possible when we implement regular
 * expressions ourselves.
 *
 */

#include "lib.h"
#include "mempool.h"
#include "buffer.h"

#include "sieve-common.h"

#include "sieve-code.h"
#include "sieve-extensions.h"
#include "sieve-commands.h"

#include "sieve-comparators.h"
#include "sieve-match-types.h"

#include "sieve-validator.h"
#include "sieve-generator.h"
#include "sieve-interpreter.h"

#include "ext-regex-common.h"

#include <sys/types.h>
#include <regex.h>

/* 
 * Extension
 */

static bool ext_regex_validator_load
	(const struct sieve_extension *ext, struct sieve_validator *validator);

const struct sieve_extension_def regex_extension = { 
	"regex", 
	NULL, NULL,
	ext_regex_validator_load,
	NULL, NULL, NULL, NULL, NULL,
	SIEVE_EXT_DEFINE_NO_OPERATIONS, 
	SIEVE_EXT_DEFINE_OPERAND(regex_match_type_operand)
};

static bool ext_regex_validator_load
(const struct sieve_extension *ext, struct sieve_validator *valdtr)
{
	sieve_match_type_register(valdtr, ext, &regex_match_type); 

	return TRUE;
}


