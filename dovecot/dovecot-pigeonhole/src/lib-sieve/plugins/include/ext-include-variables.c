/* Copyright (c) 2002-2011 Pigeonhole authors, see the included COPYING file
 */

#include "sieve-common.h"
#include "sieve-error.h"
#include "sieve-script.h"
#include "sieve-ast.h"
#include "sieve-binary.h"
#include "sieve-commands.h"
#include "sieve-validator.h"
#include "sieve-generator.h"
#include "sieve-interpreter.h"
#include "sieve-dump.h"

#include "sieve-ext-variables.h"

#include "ext-include-common.h"
#include "ext-include-binary.h"
#include "ext-include-variables.h"

/* 
 * Variable import-export
 */
 
struct sieve_variable *ext_include_variable_import_global
(struct sieve_validator *valdtr, struct sieve_command *cmd, 
	const char *variable)
{
	const struct sieve_extension *this_ext = cmd->ext;
	struct sieve_ast *ast = cmd->ast_node->ast;
	struct ext_include_ast_context *ctx = 
		ext_include_get_ast_context(this_ext, ast);
	struct ext_include_context *ectx = ext_include_get_context(this_ext);
	struct sieve_variable_scope *local_scope;
	struct sieve_variable_scope *global_scope = ctx->global_vars;
	struct sieve_variable *global_var = NULL, *local_var;

	/* Sanity safeguard */	
	i_assert ( ctx->global_vars != NULL );

	/* Get/Declare the variable in the global scope */
	global_var = sieve_variable_scope_get_variable(global_scope, variable, TRUE);

	/* Check whether scope is over its size limit */
	if ( global_var == NULL ) {
		sieve_command_validate_error(valdtr, cmd,
			"declaration of new global variable '%s' exceeds the limit "
			"(max variables: %u)", 
			variable, sieve_variables_get_max_scope_size());
		return NULL;
	}
	
	/* Import the global variable into the local script scope */
	local_scope = sieve_ext_variables_get_local_scope(ectx->var_ext, valdtr);

	local_var = sieve_variable_scope_get_variable(local_scope, variable, FALSE);
	if ( local_var != NULL && local_var->ext != this_ext ) {
		/* FIXME: indicate location of conflicting set statement */
		sieve_command_validate_error(valdtr, cmd,
			"declaration of new global variable '%s' conflicts with earlier local "
			"use", variable);
		return NULL;
	}

	return sieve_variable_scope_import(local_scope, global_var);
}

/*
 * Binary symbol table
 */
 
bool ext_include_variables_save
(struct sieve_binary_block *sblock, 
	struct sieve_variable_scope_binary *global_vars)
{
	struct sieve_variable_scope *global_scope =
		sieve_variable_scope_binary_get(global_vars);
	unsigned int count = sieve_variable_scope_size(global_scope);
	sieve_size_t jump;

	sieve_binary_emit_unsigned(sblock, count);

	jump = sieve_binary_emit_offset(sblock, 0);

	if ( count > 0 ) {
		unsigned int size, i;
		struct sieve_variable *const *vars = 
			sieve_variable_scope_get_variables(global_scope, &size);

		for ( i = 0; i < size; i++ ) {
			sieve_binary_emit_cstring(sblock, vars[i]->identifier);
		}
	}

	sieve_binary_resolve_offset(sblock, jump);

	return TRUE;
}

bool ext_include_variables_load
(const struct sieve_extension *this_ext, struct sieve_binary_block *sblock, 
	sieve_size_t *offset, struct sieve_variable_scope_binary **global_vars_r)
{
	/* Sanity assert */
	i_assert( *global_vars_r == NULL );

	*global_vars_r = sieve_variable_scope_binary_read
		(this_ext->svinst, this_ext, sblock, offset);

	return ( *global_vars_r != NULL );
}

bool ext_include_variables_dump
(struct sieve_dumptime_env *denv,
	struct sieve_variable_scope_binary *global_vars)
{
	struct sieve_variable_scope *global_scope =
		sieve_variable_scope_binary_get(global_vars);
	unsigned int size;
	struct sieve_variable *const *vars;

