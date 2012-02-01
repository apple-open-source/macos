/* Copyright (c) 2002-2011 Pigeonhole authors, see the included COPYING file
 */

#include "lib.h"
#include "ioloop.h"
#include "str-sanitize.h"
#include "istream.h"
#include "istream-header-filter.h"

#include "sieve-common.h"
#include "sieve-commands.h"
#include "sieve-code.h"
#include "sieve-actions.h"
#include "sieve-validator.h" 
#include "sieve-generator.h"
#include "sieve-interpreter.h"
#include "sieve-code-dumper.h"
#include "sieve-result.h"

#include "testsuite-common.h"
#include "testsuite-objects.h"

#include <stdio.h>

/* 
 * Test_set command 
 * 
 * Syntax
 *   test_set <testsuite object (member): string> <value: string>
 */

static bool cmd_test_set_validate
	(struct sieve_validator *valdtr, struct sieve_command *cmd);
static bool cmd_test_set_generate
	(const struct sieve_codegen_env *cgenv, struct sieve_command *cmd);

const struct sieve_command_def cmd_test_set = { 
	"test_set", 
	SCT_COMMAND,
	2, 0, FALSE, FALSE, 
	NULL, NULL,
	cmd_test_set_validate,
	NULL, 
	cmd_test_set_generate, 
	NULL 
};

/* 
 * Test_set operation 
 */

static bool cmd_test_set_operation_dump
	(const struct sieve_dumptime_env *denv, sieve_size_t *address);
static int cmd_test_set_operation_execute
	(const struct sieve_runtime_env *renv, sieve_size_t *address);

const struct sieve_operation_def test_set_operation = { 
	"TEST_SET",
	&testsuite_extension, 
	TESTSUITE_OPERATION_TEST_SET,
	cmd_test_set_operation_dump, 
	cmd_test_set_operation_execute 
};

/* 
 * Validation 
 */
 
static bool cmd_test_set_validate
(struct sieve_validator *valdtr, struct sieve_command *cmd) 
{
	struct sieve_ast_argument *arg = cmd->first_positional;

	/* Check arguments */
	
	if ( !sieve_validate_positional_argument
		(valdtr, cmd, arg, "object", 1, SAAT_STRING) ) {
		return FALSE;
	}
	
	if ( !testsuite_object_argument_activate(valdtr, arg, cmd) )
		return FALSE;
	
	arg = sieve_ast_argument_next(arg);
	
	if ( !sieve_validate_positional_argument
		(valdtr, cmd, arg, "value", 2, SAAT_STRING) ) {
		return FALSE;
	}
	
	return sieve_validator_argument_activate(valdtr, cmd, arg, FALSE);
}

/*
 * Code generation
 */
 
static bool cmd_test_set_generate
(const struct sieve_codegen_env *cgenv, struct sieve_command *cmd) 
{
	sieve_operation_emit(cgenv->sblock, cmd->ext, &test_set_operation);

	/* Generate arguments */
	return sieve_generate_arguments(cgenv, cmd, NULL);
}

/* 
 * Code dump
 */
 
static bool cmd_test_set_operation_dump
(const struct sieve_dumptime_env *denv, sieve_size_t *address)
{
	sieve_code_dumpf(denv, "TEST SET:");
	sieve_code_descend(denv);

	return 
		testsuite_object_dump(denv, address) &&
		sieve_opr_string_dump(denv, address, "value");
}

/*
 * Intepretation
 */

static int cmd_test_set_operation_execute
(const struct sieve_runtime_env *renv, sieve_size_t *address)
{
	struct testsuite_object tobj;
	string_t *value;
	int member_id;
	int ret;

	if ( !testsuite_object_read_member
		(renv->sblock, address, &tobj, &member_id) ) {
		sieve_runtime_trace_error(renv, "invalid testsuite object member");
		return SIEVE_EXEC_BIN_CORRUPT;
	}

	if ( (ret=sieve_opr_string_read(renv, address, "string", &value)) <= 0 )
		return ret;

	if ( sieve_runtime_trace_active(renv, SIEVE_TRLVL_COMMANDS) ) {
		sieve_runtime_trace(renv, 0, "testsuite: test_set command");
		sieve_runtime_trace_descend(renv);
		sieve_runtime_trace(renv, 0, 
			"set test parameter '%s' = \"%s\"", 
				testsuite_object_member_name(&tobj, member_id), str_c(value));
	}
	
	if ( tobj.def == NULL || tobj.def->set_member == NULL ) {
		sieve_runtime_trace_error(renv, "unimplemented testsuite object");
		return SIEVE_EXEC_FAILURE;
	}
		
	tobj.def->set_member(renv, member_id, value);	
	return SIEVE_EXEC_OK;
}



