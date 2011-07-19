/* Copyright (c) 2002-2011 Pigeonhole authors, see the included COPYING file
 */

#ifndef __SIEVE_EXT_ENVIRONMENT_H
#define __SIEVE_EXT_ENVIRONMENT_H

#include "sieve-common.h"

struct sieve_environment_item {
	const char *name;
	
	const char *value;
	const char *(*get_value)(const struct sieve_script_env *senv);
};

void sieve_ext_environment_item_register
	(const struct sieve_extension *ext, 
		const struct sieve_environment_item *item);

#endif
