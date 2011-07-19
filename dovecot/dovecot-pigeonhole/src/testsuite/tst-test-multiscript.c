/* Copyright (c) 2002-2011 Pigeonhole authors, see the included COPYING file
 */

#include "sieve-common.h"
#include "sieve-script.h"
#include "sieve-commands.h"
#include "sieve-validator.h"
#include "sieve-generator.h"
#include "sieve-interpreter.h"
#include "sieve-stringlist.h"
#include "sieve-code.h"
#include "sieve-binary.h"
#include "sieve-dump.h"
#include "sieve.h"

#include "testsuite-common.h"
#include "testsuite-script.h"

/*
 * Test_multiscript command
 *
 * Syntax:   
 *   test_multiscript <scripts: string-list>
 */

static bool tst_test_multiscript_validate
	(struct sieve_validator *validator, struct sieve_command *cmd);
static bool tst_test_multiscript_generate
	(const struct sieve_codegen_env *cgenv, struct sieve_command *tst);

const struct sieve_command_def tst_test_multiscript = { 
	"test_multiscript", 
	SCT_TEST, 
	1, 0, FALSE, FALSE,
	NULL, NULL,
	tst_test_multiscript_validate, 
	tst_test_multiscript_generate, 
	NULL 
};

/* 
 * Operation 
 */

static bool tst_test_multiscript_operation_dump
	(const struct sieve_dumptime_env *denv, sieve_size_t *address);
static int tst_test_multiscript_operation_execute
	(const struct sieve_runtime_env *renv, sieve_size_t *address);

const struct sieve_operation_def test_multiscript_operation = { 
	"TEST_MULTISCRIPT",
	&testsuite_extension, 
	TESTSUITE_OPERATION_TEST_MULTISCRIPT,
	tst_test_multiscript_operation_dump, 
	tst_test_multiscript_operation_execute 
};

/* 
 * Validation 
 */

static bool tst_test_multiscript_validate
(struct sieve_validator *valdtr, struct sieve_command *tst) 
{
	struct sieve_ast_argument *arg = tst->first_positional;
	
	if ( !sieve_validate_positional_argument
		(valdtr, tst, arg, "scripts", 1, SAAT_STRING_LIST) ) {
		return FALSE;
	}
	
	return sieve_validator_argument_activate(valdtr, tst, arg, FALSE);
}

/* 
 * Code generation 
 */

static bool tst_test_multiscript_generate
(const struct sieve_codegen_env *cgenv, struct sieve_command *tst)
{
	sieve_operation_emit(cgenv->sblock, tst->ext, &test_multiscript_operation);

	/* Generate arguments */
	return sieve_generate_arguments(cgenv, tst, NULL);
}

/* 
 * Code dump
 */
 
static bool tst_test_multiscript_operation_dump
(const struct sieve_dumptime_env *denv, sieve_size_t *address)
{
	sieve_code_dumpf(denv, "TEST_MULTISCRIPT:");
	sieve_code_descend(denv);

	if ( !sieve_opr_stringlist_dump(denv, address, "scripts") ) 
		return FALSE;

	return TRUE;
}

/*
 * Intepretation
 */

static int tst_test_multiscript_operation_execute
(const struct sieve_runtime_env *renv, sieve_size_t *address)
{
	struct sieve_stringlist *scripts_list;
	string_t *script_name;
	ARRAY_TYPE (const_string) scriptfiles;
	bool result = TRUE;
	int ret;

	/*
	 * Read operands
	 */

	if ( (ret=sieve_opr_stringlist_read(renv, address, "scripts", &scripts_list))
		<= 0 )
		return ret;

	/*
	 * Perform operation
	 */

	sieve_runtime_trace(renv, SIEVE_TRLVL_TESTS, 
		"testsuite: test_multiscript test");
	sieve_runtime_trace_descend(renv);

	t_array_init(&scriptfiles, 16);

	script_name = NULL;
	while ( result &&
		(ret=sieve_stringlist_next_item(scripts_list, &script_name)) > 0 ) {
		const char *script = t_strdup(str_c(script_name));

		array_append(&scriptfiles, &script, 1);
	}

	result = result && (ret >= 0) && 
		testsuite_script_multiscript(renv, &scriptfiles);

	/* Set result */
	sieve_interpreter_set_test_result(renv->interp, result);

	return SIEVE_EXEC_OK;
}




