/* Copyright (c) 2002-2011 Pigeonhole authors, see the included COPYING file
 */

#include "lib.h"

#include "sieve-common.h"
#include "sieve-extensions.h"
#include "sieve-commands.h"
#include "sieve-stringlist.h"
#include "sieve-code.h"
#include "sieve-comparators.h"
#include "sieve-match-types.h"
#include "sieve-address-parts.h"
#include "sieve-validator.h"
#include "sieve-generator.h"
#include "sieve-interpreter.h"
#include "sieve-dump.h"
#include "sieve-match.h"

#include "ext-spamvirustest-common.h"

/*
 * Tests
 */

static bool tst_spamvirustest_validate
	(struct sieve_validator *valdtr, struct sieve_command *tst);
static bool tst_spamvirustest_generate
	(const struct sieve_codegen_env *cgenv, struct sieve_command *ctx);
static bool tst_spamvirustest_registered
	(struct sieve_validator *valdtr, const struct sieve_extension *ext,
		struct sieve_command_registration *cmd_reg);
 
/* Spamtest test
 *
 * Syntax:
 *   spamtest [":percent"] [COMPARATOR] [MATCH-TYPE] <value: string> 
 */

const struct sieve_command_def spamtest_test = { 
	"spamtest", 
	SCT_TEST, 
	1, 0, FALSE, FALSE,
	tst_spamvirustest_registered,
	NULL, 
	tst_spamvirustest_validate,
	NULL,
	tst_spamvirustest_generate, 
	NULL 
};

/* Virustest test
 *
 * Syntax:
 *   virustest [COMPARATOR] [MATCH-TYPE] <value: string> 
 */

const struct sieve_command_def virustest_test = { 
	"virustest", 
	SCT_TEST, 
	1, 0, FALSE, FALSE,
	tst_spamvirustest_registered,
	NULL, 
	tst_spamvirustest_validate,
	NULL, 
	tst_spamvirustest_generate, 
	NULL 
};

/* 
 * Tagged arguments 
 */

static bool tst_spamtest_validate_percent_tag
	(struct sieve_validator *valdtr, struct sieve_ast_argument **arg, 
		struct sieve_command *tst);

static const struct sieve_argument_def spamtest_percent_tag = {
 	"percent",
	NULL, 
	tst_spamtest_validate_percent_tag,
	NULL, NULL, NULL
};

/* 
 * Spamtest and virustest operations 
 */

static bool tst_spamvirustest_operation_dump
	(const struct sieve_dumptime_env *denv, sieve_size_t *address);
static int tst_spamvirustest_operation_execute
	(const struct sieve_runtime_env *renv, sieve_size_t *address);

const struct sieve_operation_def spamtest_operation = { 
	"SPAMTEST",
	&spamtest_extension,
	0,
	tst_spamvirustest_operation_dump, 
	tst_spamvirustest_operation_execute 
};

const struct sieve_operation_def virustest_operation = { 
	"VIRUSTEST",
	&virustest_extension,
	0,
	tst_spamvirustest_operation_dump, 
	tst_spamvirustest_operation_execute 
};


/*
 * Optional operands
 */

enum tst_spamvirustest_optional {
	OPT_SPAMTEST_PERCENT = SIEVE_MATCH_OPT_LAST,
	OPT_SPAMTEST_LAST
};

/* 
 * Test registration 
 */

static bool tst_spamvirustest_registered
(struct sieve_validator *valdtr, const struct sieve_extension *ext,
	struct sieve_command_registration *cmd_reg) 
{
	sieve_comparators_link_tag(valdtr, cmd_reg, SIEVE_MATCH_OPT_COMPARATOR);
	sieve_match_types_link_tags(valdtr, cmd_reg, SIEVE_MATCH_OPT_MATCH_TYPE);

	if ( sieve_extension_is(ext, spamtestplus_extension) || 
		sieve_extension_is(ext, spamtest_extension) ) {
		sieve_validator_register_tag
			(valdtr, cmd_reg, ext, &spamtest_percent_tag, OPT_SPAMTEST_PERCENT);
	}

	return TRUE;
}

/* 
 * Validation 
 */

static bool tst_spamtest_validate_percent_tag
(struct sieve_validator *valdtr, struct sieve_ast_argument **arg, 
	struct sieve_command *tst)
{
	if ( !sieve_extension_is(tst->ext, spamtestplus_extension) ) {	
		sieve_argument_validate_error(valdtr, *arg,
			"the spamtest test only accepts the :percent argument when "
			"the spamtestplus extension is active"); 
		return FALSE; 
	}

