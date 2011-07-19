/* Copyright (c) 2002-2011 Pigeonhole authors, see the included COPYING file
 */

#ifndef __EXT_VARIABLES_COMMON_H
#define __EXT_VARIABLES_COMMON_H

#include "sieve-common.h"
#include "sieve-validator.h"

#include "sieve-ext-variables.h"

/*
 * Extension
 */

extern const struct sieve_extension_def variables_extension;

/* 
 * Commands 
 */

extern const struct sieve_command_def cmd_set;
extern const struct sieve_command_def tst_string;

/* 
 * Operands
 */

enum ext_variables_operand {
	EXT_VARIABLES_OPERAND_VARIABLE,
	EXT_VARIABLES_OPERAND_MATCH_VALUE,
	EXT_VARIABLES_OPERAND_NAMESPACE_VARIABLE,
	EXT_VARIABLES_OPERAND_MODIFIER
};

/*
 * Operations
 */

extern const struct sieve_operation_def cmd_set_operation;
extern const struct sieve_operation_def tst_string_operation;

enum ext_variables_opcode {
	EXT_VARIABLES_OPERATION_SET,
	EXT_VARIABLES_OPERATION_STRING
};

/* 
 * Validator context 
 */

struct ext_variables_validator_context {
	bool active;
	
	struct sieve_validator_object_registry *modifiers;
	struct sieve_validator_object_registry *namespaces;
	
	struct sieve_variable_scope *local_scope;
};

void ext_variables_validator_initialize
	(const struct sieve_extension *this_ext, struct sieve_validator *validator);
	
struct ext_variables_validator_context *ext_variables_validator_context_get
	(const struct sieve_extension *this_ext, struct sieve_validator *valdtr);

struct sieve_variable *ext_variables_validator_get_variable
	(const struct sieve_extension *this_ext, struct sieve_validator *validator, 
		const char *variable, bool declare);

/*
 * Code generation
 */
 
bool ext_variables_generator_load
	(const struct sieve_extension *ext, const struct sieve_codegen_env *cgenv);

/*
 * Interpreter context
 */	

bool ext_variables_interpreter_load
	(const struct sieve_extension *ext, const struct sieve_runtime_env *renv, 
		sieve_size_t *address);
	
#endif /* __EXT_VARIABLES_COMMON_H */
