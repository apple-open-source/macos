/* Copyright (c) 2002-2011 Pigeonhole authors, see the included COPYING file
 */

#ifndef __SIEVE_STORAGE_QUOTA_H
#define __SIEVE_STORAGE_QUOTA_H

#include "lib.h"

#include "sieve-storage.h"

enum sieve_storage_quota {
	SIEVE_STORAGE_QUOTA_NONE,
	SIEVE_STORAGE_QUOTA_MAXSIZE,
	SIEVE_STORAGE_QUOTA_MAXSCRIPTS,
	SIEVE_STORAGE_QUOTA_MAXSTORAGE
};

bool sieve_storage_quota_validsize
	(struct sieve_storage *storage, size_t size, uint64_t *limit_r);

int sieve_storage_quota_havespace
	(struct sieve_storage *storage, const char *scriptname, size_t size,
		enum sieve_storage_quota *quota_r, uint64_t *limit_r);
    
#endif /* __SIEVE_STORAGE_QUOTA_H */
