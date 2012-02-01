/* Copyright (c) 2002-2011 Pigeonhole authors, see the included COPYING file
 */
 
#include "lib.h"

#include "sieve-commands.h"
#include "sieve-stringlist.h"
#include "sieve-code.h"
#include "sieve-comparators.h"
#include "sieve-match-types.h"
#include "sieve-validator.h" 
#include "sieve-generator.h"
#include "sieve-interpreter.h"
#include "sieve-dump.h"
#include "sieve-match.h"

#include "ext-imap4flags-common.h"

/*
 * Hasflag test
 *
 * Syntax: 
 *   hasflag [MATCH-TYPE] [COMPARATOR] [<variable-list: string-list>]
 *       <list-of-flags: string-list>
 */

static bool tst_hasflag_registered
	(struct sieve_validator *valdtr, const struct sieve_extension *ext,
		struct sieve_command_registration *cmd_reg);
static bool tst_hasflag_validate
	(struct sieve_validator *valdtr,	struct sieve_command *tst);
static bool tst_hasflag_generate
	(const struct sieve_codegen_env *cgenv, struct sieve_command *cmd);
 
const struct sieve_command_def tst_hasflag = { 
	"hasflag", 
	SCT_TEST,
	-1, /* We check positional arguments ourselves */
	0, FALSE, FALSE, 
	tst_hasflag_registered, 
	NULL,
	tst_hasflag_validate,
	NULL, 
	tst_hasflag_generate, 
	NULL 
};

/* 
 * Hasflag operation 
 */

static bool tst_hasflag_operation_dump
	(const struct sieve_dumptime_env *denv, sieve_size_t *address);
static int tst_hasflag_operation_execute
	(const struct sieve_runtime_env *renv, sieve_size_t *address);

const struct sieve_operation_def hasflag_operation = { 
	"HASFLAG",
	&imap4flags_extension,
	ext_imap4flags_OPERATION_HASFLAG,
	tst_hasflag_operation_dump,
	tst_hasflag_operation_execute
};

/* 
 * Optional arguments 
 */

enum tst_hasflag_optional {	
	OPT_VARIABLES = SIEVE_MATCH_OPT_LAST,
};

/* 
 * Tag registration 
 */

static bool tst_hasflag_registered
(struct sieve_validator *valdtr, const struct sieve_extension *ext ATTR_UNUSED,
	struct sieve_command_registration *cmd_reg) 
{
	/* The order of these is not significant */
	sieve_comparators_link_tag(valdtr, cmd_reg, SIEVE_MATCH_OPT_COMPARATOR);
	sieve_match_types_link_tags(valdtr, cmd_reg, SIEVE_MATCH_OPT_MATCH_TYPE);

	return TRUE;
}

/* 
 * Validation 
 */

static bool tst_hasflag_validate
(struct sieve_validator *valdtr,	struct sieve_command *tst)
{
	struct sieve_ast_argument *vars = tst->first_positional;
	struct sieve_ast_argument *keys = sieve_ast_argument_next(vars);
	const struct sieve_match_type mcht_default = 
		SIEVE_MATCH_TYPE_DEFAULT(is_match_type);
	const struct sieve_comparator cmp_default = 
		SIEVE_COMPARATOR_DEFAULT(i_ascii_casemap_comparator);
		
	if ( !ext_imap4flags_command_validate(valdtr, tst) )
		return FALSE;
	
	if ( keys == NULL ) {
		keys = vars;
		vars = NULL;
	} else {
		vars->argument->id_code = OPT_VARIABLES;
	}
	
	/* Validate the key argument to a specified match type */
	return sieve_match_type_validate
		(valdtr, tst, keys, &mcht_default, &cmp_default);
}

/*
 * Code generation 
 */

static bool tst_hasflag_generate
(const struct sieve_codegen_env *cgenv, struct sieve_command *cmd)
{
	sieve_operation_emit(cgenv->sblock, cmd->ext, &hasflag_operation);

	/* Generate arguments */
	if ( !sieve_generate_arguments(cgenv, cmd, NULL) )
		return FALSE;	

	return TRUE;
}

/* 
 * Code dump 
 */
 
static bool tst_hasflag_operation_dump
(const struct sieve_dumptime_env *denv, sieve_size_t *address)
{
	int opt_code = 0;

	sieve_code_dumpf(denv, "HASFLAG");
	sieve_code_descend(denv);

	/* Optional operands */

	for (;;) {
		bool opok = TRUE;
		int opt;

		if ( (opt=sieve_match_opr_optional_dump(denv, address, &opt_code)) 
			< 0 )
			return FALSE;

		if ( opt == 0 ) break;

		switch ( opt_code ) {
		case OPT_VARIABLES:
			opok = sieve_opr_stringlist_dump(denv, address, "variables");
			break;
		default:
			return FALSE;
		}

		if ( !opok ) return FALSE;
	}
			
	return 
		sieve_opr_stringlist_dump(denv, address, "list of flags");
}

/*
 * Interpretation
 */

static int tst_hasflag_operation_execute
(const struct sieve_runtime_env *renv, sieve_size_t *address)
{
	int opt_code = 0;
	struct sieve_comparator cmp = 
		SIEVE_COMPARATOR_DEFAULT(i_ascii_casemap_comparator);
	struct sieve_match_type mcht = 
		SIEVE_MATCH_TYPE_DEFAULT(is_match_type);
	struct sieve_stringlist *flag_list, *variables_list, *value_list, *key_list;
	int match, ret;
	
	/*
	 * Read operands
	 */

	/* Optional operands */

	variables_list = NULL;
	for (;;) {
		int opt;

		if ( (opt=sieve_match_opr_optional_read
			(renv, address, &opt_code, &ret, &cmp, &mcht)) < 0 )
			return ret;

		if ( opt == 0 ) break;
	
		switch ( opt_code ) { 
		case OPT_VARIABLES:
			ret = sieve_opr_stringlist_read
				(renv, address, "variables-list", &variables_list);
			break;
		default:
			sieve_runtime_trace_error(renv, "invalid optional operand");
			ret = SIEVE_EXEC_BIN_CORRUPT;
		}

		if ( ret <= 0 ) return ret;
	} 

	/* Fixed operands */

	if ( (ret=sieve_opr_stringlist_read(renv, address, "flag-list", &flag_list))
		<= 0 )
		return ret;

	/*
	 * Perform operation
	 */

	sieve_runtime_trace(renv, SIEVE_TRLVL_TESTS, "hasflag test");

	value_list = ext_imap4flags_get_flags(renv, variables_list);

	if ( sieve_match_type_is(&mcht, is_match_type) ||
		sieve_match_type_is(&mcht, contains_match_type) )
		key_list = ext_imap4flags_get_flags(renv, flag_list);
	else
		key_list = flag_list;

	/* Perform match */
	if ( (match=sieve_match(renv, &mcht, &cmp, value_list, key_list, &ret)) < 0 )
		return ret;	

	/* Set test result for subsequent conditional jump */
	sieve_interpreter_set_test_result(renv->interp, match > 0);
	return SIEVE_EXEC_OK;
}


