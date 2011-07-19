/* Copyright (c) 2002-2011 Pigeonhole authors, see the included COPYING file
 */

#include "lib.h"
#include "str-sanitize.h"

#include "sieve-common.h"
#include "sieve-commands.h"
#include "sieve-stringlist.h"
#include "sieve-code.h"
#include "sieve-validator.h"
#include "sieve-generator.h"
#include "sieve-interpreter.h"
#include "sieve-code-dumper.h"

/* 
 * Exists test
 *  
 * Syntax:
 *    exists <header-names: string-list>
 */

static bool tst_exists_validate
	(struct sieve_validator *valdtr, struct sieve_command *tst);
static bool tst_exists_generate
	(const struct sieve_codegen_env *cgenv, struct sieve_command *tst);

const struct sieve_command_def tst_exists = { 
	"exists", 
	SCT_TEST, 
	1, 0, FALSE, FALSE,
	NULL, 
	NULL,
	tst_exists_validate, 
	tst_exists_generate, 
	NULL 
};

/* 
 * Exists operation 
 */

static bool tst_exists_operation_dump
	(const struct sieve_dumptime_env *denv, sieve_size_t *address);
static int tst_exists_operation_execute
	(const struct sieve_runtime_env *renv, sieve_size_t *address);

const struct sieve_operation_def tst_exists_operation = { 
	"EXISTS",
	NULL,
	SIEVE_OPERATION_EXISTS,
	tst_exists_operation_dump, 
	tst_exists_operation_execute 
};

/* 
 * Validation 
 */

static bool tst_exists_validate
  (struct sieve_validator *valdtr, struct sieve_command *tst) 
{
	struct sieve_ast_argument *arg = tst->first_positional;
		
	if ( !sieve_validate_positional_argument
		(valdtr, tst, arg, "header names", 1, SAAT_STRING_LIST) ) {
		return FALSE;
	}
	
	if ( !sieve_validator_argument_activate(valdtr, tst, arg, FALSE) )
		return FALSE;

	return sieve_command_verify_headers_argument(valdtr, arg);
}

/* 
 * Code generation 
 */

static bool tst_exists_generate
(const struct sieve_codegen_env *cgenv, struct sieve_command *tst) 
{
	sieve_operation_emit(cgenv->sblock, NULL, &tst_exists_operation);

 	/* Generate arguments */
    return sieve_generate_arguments(cgenv, tst, NULL);
}

/* 
 * Code dump 
 */

static bool tst_exists_operation_dump
(const struct sieve_dumptime_env *denv, sieve_size_t *address)
{
	sieve_code_dumpf(denv, "EXISTS");
	sieve_code_descend(denv);

	return sieve_opr_stringlist_dump(denv, address, "header names");
}

/* 
 * Code execution 
 */

static int tst_exists_operation_execute
(const struct sieve_runtime_env *renv, sieve_size_t *address)
{
	struct sieve_stringlist *hdr_list;
	string_t *hdr_item;
	bool matched;
	int ret;
	
	/*
	 * Read operands
	 */

	/* Read header-list */
	if ( (ret=sieve_opr_stringlist_read(renv, address, "header-list", &hdr_list))
		<= 0 )
		return ret;

	/*
	 * Perfrom test
	 */

	sieve_runtime_trace(renv, SIEVE_TRLVL_TESTS, "exists test");
	sieve_runtime_trace_descend(renv);

	/* Iterate through all requested headers to match (must find all specified) */
	hdr_item = NULL;
	matched = TRUE;
	while ( matched &&
		(ret=sieve_stringlist_next_item(hdr_list, &hdr_item)) > 0 ) {
		const char *const *headers;
			
		if ( mail_get_headers
			(renv->msgdata->mail, str_c(hdr_item), &headers) < 0 ||
			headers[0] == NULL ) {
			matched = FALSE;				 
		}	
	
		sieve_runtime_trace(renv, SIEVE_TRLVL_MATCHING,
        	"header `%s' %s", str_sanitize(str_c(hdr_item), 80),
			( matched ? "exists" : "is missing" ));
	}

	if ( matched )
		sieve_runtime_trace(renv, SIEVE_TRLVL_MATCHING, "all headers exist");
	else
		sieve_runtime_trace(renv, SIEVE_TRLVL_MATCHING, "headers are missing");
	
	/* Set test result for subsequent conditional jump */
	if ( ret >= 0 ) {
		sieve_interpreter_set_test_result(renv->interp, matched);
		return SIEVE_EXEC_OK;
	}
	
	sieve_runtime_trace_error(renv, "invalid header-list item");
	return SIEVE_EXEC_BIN_CORRUPT;
}
