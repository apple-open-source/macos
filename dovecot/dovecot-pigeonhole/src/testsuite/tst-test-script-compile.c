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
#include "testsuite-script.h"

/*
 * Test_script_compile command
 *
 * Syntax:   
 *   test_script_compile <scriptpath: string>
 */

static bool tst_test_script_compile_validate
	(struct sieve_validator *valdtr, struct sieve_command *cmd);
static bool tst_test_script_compile_generate
	(const struct sieve_codegen_env *cgenv, struct sieve_command *cmd);

const struct sieve_command_def tst_test_script_compile = { 
	"test_script_compile", 
	SCT_TEST, 
	1, 0, FALSE, FALSE,
	NULL, NULL,
	tst_test_script_compile_validate,
	NULL,
	tst_test_script_compile_generate, 
	NULL 
};

/* 
 * Operation 
 */

static bool tst_test_script_compile_operation_dump
	(const struct sieve_dumptime_env *denv, sieve_size_t *address);
static int tst_test_script_compile_operation_execute
	(const struct sieve_runtime_env *renv, sieve_size_t *address);

const struct sieve_operation_def test_script_compile_operation = { 
	"TEST_SCRIPT_COMPILE",
	&testsuite_extension, 
	TESTSUITE_OPERATION_TEST_SCRIPT_COMPILE,
	tst_test_script_compile_operation_dump, 
	tst_test_script_compile_operation_execute 
};

/* 
 * Validation 
 */

static bool tst_test_script_compile_validate
(struct sieve_validator *valdtr ATTR_UNUSED, struct sieve_command *tst) 
{
	struct sieve_ast_argument *arg = tst->first_positional;
	
	if ( !sieve_validate_positional_argument
		(valdtr, tst, arg, "script", 1, SAAT_STRING) ) {
		return FALSE;
	}
	
	return sieve_validator_argument_activate(valdtr, tst, arg, FALSE);
}

/* 
 * Code generation 
 */

static bool tst_test_script_compile_generate
(const struct sieve_codegen_env *cgenv, struct sieve_command *tst)
{
	sieve_operation_emit(cgenv->sblock, tst->ext, &test_script_compile_operation);

	/* Generate arguments */
	return sieve_generate_arguments(cgenv, tst, NULL);
}

/* 
 * Code dump
 */
 
static bool tst_test_script_compile_operation_dump
(const struct sieve_dumptime_env *denv, sieve_size_t *address)
{
	sieve_code_dumpf(denv, "TEST_SCRIPT_COMPILE:");
	sieve_code_descend(denv);

	if ( !sieve_opr_string_dump(denv, address, "script-name") ) 
		return FALSE;

	return TRUE;
}

/*
 * Intepretation
 */

static int tst_test_script_compile_operation_execute
(const struct sieve_runtime_env *renv, sieve_size_t *address)
{
	string_t *script_name;
	bool result = TRUE;
	int ret;

	/*
	 * Read operands
	 */

	if ( (ret=sieve_opr_string_read(renv, address, "script-name", &script_name))
		<= 0 )
		return ret;

	/*
	 * Perform operation
	 */

	if ( sieve_runtime_trace_active(renv, SIEVE_TRLVL_TESTS) ) {
		sieve_runtime_trace(renv, 0, "testsuite: test_script_compile test");
		sieve_runtime_trace_descend(renv);
	}

	/* Attempt script compile */

	result = testsuite_script_compile(renv, str_c(script_name));

	/* Set result */
	sieve_interpreter_set_test_result(renv->interp, result);

	return SIEVE_EXEC_OK;
}




