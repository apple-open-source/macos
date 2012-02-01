/* Copyright (c) 2002-2011 Pigeonhole authors, see the included COPYING file
 */
 
#include "sieve-extensions.h"
#include "sieve-commands.h"
#include "sieve-stringlist.h"
#include "sieve-code.h"
#include "sieve-comparators.h"
#include "sieve-match-types.h"
#include "sieve-address-parts.h"

#include "sieve-validator.h"
#include "sieve-generator.h"
#include "sieve-binary.h"
#include "sieve-interpreter.h"
#include "sieve-dump.h"
#include "sieve-match.h"

#include "ext-body-common.h"

/* 
 * Body test 
 *
 * Syntax
 *   body [COMPARATOR] [MATCH-TYPE] [BODY-TRANSFORM]
 *     <key-list: string-list>
 */

static bool tst_body_registered
	(struct sieve_validator *valdtr, const struct sieve_extension *ext,
		struct sieve_command_registration *cmd_reg);
static bool tst_body_validate
	(struct sieve_validator *valdtr, struct sieve_command *tst);
static bool tst_body_generate
	(const struct sieve_codegen_env *cgenv,	struct sieve_command *ctx);

const struct sieve_command_def body_test = { 
	"body", 
	SCT_TEST, 
	1, 0, FALSE, FALSE,
	tst_body_registered, 
	NULL,
	tst_body_validate,
	NULL, 
	tst_body_generate, 
	NULL 
};

/* 
 * Body operation 
 */

static bool ext_body_operation_dump
	(const struct sieve_dumptime_env *denv, sieve_size_t *address);
static int ext_body_operation_execute
	(const struct sieve_runtime_env *renv, sieve_size_t *address);

const struct sieve_operation_def body_operation = { 
	"body",
	&body_extension,
	0,
	ext_body_operation_dump, 
	ext_body_operation_execute 
};

/*
 * Optional operands
 */

enum tst_body_optional {	
	OPT_BODY_TRANSFORM = SIEVE_MATCH_OPT_LAST
};

/* 
 * Tagged arguments 
 */

/* Forward declarations */

static bool tag_body_transform_validate
	(struct sieve_validator *valdtr, struct sieve_ast_argument **arg, 
		struct sieve_command *cmd);
static bool tag_body_transform_generate	
	(const struct sieve_codegen_env *cgenv, struct sieve_ast_argument *arg, 
		struct sieve_command *cmd);

/* Argument objects */
 
static const struct sieve_argument_def body_raw_tag = { 
	"raw", 
	NULL,
	tag_body_transform_validate, 
	NULL, NULL, 
	tag_body_transform_generate 
};

static const struct sieve_argument_def body_content_tag = { 
	"content", 
	NULL,
	tag_body_transform_validate, 
	NULL, NULL, 
	tag_body_transform_generate 
};

static const struct sieve_argument_def body_text_tag = { 
	"text", 
	NULL,
	tag_body_transform_validate, 
	NULL, NULL, 
	tag_body_transform_generate
};

/* Argument implementation */
 
static bool tag_body_transform_validate
(struct sieve_validator *valdtr, struct sieve_ast_argument **arg, 
	struct sieve_command *cmd)
{
	enum tst_body_transform transform;
	struct sieve_ast_argument *tag = *arg;

	/* BODY-TRANSFORM:
	 *   :raw
	 *     / :content <content-types: string-list>
	 *     / :text
	 */
	if ( (bool) cmd->data ) {
		sieve_argument_validate_error(valdtr, *arg, 
			"the :raw, :content and :text arguments for the body test are mutually "
			"exclusive, but more than one was specified");
		return FALSE;
	}

	/* Skip tag */
	*arg = sieve_ast_argument_next(*arg);

	/* :content tag has a string-list argument */
	if ( sieve_argument_is(tag, body_raw_tag) ) 
		transform = TST_BODY_TRANSFORM_RAW;
		
