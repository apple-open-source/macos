/* Copyright (c) 2002-2011 Pigeonhole authors, see the included COPYING file
 */

/* Extension vacation
 * ------------------
 *
 * Authors: Stephan Bosch <stephan@rename-it.nl>
 * Specification: RFC 5230
 * Implementation: full
 * Status: testing
 *
 */

#include "lib.h"

#include "sieve-common.h"

#include "sieve-extensions.h"
#include "sieve-commands.h"
#include "sieve-validator.h"
#include "sieve-generator.h"
#include "sieve-interpreter.h"
#include "sieve-dump.h"

#include "ext-vacation-common.h"

/*
 * Extension
 */

static bool ext_vacation_validator_load
	(const struct sieve_extension *ext, struct sieve_validator *valdtr);

const struct sieve_extension_def vacation_extension = { 
	"vacation",
	ext_vacation_load,
	ext_vacation_unload,
	ext_vacation_validator_load, 
	NULL, NULL, NULL, NULL, NULL,
	SIEVE_EXT_DEFINE_OPERATION(vacation_operation),
	SIEVE_EXT_DEFINE_NO_OPERANDS
};

static bool ext_vacation_validator_load
(const struct sieve_extension *ext, struct sieve_validator *valdtr)
{
	/* Register new command */
	sieve_validator_register_command(valdtr, ext, &vacation_command);

	return TRUE;
}
