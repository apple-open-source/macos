/* Copyright (c) 2002-2011 Pigeonhole authors, see the included COPYING file
 */

#include "lib.h"
#include "hash.h"
#include "str.h"
#include "array.h"

#include "sieve-common.h"
#include "sieve-extensions.h"
#include "sieve-ast.h"
#include "sieve-binary.h"
#include "sieve-code.h"
#include "sieve-match-types.h"
#include "sieve-commands.h"
#include "sieve-validator.h"
#include "sieve-generator.h"
#include "sieve-dump.h"
#include "sieve-interpreter.h"

#include "ext-variables-common.h"
#include "ext-variables-limits.h"
#include "ext-variables-name.h"
#include "ext-variables-dump.h"
#include "ext-variables-operands.h"

/* 
 * Variable operand 
 */

static bool opr_variable_dump
	(const struct sieve_dumptime_env *denv, const struct sieve_operand *oprnd,
		sieve_size_t *address);
static int opr_variable_read_value
	(const struct sieve_runtime_env *renv, const struct sieve_operand *oprnd,
		sieve_size_t *address, string_t **str_r);

const struct sieve_opr_string_interface variable_interface = { 
	opr_variable_dump,
	opr_variable_read_value
};
		
const struct sieve_operand_def variable_operand = { 
	"variable", 
	&variables_extension, 
	EXT_VARIABLES_OPERAND_VARIABLE,
	&string_class,
	&variable_interface
};

void sieve_variables_opr_variable_emit
(struct sieve_binary_block *sblock, const struct sieve_extension *var_ext, 
	struct sieve_variable *var) 
{
	if ( var->ext == NULL ) {
		/* Default variable storage */
		(void) sieve_operand_emit(sblock, var_ext, &variable_operand);
		(void) sieve_binary_emit_byte(sblock, 0); /* Default */
		(void) sieve_binary_emit_unsigned(sblock, var->index);
		return;
	} 

	(void) sieve_operand_emit(sblock, var_ext, &variable_operand);
	(void) sieve_binary_emit_extension(sblock, var->ext, 1); /* Extension */
	(void) sieve_binary_emit_unsigned(sblock, var->index);
}

static bool opr_variable_dump
(const struct sieve_dumptime_env *denv, const struct sieve_operand *oprnd,
	sieve_size_t *address) 
{
	const struct sieve_extension *this_ext = oprnd->ext;
	unsigned int index = 0;
	const struct sieve_extension *ext;
	unsigned int code = 1; /* Initially set to offset value */
	const char *identifier;

	if ( !sieve_binary_read_extension(denv->sblock, address, &code, &ext) )
		return FALSE;
	
	if ( !sieve_binary_read_unsigned(denv->sblock, address, &index) )
		return FALSE;
		
	identifier = ext_variables_dump_get_identifier(this_ext, denv, ext, index);
	identifier = identifier == NULL ? "??" : identifier;

	if ( oprnd->field_name != NULL ) 
		sieve_code_dumpf(denv, "%s: VAR[%s] ${%s}", 
			oprnd->field_name, sieve_ext_variables_get_varid(ext, index), identifier);
	else
		sieve_code_dumpf(denv, "VAR[%s] ${%s}", 
			sieve_ext_variables_get_varid(ext, index), identifier);

	return TRUE;
}

static int opr_variable_read_value
(const struct sieve_runtime_env *renv, const struct sieve_operand *oprnd,
	sieve_size_t *address, string_t **str_r)
{ 
	const struct sieve_extension *this_ext = oprnd->ext;
	const struct sieve_extension *ext;
	unsigned int code = 1; /* Initially set to offset value */
	struct sieve_variable_storage *storage;
	unsigned int index = 0;
	
	if ( !sieve_binary_read_extension(renv->sblock, address, &code, &ext) ) {
		sieve_runtime_trace_operand_error(renv, oprnd,
			"variable operand corrupt: invalid extension byte");
		return SIEVE_EXEC_BIN_CORRUPT;
	}

	storage = sieve_ext_variables_runtime_get_storage
		(this_ext, renv, ext);
	if ( storage == NULL ) {
		sieve_runtime_trace_operand_error(renv, oprnd,
			"variable operand corrupt: extension has no storage");
		return SIEVE_EXEC_BIN_CORRUPT;
	}
	
	if ( sieve_binary_read_unsigned(renv->sblock, address, &index) ) {
		/* Parameter str can be NULL if we are requested to only skip and not 
		 * actually read the argument.
		 */
		if ( str_r != NULL ) {
			if ( !sieve_variable_get(storage, index, str_r) )
				return SIEVE_EXEC_FAILURE;
		
			if ( *str_r == NULL ) *str_r = t_str_new(0);
		}

		return SIEVE_EXEC_OK;
	}
	
	sieve_runtime_trace_operand_error(renv, oprnd,
		"variable operand corrupt: invalid variable index");
	return SIEVE_EXEC_BIN_CORRUPT;
}
		
