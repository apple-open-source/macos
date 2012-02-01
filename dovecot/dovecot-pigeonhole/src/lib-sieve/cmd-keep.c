/* Copyright (c) 2002-2011 Pigeonhole authors, see the included COPYING file
 */

#include "lib.h"

#include "sieve-common.h"
#include "sieve-commands.h"
#include "sieve-code.h"
#include "sieve-dump.h"
#include "sieve-actions.h"
#include "sieve-validator.h" 
#include "sieve-generator.h"
#include "sieve-interpreter.h"
#include "sieve-result.h"

/* 
 * Keep command 
 *
 * Syntax:
 *   keep
 */	

static bool cmd_keep_generate
	(const struct sieve_codegen_env *cgenv, struct sieve_command *cmd);

const struct sieve_command_def cmd_keep = { 
	"keep", 
	SCT_COMMAND, 
	0, 0, FALSE, FALSE,
	NULL, NULL, NULL, NULL,
	cmd_keep_generate, 
	NULL
};

/* 
 * Keep operation 
 */

static bool cmd_keep_operation_dump
	(const struct sieve_dumptime_env *denv, sieve_size_t *address);
static int cmd_keep_operation_execute
	(const struct sieve_runtime_env *renv, sieve_size_t *address);

const struct sieve_operation_def cmd_keep_operation = { 
	"KEEP",
	NULL,
	SIEVE_OPERATION_KEEP,
	cmd_keep_operation_dump, 
	cmd_keep_operation_execute 
};

/*
 * Code generation
 */

static bool cmd_keep_generate
(const struct sieve_codegen_env *cgenv, struct sieve_command *cmd) 
{
	/* Emit opcode */
	sieve_operation_emit(cgenv->sblock, NULL, &cmd_keep_operation);

	/* Generate arguments */
	return sieve_generate_arguments(cgenv, cmd, NULL);
}

/* 
 * Code dump
 */

static bool cmd_keep_operation_dump
(const struct sieve_dumptime_env *denv, sieve_size_t *address)
{
	sieve_code_dumpf(denv, "KEEP");
	sieve_code_descend(denv);

	return ( sieve_action_opr_optional_dump(denv, address, NULL) == 0 );
}

/*
 * Interpretation
 */

static int cmd_keep_operation_execute
(const struct sieve_runtime_env *renv, sieve_size_t *address)
{
	struct sieve_side_effects_list *slist = NULL;
	int ret = 0;

	/*
	 * Read data
	 */

	/* Optional operands (side effects only) */
	if ( sieve_action_opr_optional_read(renv, address, NULL, &ret, &slist) != 0 ) 
		return ret;

	/*
	 * Perform operation
	 */

	sieve_runtime_trace(renv, SIEVE_TRLVL_ACTIONS, 
		"keep action; store message in default mailbox");

	/* Add keep action to result. 
	 */
	if ( sieve_result_add_keep(renv, slist) < 0 )
		return SIEVE_EXEC_FAILURE;

	return SIEVE_EXEC_OK;
}