	else if ( sieve_argument_is(tag, body_text_tag) )
		transform = TST_BODY_TRANSFORM_TEXT;
		
	else if ( sieve_argument_is(tag, body_content_tag) ) {
		/* Check syntax:
		 *   :content <content-types: string-list>
		 */
		if ( !sieve_validate_tag_parameter
			(valdtr, cmd, tag, *arg, NULL, 0, SAAT_STRING_LIST, FALSE) ) {
			return FALSE;
		}
		
		/* Assign tag parameters */
		tag->parameters = *arg;
		*arg = sieve_ast_arguments_detach(*arg,1);
		
		transform = TST_BODY_TRANSFORM_CONTENT;
	} else 
		return FALSE;
	
	/* Signal the presence of this tag */
	cmd->data = (void *) TRUE;
		
	/* Assign context data */
	tag->argument->data = (void *) transform;	
		
	return TRUE;
}

/* 
 * Command Registration 
 */

static bool tst_body_registered
(struct sieve_validator *valdtr, const struct sieve_extension *ext,
	struct sieve_command_registration *cmd_reg) 
{
	/* The order of these is not significant */
	sieve_comparators_link_tag(valdtr, cmd_reg, SIEVE_MATCH_OPT_COMPARATOR);
	sieve_match_types_link_tags(valdtr, cmd_reg, SIEVE_MATCH_OPT_MATCH_TYPE);
	
	sieve_validator_register_tag
		(valdtr, cmd_reg, ext, &body_raw_tag, OPT_BODY_TRANSFORM); 	
	sieve_validator_register_tag
		(valdtr, cmd_reg, ext, &body_content_tag, OPT_BODY_TRANSFORM); 	
	sieve_validator_register_tag
		(valdtr, cmd_reg, ext, &body_text_tag, OPT_BODY_TRANSFORM); 	
	
	return TRUE;
}

/* 
 * Validation 
 */
 
