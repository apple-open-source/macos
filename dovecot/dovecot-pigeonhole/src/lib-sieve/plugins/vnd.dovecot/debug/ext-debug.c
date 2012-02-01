/* Copyright (c) 2002-2011 Pigeonhole authors, see the included COPYING file
 */

/* Extension debug
 * ---------------
 *
 * Authors: Stephan Bosch
 * Specification: vendor-defined
 * Implementation: full
 * Status: experimental
 *
 */

#include "lib.h"
#include "array.h"

#include "sieve-extensions.h"
#include "sieve-commands.h"
#include "sieve-comparators.h"
#include "sieve-match-types.h"
#include "sieve-address-parts.h"

#include "sieve-validator.h"
#include "sieve-generator.h"
#include "sieve-binary.h"
#include "sieve-interpreter.h"
#include "sieve-dump.h"

#include "ext-debug-common.h"

/*
 * Extension
 */

static bool ext_debug_validator_load
	(const struct sieve_extension *ext, struct sieve_validator *validator);
static bool ext_debug_interpreter_load
	(const struct sieve_extension *ext ATTR_UNUSED,
		const struct sieve_runtime_env *renv, sieve_size_t *address ATTR_UNUSED);


const struct sieve_extension_def debug_extension = {
	"vnd.dovecot.debug",
	NULL, NULL,
	ext_debug_validator_load,
	NULL,
	ext_debug_interpreter_load,
	NULL, NULL, NULL,
	SIEVE_EXT_DEFINE_OPERATION(debug_log_operation),
	SIEVE_EXT_DEFINE_NO_OPERANDS
};

static bool ext_debug_validator_load
(const struct sieve_extension *ext, struct sieve_validator *validator)
{
	/* Register new test */
	sieve_validator_register_command(validator, ext, &debug_log_command);

	return TRUE;
}

static bool ext_debug_interpreter_load
(const struct sieve_extension *ext ATTR_UNUSED,
	const struct sieve_runtime_env *renv, sieve_size_t *address ATTR_UNUSED)
{
	struct sieve_error_handler *ehandler =
		sieve_interpreter_get_error_handler(renv->interp);

	if ( ehandler != NULL ) {
		sieve_error_handler_accept_infolog(ehandler, TRUE);
	}

	return TRUE;
}



