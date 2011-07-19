/* Copyright (c) 2002-2011 Pigeonhole authors, see the included COPYING file
 */

#ifndef __SIEVE_RUNTIME_H
#define __SIEVE_RUNTIME_H

#include "sieve-common.h"

/*
 * Runtime environment
 */

struct sieve_runtime_env {
	/* Interpreter */
	struct sieve_instance *svinst;
	struct sieve_interpreter *interp;

	/* Executing script */
	struct sieve_script *script;
	const struct sieve_script_env *scriptenv;
	struct sieve_exec_status *exec_status;

	/* Executing binary */
	struct sieve_binary *sbin;
	struct sieve_binary_block *sblock;
	
	/* Current code */
	sieve_size_t pc;
	const struct sieve_operation *oprtn; 	
	
	/* Tested message */
	const struct sieve_message_data *msgdata;
	struct sieve_message_context *msgctx;

	/* Filter result */
	struct sieve_result *result;

	/* Runtime tracing */
	struct sieve_runtime_trace *trace;
};

#endif /* __SIEVE_RUNTIME_H */
