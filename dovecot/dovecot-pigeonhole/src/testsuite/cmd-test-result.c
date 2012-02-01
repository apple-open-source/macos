/* Copyright (c) 2002-2011 Pigeonhole authors, see the included COPYING file
 */

#include "sieve-common.h"
#include "sieve-script.h"
#include "sieve-commands.h"
#include "sieve-validator.h"
#include "sieve-generator.h"
#include "sieve-interpreter.h"
#include "sieve-code.h"
#include "sieve-binary.h"
#include "sieve-dump.h"
#include "sieve.h"

#include "testsuite-common.h"
#include "testsuite-result.h"
#include "testsuite-smtp.h"

/*
 * Commands
 */

static bool cmd_test_result_generate
	(const struct sieve_codegen_env *cgenv, struct sieve_command *cmd);

/* Test_result_reset command
 *
 * Syntax:   
 *   test_result_reset
 */

const struct sieve_command_def cmd_test_result_reset = { 
	"test_result_reset", 
	SCT_COMMAND, 
	0, 0, FALSE, FALSE,
	NULL, NULL, NULL, NULL,
	cmd_test_result_generate, 
	NULL 
};

/* Test_result_print command
 *
 * Syntax:   
 *   test_result_print
 */

const struct sieve_command_def cmd_test_result_print = { 
	"test_result_print", 
	SCT_COMMAND, 
	0, 0, FALSE, FALSE,
	NULL, NULL, NULL, NULL,
	cmd_test_result_generate, 
	NULL 
};

/* 
 * Operations 
 */

/* test_result_reset */

static int cmd_test_result_reset_operation_execute
	(const struct sieve_runtime_env *renv, sieve_size_t *address);

const struct sieve_operation_def test_result_reset_operation = { 
	"TEST_RESULT_RESET",
	&testsuite_extension, 
	TESTSUITE_OPERATION_TEST_RESULT_RESET,
	NULL, 
	cmd_test_result_reset_operation_execute 
};

/* test_result_print */

static int cmd_test_result_print_operation_execute
	(const struct sieve_runtime_env *renv, sieve_size_t *address);

const struct sieve_operation_def test_result_print_operation = { 
	"TEST_RESULT_PRINT",
	&testsuite_extension, 
	TESTSUITE_OPERATION_TEST_RESULT_PRINT,
	NULL, 
	cmd_test_result_print_operation_execute 
};

/* 
 * Code generation 
 */

static bool cmd_test_result_generate
(const struct sieve_codegen_env *cgenv, struct sieve_command *cmd)
{
	if ( sieve_command_is(cmd, cmd_test_result_reset) )
		sieve_operation_emit(cgenv->sblock, cmd->ext, &test_result_reset_operation);
	else if ( sieve_command_is(cmd, cmd_test_result_print) )
		sieve_operation_emit(cgenv->sblock, cmd->ext, &test_result_print_operation);
	else
		i_unreached();

	return TRUE;
}

/*
 * Intepretation
 */

static int cmd_test_result_reset_operation_execute
(const struct sieve_runtime_env *renv, sieve_size_t *address ATTR_UNUSED)
{
	sieve_runtime_trace(renv, SIEVE_TRLVL_COMMANDS, 
			"testsuite: test_result_reset command; reset script result");

	testsuite_result_reset(renv);
	testsuite_smtp_reset();

	return SIEVE_EXEC_OK;
}

static int cmd_test_result_print_operation_execute
(const struct sieve_runtime_env *renv, sieve_size_t *address ATTR_UNUSED)
{
	sieve_runtime_trace(renv, SIEVE_TRLVL_COMMANDS, 
			"testsuite: test_result_print command; print script result ");

	testsuite_result_print(renv);

	return SIEVE_EXEC_OK;
}




