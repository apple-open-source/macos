/* Copyright (c) 2002-2011 Pigeonhole authors, see the included COPYING file
 */

#include "sieve-common.h"
#include "sieve-commands.h"
#include "sieve-generator.h"
#include "sieve-validator.h"
#include "sieve-binary.h"
#include "sieve-code.h"
#include "sieve-binary.h"

/* 
 * Anyof test 
 *
 * Syntax 
 *   anyof <tests: test-list>   
 */

static bool tst_anyof_generate	
	(const struct sieve_codegen_env *cgenv, struct sieve_command *ctx,
		struct sieve_jumplist *jumps, bool jump_true);

const struct sieve_command_def tst_anyof = { 
	"anyof", 
	SCT_TEST, 
	0, 2, FALSE, FALSE,
	NULL, NULL, NULL, NULL, 
	tst_anyof_generate 
};

/* 
 * Code generation 
 */

static bool tst_anyof_generate	
	(const struct sieve_codegen_env *cgenv, struct sieve_command *ctx,
		struct sieve_jumplist *jumps, bool jump_true)
{
	struct sieve_binary_block *sblock = cgenv->sblock;
	struct sieve_ast_node *test;
	struct sieve_jumplist true_jumps;

	if ( sieve_ast_test_count(ctx->ast_node) > 1 ) {	
		if ( !jump_true ) {
			/* Prepare jumplist */
			sieve_jumplist_init_temp(&true_jumps, sblock);
		}
	
		test = sieve_ast_test_first(ctx->ast_node);
		while ( test != NULL ) {	
			bool result;

			/* If this test list must jump on true, all sub-tests can simply add their jumps
			 * to the caller's jump list, otherwise this test redirects all true jumps to the 
			 * end of the currently generated code. This is just after a final jump to the false
			 * case 
			 */
			if ( !jump_true ) 
				result = sieve_generate_test(cgenv, test, &true_jumps, TRUE);
			else
				result = sieve_generate_test(cgenv, test, jumps, TRUE);

			if ( !result ) return FALSE;
		
			test = sieve_ast_test_next(test);
		}	
	
		if ( !jump_true ) {
			/* All tests failed, jump to case FALSE */
			sieve_operation_emit(sblock, NULL, &sieve_jmp_operation);
			sieve_jumplist_add(jumps, sieve_binary_emit_offset(sblock, 0));
			
			/* All true exits jump here */
			sieve_jumplist_resolve(&true_jumps);
		}
	} else {
		/* Script author is being inefficient; we can optimize the allof test away */
        test = sieve_ast_test_first(ctx->ast_node);
        sieve_generate_test(cgenv, test, jumps, jump_true);
    }		
		
	return TRUE;
}
