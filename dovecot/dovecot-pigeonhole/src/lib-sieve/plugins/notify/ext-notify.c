/* Copyright (c) 2002-2011 Pigeonhole authors, see the included COPYING file
 */

/* Extension notify
 * ----------------
 *
 * Authors: Stephan Bosch
 * Specification: draft-ietf-sieve-notify-00.txt
 * Implementation: full, but deprecated; provided for backwards compatibility
 * Status: testing
 *
 */

#include "sieve-common.h"

#include "sieve-code.h"
#include "sieve-extensions.h"
#include "sieve-actions.h"
#include "sieve-commands.h"
#include "sieve-validator.h"
#include "sieve-generator.h"
#include "sieve-interpreter.h"
#include "sieve-result.h"

#include "ext-notify-common.h"

/*
 * Operations
 */

const struct sieve_operation_def *ext_notify_operations[] = {
	&notify_old_operation,
	&denotify_operation
};

/* 
 * Extension
 */

static bool ext_notify_validator_load
	(const struct sieve_extension *ext, struct sieve_validator *valdtr);

const struct sieve_extension_def notify_extension = { 
	"notify", 
	NULL,
	NULL,
	ext_notify_validator_load, 
	NULL, NULL, NULL, NULL, NULL,
	SIEVE_EXT_DEFINE_OPERATIONS(ext_notify_operations),
	SIEVE_EXT_DEFINE_NO_OPERANDS,
};

/*
 * Extension validation
 */

static bool ext_notify_validator_extension_validate
	(const struct sieve_extension *ext, struct sieve_validator *valdtr, 
		void *context, struct sieve_ast_argument *require_arg);

const struct sieve_validator_extension notify_validator_extension = {
	&notify_extension,
	ext_notify_validator_extension_validate,
	NULL
};

static bool ext_notify_validator_load
(const struct sieve_extension *ext, struct sieve_validator *valdtr)
{
	/* Register validator extension to check for conflict with enotify */
	sieve_validator_extension_register
		(valdtr, ext, &notify_validator_extension, NULL);

	/* Register new commands */
	sieve_validator_register_command(valdtr, ext, &cmd_notify_old);
	sieve_validator_register_command(valdtr, ext, &cmd_denotify);
	
	return TRUE;
}

static bool ext_notify_validator_extension_validate
(const struct sieve_extension *ext, struct sieve_validator *valdtr, 
	void *context ATTR_UNUSED, struct sieve_ast_argument *require_arg)
{
	const struct sieve_extension *ext_entfy;

	if ( (ext_entfy=sieve_extension_get_by_name(ext->svinst, "enotify")) != NULL ) {

		/* Check for conflict with enotify */
		if ( sieve_validator_extension_loaded(valdtr, ext_entfy) ) {
			sieve_argument_validate_error(valdtr, require_arg,
				"the (deprecated) notify extension cannot be used "
				"together with the enotify extension");
			return FALSE;
		}
	}

	return TRUE;
}


