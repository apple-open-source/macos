/* Copyright (c) 2002-2011 Pigeonhole authors, see the included COPYING file
 */

#include "lib.h"

#include "sieve-common.h"
#include "sieve-code.h"
#include "sieve-commands.h"
#include "sieve-validator.h"
#include "sieve-generator.h"
#include "sieve-interpreter.h"
#include "sieve-dump.h"

/* 
 * Size test 
 *
 * Syntax:
 *    size <":over" / ":under"> <limit: number>
 */

static bool tst_size_registered
	(struct sieve_validator *valdtr, const struct sieve_extension *ext, 
		struct sieve_command_registration *cmd_reg);
static bool tst_size_pre_validate
	(struct sieve_validator *valdtr, struct sieve_command *tst);
static bool tst_size_validate
	(struct sieve_validator *valdtr, struct sieve_command *tst);
static bool tst_size_generate
	(const struct sieve_codegen_env *cgenv, struct sieve_command *ctx); 

const struct sieve_command_def tst_size = { 
	"size", 
	SCT_TEST, 
	1, 0, FALSE, FALSE,
	tst_size_registered, 
	tst_size_pre_validate,
	tst_size_validate, 
	NULL,
	tst_size_generate, 
	NULL 
};

/* 
 * Size operations 
 */

static bool tst_size_operation_dump
	(const struct sieve_dumptime_env *denv, sieve_size_t *address);
static int tst_size_operation_execute
	(const struct sieve_runtime_env *renv, sieve_size_t *address);

const struct sieve_operation_def tst_size_over_operation = { 
	"SIZE-OVER",
	NULL, 
	SIEVE_OPERATION_SIZE_OVER,
	tst_size_operation_dump, 
	tst_size_operation_execute 
};

const struct sieve_operation_def tst_size_under_operation = {
	"SIZE-UNDER",
	NULL, 
	SIEVE_OPERATION_SIZE_UNDER,
	tst_size_operation_dump, 
	tst_size_operation_execute 
};

/* 
 * Context data
 */

struct tst_size_context_data {
	enum { SIZE_UNASSIGNED, SIZE_UNDER, SIZE_OVER } type;
};

#define TST_SIZE_ERROR_DUP_TAG \
	"exactly one of the ':under' or ':over' tags must be specified " \
	"for the size test, but more were found"

/* 
 * Tag validation 
 */

static bool tst_size_validate_over_tag
(struct sieve_validator *valdtr, struct sieve_ast_argument **arg, 
	struct sieve_command *tst)
{
	struct tst_size_context_data *ctx_data = 
		(struct tst_size_context_data *) tst->data;	
	
	if ( ctx_data->type != SIZE_UNASSIGNED ) {
		sieve_argument_validate_error(valdtr, *arg, TST_SIZE_ERROR_DUP_TAG);
		return FALSE;		
	}
	
	ctx_data->type = SIZE_OVER;
	
	/* Delete this tag */
	*arg = sieve_ast_arguments_detach(*arg, 1);
	
	return TRUE;
}

static bool tst_size_validate_under_tag
(struct sieve_validator *valdtr, struct sieve_ast_argument **arg ATTR_UNUSED, 
	struct sieve_command *tst)
{
	struct tst_size_context_data *ctx_data = 
		(struct tst_size_context_data *) tst->data;	
	
	if ( ctx_data->type != SIZE_UNASSIGNED ) {
		sieve_argument_validate_error(valdtr, *arg, TST_SIZE_ERROR_DUP_TAG);
		return FALSE;		
	}
	
	ctx_data->type = SIZE_UNDER;
	
	/* Delete this tag */
	*arg = sieve_ast_arguments_detach(*arg, 1);
		
	return TRUE;
}

/* 
 * Test registration 
 */

static const struct sieve_argument_def size_over_tag = { 
	"over", 
	NULL,
	tst_size_validate_over_tag, 
	NULL, NULL, NULL
};

static const struct sieve_argument_def size_under_tag = { 
	"under", 
	NULL,
	tst_size_validate_under_tag, 
	NULL, NULL,  NULL
};

static bool tst_size_registered
(struct sieve_validator *valdtr, const struct sieve_extension *ext ATTR_UNUSED, 
	struct sieve_command_registration *cmd_reg) 
{
	/* Register our tags */
	sieve_validator_register_tag(valdtr, cmd_reg, NULL, &size_over_tag, 0); 	
	sieve_validator_register_tag(valdtr, cmd_reg, NULL, &size_under_tag, 0); 	

	return TRUE;
}

