/* Copyright (c) 2002-2011 Pigeonhole authors, see the included COPYING file
 */

#include "lib.h"

#include "sieve-code.h"
#include "sieve-commands.h"
#include "sieve-validator.h" 
#include "sieve-generator.h"
#include "sieve-interpreter.h"

#include "ext-include-common.h"

/* 
 * Return command 
 * 
 * Syntax
 *   return
 */

static bool cmd_return_generate
	(const struct sieve_codegen_env *cgenv, struct sieve_command *cmd);
	
const struct sieve_command_def cmd_return = { 
	"return", 
	SCT_COMMAND, 
	0, 0, FALSE, FALSE,
	NULL, NULL, NULL, NULL,
	cmd_return_generate, 
	NULL
};

/* 
 * Return operation 
 */

static int opc_return_execute
	(const struct sieve_runtime_env *renv, sieve_size_t *address);

const struct sieve_operation_def return_operation = { 
	"return",
	&include_extension,
	EXT_INCLUDE_OPERATION_RETURN,
	NULL, 
	opc_return_execute 
};

/*
 * Code generation
 */

static bool cmd_return_generate
(const struct sieve_codegen_env *cgenv, struct sieve_command *cmd) 
{
	sieve_operation_emit(cgenv->sblock, cmd->ext, &return_operation);

	return TRUE;
}

/*
 * Execution
 */

static int opc_return_execute
(const struct sieve_runtime_env *renv, sieve_size_t *address ATTR_UNUSED)
{	
	ext_include_execute_return(renv);
	return SIEVE_EXEC_OK;
}