static bool tst_body_validate
(struct sieve_validator *valdtr, struct sieve_command *tst) 
{ 		
	struct sieve_ast_argument *arg = tst->first_positional;
	const struct sieve_match_type mcht_default = 
		SIEVE_MATCH_TYPE_DEFAULT(is_match_type);
	const struct sieve_comparator cmp_default = 
		SIEVE_COMPARATOR_DEFAULT(i_ascii_casemap_comparator);
					
	if ( !sieve_validate_positional_argument
		(valdtr, tst, arg, "key list", 1, SAAT_STRING_LIST) ) {
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
 
static bool tst_body_generate
(const struct sieve_codegen_env *cgenv, struct sieve_command *cmd) 
{
	(void)sieve_operation_emit(cgenv->sblock, cmd->ext, &body_operation);

	/* Generate arguments */
	return sieve_generate_arguments(cgenv, cmd, NULL);
}

static bool tag_body_transform_generate
(const struct sieve_codegen_env *cgenv, struct sieve_ast_argument *arg, 
	struct sieve_command *cmd ATTR_UNUSED)
{
	enum tst_body_transform transform =	
		(enum tst_body_transform) arg->argument->data;
	
	sieve_binary_emit_byte(cgenv->sblock, transform);
	sieve_generate_argument_parameters(cgenv, cmd, arg); 
			
	return TRUE;
}

/* 
 * Code dump 
 */
 
static bool ext_body_operation_dump
(const struct sieve_dumptime_env *denv, sieve_size_t *address)
{
	unsigned int transform;
	int opt_code = 0;

	sieve_code_dumpf(denv, "BODY");
	sieve_code_descend(denv);

	/* Handle any optional arguments */
	for (;;) {
		int opt;

		if ( (opt=sieve_match_opr_optional_dump(denv, address, &opt_code)) 
			< 0 )
			return FALSE;

		if ( opt == 0 ) break;

		switch ( opt_code ) {
		case OPT_BODY_TRANSFORM:
			if ( !sieve_binary_read_byte(denv->sblock, address, &transform) )
				return FALSE;
			
			switch ( transform ) {
			case TST_BODY_TRANSFORM_RAW:
				sieve_code_dumpf(denv, "BODY-TRANSFORM: RAW");
				break;
			case TST_BODY_TRANSFORM_TEXT:
				sieve_code_dumpf(denv, "BODY-TRANSFORM: TEXT");
				break;
			case TST_BODY_TRANSFORM_CONTENT:
				sieve_code_dumpf(denv, "BODY-TRANSFORM: CONTENT");
				
				sieve_code_descend(denv);
				if ( !sieve_opr_stringlist_dump(denv, address, "content types") )
					return FALSE;
				sieve_code_ascend(denv);
				break;
			default:
				return FALSE;
			}
			break;
		default: 
			return FALSE;
		}
	};

	return sieve_opr_stringlist_dump(denv, address, "key list");
}

/*
 * Interpretation
 */

static int ext_body_operation_execute
(const struct sieve_runtime_env *renv, sieve_size_t *address)
{
	int opt_code = 0;
	struct sieve_comparator cmp = 
		SIEVE_COMPARATOR_DEFAULT(i_ascii_casemap_comparator);
	struct sieve_match_type mcht = 
		SIEVE_MATCH_TYPE_DEFAULT(is_match_type);
	unsigned int transform = TST_BODY_TRANSFORM_TEXT;
	struct sieve_stringlist *ctype_list, *value_list, *key_list;
	bool mvalues_active;
	const char * const *content_types = NULL;
	int match, ret;

	/*
	 * Read operands
	 */
	
	/* Optional operands */

	ctype_list = NULL;
	for (;;) {
		int opt;

		if ( (opt=sieve_match_opr_optional_read
			(renv, address, &opt_code, &ret, &cmp, &mcht)) < 0 )
			return ret;

		if ( opt == 0 ) break;
			
		switch ( opt_code ) {
		case OPT_BODY_TRANSFORM:
			if ( !sieve_binary_read_byte(renv->sblock, address, &transform) ||
				transform > TST_BODY_TRANSFORM_TEXT ) {
				sieve_runtime_trace_error(renv, "invalid body transform type");
				return SIEVE_EXEC_BIN_CORRUPT;
			}
			
			if ( transform == TST_BODY_TRANSFORM_CONTENT && 
				(ret=sieve_opr_stringlist_read
					(renv, address, "content-type-list", &ctype_list)) <= 0 )
				return ret;

			break;

		default:
			sieve_runtime_trace_error(renv, "unknown optional operand");
			return SIEVE_EXEC_BIN_CORRUPT;
		}
	} 
		
	/* Read key-list */

	if ( (ret=sieve_opr_stringlist_read(renv, address, "key-list", &key_list)) 
		<= 0 ) 
		return ret;
	
	if ( ctype_list != NULL && sieve_stringlist_read_all
		(ctype_list, pool_datastack_create(), &content_types) < 0 ) {
		sieve_runtime_trace_error(renv, "failed to read content-type-list operand");
		return ctype_list->exec_status;
	}
	
	/*
	 * Perform operation
	 */

	sieve_runtime_trace(renv, SIEVE_TRLVL_TESTS, "body test");
	
	/* Extract requested parts */
	value_list = ext_body_get_part_list
		(renv, (enum tst_body_transform) transform, content_types);
	if ( value_list == FALSE )
		return SIEVE_EXEC_FAILURE;

	/* Disable match values processing as required by RFC */
	mvalues_active = sieve_match_values_set_enabled(renv, FALSE);

	/* Perform match */
	match = sieve_match(renv, &mcht, &cmp, value_list, key_list, &ret); 	

	/* Restore match values processing */ 	
	(void)sieve_match_values_set_enabled(renv, mvalues_active);
	
	if ( match < 0 )
		return ret;

	/* Set test result for subsequent conditional jump */
	sieve_interpreter_set_test_result(renv->interp, match > 0);
	return SIEVE_EXEC_OK;
}