/* 
 * Test validation 
 */

static bool tst_size_pre_validate
(struct sieve_validator *valdtr ATTR_UNUSED, 
	struct sieve_command *tst) 
{
	struct tst_size_context_data *ctx_data;
	
	/* Assign context */
	ctx_data = p_new(sieve_command_pool(tst), struct tst_size_context_data, 1);
	ctx_data->type = SIZE_UNASSIGNED;
	tst->data = ctx_data;

	return TRUE;
}

static bool tst_size_validate
	(struct sieve_validator *valdtr, struct sieve_command *tst) 
{
	struct tst_size_context_data *ctx_data = 
		(struct tst_size_context_data *) tst->data;
	struct sieve_ast_argument *arg = tst->first_positional;
	
	if ( ctx_data->type == SIZE_UNASSIGNED ) {
		sieve_command_validate_error(valdtr, tst, 
			"the size test requires either the :under or the :over tag "
			"to be specified");
		return FALSE;		
	}
		
	if ( !sieve_validate_positional_argument
		(valdtr, tst, arg, "limit", 1, SAAT_NUMBER) ) {
		return FALSE;
	}
	
	return sieve_validator_argument_activate(valdtr, tst, arg, FALSE);
}

/* 
 * Code generation 
 */

bool tst_size_generate
(const struct sieve_codegen_env *cgenv, struct sieve_command *tst) 
{
	struct tst_size_context_data *ctx_data = 
		(struct tst_size_context_data *) tst->data;

	if ( ctx_data->type == SIZE_OVER ) 
		sieve_operation_emit(cgenv->sblock, NULL, &tst_size_over_operation);
	else
		sieve_operation_emit(cgenv->sblock, NULL, &tst_size_under_operation);

 	/* Generate arguments */
	if ( !sieve_generate_arguments(cgenv, tst, NULL) )
		return FALSE;
	  
	return TRUE;
}

/* 
 * Code dump 
 */

static bool tst_size_operation_dump
(const struct sieve_dumptime_env *denv, sieve_size_t *address)
{
	sieve_code_dumpf(denv, "%s", sieve_operation_mnemonic(denv->oprtn));
	sieve_code_descend(denv);
	
	return 
		sieve_opr_number_dump(denv, address, "limit");
}

/* 
 * Code execution 
 */

static inline bool tst_size_get
(const struct sieve_runtime_env *renv, sieve_number_t *size) 
{
	uoff_t psize;

	if ( mail_get_physical_size(renv->msgdata->mail, &psize) < 0 )
		return FALSE;

	*size = psize;
  
	return TRUE;
}

static int tst_size_operation_execute
(const struct sieve_runtime_env *renv, sieve_size_t *address)
{
	sieve_number_t mail_size, limit;
	int ret;
		
	/*
	 * Read operands
	 */

	/* Read size limit */
	if ( (ret=sieve_opr_number_read(renv, address, "limit", &limit)) <= 0 )
		return ret;	

	/*
	 * Perform test
	 */
	
	/* Get the size of the message */
	if ( !tst_size_get(renv, &mail_size) ) {
		/* FIXME: improve this error */
		sieve_sys_error(renv->svinst, "failed to assess message size");
		return SIEVE_EXEC_FAILURE;
	}
	
	/* Perform the test */
	if ( sieve_operation_is(renv->oprtn, tst_size_over_operation) ) {
		sieve_runtime_trace(renv, SIEVE_TRLVL_TESTS, "size :over test");
		
		if ( sieve_runtime_trace_active(renv, SIEVE_TRLVL_MATCHING) ) {
			sieve_runtime_trace_descend(renv);

			sieve_runtime_trace(renv, 0,
				"comparing message size %lu", (unsigned long) mail_size);
			sieve_runtime_trace(renv, 0,
				"with upper limit %lu", (unsigned long) limit);
		}

		sieve_interpreter_set_test_result(renv->interp, (mail_size > limit));
	} else {
		sieve_runtime_trace(renv, SIEVE_TRLVL_TESTS, "size :under test");

		if ( sieve_runtime_trace_active(renv, SIEVE_TRLVL_MATCHING) ) {
			sieve_runtime_trace_descend(renv);

			sieve_runtime_trace(renv, 0,
				"comparing message size %lu", (unsigned long) mail_size);
			sieve_runtime_trace(renv, 0,
				"with lower limit %lu", (unsigned long) limit);
		}

		sieve_interpreter_set_test_result(renv->interp, (mail_size < limit));
	}

	return SIEVE_EXEC_OK;
}

