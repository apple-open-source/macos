/* Copyright (c) 2002-2011 Pigeonhole authors, see the included COPYING file
 */

#include "sieve-common.h"
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

#include "ext-variables-common.h"

/* 
 * String test 
 *
 * Syntax:
 *   string [COMPARATOR] [MATCH-TYPE]
 *     <source: string-list> <key-list: string-list>
 */

static bool tst_string_registered
	(struct sieve_validator *valdtr, const struct sieve_extension *ext, 
		struct sieve_command_registration *cmd_reg);
static bool tst_string_validate
	(struct sieve_validator *valdtr, struct sieve_command *tst);
static bool tst_string_generate
	(const struct sieve_codegen_env *cgenv, struct sieve_command *ctx);

const struct sieve_command_def tst_string = { 
	"string", 
	SCT_TEST, 
	2, 0, FALSE, FALSE,
	tst_string_registered, 
	NULL,
	tst_string_validate,
	NULL, 
	tst_string_generate, 
	NULL 
};

/* 
 * String operation
 */

static bool tst_string_operation_dump
	(const struct sieve_dumptime_env *denv, sieve_size_t *address);
static int tst_string_operation_execute
	(const struct sieve_runtime_env *renv, sieve_size_t *address);

const struct sieve_operation_def tst_string_operation = { 
	"STRING",
	&variables_extension, 
	EXT_VARIABLES_OPERATION_STRING, 
	tst_string_operation_dump, 
	tst_string_operation_execute 
};

/* 
 * Optional arguments 
 */

enum tst_string_optional {	
	OPT_END,
	OPT_COMPARATOR,
	OPT_MATCH_TYPE
};

/* 
 * Test registration 
 */

static bool tst_string_registered
(struct sieve_validator *valdtr, const struct sieve_extension *ext ATTR_UNUSED,
	struct sieve_command_registration *cmd_reg) 
{
	/* The order of these is not significant */
	sieve_comparators_link_tag(valdtr, cmd_reg, OPT_COMPARATOR);
	sieve_match_types_link_tags(valdtr, cmd_reg, OPT_MATCH_TYPE);

	return TRUE;
}

/* 
 * Test validation 
 */

static bool tst_string_validate
(struct sieve_validator *valdtr, struct sieve_command *tst) 
{ 		
	struct sieve_ast_argument *arg = tst->first_positional;
	const struct sieve_match_type mcht_default = 
		SIEVE_MATCH_TYPE_DEFAULT(is_match_type);
	const struct sieve_comparator cmp_default = 
		SIEVE_COMPARATOR_DEFAULT(i_octet_comparator);
	
	if ( !sieve_validate_positional_argument
		(valdtr, tst, arg, "source", 1, SAAT_STRING_LIST) ) {
		return FALSE;
	}
	
	if ( !sieve_validator_argument_activate(valdtr, tst, arg, FALSE) )
		return FALSE;
	
	arg = sieve_ast_argument_next(arg);

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
 * Test generation 
 */

static bool tst_string_generate
(const struct sieve_codegen_env *cgenv, struct sieve_command *cmd) 
{
	sieve_operation_emit(cgenv->sblock, cmd->ext, &tst_string_operation);

 	/* Generate arguments */
	if ( !sieve_generate_arguments(cgenv, cmd, NULL) )
		return FALSE;
	
	return TRUE;
}

/* 
 * Code dump 
 */

static bool tst_string_operation_dump
(const struct sieve_dumptime_env *denv, sieve_size_t *address)
{
	sieve_code_dumpf(denv, "STRING-TEST");
	sieve_code_descend(denv);

	/* Optional operands */
	if ( sieve_match_opr_optional_dump(denv, address, NULL) != 0 )
		return FALSE;

	return
		sieve_opr_stringlist_dump(denv, address, "source") &&
		sieve_opr_stringlist_dump(denv, address, "key list");
}

/* 
 * Code execution 
 */

static int tst_string_stringlist_next_item
	(struct sieve_stringlist *_strlist, string_t **str_r);
static void tst_string_stringlist_reset
	(struct sieve_stringlist *_strlist);
static int tst_string_stringlist_get_length
	(struct sieve_stringlist *_strlist);

struct tst_string_stringlist {
	struct sieve_stringlist strlist;

	struct sieve_stringlist *value_list;
};

static struct sieve_stringlist *tst_string_stringlist_create
(const struct sieve_runtime_env *renv, struct sieve_stringlist *value_list)
{
	struct tst_string_stringlist *strlist;

	strlist = t_new(struct tst_string_stringlist, 1);
	strlist->strlist.runenv = renv;
	strlist->strlist.exec_status = SIEVE_EXEC_OK;
	strlist->strlist.next_item = tst_string_stringlist_next_item;
	strlist->strlist.reset = tst_string_stringlist_reset;
	strlist->strlist.get_length = tst_string_stringlist_get_length;
	strlist->value_list = value_list;

	return &strlist->strlist;
}

static int tst_string_stringlist_next_item
(struct sieve_stringlist *_strlist, string_t **str_r)
{
	struct tst_string_stringlist *strlist = 
		(struct tst_string_stringlist *)_strlist;

	return sieve_stringlist_next_item(strlist->value_list, str_r);
}

static void tst_string_stringlist_reset
(struct sieve_stringlist *_strlist)
{
	struct tst_string_stringlist *strlist = 
		(struct tst_string_stringlist *)_strlist;

	sieve_stringlist_reset(strlist->value_list);
}

static int tst_string_stringlist_get_length
(struct sieve_stringlist *_strlist)
{
	struct tst_string_stringlist *strlist = 
		(struct tst_string_stringlist *)_strlist;
	string_t *item;
	int length = 0;
	int ret;

	while ( (ret=sieve_stringlist_next_item(strlist->value_list, &item)) > 0 ) {
		if ( str_len(item) > 0 )
			length++;
	}

	return ( ret < 0 ? -1 : length );
}

static int tst_string_operation_execute
(const struct sieve_runtime_env *renv, sieve_size_t *address)
{
	struct sieve_match_type mcht = 
		SIEVE_MATCH_TYPE_DEFAULT(is_match_type);
	struct sieve_comparator cmp = 
		SIEVE_COMPARATOR_DEFAULT(i_octet_comparator);
	struct sieve_stringlist *source, *value_list, *key_list;
	int match, ret;

	/*
	 * Read operands 
	 */
	
	/* Handle match-type and comparator operands */
	if ( sieve_match_opr_optional_read
		(renv, address, NULL, &ret, &cmp, &mcht) < 0 )
		return ret;
	
	/* Read source */
	if ( (ret=sieve_opr_stringlist_read(renv, address, "source", &source)) <= 0 )
		return ret;
	
	/* Read key-list */
	if ( (ret=sieve_opr_stringlist_read(renv, address, "key-list", &key_list)) 
		<= 0 )
		return ret;

	/*
	 * Perform operation
	 */

	sieve_runtime_trace(renv, SIEVE_TRLVL_TESTS, "string test");

	/* Create wrapper string list wich does not count empty string items */
	value_list = tst_string_stringlist_create(renv, source);

	/* Perform match */
	if ( (match=sieve_match(renv, &mcht, &cmp, value_list, key_list, &ret)) < 0 )
		return ret; 	

	/* Set test result for subsequent conditional jump */
	sieve_interpreter_set_test_result(renv->interp, match > 0);
	return SIEVE_EXEC_OK;
}
