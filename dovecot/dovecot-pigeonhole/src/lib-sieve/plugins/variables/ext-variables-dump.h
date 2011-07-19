/* Copyright (c) 2002-2011 Pigeonhole authors, see the included COPYING file
 */

#ifndef __EXT_VARIABLES_DUMP_H
#define __EXT_VARIABLES_DUMP_H

#include "sieve-common.h"

/*
 * Code dump context
 */
 
bool ext_variables_code_dump
	(const struct sieve_extension *ext, const struct sieve_dumptime_env *denv,
		sieve_size_t *address);

/*
 * Variable identifier dump
 */
 
const char *ext_variables_dump_get_identifier
(const struct sieve_extension *var_ext, const struct sieve_dumptime_env *denv,
	const struct sieve_extension *ext, unsigned int index);

#endif /* __EXT_VARIABLES_DUMP_H */
