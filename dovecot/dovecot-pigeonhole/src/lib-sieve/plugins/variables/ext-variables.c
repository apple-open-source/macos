/* Copyright (c) 2002-2011 Pigeonhole authors, see the included COPYING file
 */

/* Extension variables
 * -------------------
 *
 * Authors: Stephan Bosch
 * Specification: RFC 5229
 * Implementation: full
 * Status: testing
 *
 */

#include "lib.h"
#include "str.h"
#include "unichar.h"

#include "sieve-extensions.h"
#include "sieve-commands.h"
#include "sieve-binary.h"
#include "sieve-interpreter.h"

#include "sieve-validator.h"

#include "ext-variables-common.h"
#include "ext-variables-arguments.h"
#include "ext-variables-operands.h"
#include "ext-variables-namespaces.h"
#include "ext-variables-modifiers.h"
#include "ext-variables-dump.h"

/* 
 * Operations 
 */

const struct sieve_operation_def *ext_variables_operations[] = {
	&cmd_set_operation, 
	&tst_string_operation
};

/* 
 * Operands 
 */

const struct sieve_operand_def *ext_variables_operands[] = {
	&variable_operand, 
	&match_value_operand,
	&namespace_variable_operand,
	&modifier_operand
};

/* 
 * Extension 
 */

static bool ext_variables_validator_load
	(const struct sieve_extension *ext, struct sieve_validator *validator);
	
const struct sieve_extension_def variables_extension = { 
	"variables", 
	NULL, NULL,
	ext_variables_validator_load, 
	ext_variables_generator_load,
	ext_variables_interpreter_load,
	NULL, NULL, 
	ext_variables_code_dump,
	SIEVE_EXT_DEFINE_OPERATIONS(ext_variables_operations), 
	SIEVE_EXT_DEFINE_OPERANDS(ext_variables_operands)
};

static bool ext_variables_validator_load
(const struct sieve_extension *ext, struct sieve_validator *validator)
{
	sieve_validator_argument_override
		(validator, SAT_VAR_STRING, ext, &variable_string_argument); 
		
	sieve_validator_register_command(validator, ext, &cmd_set);
	sieve_validator_register_command(validator, ext, &tst_string);
	
	ext_variables_validator_initialize(ext, validator);

	return TRUE;
}

