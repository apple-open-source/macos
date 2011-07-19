/* Copyright (c) 2002-2011 Pigeonhole authors, see the included COPYING file
 */

#ifndef __EXT_VARIABLES_OPERANDS_H
#define __EXT_VARIABLES_OPERANDS_H

#include "lib.h"
#include "hash.h"
#include "str.h"
#include "array.h"

#include "sieve-common.h"
#include "ext-variables-common.h"

/* 
 * Variable operand 
 */
		
extern const struct sieve_operand_def variable_operand;	

bool ext_variables_opr_variable_read
	(const struct sieve_runtime_env *renv, sieve_size_t *address, 
		struct sieve_variable_storage **storage, unsigned int *var_index);

/* 
 * Match value operand 
 */
		
extern const struct sieve_operand_def match_value_operand;	

/*
 * Variable string operand
 */

void ext_variables_opr_variable_string_emit
	(struct sieve_binary *sbin, unsigned int elements);

	
#endif /* __EXT_VARIABLES_OPERANDS_H */