int sieve_variable_operand_read_data
(const struct sieve_runtime_env *renv, struct sieve_operand *oprnd, 
	sieve_size_t *address, const char *field_name,
	struct sieve_variable_storage **storage_r, unsigned int *var_index_r)
{
	const struct sieve_extension *ext;
	unsigned int code = 1; /* Initially set to offset value */
	unsigned int idx = 0;
	
	oprnd->field_name = field_name;

	if ( !sieve_operand_is_variable(oprnd) ) {
		sieve_runtime_trace_operand_error(renv, oprnd, 
			"expected variable operand but found %s",	sieve_operand_name(oprnd));
		return SIEVE_EXEC_BIN_CORRUPT;
	}

	if ( !sieve_binary_read_extension(renv->sblock, address, &code, &ext) ) {
		sieve_runtime_trace_operand_error(renv, oprnd,
			"variable operand corrupt: invalid extension byte");
		return SIEVE_EXEC_BIN_CORRUPT;
	}
		
	*storage_r = sieve_ext_variables_runtime_get_storage
		(oprnd->ext, renv, ext);
	if ( *storage_r == NULL )	{
		sieve_runtime_trace_operand_error(renv, oprnd,
			"variable operand corrupt: extension has no storage");
		return SIEVE_EXEC_BIN_CORRUPT;
	}
	
	if ( !sieve_binary_read_unsigned(renv->sblock, address, &idx) ) {
		sieve_runtime_trace_operand_error(renv, oprnd, 
			"variable operand corrupt: invalid variable index");
		return SIEVE_EXEC_BIN_CORRUPT;
	}

	*var_index_r = idx;
	return SIEVE_EXEC_OK;
}

int sieve_variable_operand_read
(const struct sieve_runtime_env *renv, sieve_size_t *address,
	const char *field_name, struct sieve_variable_storage **storage_r, 
	unsigned int *var_index_r)
{
	struct sieve_operand operand;
	int ret;

	if ( (ret=sieve_operand_runtime_read(renv, address, field_name, &operand))
		<= 0)
		return ret;

	return sieve_variable_operand_read_data
		(renv, &operand, address, field_name, storage_r, var_index_r);
}
	
/* 
 * Match value operand 
 */

static bool opr_match_value_dump
	(const struct sieve_dumptime_env *denv, const struct sieve_operand *oprnd,
		sieve_size_t *address);
static int opr_match_value_read
	(const struct sieve_runtime_env *renv, const struct sieve_operand *oprnd,
		sieve_size_t *address, string_t **str_r);

const struct sieve_opr_string_interface match_value_interface = { 
	opr_match_value_dump,
	opr_match_value_read
};
		
const struct sieve_operand_def match_value_operand = { 
	"match-value", 
	&variables_extension, 
	EXT_VARIABLES_OPERAND_MATCH_VALUE,
	&string_class,
	&match_value_interface
};	

void sieve_variables_opr_match_value_emit
(struct sieve_binary_block *sblock, const struct sieve_extension *var_ext, 
	unsigned int index) 
{
	(void) sieve_operand_emit(sblock, var_ext, &match_value_operand);
	(void) sieve_binary_emit_unsigned(sblock, index);
}

static bool opr_match_value_dump
(const struct sieve_dumptime_env *denv,	const struct sieve_operand *oprnd,
	sieve_size_t *address) 
{
	unsigned int index = 0;
	
	if (sieve_binary_read_unsigned(denv->sblock, address, &index) ) {
		if ( oprnd->field_name != NULL )
			sieve_code_dumpf
				(denv, "%s: MATCHVAL %lu", oprnd->field_name, (unsigned long) index);
		else
			sieve_code_dumpf(denv, "MATCHVAL %lu", (unsigned long) index);

		return TRUE;
	}
	
	return FALSE;
}

static int opr_match_value_read
(const struct sieve_runtime_env *renv, const struct sieve_operand *oprnd,
	sieve_size_t *address, string_t **str_r)
{ 
	unsigned int index = 0;
			
	if ( sieve_binary_read_unsigned(renv->sblock, address, &index) ) {
		/* Parameter str can be NULL if we are requested to only skip and not 
		 * actually read the argument.
		 	*/
		if ( str_r != NULL ) {
			sieve_match_values_get(renv, index, str_r);
		
			if ( *str_r == NULL ) 
				*str_r = t_str_new(0);
			else if ( str_len(*str_r) > EXT_VARIABLES_MAX_VARIABLE_SIZE ) 
				str_truncate(*str_r, EXT_VARIABLES_MAX_VARIABLE_SIZE);
		}

		return SIEVE_EXEC_OK;
	}
	
	sieve_runtime_trace_operand_error(renv, oprnd,
		"match value operand corrupt: invalid index data");
	return SIEVE_EXEC_BIN_CORRUPT;
}