	i_assert(global_scope != NULL);

	vars = sieve_variable_scope_get_variables(global_scope, &size);

	if ( size > 0 ) {
		unsigned int i;

		sieve_binary_dump_sectionf(denv, "Global variables");
	
		for ( i = 0; i < size; i++ ) {
			sieve_binary_dumpf(denv, "%3d: '%s' \n", i, vars[i]->identifier);
		}	
	}

	return TRUE;
}

/*
 * Global variables namespace
 */

bool vnspc_global_variables_validate
	(struct sieve_validator *valdtr, const struct sieve_variables_namespace *nspc,
		struct sieve_ast_argument *arg, struct sieve_command *cmd, 
		ARRAY_TYPE(sieve_variable_name) *var_name, void **var_data, 
		bool assignment);
bool vnspc_global_variables_generate
	(const struct sieve_codegen_env *cgenv, 
		const struct sieve_variables_namespace *nspc,
		struct sieve_ast_argument *arg, struct sieve_command *cmd, void *var_data);

static const struct sieve_variables_namespace_def global_variables_namespace = {
	SIEVE_OBJECT("global", NULL, 0),
	vnspc_global_variables_validate,  
	vnspc_global_variables_generate,
	NULL, NULL
};


bool vnspc_global_variables_validate
(struct sieve_validator *valdtr, 
	const struct sieve_variables_namespace *nspc, struct sieve_ast_argument *arg,
	struct sieve_command *cmd ATTR_UNUSED, 
	ARRAY_TYPE(sieve_variable_name) *var_name, void **var_data, 
	bool assignment ATTR_UNUSED)
{
	const struct sieve_extension *this_ext = SIEVE_OBJECT_EXTENSION(nspc);
	struct sieve_ast *ast = arg->ast;
	struct ext_include_ast_context *ctx = 
		ext_include_get_ast_context(this_ext, ast);
	struct sieve_variable *var = NULL;
	const struct sieve_variable_name *name_element;
	const char *variable;

	/* Sanity safeguard */	
	i_assert ( ctx->global_vars != NULL );

	/* Check variable name */

	if ( array_count(var_name) != 2 ) {
		sieve_argument_validate_error(valdtr, arg, 
			"invalid variable name within global namespace: "
			"encountered sub-namespace");
		return FALSE;
	}

	name_element = array_idx(var_name, 1);
	if ( name_element->num_variable >= 0 ) {
		sieve_argument_validate_error(valdtr, arg, 
			"invalid variable name within global namespace: "
			"encountered numeric variable name");
		return FALSE;
	}
	
	variable = str_c(name_element->identifier);

	/* Get/Declare the variable in the global scope */

	var = sieve_variable_scope_get_variable(ctx->global_vars, variable, TRUE);

	if ( var == NULL ) {
		sieve_argument_validate_error(valdtr, arg, 
			"(implicit) declaration of new global variable '%s' exceeds the limit "
			"(max variables: %u)", variable, 
			sieve_variables_get_max_scope_size());
		return FALSE;
	}
	
	*var_data = (void *) var;

	return TRUE;
}

bool vnspc_global_variables_generate
(const struct sieve_codegen_env *cgenv, 
	const struct sieve_variables_namespace *nspc,	
	struct sieve_ast_argument *arg ATTR_UNUSED,	
	struct sieve_command *cmd ATTR_UNUSED, void *var_data)
{
	const struct sieve_extension *this_ext = SIEVE_OBJECT_EXTENSION(nspc);
	struct ext_include_context *ectx = ext_include_get_context(this_ext);
	struct sieve_variable *var = (struct sieve_variable *) var_data;
	
	sieve_variables_opr_variable_emit(cgenv->sblock, ectx->var_ext, var);

	return TRUE;
}

void ext_include_variables_global_namespace_init
(const struct sieve_extension *this_ext, struct sieve_validator *valdtr)
{
	struct ext_include_context *ectx = ext_include_get_context(this_ext);

	sieve_variables_namespace_register
		(ectx->var_ext, valdtr, this_ext, &global_variables_namespace);
}



