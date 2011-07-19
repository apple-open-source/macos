/* Copyright (c) 2002-2011 Pigeonhole authors, see the included COPYING file
 */

/* Extension imap4flags
 * --------------------
 *
 * Authors: Stephan Bosch
 * Specification: RFC 5232
 * Implementation: full
 * Status: testing
 *
 */

#include "lib.h"
#include "mempool.h"
#include "str.h"

#include "sieve-common.h"

#include "sieve-code.h"
#include "sieve-extensions.h"
#include "sieve-actions.h"
#include "sieve-commands.h"
#include "sieve-validator.h"
#include "sieve-generator.h"
#include "sieve-interpreter.h"

#include "ext-imap4flags-common.h"

/* 
 * Operations 
 */

const struct sieve_operation_def *imap4flags_operations[] = { 
	&setflag_operation, 
	&addflag_operation, 
	&removeflag_operation,
	&hasflag_operation 
};

/* 
 * Extension
 */

static bool ext_imap4flags_validator_load
	(const struct sieve_extension *ext, struct sieve_validator *valdtr);
static bool ext_imap4flags_interpreter_load
	(const struct sieve_extension *ext, const struct sieve_runtime_env *renv, 
		sieve_size_t *address);

const struct sieve_extension_def imap4flags_extension = { 
	"imap4flags", 
	NULL, NULL,
	ext_imap4flags_validator_load, 
	NULL, 
	ext_imap4flags_interpreter_load, 
	NULL, NULL, NULL,
	SIEVE_EXT_DEFINE_OPERATIONS(imap4flags_operations), 
	SIEVE_EXT_DEFINE_OPERAND(flags_side_effect_operand)
};

static bool ext_imap4flags_validator_load
(const struct sieve_extension *ext, struct sieve_validator *valdtr)
{
	/* Register commands */
	sieve_validator_register_command(valdtr, ext, &cmd_setflag);
	sieve_validator_register_command(valdtr, ext, &cmd_addflag);
	sieve_validator_register_command(valdtr, ext, &cmd_removeflag);
	sieve_validator_register_command(valdtr, ext, &tst_hasflag);

	/* Attach :flags tag to keep and fileinto commands */	
	ext_imap4flags_attach_flags_tag(valdtr, ext, "keep", FALSE);
	ext_imap4flags_attach_flags_tag(valdtr, ext, "fileinto", FALSE);

	return TRUE;
}

static bool ext_imap4flags_interpreter_load
(const struct sieve_extension *ext, const struct sieve_runtime_env *renv, 
	sieve_size_t *address ATTR_UNUSED)
{
	sieve_interpreter_extension_register
		(renv->interp, ext, &imap4flags_interpreter_extension, NULL);

	return TRUE;
}



