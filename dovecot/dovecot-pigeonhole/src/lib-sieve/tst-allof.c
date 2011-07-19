/* Copyright (c) 2002-2011 Pigeonhole authors, see the included COPYING file
 */

#include "sieve-common.h"
#include "sieve-commands.h"
#include "sieve-validator.h"
#include "sieve-generator.h"
#include "sieve-binary.h"
#include "sieve-code.h"
#include "sieve-binary.h"

/* 
 * Allof test 
 * 
 * Syntax 
 *   allof <tests: test-list>   
 */

static bool tst_allof_generate
	(const struct sieve_codegen_env *cgenv, struct sieve_command *ctx,
		struct sieve_jumplist *jumps, bool jump_true);

const struct sieve_command_def tst_allof = { 
	"allof", 
	SCT_TEST, 
	0, 2, FALSE, FALSE,
	NULL, NULL, NULL, NULL, 
	tst_allof_generate 
};

/* 
 * Code generation 
 */

static bool tst_allof_generate
(const struct sieve_codegen_env *cgenv, struct sieve_command *ctx,
	struct sieve_jumplist *jumps, bool jump_true)
{
	struct sieve_binary_block *sblock = cgenv->sblock;
	struct sieve_ast_node *test;
	struct sieve_jumplist false_jumps;

	if ( sieve_ast_test_count(ctx->ast_node) > 1 ) {	
		if ( jump_true ) {
			/* Prepare jumplist */
			sieve_jumplist_init_temp(&false_jumps, sblock);
		}
	
		test = sieve_ast_test_first(ctx->ast_node);
		while ( test != NULL ) {	
			bool result; 

			/* If this test list must jump on false, all sub-tests can simply add their jumps
			 * to the caller's jump list, otherwise this test redirects all false jumps to the 
			 * end of the currently generated code. This is just after a final jump to the true
			 * case 
			 */
			if ( jump_true ) 
				result = sieve_generate_test(cgenv, test, &false_jumps, FALSE);
			else
				result = sieve_generate_test(cgenv, test, jumps, FALSE);
		
			if ( !result ) return FALSE;

			test = sieve_ast_test_next(test);
		}	
	
		if ( jump_true ) {
			/* All tests succeeded, jump to case TRUE */
			sieve_operation_emit(cgenv->sblock, NULL, &sieve_jmp_operation);
			sieve_jumplist_add(jumps, sieve_binary_emit_offset(sblock, 0));
			
			/* All false exits jump here */
			sieve_jumplist_resolve(&false_jumps);
		}
	} else {
		/* Script author is being inefficient; we can optimize the allof test away */
		test = sieve_ast_test_first(ctx->ast_node);
		sieve_generate_test(cgenv, test, jumps, jump_true);
	}
		
	return TRUE;
}