	/* Skip tag */
	*arg = sieve_ast_argument_next(*arg);

	return TRUE;
}

static bool tst_spamvirustest_validate
(struct sieve_validator *valdtr, struct sieve_command *tst) 
{
	struct sieve_ast_argument *arg = tst->first_positional;
	const struct sieve_match_type mcht_default = 
		SIEVE_MATCH_TYPE_DEFAULT(is_match_type);
	const struct sieve_comparator cmp_default = 
		SIEVE_COMPARATOR_DEFAULT(i_ascii_casemap_comparator);
		
	/* Check value */
		
	if ( !sieve_validate_positional_argument
		(valdtr, tst, arg, "value", 1, SAAT_STRING) ) {
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

static bool tst_spamvirustest_generate
(const struct sieve_codegen_env *cgenv, struct sieve_command *tst) 
{ 
	if ( sieve_command_is(tst, spamtest_test) )
		sieve_operation_emit(cgenv->sblock, tst->ext, &spamtest_operation);
	else if ( sieve_command_is(tst, virustest_test) )
		sieve_operation_emit(cgenv->sblock, tst->ext, &virustest_operation);
	else
		i_unreached();

	/* Generate arguments */  	
	return sieve_generate_arguments(cgenv, tst, NULL);
}

/* 
 * Code dump 
 */

static bool tst_spamvirustest_operation_dump
(const struct sieve_dumptime_env *denv, sieve_size_t *address)
{
	int opt_code = 0;
	const struct sieve_operation *op = denv->oprtn;

	sieve_code_dumpf(denv, "%s", sieve_operation_mnemonic(op));
	sieve_code_descend(denv);
	
	/* Optional operands */
	for (;;) {
		int opt;

		if ( (opt=sieve_match_opr_optional_dump(denv, address, &opt_code)) < 0 )
			return FALSE;

		if ( opt == 0 ) break;

		switch ( opt_code ) {
		case OPT_SPAMTEST_PERCENT:
			sieve_code_dumpf(denv, "percent");
			break;
    default:
			return FALSE;
		}
	}

	return
		sieve_opr_string_dump(denv, address, "value");
}

/* 
 * Code execution 
 */

static int tst_spamvirustest_operation_execute
(const struct sieve_runtime_env *renv, sieve_size_t *address)
{	
	const struct sieve_operation *op = renv->oprtn;
	const struct sieve_extension *this_ext = op->ext;
	int opt_code = 0;
	struct sieve_match_type mcht = 
		SIEVE_MATCH_TYPE_DEFAULT(is_match_type);
	struct sieve_comparator cmp = 
		SIEVE_COMPARATOR_DEFAULT(i_ascii_casemap_comparator);
	bool percent = FALSE;
	struct sieve_stringlist *value_list, *key_list;
	const char *score_value;
	int match, ret;
	
	/* Read optional operands */
	for (;;) {
		int opt;

		if ( (opt=sieve_match_opr_optional_read
			(renv, address, &opt_code, &ret, &cmp, &mcht)) < 0 )
			return ret;

		if ( opt == 0 ) break;
	
		switch ( opt_code ) {
		case OPT_SPAMTEST_PERCENT:
			percent = TRUE;
			break;
		default:
			sieve_runtime_trace_error(renv, "unknown optional operand");
			return SIEVE_EXEC_BIN_CORRUPT;
		}
	}

	/* Read value part */
	if ( (ret=sieve_opr_stringlist_read(renv, address, "value", &key_list)) <= 0 )
		return ret;
			
	/* Perform test */

	if ( sieve_operation_is(op, spamtest_operation) ) {
		sieve_runtime_trace
			(renv, SIEVE_TRLVL_TESTS, "spamtest test [percent=%s]",
				( percent ? "true" : "false" ));
	} else {
		sieve_runtime_trace
			(renv, SIEVE_TRLVL_TESTS, "virustest test");
	}

	/* Get score value */
	score_value = ext_spamvirustest_get_value(renv, this_ext, percent);

	/* Construct value list */
	value_list = sieve_single_stringlist_create_cstr(renv, score_value, TRUE);

	/* Perform match */
	if ( (match=sieve_match(renv, &mcht, &cmp, value_list, key_list, &ret)) < 0 )
		return ret; 	

	/* Set test result for subsequent conditional jump */
	sieve_interpreter_set_test_result(renv->interp, match > 0);
	return SIEVE_EXEC_OK;
}
