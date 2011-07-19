/* Copyright (c) 2002-2011 Pigeonhole authors, see the included COPYING file
 */

#include "sieve-ast.h"
#include "sieve-validator.h"
#include "sieve-generator.h"
#include "sieve-binary.h"

#include "sieve-commands.h"
#include "sieve-code.h"
#include "sieve-interpreter.h"

/*
 * True/False test command
 */

static bool tst_false_generate
	(const struct sieve_codegen_env *cgenv, struct sieve_command *cmd,
		struct sieve_jumplist *jumps, bool jump_true);
static bool tst_true_generate
	(const struct sieve_codegen_env *cgenv, struct sieve_command *cmd,
		struct sieve_jumplist *jumps, bool jump_true);

const struct sieve_command_def tst_false = { 
	"false", 
	SCT_TEST, 
	0, 0, FALSE, FALSE,
	NULL, NULL, NULL, NULL, 
	tst_false_generate 
};

const struct sieve_command_def tst_true = { 
	"true", 
	SCT_TEST, 
	0, 0, FALSE, FALSE,
	NULL, NULL, NULL, NULL, 
	tst_true_generate 
};

static bool tst_false_generate
(const struct sieve_codegen_env *cgenv, 
	struct sieve_command *cmd ATTR_UNUSED,
	struct sieve_jumplist *jumps, bool jump_true)
{
	if ( !jump_true ) {
		sieve_operation_emit(cgenv->sblock, NULL, &sieve_jmp_operation);
		sieve_jumplist_add(jumps, sieve_binary_emit_offset(cgenv->sblock, 0));
	}
	
	return TRUE;
}

static bool tst_true_generate
(const struct sieve_codegen_env *cgenv,	
	struct sieve_command *cmd ATTR_UNUSED,
	struct sieve_jumplist *jumps, bool jump_true)
{
	if ( jump_true ) {
		sieve_operation_emit(cgenv->sblock, NULL, &sieve_jmp_operation);
		sieve_jumplist_add(jumps, sieve_binary_emit_offset(cgenv->sblock, 0));
	}
	
	return TRUE;
}

