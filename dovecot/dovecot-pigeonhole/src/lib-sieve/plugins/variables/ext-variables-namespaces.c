/* Copyright (c) 2002-2011 Pigeonhole authors, see the included COPYING file
 */

#include "sieve-common.h"
#include "sieve-commands.h"
#include "sieve-code.h"
#include "sieve-binary.h"
#include "sieve-dump.h"

#include "ext-variables-common.h"
#include "ext-variables-namespaces.h"

#include <ctype.h>

/*
 * Namespace registry
 */

void sieve_variables_namespace_register
(const struct sieve_extension *var_ext, struct sieve_validator *valdtr,
	const struct sieve_extension *ext,
	const struct sieve_variables_namespace_def *nspc_def) 
{
	struct ext_variables_validator_context *ctx = 
		ext_variables_validator_context_get(var_ext, valdtr);
	
	sieve_validator_object_registry_add(ctx->namespaces, ext, &nspc_def->obj_def);
}

bool ext_variables_namespace_exists
(const struct sieve_extension *var_ext, struct sieve_validator *valdtr,
	const char *identifier) 
{
	struct ext_variables_validator_context *ctx = 
		ext_variables_validator_context_get(var_ext, valdtr);

	return sieve_validator_object_registry_find
		(ctx->namespaces, identifier, NULL);
}

const struct sieve_variables_namespace *ext_variables_namespace_create_instance
(const struct sieve_extension *var_ext, struct sieve_validator *valdtr,
	struct sieve_command *cmd, const char *identifier) 
{
	struct ext_variables_validator_context *ctx = 
		ext_variables_validator_context_get(var_ext, valdtr);
	struct sieve_object object;
	struct sieve_variables_namespace *nspc;
	pool_t pool;

	if ( !sieve_validator_object_registry_find
		(ctx->namespaces, identifier, &object) )
		return NULL;

	pool = sieve_command_pool(cmd);
	nspc = p_new(pool, struct sieve_variables_namespace, 1);
	nspc->object = object;
	nspc->def = (const struct sieve_variables_namespace_def *) object.def;

  return nspc;
}

/*
 * Namespace variable argument
 */

struct arg_namespace_variable {
	const struct sieve_variables_namespace *namespace;

	void *data;
};

static bool arg_namespace_generate
(const struct sieve_codegen_env *cgenv, struct sieve_ast_argument *arg, 
	struct sieve_command *context ATTR_UNUSED);

const struct sieve_argument_def namespace_argument = { 
	"@namespace", 
	NULL, NULL, NULL, NULL,
	arg_namespace_generate 
};

bool ext_variables_namespace_argument_activate
(const struct sieve_extension *this_ext, struct sieve_validator *valdtr,
	struct sieve_ast_argument *arg, struct sieve_command *cmd, 
	ARRAY_TYPE(sieve_variable_name) *var_name, bool assignment)
{
	pool_t pool = sieve_command_pool(cmd);
	struct sieve_ast *ast = arg->ast;
	const struct sieve_variables_namespace *nspc;
	struct arg_namespace_variable *var;
	const struct sieve_variable_name *name_element = array_idx(var_name, 0);
	void *var_data;

	nspc = ext_variables_namespace_create_instance
		(this_ext, valdtr, cmd, str_c(name_element->identifier));
	if ( nspc == NULL ) {
		sieve_argument_validate_error(valdtr, arg, 
			"referring to variable in unknown namespace '%s'", 
			str_c(name_element->identifier));
		return FALSE;
	}

	if ( nspc->def != NULL && nspc->def->validate != NULL &&
		!nspc->def->validate
			(valdtr, nspc, arg, cmd, var_name, &var_data, assignment) ) {
		return FALSE;
	}

	var = p_new(pool, struct arg_namespace_variable, 1);
	var->namespace = nspc;
	var->data = var_data;
	
	arg->argument = sieve_argument_create(ast, &namespace_argument, this_ext, 0);
	arg->argument->data = (void *) var;
	
	return TRUE;
}

