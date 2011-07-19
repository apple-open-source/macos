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

const struct sieve_command_def tst_not = { 
	"not", 
	SCT_TEST, 
	0, 1, FALSE, FALSE,
	NULL, NULL, NULL, NULL, 
	tst_not_generate 
};

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

