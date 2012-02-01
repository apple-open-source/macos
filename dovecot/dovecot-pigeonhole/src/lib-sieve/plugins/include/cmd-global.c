/* Copyright (c) 2002-2011 Pigeonhole authors, see the included COPYING file
 */

#include "lib.h"

#include "sieve-common.h"
#include "sieve-code.h"
#include "sieve-commands.h"
#include "sieve-validator.h" 
#include "sieve-generator.h"
#include "sieve-binary.h"
#include "sieve-interpreter.h"
#include "sieve-dump.h"

#include "sieve-ext-variables.h"

#include "ext-include-common.h"
#include "ext-include-binary.h"
#include "ext-include-variables.h"

/* 
 * Commands 
 */

static bool cmd_global_validate
  (struct sieve_validator *valdtr, struct sieve_command *cmd);
static bool cmd_global_generate
(const struct sieve_codegen_env *cgenv, struct sieve_command *cmd);

const struct sieve_command_def cmd_global = {
  "global",
  SCT_COMMAND,
  1, 0, FALSE, FALSE,
  NULL, NULL,
  cmd_global_validate,
	NULL,
  cmd_global_generate,
  NULL
};

/* DEPRICATED:
 */
		
/* Import command 
 * 
 * Syntax
 *   import
 */	
const struct sieve_command_def cmd_import = { 
	"import", 
	SCT_COMMAND, 
	1, 0, FALSE, FALSE,
	NULL, NULL,
	cmd_global_validate,
	NULL,
	cmd_global_generate, 
	NULL
};

/* Export command 
 * 
 * Syntax
 *   export
 */	
const struct sieve_command_def cmd_export = { 
	"export", 
	SCT_COMMAND, 
	1, 0, FALSE, FALSE,
	NULL, NULL, 
	cmd_global_validate,
	NULL,
	cmd_global_generate, 
	NULL
};

/*
 * Operations
 */

static bool opc_global_dump
	(const struct sieve_dumptime_env *denv, sieve_size_t *address);
static int opc_global_execute
	(const struct sieve_runtime_env *renv, sieve_size_t *address);

/* Global operation */

const struct sieve_operation_def global_operation = { 
	"global",
	&include_extension,
	EXT_INCLUDE_OPERATION_GLOBAL,
	opc_global_dump, 
	opc_global_execute
};

/*
 * Validation
 */

static inline struct sieve_argument *_create_variable_argument
(struct sieve_command *cmd, struct sieve_variable *var)
{
	struct sieve_argument *argument = sieve_argument_create
		(cmd->ast_node->ast, NULL, cmd->ext, 0);

	argument->data = (void *) var;

	return argument;
}

static bool cmd_global_validate
(struct sieve_validator *valdtr, struct sieve_command *cmd) 
{
	const struct sieve_extension *this_ext = cmd->ext;
	struct sieve_ast_argument *arg = cmd->first_positional;
	struct sieve_command *prev = sieve_command_prev(cmd);

	/* DEPRECATED: Check valid command placement */
	if ( !sieve_command_is(cmd, cmd_global) ) {
		if ( !sieve_command_is_toplevel(cmd) ||
			( !sieve_command_is_first(cmd) && prev != NULL &&
				!sieve_command_is(prev, cmd_require) && 
				!sieve_command_is(prev, cmd_import) && 
				!sieve_command_is(prev, cmd_export) ) ) {
			sieve_command_validate_error(valdtr, cmd,
				"the DEPRECATED %s command can only be placed at top level "
				"at the beginning of the file after any require or "
				"import/export commands",
				sieve_command_identifier(cmd));
			return FALSE;
		}
	}

	/* Check for use of variables extension */	
	if ( !ext_include_validator_have_variables(this_ext, valdtr) ) {
		sieve_command_validate_error(valdtr, cmd, 
			"%s command requires that variables extension is active",
			sieve_command_identifier(cmd));
		return FALSE;
	}
		
	/* Register global variable */
	if ( sieve_ast_argument_type(arg) == SAAT_STRING ) {
		/* Single string */
		const char *identifier = sieve_ast_argument_strc(arg);
		struct sieve_variable *var;
		
		if ( (var=ext_include_variable_import_global
			(valdtr, cmd, identifier)) == NULL )
			return FALSE;
			
		arg->argument = _create_variable_argument(cmd, var);

	} else if ( sieve_ast_argument_type(arg) == SAAT_STRING_LIST ) {
		/* String list */
		struct sieve_ast_argument *stritem = sieve_ast_strlist_first(arg);
		
		while ( stritem != NULL ) {
			const char *identifier = sieve_ast_argument_strc(stritem);
			struct sieve_variable *var;
			
			if ( (var=ext_include_variable_import_global
				(valdtr, cmd, identifier)) == NULL )
				return FALSE;

			stritem->argument = _create_variable_argument(cmd, var);
	
			stritem = sieve_ast_strlist_next(stritem);
		}
	} else {
		/* Something else */
		sieve_argument_validate_error(valdtr, arg, 
			"the %s command accepts a single string or string list argument, "
			"but %s was found", sieve_command_identifier(cmd),
			sieve_ast_argument_name(arg));
		return FALSE;
	}

	/* Join global commands with predecessors if possible */
	if ( sieve_commands_equal(prev, cmd) ) {
		/* Join this command's string list with the previous one */
		prev->first_positional = sieve_ast_stringlist_join
			(prev->first_positional, cmd->first_positional);

		if ( prev->first_positional == NULL ) {
			/* Not going to happen unless MAXINT stringlist items are specified */
			sieve_command_validate_error(valdtr, cmd, 
				"compiler reached AST limit (script too complex)");
			return FALSE;
		}

		/* Detach this command node */
		sieve_ast_node_detach(cmd->ast_node);
	}
		
	return TRUE;
}

