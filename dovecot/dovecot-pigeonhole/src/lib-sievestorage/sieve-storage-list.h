/* Copyright (c) 2002-2011 Pigeonhole authors, see the included COPYING file
 */

#ifndef __SIEVE_STORAGE_LIST_H
#define __SIEVE_STORAGE_LIST_H

#include "lib.h"
#include "str.h"
#include "sieve-storage.h"

struct sieve_list_context;

/* Create a context for listing the scripts in the storage */
struct sieve_list_context *sieve_storage_list_init
	(struct sieve_storage *storage);

/* Get the next script in the storage. */
const char *sieve_storage_list_next
	(struct sieve_list_context *ctx, bool *active);

/* Destroy the listing context */
int sieve_storage_list_deinit
	(struct sieve_list_context **ctx);

#endif


	
    