struct sieve_ast_argument *ext_variables_namespace_argument_create
(const struct sieve_extension *this_ext, 
	struct sieve_validator *valdtr, struct sieve_ast_argument *parent_arg, 
	struct sieve_command *cmd, ARRAY_TYPE(sieve_variable_name) *var_name)
{
	struct sieve_ast *ast = parent_arg->ast;
	struct sieve_ast_argument *new_arg;

	new_arg = sieve_ast_argument_create(ast, sieve_ast_argument_line(parent_arg));
	new_arg->type = SAAT_STRING;
	
	if ( !ext_variables_namespace_argument_activate
		(this_ext, valdtr, new_arg, cmd, var_name, FALSE) )
		return NULL;
	
	return new_arg;
}

static bool arg_namespace_generate
(const struct sieve_codegen_env *cgenv, struct sieve_ast_argument *arg, 
	struct sieve_command *cmd)
{
	struct sieve_argument *argument = arg->argument;
	struct arg_namespace_variable *var = 
		(struct arg_namespace_variable *) argument->data;
	const struct sieve_variables_namespace *nspc = var->namespace;

	if ( nspc->def != NULL && nspc->def->generate != NULL )
		return nspc->def->generate(cgenv, nspc, arg, cmd, var->data);

	return TRUE;
}

/*
 * Namespace variable operands
 */

const struct sieve_operand_class sieve_variables_namespace_operand_class = 
	{ "variable-namespace" };

static bool opr_namespace_variable_dump
	(const struct sieve_dumptime_env *denv, const struct sieve_operand *oprnd,
		sieve_size_t *address);
static int opr_namespace_variable_read
	(const struct sieve_runtime_env *renv, const struct sieve_operand *oprnd,
		sieve_size_t *address, string_t **str_r);

static const struct sieve_opr_string_interface namespace_variable_interface = { 
	opr_namespace_variable_dump,
	opr_namespace_variable_read
};
		
const struct sieve_operand_def namespace_variable_operand = { 
	"namespace", 
	&variables_extension, 
	EXT_VARIABLES_OPERAND_NAMESPACE_VARIABLE,
	&string_class,
	&namespace_variable_interface
};

void sieve_variables_opr_namespace_variable_emit
(struct sieve_binary_block *sblock, const struct sieve_extension *var_ext,
	const struct sieve_extension *ext,
	const struct sieve_variables_namespace_def *nspc_def)
{ 
	sieve_operand_emit(sblock, var_ext, &namespace_variable_operand);
	sieve_opr_object_emit(sblock, ext, &nspc_def->obj_def);
}

static bool opr_namespace_variable_dump
(const struct sieve_dumptime_env *denv, const struct sieve_operand *oprnd,
	sieve_size_t *address)
{
	struct sieve_variables_namespace nspc;
	struct sieve_operand nsoprnd; 

	if ( !sieve_operand_read(denv->sblock, address, NULL, &nsoprnd) ) {
		return FALSE;
	}

	if ( !sieve_opr_object_read_data
		(denv->sblock, &nsoprnd, &sieve_variables_namespace_operand_class, address, 
			&nspc.object) ) {
		return FALSE;
  }

	nspc.def = (const struct sieve_variables_namespace_def *) nspc.object.def;

	if ( nspc.def == NULL || nspc.def->dump_variable == NULL )
		return FALSE;

	return nspc.def->dump_variable(denv, &nspc, oprnd, address);
}

static int opr_namespace_variable_read
(const struct sieve_runtime_env *renv, const struct sieve_operand *oprnd,
	sieve_size_t *address, string_t **str_r)
{
	struct sieve_variables_namespace nspc;

	if ( !sieve_opr_object_read
		(renv, &sieve_variables_namespace_operand_class, address, &nspc.object) ) {
		sieve_runtime_trace_operand_error(renv, oprnd,
			"variable namespace operand corrupt: failed to read");
		return SIEVE_EXEC_BIN_CORRUPT;
	}

	nspc.def = (const struct sieve_variables_namespace_def *) nspc.object.def;

	if ( nspc.def == NULL || nspc.def->read_variable == NULL )
		return SIEVE_EXEC_FAILURE;
		
	return nspc.def->read_variable(renv, &nspc, oprnd, address, str_r);
}



