/* Copyright (c) 2002-2011 Pigeonhole authors, see the included COPYING file
 */

#include "sieve-common.h"
#include "sieve-commands.h"
#include "sieve-validator.h"
#include "sieve-generator.h"
#include "sieve-code.h"
#include "sieve-binary.h"

/*
 * Commands
 */

/* If command
 *
 * Syntax:   
 *   if <test1: test> <block1: block>
 */

static bool cmd_if_validate
	(struct sieve_validator *valdtr, struct sieve_command *cmd);
static bool cmd_if_generate
	(const struct sieve_codegen_env *cgenv, struct sieve_command *cmd);

const struct sieve_command_def cmd_if = { 
	"if", 
	SCT_COMMAND, 
	0, 1, TRUE, TRUE,
	NULL, NULL,
	cmd_if_validate, 
	cmd_if_generate, 
	NULL 
};

/* ElsIf command
 *
 * Santax:
 *   elsif <test2: test> <block2: block>
 */

static bool cmd_elsif_validate
	(struct sieve_validator *valdtr, struct sieve_command *cmd);

const struct sieve_command_def cmd_elsif = {
    "elsif", 
	SCT_COMMAND,
	0, 1, TRUE, TRUE, 
	NULL, NULL, 
	cmd_elsif_validate, 
	cmd_if_generate, 
	NULL 
};

/* Else command 
 *
 * Syntax:   
 *   else <block>
 */

static bool cmd_else_generate
	(const struct sieve_codegen_env *cgenv, struct sieve_command *cmd);

const struct sieve_command_def cmd_else = {
    "else", 
	SCT_COMMAND, 
	0, 0, TRUE, TRUE,
	NULL, NULL,
	cmd_elsif_validate, 
	cmd_else_generate, 
	NULL 
};

/* 
 * Context management
 */

struct cmd_if_context_data {
	struct cmd_if_context_data *previous;
	struct cmd_if_context_data *next;
	
	bool jump_generated;
	sieve_size_t exit_jump;
};

static void cmd_if_initialize_context_data
(struct sieve_command *cmd, struct cmd_if_context_data *previous) 
{ 	
	struct cmd_if_context_data *cmd_data;

	/* Assign context */
	cmd_data = p_new(sieve_command_pool(cmd), struct cmd_if_context_data, 1);
	cmd_data->exit_jump = 0;
	cmd_data->jump_generated = FALSE;

	/* Update linked list of contexts */
	cmd_data->previous = previous;
	cmd_data->next = NULL;	
	if ( previous != NULL )
		previous->next = cmd_data;
	
	/* Assign to command context */
	cmd->data = cmd_data;
}

/* 
 * Validation 
 */

static bool cmd_if_validate
(struct sieve_validator *valdtr ATTR_UNUSED, struct sieve_command *cmd) 
{ 
	/* Start if-command structure */
	cmd_if_initialize_context_data(cmd, NULL);
	
	return TRUE;
}

static bool cmd_elsif_validate
(struct sieve_validator *valdtr, struct sieve_command *cmd)
{
	struct sieve_command *prev = sieve_command_prev(cmd);

	/* Check valid command placement */
	if ( prev == NULL ||
		( !sieve_command_is(prev, cmd_if) && !sieve_command_is(prev, cmd_elsif) ) ) 
	{		
		sieve_command_validate_error(valdtr, cmd, 
			"the %s command must follow an if or elseif command", 
			sieve_command_identifier(cmd));
		return FALSE;
	}
	
	/* Previous command in this block is 'if' or 'elsif', so we can safely refer 
	 * to its context data 
	 */
	cmd_if_initialize_context_data(cmd, prev->data);

	return TRUE;
}

/* 
 * Code generation 
 */

/* The if command does not generate specific IF-ELSIF-ELSE opcodes, but only uses
 * JMP instructions. This is why the implementation of the if command does not 
 * include an opcode implementation.
 */

static void cmd_if_resolve_exit_jumps
(struct sieve_binary_block *sblock, struct cmd_if_context_data *cmd_data) 
{
	struct cmd_if_context_data *if_ctx = cmd_data->previous;
	
	/* Iterate backwards through all if-command contexts and resolve the 
	 * exit jumps to the current code position.
	 */
	while ( if_ctx != NULL ) {
		if ( if_ctx->jump_generated ) 
			sieve_binary_resolve_offset(sblock, if_ctx->exit_jump);
		if_ctx = if_ctx->previous;	
	}
}

static bool cmd_if_generate
(const struct sieve_codegen_env *cgenv, struct sieve_command *cmd)
{
	struct sieve_binary_block *sblock = cgenv->sblock;
	struct cmd_if_context_data *cmd_data = 
		(struct cmd_if_context_data *) cmd->data;
	struct sieve_ast_node *test;
	struct sieve_jumplist jmplist;
	
	/* Prepare jumplist */
	sieve_jumplist_init_temp(&jmplist, sblock);
	
	/* Generate test condition */
	test = sieve_ast_test_first(cmd->ast_node);
	if ( !sieve_generate_test(cgenv, test, &jmplist, FALSE) )
		return FALSE;
		
	/* Case true { */
	if ( !sieve_generate_block(cgenv, cmd->ast_node) ) 
		return FALSE;
	
	/* Are we the final command in this if-elsif-else structure? */
	if ( cmd_data->next != NULL ) {
		/* No, generate jump to end of if-elsif-else structure (resolved later) 
		 * This of course is not necessary if the {} block contains a command 
		 * like stop at top level that unconditionally exits the block already
		 * anyway. 
		 */
		if ( !sieve_command_block_exits_unconditionally(cmd) ) {
			sieve_operation_emit(sblock, NULL, &sieve_jmp_operation);
			cmd_data->exit_jump = sieve_binary_emit_offset(sblock, 0);
			cmd_data->jump_generated = TRUE;
		}
	} else {
		/* Yes, Resolve previous exit jumps to this point */
		cmd_if_resolve_exit_jumps(sblock, cmd_data);
	}
	
	/* Case false ... (subsequent elsif/else commands might generate more) */
	sieve_jumplist_resolve(&jmplist);	
		
	return TRUE;
}

static bool cmd_else_generate
(const struct sieve_codegen_env *cgenv, struct sieve_command *cmd)
{
	struct cmd_if_context_data *cmd_data =
		(struct cmd_if_context_data *) cmd->data;
	
	/* Else { */
	if ( !sieve_generate_block(cgenv, cmd->ast_node) ) 
		return FALSE;
		
	/* } End: resolve all exit blocks */	
	cmd_if_resolve_exit_jumps(cgenv->sblock, cmd_data);
		
	return TRUE;
}

