/* Copyright (c) 2002-2011 Pigeonhole authors, see the included COPYING file
 */

#ifndef __SIEVE_CODE_DUMPER_H
#define __SIEVE_CODE_DUMPER_H

#include "sieve-common.h"

struct sieve_code_dumper;

struct sieve_code_dumper *sieve_code_dumper_create
	(struct sieve_dumptime_env *denv);
void sieve_code_dumper_free
	(struct sieve_code_dumper **dumper);
pool_t sieve_code_dumper_pool
	(struct sieve_code_dumper *dumper);
	
/* 
 * Extension support
 */

struct sieve_code_dumper_extension {
	const struct sieve_extension_def *ext;	

	void (*free)(struct sieve_code_dumper *dumper, void *context);
};

void sieve_dump_extension_register
(struct sieve_code_dumper *dumper, const struct sieve_extension *ext,
	const struct sieve_code_dumper_extension *dump_ext, void *context);
void sieve_dump_extension_set_context
	(struct sieve_code_dumper *dumper, const struct sieve_extension *ext, 
		void *context);
void *sieve_dump_extension_get_context
	(struct sieve_code_dumper *dumper, const struct sieve_extension *ext); 
	
/* Dump functions */	
	
void sieve_code_dumpf
	(const struct sieve_dumptime_env *denv, const char *fmt, ...)
		ATTR_FORMAT(2, 3);

void sieve_code_mark(const struct sieve_dumptime_env *denv);
void sieve_code_mark_specific
	(const struct sieve_dumptime_env *denv, sieve_size_t location);
void sieve_code_descend(const struct sieve_dumptime_env *denv);
void sieve_code_ascend(const struct sieve_dumptime_env *denv);

/* Operations and operands */
	
bool sieve_code_dumper_print_optional_operands
	(const struct sieve_dumptime_env *denv, sieve_size_t *address);

/* Code dump (debugging purposes) */

void sieve_code_dumper_run(struct sieve_code_dumper *dumper);

#endif /* __SIEVE_CODE_DUMPER_H */
