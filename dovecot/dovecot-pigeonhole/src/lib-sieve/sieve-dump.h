/* Copyright (c) 2002-2011 Pigeonhole authors, see the included COPYING file
 */

#ifndef __SIEVE_DUMP_H
#define __SIEVE_DUMP_H

#include "sieve-common.h"
#include "sieve-code-dumper.h"
#include "sieve-binary-dumper.h"

/*
 * Dumptime environment
 */

struct sieve_dumptime_env {
	/* Dumpers */
	struct sieve_instance *svinst;
	struct sieve_binary_dumper *dumper;
	struct sieve_code_dumper *cdumper;

	/* Binary */
	struct sieve_binary *sbin;
	struct sieve_binary_block *sblock;

	/* Code position */
	const struct sieve_operation *oprtn;
	sieve_size_t offset;
	
	/* Output stream */
	struct ostream *stream;
};

#endif /* __SIEVE_DUMP_H */
