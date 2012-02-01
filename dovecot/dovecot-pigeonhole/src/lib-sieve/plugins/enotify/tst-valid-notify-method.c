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

#include "ext-enotify-common.h"

/* 
 * Valid_notify_method test 
 *
 * Syntax:
 *   valid_notify_method <notification-uris: string-list>
 */

static bool tst_vnotifym_validate
	(struct sieve_validator *valdtr, struct sieve_command *tst);
static bool tst_vnotifym_generate
	(const struct sieve_codegen_env *cgenv, struct sieve_command *ctx);

const struct sieve_command_def valid_notify_method_test = { 
	"valid_notify_method", 
	SCT_TEST, 
	1, 0, FALSE, FALSE,
	NULL, NULL,
	tst_vnotifym_validate,
	NULL, 
	tst_vnotifym_generate, 
	NULL 
};

/* 
 * Valid_notify_method operation
 */

static bool tst_vnotifym_operation_dump
	(const struct sieve_dumptime_env *denv, sieve_size_t *address);
static int tst_vnotifym_operation_execute
	(const struct sieve_runtime_env *renv, sieve_size_t *address);

const struct sieve_operation_def valid_notify_method_operation = { 
	"VALID_NOTIFY_METHOD",
	&enotify_extension, 
	EXT_ENOTIFY_OPERATION_VALID_NOTIFY_METHOD, 
	tst_vnotifym_operation_dump, 
	tst_vnotifym_operation_execute 
};

/* 
 * Test validation 
 */

static bool tst_vnotifym_validate
	(struct sieve_validator *valdtr, struct sieve_command *tst) 
{ 		
	struct sieve_ast_argument *arg = tst->first_positional;
	
	if ( !sieve_validate_positional_argument
		(valdtr, tst, arg, "notification-uris", 1, SAAT_STRING_LIST) ) {
		return FALSE;
	}
	
	return sieve_validator_argument_activate(valdtr, tst, arg, FALSE);
}

/* 
 * Test generation 
 */

static bool tst_vnotifym_generate
(const struct sieve_codegen_env *cgenv, struct sieve_command *cmd) 
{
	sieve_operation_emit(cgenv->sblock, cmd->ext, &valid_notify_method_operation);

 	/* Generate arguments */
	return sieve_generate_arguments(cgenv, cmd, NULL);
}

/* 
 * Code dump 
 */

static bool tst_vnotifym_operation_dump
(const struct sieve_dumptime_env *denv, sieve_size_t *address)
{
	sieve_code_dumpf(denv, "VALID_NOTIFY_METHOD");
	sieve_code_descend(denv);
		
	return
		sieve_opr_stringlist_dump(denv, address, "notify-uris");
}

/* 
 * Code execution 
 */

static int tst_vnotifym_operation_execute
(const struct sieve_runtime_env *renv, sieve_size_t *address)
{
	struct sieve_stringlist *notify_uris;
	string_t *uri_item;
	bool all_valid = TRUE;
	int ret;

	/*
	 * Read operands 
	 */
	
	/* Read notify uris */
	if ( (ret=sieve_opr_stringlist_read
		(renv, address, "notify-uris", &notify_uris)) <= 0 ) 
		return ret;
	
	/*
	 * Perform operation
	 */

	sieve_runtime_trace(renv, SIEVE_TRLVL_TESTS, "valid_notify_method test");

	uri_item = NULL;
	while ( (ret=sieve_stringlist_next_item(notify_uris, &uri_item)) > 0 ) {		
		if ( !ext_enotify_runtime_method_validate(renv, uri_item) ) {
			all_valid = FALSE;
			break;
		}
	}
	
	if ( ret < 0 ) {
		sieve_runtime_trace_error(renv, "invalid method uri item");
		return SIEVE_EXEC_BIN_CORRUPT;
	}
	
	sieve_interpreter_set_test_result(renv->interp, all_valid);
	return SIEVE_EXEC_OK;
}
