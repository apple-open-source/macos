/* Copyright (c) 2002-2011 Pigeonhole authors, see the included COPYING file
 */

#include "sieve-common.h"
#include "sieve-commands.h"
#include "sieve-validator.h"
#include "sieve-generator.h"
#include "sieve-interpreter.h"
#include "sieve-code.h"
#include "sieve-binary.h"
#include "sieve-dump.h"

#include "testsuite-common.h"
#include "testsuite-mailstore.h"

/*
 * Commands
 */

static bool cmd_test_mailbox_validate
	(struct sieve_validator *valdtr, struct sieve_command *cmd);
static bool cmd_test_mailbox_generate
	(const struct sieve_codegen_env *cgenv, struct sieve_command *ctx);

/* Test_mailbox_create command
 *
 * Syntax:   
 *   test_mailbox_create <mailbox: string>
 */

const struct sieve_command_def cmd_test_mailbox_create = { 
	"test_mailbox_create", 
	SCT_COMMAND, 
	1, 0, FALSE, FALSE,
	NULL, NULL,
	cmd_test_mailbox_validate, 
	cmd_test_mailbox_generate, 
	NULL 
};

/* Test_mailbox_delete command
 *
 * Syntax:   
 *   test_mailbox_create <mailbox: string>
 */

const struct sieve_command_def cmd_test_mailbox_delete = { 
	"test_mailbox_delete", 
	SCT_COMMAND, 
	1, 0, FALSE, FALSE,
	NULL, NULL,
	cmd_test_mailbox_validate, 
	cmd_test_mailbox_generate, 
	NULL 
};

/* 
 * Operations
 */ 

static bool cmd_test_mailbox_operation_dump
	(const struct sieve_dumptime_env *denv, sieve_size_t *address);
static int cmd_test_mailbox_operation_execute
	(const struct sieve_runtime_env *renv, sieve_size_t *address);
 
/* Test_mailbox_create operation */

const struct sieve_operation_def test_mailbox_create_operation = { 
	"TEST_MAILBOX_CREATE",
	&testsuite_extension, 
	TESTSUITE_OPERATION_TEST_MAILBOX_CREATE,
	cmd_test_mailbox_operation_dump, 
	cmd_test_mailbox_operation_execute 
};

/* Test_mailbox_delete operation */

const struct sieve_operation_def test_mailbox_delete_operation = { 
	"TEST_MAILBOX_DELETE",
	&testsuite_extension, 
	TESTSUITE_OPERATION_TEST_MAILBOX_DELETE,
	cmd_test_mailbox_operation_dump, 
	cmd_test_mailbox_operation_execute 
};

/* 
 * Validation 
 */

static bool cmd_test_mailbox_validate
(struct sieve_validator *valdtr, struct sieve_command *cmd) 
{
	struct sieve_ast_argument *arg = cmd->first_positional;
			
	if ( !sieve_validate_positional_argument
		(valdtr, cmd, arg, "mailbox", 1, SAAT_STRING) ) {
		return FALSE;
	}
	
	return sieve_validator_argument_activate(valdtr, cmd, arg, FALSE);
}

/* 
 * Code generation 
 */

static bool cmd_test_mailbox_generate
(const struct sieve_codegen_env *cgenv, struct sieve_command *cmd)
{
	/* Emit operation */
	if ( sieve_command_is(cmd, cmd_test_mailbox_create) )	
		sieve_operation_emit
			(cgenv->sblock, cmd->ext, &test_mailbox_create_operation);
	else if ( sieve_command_is(cmd, cmd_test_mailbox_delete) )	
		sieve_operation_emit
			(cgenv->sblock, cmd->ext, &test_mailbox_delete_operation);
	else
		i_unreached();
	  	
 	/* Generate arguments */
	if ( !sieve_generate_arguments(cgenv, cmd, NULL) )
		return FALSE;

	return TRUE;
}

/* 
 * Code dump
 */
 
static bool cmd_test_mailbox_operation_dump
(const struct sieve_dumptime_env *denv, sieve_size_t *address)
{
	sieve_code_dumpf(denv, "%s:", sieve_operation_mnemonic(denv->oprtn));
	
	sieve_code_descend(denv);
	
	return sieve_opr_string_dump(denv, address, "mailbox");
}


/*
 * Intepretation
 */
 
static int cmd_test_mailbox_operation_execute
(const struct sieve_runtime_env *renv, sieve_size_t *address)
{
	const struct sieve_operation *oprtn = renv->oprtn;
	string_t *mailbox = NULL;
	int ret;

	/* 
	 * Read operands 
	 */

	/* Index */

	if ( (ret=sieve_opr_string_read(renv, address, "mailbox", &mailbox)) <= 0 )
		return ret;

	/*
	 * Perform operation
	 */
		
	if ( sieve_operation_is(oprtn, test_mailbox_create_operation) ) {
		if ( sieve_runtime_trace_active(renv, SIEVE_TRLVL_COMMANDS) ) {
			sieve_runtime_trace(renv, 0, "testsuite/test_mailbox_create command");
			sieve_runtime_trace_descend(renv);
			sieve_runtime_trace(renv, 0, "create mailbox `%s'", str_c(mailbox));
		}

		testsuite_mailstore_mailbox_create(renv, str_c(mailbox));
	} else {
		if ( sieve_runtime_trace_active(renv, SIEVE_TRLVL_COMMANDS) ) {
			sieve_runtime_trace(renv, 0, "testsuite/test_mailbox_delete command");
			sieve_runtime_trace_descend(renv);
			sieve_runtime_trace(renv, 0, "delete mailbox `%s'", str_c(mailbox));
		}
		
		/* FIXME: implement */
		testsuite_test_failf("test_mailbox_delete: NOT IMPLEMENTED");
	}

	return SIEVE_EXEC_OK;
}