/*
 * Code generation
 */
 
static bool cmd_global_generate
(const struct sieve_codegen_env *cgenv, struct sieve_command *cmd) 
{
	struct sieve_ast_argument *arg = cmd->first_positional;

	sieve_operation_emit(cgenv->sblock, cmd->ext, &global_operation);
 	 			
	if ( sieve_ast_argument_type(arg) == SAAT_STRING ) {
		/* Single string */
		struct sieve_variable *var = (struct sieve_variable *) arg->argument->data;
		
		(void)sieve_binary_emit_unsigned(cgenv->sblock, 1);
		(void)sieve_binary_emit_unsigned(cgenv->sblock, var->index);
		
	} else if ( sieve_ast_argument_type(arg) == SAAT_STRING_LIST ) {
		/* String list */
		struct sieve_ast_argument *stritem = sieve_ast_strlist_first(arg);
		
		(void)sieve_binary_emit_unsigned
			(cgenv->sblock, sieve_ast_strlist_count(arg));
						
		while ( stritem != NULL ) {
			struct sieve_variable *var = 
				(struct sieve_variable *) stritem->argument->data;
			
			(void)sieve_binary_emit_unsigned(cgenv->sblock, var->index);
			
			stritem = sieve_ast_strlist_next(stritem);
		}
	} else {
		i_unreached();
	}
 	 		
	return TRUE;
}

/* 
 * Code dump
 */
 
static bool opc_global_dump
(const struct sieve_dumptime_env *denv, sieve_size_t *address)
{
	const struct sieve_extension *this_ext = denv->oprtn->ext;
	unsigned int count, i, var_count;
	struct sieve_variable_scope_binary *global_vars;
	struct sieve_variable_scope *global_scope;
	struct sieve_variable * const *vars;
	
	if ( !sieve_binary_read_unsigned(denv->sblock, address, &count) )
		return FALSE;

	sieve_code_dumpf(denv, "GLOBAL (count: %u):", count);

	global_vars = ext_include_binary_get_global_scope(this_ext, denv->sbin);
	global_scope = sieve_variable_scope_binary_get(global_vars);
	vars = sieve_variable_scope_get_variables(global_scope, &var_count);

	sieve_code_descend(denv);

	for ( i = 0; i < count; i++ ) {
		unsigned int index;
		
		sieve_code_mark(denv);
		if ( !sieve_binary_read_unsigned(denv->sblock, address, &index) ||
			index >= var_count )
			return FALSE;
			
		sieve_code_dumpf(denv, "%d: VAR[%d]: '%s'", i, index, vars[index]->identifier); 
	}
	 
	return TRUE;
}

/* 
 * Execution
 */
 
static int opc_global_execute
(const struct sieve_runtime_env *renv, sieve_size_t *address)
{
	const struct sieve_extension *this_ext = renv->oprtn->ext;
	struct sieve_variable_scope_binary *global_vars;	
	struct sieve_variable_scope *global_scope;	
	struct sieve_variable_storage *storage;
	struct sieve_variable * const *vars;
	unsigned int var_count, count, i;
		
	if ( !sieve_binary_read_unsigned(renv->sblock, address, &count) ) {
		sieve_runtime_trace_error(renv, 
			"global: count operand invalid");
		return SIEVE_EXEC_BIN_CORRUPT;
	}
	
	global_vars = ext_include_binary_get_global_scope(this_ext, renv->sbin);
	global_scope = sieve_variable_scope_binary_get(global_vars);
	vars = sieve_variable_scope_get_variables(global_scope, &var_count);
	storage = ext_include_interpreter_get_global_variables
		(this_ext, renv->interp);

	for ( i = 0; i < count; i++ ) {
		unsigned int index;
		
		if ( !sieve_binary_read_unsigned(renv->sblock, address, &index) ) {
			sieve_runtime_trace_error(renv, 
				"global: variable index operand invalid");
			return SIEVE_EXEC_BIN_CORRUPT;
		}
		
		if ( index >= var_count ) {
			sieve_runtime_trace_error(renv, 
				"global: variable index %u is invalid in global storage (> %u)", 
				index, var_count);
			return SIEVE_EXEC_BIN_CORRUPT;
		}

		sieve_runtime_trace(renv, SIEVE_TRLVL_COMMANDS,
			"global: exporting variable '%s' [gvid: %u, vid: %u]",
			vars[index]->identifier, i, index);
		
		/* Make sure variable is initialized (export) */
		(void)sieve_variable_get_modifiable(storage, index, NULL); 
	}

	return SIEVE_EXEC_OK;
}


