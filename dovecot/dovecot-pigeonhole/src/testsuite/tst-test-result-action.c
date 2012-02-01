/* Copyright (c) 2002-2011 Pigeonhole authors, see the included COPYING file
 */

#include "sieve-common.h"
#include "sieve-error.h"
#include "sieve-script.h"
#include "sieve-commands.h"
#include "sieve-actions.h"
#include "sieve-comparators.h"
#include "sieve-match-types.h"
#include "sieve-validator.h"
#include "sieve-generator.h"
#include "sieve-interpreter.h"
#include "sieve-code.h"
#include "sieve-binary.h"
#include "sieve-result.h"
#include "sieve-dump.h"
#include "sieve-match.h"

#include "testsuite-common.h"
#include "testsuite-result.h"

/*
 * test_result_action command
 *
 * Syntax:   
 *   test_result_action [MATCH-TYPE] [COMPARATOR] [:index number] 
 *     <key-list: string-list>
 */

static bool tst_test_result_action_registered
	(struct sieve_validator *validator, const struct sieve_extension *ext,
		struct sieve_command_registration *cmd_reg);
static bool tst_test_result_action_validate
	(struct sieve_validator *validator, struct sieve_command *cmd);
static bool tst_test_result_action_generate
	(const struct sieve_codegen_env *cgenv, struct sieve_command *ctx);

const struct sieve_command_def tst_test_result_action = { 
	"test_result_action", 
	SCT_TEST, 
	1, 0, FALSE, FALSE,
	tst_test_result_action_registered, 
	NULL,
	tst_test_result_action_validate,
	NULL,
	tst_test_result_action_generate, 
	NULL 
};

/* 
 * Operation 
 */

static bool tst_test_result_action_operation_dump
	(const struct sieve_dumptime_env *denv, sieve_size_t *address);
static int tst_test_result_action_operation_execute
	(const struct sieve_runtime_env *renv, sieve_size_t *address);

const struct sieve_operation_def test_result_action_operation = { 
	"TEST_RESULT_ACTION",
	&testsuite_extension, 
	TESTSUITE_OPERATION_TEST_RESULT_ACTION,
	tst_test_result_action_operation_dump, 
	tst_test_result_action_operation_execute 
};

/*
 * Tagged arguments
 */ 

/* FIXME: merge this with the test_error version of this tag */

static bool tst_test_result_action_validate_index_tag
	(struct sieve_validator *validator, struct sieve_ast_argument **arg,
		struct sieve_command *cmd);

static const struct sieve_argument_def test_result_action_index_tag = {
    "index",
    NULL,
    tst_test_result_action_validate_index_tag,
    NULL, NULL, NULL
};

enum tst_test_result_action_optional {
	OPT_INDEX = SIEVE_MATCH_OPT_LAST,
};

/*
 * Argument implementation
 */

static bool tst_test_result_action_validate_index_tag
(struct sieve_validator *validator, struct sieve_ast_argument **arg,
	struct sieve_command *cmd)
{
	struct sieve_ast_argument *tag = *arg;

	/* Detach the tag itself */
	*arg = sieve_ast_arguments_detach(*arg,1);

	/* Check syntax:
	 *   :index number
	 */
	if ( !sieve_validate_tag_parameter
		(validator, cmd, tag, *arg, NULL, 0, SAAT_NUMBER, FALSE) ) {
		return FALSE;
	}

	/* Skip parameter */
	*arg = sieve_ast_argument_next(*arg);
	return TRUE;
}


/*
 * Command registration
 */

static bool tst_test_result_action_registered
(struct sieve_validator *validator, const struct sieve_extension *ext,
	struct sieve_command_registration *cmd_reg)
{
	/* The order of these is not significant */
	sieve_comparators_link_tag(validator, cmd_reg, SIEVE_MATCH_OPT_COMPARATOR);
	sieve_match_types_link_tags(validator, cmd_reg, SIEVE_MATCH_OPT_MATCH_TYPE);

	sieve_validator_register_tag
		(validator, cmd_reg, ext, &test_result_action_index_tag, OPT_INDEX);

