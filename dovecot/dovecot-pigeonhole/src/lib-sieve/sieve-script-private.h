/* Copyright (c) 2002-2011 Pigeonhole authors, see the included COPYING file
 */

#ifndef __SIEVE_SCRIPT_PRIVATE_H
#define __SIEVE_SCRIPT_PRIVATE_H

#include "sieve-script.h"

/*
 * Script object
 */

struct sieve_script {
	pool_t pool;
	unsigned int refcount;

	struct sieve_instance *svinst;

	struct stat st;
	struct stat lnk_st;
	time_t mtime;

	struct sieve_error_handler *ehandler;

	/* Parameters */
	const char *name;
	const char *basename;
	const char *filename;
	const char *path;
	const char *dirpath;
	const char *binpath;

	/* Stream */
	int fd; /* FIXME: we could use the stream's autoclose facility */
	struct istream *stream;
};

struct sieve_script *sieve_script_init
(struct sieve_script *script, struct sieve_instance *svinst,
	const char *path, const char *name, struct sieve_error_handler *ehandler, 
	enum sieve_error *error_r);

#endif /* __SIEVE_SCRIPT_PRIVATE_H */
