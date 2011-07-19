/* Copyright (c) 2002-2011 Pigeonhole authors, see the included COPYING file
 */

/* Extensions spamtest, spamtestplus and virustest
 * -----------------------------------------------
 *
 * Authors: Stephan Bosch
 * Specification: RFC 5235
 * Implementation: full
 * Status: experimental
 *
 */

/* Configuration examples:
 *
 * # 1: X-Spam-Score: No, score=-3.2
 *
 * sieve_spamtest_status_header = \
 *   X-Spam-Score: [[:alnum:]]+, score=(-?[[:digit:]]+\.[[:digit:]])
 * sieve_spamtest_max_value = 5.0
 * 
 * # 2: X-Spam-Status: Yes
 *
 * sieve_spamtest_status_header = X-Spam-Status
 * sieve_spamtest_status_type = yesno
 * sieve_spamtest_max_value = Yes
 *
 * # 3: X-Spam-Score: sssssss
 * sieve_spamtest_status_header = X-Spam-Score
 * sieve_spamtest_status_type = strlen
 * sieve_spamtest_max_value = 5
 *
 * # 4: X-Spam-Score: status=3.2 required=5.0
 *
 * sieve_spamtest_status_header = \
 *   X-Spam-Score: score=(-?[[:digit:]]+\.[[:digit:]]).* 
 * sieve_spamtest_max_header = \
 *   X-Spam-Score: score=-?[[:digit:]]+\.[[:digit:]] required=([[:digit:]]+\.[[:digit:]]) 
 *
 * # 5: X-Virus-Scan: Found to be clean.
 *
 * sieve_virustest_status_header = \
 *   X-Virus-Scan: Found to be (.+)\.
 * sieve_virustest_status_type = text
 * sieve_virustest_text_value1 = clean
 * sieve_virustest_text_value5 = infected
 */
 
#include "lib.h"
#include "array.h"

#include "sieve-common.h"

#include "sieve-extensions.h"
#include "sieve-commands.h"

#include "sieve-validator.h"

#include "ext-spamvirustest-common.h"

/* 
 * Extensions 
 */

/* Spamtest */

static bool ext_spamvirustest_validator_load
(const struct sieve_extension *ext, struct sieve_validator *validator);

const struct sieve_extension_def spamtest_extension = { 
	"spamtest", 
	ext_spamvirustest_load, 
	ext_spamvirustest_unload,
	ext_spamvirustest_validator_load, 
	NULL, NULL, NULL, NULL, NULL,
	SIEVE_EXT_DEFINE_OPERATION(spamtest_operation), 
	SIEVE_EXT_DEFINE_NO_OPERANDS
};

const struct sieve_extension_def spamtestplus_extension = { 
	"spamtestplus", 
	ext_spamvirustest_load,  
	ext_spamvirustest_unload,
	ext_spamvirustest_validator_load, 
	NULL, NULL, NULL, NULL, NULL,
	SIEVE_EXT_DEFINE_OPERATION(spamtest_operation), 
	SIEVE_EXT_DEFINE_NO_OPERANDS
};

const struct sieve_extension_def virustest_extension = { 
	"virustest", 
	ext_spamvirustest_load, 
	ext_spamvirustest_unload,
	ext_spamvirustest_validator_load, 
	NULL, NULL, NULL, NULL, NULL,
	SIEVE_EXT_DEFINE_OPERATION(virustest_operation), 
	SIEVE_EXT_DEFINE_NO_OPERANDS
};

/*
 * Implementation
 */

static bool ext_spamtest_validator_extension_validate
	(const struct sieve_extension *ext, struct sieve_validator *valdtr, 
		void *context, struct sieve_ast_argument *require_arg);

const struct sieve_validator_extension spamtest_validator_extension = {
	&spamtest_extension,
	ext_spamtest_validator_extension_validate,
	NULL
};

static bool ext_spamvirustest_validator_load
(const struct sieve_extension *ext, struct sieve_validator *valdtr)
{
	/* Register new test */

	if ( sieve_extension_is(ext, virustest_extension) ) {
		sieve_validator_register_command(valdtr, ext, &virustest_test);
	} else {
		if ( sieve_extension_is(ext, spamtest_extension) ) {
			/* Register validator extension to warn for duplicate */
			sieve_validator_extension_register
				(valdtr, ext, &spamtest_validator_extension, NULL);
		}

		sieve_validator_register_command(valdtr, ext, &spamtest_test);
	}

	return TRUE;
}

static bool ext_spamtest_validator_extension_validate
(const struct sieve_extension *ext, struct sieve_validator *valdtr, 
	void *context ATTR_UNUSED, struct sieve_ast_argument *require_arg)
{
	const struct sieve_extension *ext_spamtestplus =
		sieve_extension_get_by_name(ext->svinst, "spamtestplus");

	if ( ext_spamtestplus != NULL &&
		sieve_validator_extension_loaded(valdtr, ext_spamtestplus) ) {
		sieve_argument_validate_warning(valdtr, require_arg,
			"the spamtest and spamtestplus extensions should not be specified "
			"at the same time");
	}

	return TRUE;
}