	return TRUE;
}

/* 
 * Validation 
 */

static bool tst_test_result_action_validate
(struct sieve_validator *valdtr ATTR_UNUSED, struct sieve_command *tst) 
{
	struct sieve_ast_argument *arg = tst->first_positional;
	struct sieve_comparator cmp_default = 
		SIEVE_COMPARATOR_DEFAULT(i_octet_comparator);
	struct sieve_match_type mcht_default = 
		SIEVE_COMPARATOR_DEFAULT(is_match_type);
	
	if ( !sieve_validate_positional_argument
		(valdtr, tst, arg, "key list", 2, SAAT_STRING_LIST) ) {
		return FALSE;
	}

	if ( !sieve_validator_argument_activate(valdtr, tst, arg, FALSE) )
		return FALSE;

	/* Validate the key argument to a specified match type */
	return sieve_match_type_validate
		(valdtr, tst, arg, &mcht_default, &cmp_default);
}

/* 
 * Code generation 
 */

static bool tst_test_result_action_generate
(const struct sieve_codegen_env *cgenv, struct sieve_command *tst)
{
	sieve_operation_emit(cgenv->sblock, tst->ext, &test_result_action_operation);

	/* Generate arguments */
	return sieve_generate_arguments(cgenv, tst, NULL);
}

/* 
 * Code dump
 */
 
static bool tst_test_result_action_operation_dump
(const struct sieve_dumptime_env *denv, sieve_size_t *address)
{
	int opt_code = 0;

	sieve_code_dumpf(denv, "TEST_RESULT_ACTION:");
	sieve_code_descend(denv);

	/* Handle any optional arguments */
	for (;;) {
		int opt;

		if ( (opt=sieve_match_opr_optional_dump(denv, address, &opt_code)) 
			< 0 )
			return FALSE;

		if ( opt == 0 ) break;

		if ( opt_code == OPT_INDEX ) {
			if ( !sieve_opr_number_dump(denv, address, "index") )
				return FALSE;
		} else {
			return FALSE;
		}
	} 

	return sieve_opr_stringlist_dump(denv, address, "key list");
}

/*
 * Intepretation
 */

static int tst_test_result_action_operation_execute
(const struct sieve_runtime_env *renv, sieve_size_t *address)
{	
	int opt_code = 0;
	struct sieve_comparator cmp = SIEVE_COMPARATOR_DEFAULT(i_octet_comparator);
	struct sieve_match_type mcht = SIEVE_MATCH_TYPE_DEFAULT(is_match_type);
	struct sieve_stringlist *value_list, *key_list;
	int index = 0;
	int match, ret;

	/*
	 * Read operands
	 */

	/* Read optional operands */
	for (;;) {
		sieve_number_t number; 
		int opt;

		if ( (opt=sieve_match_opr_optional_read
			(renv, address, &opt_code, &ret, &cmp, &mcht)) < 0 )
			return ret;

		if ( opt == 0 ) break;
	
		if ( opt_code == OPT_INDEX ) {
			if ( (ret=sieve_opr_number_read(renv, address, "index", &number)) <= 0 )
				return ret;
			index = (int) number;
		} else {
			sieve_runtime_trace_error(renv, "invalid optional operand");
			return SIEVE_EXEC_BIN_CORRUPT;
		}	
	}

	/* Read key-list */
	if ( (ret=sieve_opr_stringlist_read(renv, address, "key-list", &key_list))
		<= 0 )
		return ret;

	/*
	 * Perform operation
	 */
	
	sieve_runtime_trace(renv, SIEVE_TRLVL_TESTS,
		"testsuite: test_result_action test; match result name (index: %d)", index);

	/* Create value stringlist */
	value_list = testsuite_result_stringlist_create(renv, index);

	/* Perform match */
	if ( (match=sieve_match(renv, &mcht, &cmp, value_list, key_list, &ret)) < 0 )
		return ret;	
	
	/* Set test result for subsequent conditional jump */
	sieve_interpreter_set_test_result(renv->interp, match > 0);
	return SIEVE_EXEC_OK;
}



