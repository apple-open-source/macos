/* Copyright (c) 2002-2011 Pigeonhole authors, see the included COPYING file
 */

#ifndef __SIEVE_STORAGE_PRIVATE_H
#define __SIEVE_STORAGE_PRIVATE_H

#include "sieve.h"
#include "sieve-error-private.h"

#include "sieve-storage.h"


enum sieve_storage_flags {
	/* Print debugging information while initializing the storage */
	SIEVE_STORAGE_FLAG_DEBUG     = 0x01,
	/* Use CRLF linefeeds when saving mails. */
	SIEVE_STORAGE_FLAG_SAVE_CRLF   = 0x02,
};

#define SIEVE_READ_BLOCK_SIZE (1024*8)

struct sieve_storage;

struct sieve_storage_ehandler {
	struct sieve_error_handler handler;
	struct sieve_storage *storage;
};

/* All methods returning int return either TRUE or FALSE. */
struct sieve_storage {
	pool_t pool;
	struct sieve_instance *svinst;

	char *name;
	char *dir;
	bool debug;

	/* Private */	
	char *active_path;
	char *active_fname;
	char *link_path;
	char *error;
	char *user; /* name of user accessing the storage */

	mode_t dir_create_mode;
	mode_t file_create_mode;
	gid_t file_create_gid;

	uint64_t max_scripts;
	uint64_t max_storage;

	enum sieve_error error_code;
	struct sieve_error_handler *ehandler;

	enum sieve_storage_flags flags;
};

struct sieve_script *sieve_storage_script_init_from_path
	(struct sieve_storage *storage, const char *path, const char *scriptname);

#endif

