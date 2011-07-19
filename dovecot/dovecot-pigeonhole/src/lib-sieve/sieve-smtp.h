/* Copyright (c) 2002-2011 Pigeonhole authors, see the included COPYING file
 */

#ifndef __SIEVE_SMTP_H
#define __SIEVE_SMTP_H

#include "sieve-common.h"

bool sieve_smtp_available
	(const struct sieve_script_env *senv);

void *sieve_smtp_open
	(const struct sieve_script_env *senv, const char *destination,
    	const char *return_path, FILE **file_r);

bool sieve_smtp_close
	(const struct sieve_script_env *senv, void *handle);

#endif /* __SIEVE_SMTP_H */
