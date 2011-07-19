/* Copyright (c) 2002-2011 Pigeonhole authors, see the included COPYING file
 */

#include "lib.h"
#include "str-sanitize.h"

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
 * Discard command 
 * 
 * Syntax
 *   discard
 */	

static bool cmd_discard_generate
	(const struct sieve_codegen_env *cgenv, 
		struct sieve_command *ctx ATTR_UNUSED); 

const struct sieve_command_def cmd_discard = { 
	"discard", 
	SCT_COMMAND, 
	0, 0, FALSE, FALSE,
	NULL, NULL, NULL, 
	cmd_discard_generate, 
	NULL 
};

/*
 * Discard operation
 */

static bool cmd_discard_operation_dump
	(const struct sieve_dumptime_env *denv, sieve_size_t *address);
static int cmd_discard_operation_execute
	(const struct sieve_runtime_env *renv, sieve_size_t *address);

const struct sieve_operation_def cmd_discard_operation = { 
	"DISCARD",
	NULL,
	SIEVE_OPERATION_DISCARD,
	cmd_discard_operation_dump, 
	cmd_discard_operation_execute 
};

/* 
 * Discard actions
 */

static void act_discard_print
	(const struct sieve_action *action, 
		const struct sieve_result_print_env *rpenv, bool *keep);	
static bool act_discard_commit
	(const struct sieve_action *action, 
		const struct sieve_action_exec_env *aenv, void *tr_context, bool *keep);
		
const struct sieve_action_def act_discard = {
	"discard",
	0,
	NULL, NULL, NULL,
	act_discard_print,
	NULL, NULL,
	act_discard_commit,
	NULL
};

/*
 * Code generation
 */
 
static bool cmd_discard_generate
(const struct sieve_codegen_env *cgenv, struct sieve_command *cmd ATTR_UNUSED) 
{
	sieve_operation_emit(cgenv->sblock, NULL, &cmd_discard_operation);

	return TRUE;
}

/* 
 * Code dump
 */

static bool cmd_discard_operation_dump
(const struct sieve_dumptime_env *denv, sieve_size_t *address)
{
	sieve_code_dumpf(denv, "DISCARD");
	sieve_code_descend(denv);

	return ( sieve_action_opr_optional_dump(denv, address, NULL) == 0 );
}

/*
 * Interpretation
 */

static int cmd_discard_operation_execute
(const struct sieve_runtime_env *renv ATTR_UNUSED,
	sieve_size_t *address ATTR_UNUSED)
{
	sieve_runtime_trace(renv, SIEVE_TRLVL_ACTIONS,
		"discard action; cancel implicit keep");

	if ( sieve_result_add_action
		(renv, NULL, &act_discard, NULL, NULL, 0) < 0 )
		return SIEVE_EXEC_FAILURE;

	return SIEVE_EXEC_OK;
}

/*
 * Action implementation
 */
 
static void act_discard_print
(const struct sieve_action *action ATTR_UNUSED, 
	const struct sieve_result_print_env *rpenv, bool *keep)	
{
	sieve_result_action_printf(rpenv, "discard");
	
	*keep = FALSE;
}

static bool act_discard_commit
(const struct sieve_action *action ATTR_UNUSED, 
	const struct sieve_action_exec_env *aenv, 
	void *tr_context ATTR_UNUSED, bool *keep)
{
	sieve_result_global_log(aenv, 
		"marked message to be discarded if not explicitly delivered "
		"(discard action)");
	*keep = FALSE;

	return TRUE;
}

