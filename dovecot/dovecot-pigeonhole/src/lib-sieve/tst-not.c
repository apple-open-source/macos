/* Copyright (c) 2002-2011 Pigeonhole authors, see the included COPYING file
 */

#include "sieve-common.h"
#include "sieve-commands.h"
#include "sieve-validator.h"
#include "sieve-generator.h"

/* 
 * Not test 
 *
 * Syntax:
 *   not <tests: test-list>   
 */

static bool tst_not_generate
	(const struct sieve_codegen_env *cgenv, struct sieve_command *ctx,
		struct sieve_jumplist *jumps, bool jump_true);
static bool tst_not_validate_const
	(struct sieve_validator *valdtr, struct sieve_command *tst,
		int *const_current, int const_next);

const struct sieve_command_def tst_not = { 
	"not", 
	SCT_TEST, 
	0, 1, FALSE, FALSE,
	NULL, NULL, NULL,
	tst_not_validate_const,
	NULL, 
	tst_not_generate 
};

/*
 * Code validation
 */

static bool tst_not_validate_const
(struct sieve_validator *valdtr ATTR_UNUSED,
	struct sieve_command *tst ATTR_UNUSED, int *const_current, int const_next)
{
	if ( const_next < 0 )
		*const_current = -1;
	else if ( const_next > 0 )
		*const_current = 0;
	else
		*const_current = 1;

	return TRUE;
}

/* 
 * Code generation 
 */

static bool tst_not_generate
(const struct sieve_codegen_env *cgenv, struct sieve_command *ctx,
	struct sieve_jumplist *jumps, bool jump_true)
{
	struct sieve_ast_node *test;
	
	/* Validator verified the existance of the single test already */
	test = sieve_ast_test_first(ctx->ast_node); 
	
	return sieve_generate_test(cgenv, test, jumps, !jump_true);
}

