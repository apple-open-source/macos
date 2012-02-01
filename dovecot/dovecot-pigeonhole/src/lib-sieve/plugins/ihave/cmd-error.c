/* Copyright (c) 2002-2011 Pigeonhole authors, see the included COPYING file
 */

#include "lib.h"
#include "str-sanitize.h"

#include "sieve-extensions.h"
#include "sieve-commands.h"
#include "sieve-code.h"

#include "sieve-validator.h"
#include "sieve-generator.h"
#include "sieve-binary.h"
#include "sieve-interpreter.h"
#include "sieve-dump.h"

#include "ext-ihave-common.h"

/*
 * Error command
 *
 * Syntax
 *   error <message: string>
 */

static bool cmd_error_validate
	(struct sieve_validator *valdtr, struct sieve_command *tst);
static bool cmd_error_generate
	(const struct sieve_codegen_env *cgenv,	struct sieve_command *ctx);

const struct sieve_command_def error_command = {
	"error",
	SCT_COMMAND,
	1, 0, FALSE, FALSE,
	NULL, NULL,
	cmd_error_validate,
	NULL,
	cmd_error_generate,
	NULL
};

/*
 * Body operation
 */

static bool cmd_error_operation_dump
	(const struct sieve_dumptime_env *denv, sieve_size_t *address);
static int cmd_error_operation_execute
	(const struct sieve_runtime_env *renv, sieve_size_t *address);

const struct sieve_operation_def error_operation = {
	"ERROR",
	&ihave_extension,
	0,
	cmd_error_operation_dump,
	cmd_error_operation_execute
};

/*
 * Validation
 */

static bool cmd_error_validate
(struct sieve_validator *valdtr, struct sieve_command *tst)
{
	struct sieve_ast_argument *arg = tst->first_positional;

	if ( !sieve_validate_positional_argument
		(valdtr, tst, arg, "message", 1, SAAT_STRING) ) {
		return FALSE;
	}

	return sieve_validator_argument_activate(valdtr, tst, arg, FALSE);
}

/*
 * Code generation
 */

static bool cmd_error_generate
(const struct sieve_codegen_env *cgenv, struct sieve_command *cmd)
{
	(void)sieve_operation_emit(cgenv->sblock, cmd->ext, &error_operation);

	/* Generate arguments */
	return sieve_generate_arguments(cgenv, cmd, NULL);
}

/*
 * Code dump
 */

static bool cmd_error_operation_dump
(const struct sieve_dumptime_env *denv, sieve_size_t *address)
{
	sieve_code_dumpf(denv, "ERROR");
	sieve_code_descend(denv);

	return sieve_opr_string_dump(denv, address, "message");
}

/*
 * Interpretation
 */

static int cmd_error_operation_execute
(const struct sieve_runtime_env *renv, sieve_size_t *address)
{
	string_t *message;
	int ret;

	/*
	 * Read operands
	 */

	/* Read message */

	if ( (ret=sieve_opr_string_read(renv, address, "message", &message)) <= 0 )
		return ret;

	/*
	 * Perform operation
	 */

	sieve_runtime_trace(renv, SIEVE_TRLVL_COMMANDS, "error \"%s\"",
		str_sanitize(str_c(message), 80));

	sieve_runtime_error(renv, NULL, "%s", str_c(message));

	return SIEVE_EXEC_FAILURE;
}
