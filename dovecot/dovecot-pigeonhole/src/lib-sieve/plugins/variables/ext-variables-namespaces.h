/* Copyright (c) 2002-2011 Pigeonhole authors, see the included COPYING file
 */

#ifndef __EXT_VARIABLES_NAMESPACES_H
#define __EXT_VARIABLES_NAMESPACES_H

#include "sieve-common.h"

#include "ext-variables-common.h"
#include "sieve-ext-variables.h"

/*
 * Namespace registry
 */

bool ext_variables_namespace_exists
	(const struct sieve_extension *var_ext, struct sieve_validator *valdtr,
		const char *identifier);
const struct sieve_variables_namespace *ext_variables_namespace_create_instance
	(const struct sieve_extension *var_ext, struct sieve_validator *valdtr,
		struct sieve_command *cmd, const char *identifier);
	
void ext_variables_register_core_namespaces
	(const struct sieve_extension *var_ext,
		struct ext_variables_validator_context *ctx);

/*
 * Namespace argument
 */

struct sieve_ast_argument *ext_variables_namespace_argument_create
	(const struct sieve_extension *this_ext, 
		struct sieve_validator *valdtr, struct sieve_ast_argument *parent_arg, 
		struct sieve_command *cmd, ARRAY_TYPE(sieve_variable_name) *var_name);
bool ext_variables_namespace_argument_activate
	(const struct sieve_extension *this_ext, struct sieve_validator *valdtr,
		struct sieve_ast_argument *arg, struct sieve_command *cmd, 
		ARRAY_TYPE(sieve_variable_name) *var_name, bool assignment);
	
/*
 * Namespace operand
 */

extern const struct sieve_operand_def namespace_variable_operand;

#endif /* __EXT_VARIABLES_NAMESPACES_H */
